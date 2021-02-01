/*
 * Copyright(c) 2006 iniCom Networks, Inc.
 *
 * This file is part of ioFTPD.
 *
 * ioFTPD is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ioFTPD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ioFTPD; see the file COPYING.  if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <ioFTPD.h>

// max client id ever used
DWORD volatile  dwMaxClientId;
static DWORD volatile  dwActualMaxClientId;

static LPCLIENT volatile  lpClientArray[MAX_CLIENTS];
static CLIENTSLOT      lpClientSlot[MAX_CLIENTS];
static LPCLIENTSLOT    lpClientSlotList;
static CRITICAL_SECTION  csClientArray;
static LPRULE        lpRuleList;
static INT          iDefaultPolicy, iMaxConnectionsPerIp;
static LPCLASS        lpClassList;
static DWORD        dwClassListId;
static LOCKOBJECT      loRuleList;
static LONG volatile   lClientCounter;
static BOOL volatile   bKillShutdownDone;



LPCLIENT LockClient(DWORD dwClientId)
{
	volatile LONG     *pLock;
	volatile LPCLIENT lpClient;

	pLock  = &lpClientSlot[dwClientId].lDataLock;
	while (lpClientArray[dwClientId])
	{
		if (! InterlockedExchange(pLock, TRUE))
		{
			lpClient = lpClientArray[dwClientId];
			if (lpClient) return lpClient;
			InterlockedExchange(pLock, FALSE);
			break;
		}
		SwitchToThread();
	}
	return NULL;
}



VOID UnlockClient(DWORD dwClientId)
{
  InterlockedExchange(&lpClientSlot[dwClientId].lDataLock, FALSE);
}





DWORD RegisterClient(PCONNECTION_INFO pConnectionInfo, LPFTPUSER lpUser)
{
  LPCLIENTSLOT  lpSlot;
  LPCLIENT    lpClient;
  LPSTR      szHostName, szIdent;

  szHostName  = (pConnectionInfo->szHostName ? pConnectionInfo->szHostName : "");
  szIdent    = (pConnectionInfo->szIdent ? pConnectionInfo->szIdent : "*");
  //  Allocate memory
  lpClient = (LPCLIENT)Allocate("ClientInfo", sizeof(CLIENT));
  if (! lpClient) return (DWORD)-1;

  //  Update client structure
  ZeroMemory(lpClient, sizeof(CLIENT));
  lpClient->lpService  = pConnectionInfo->lpService;
  lpClient->lpHostInfo = pConnectionInfo->lpHostInfo;
  lpClient->lpClass    = pConnectionInfo->lpHostInfo->ClassInfo.lpClass;
  lpClient->lpUser     = lpUser;
  lpClient->dwLoginTime  = GetTickCount(); // unused now
  //  Update static structure
  lpClient->Static.dwIdleTickCount  = GetTickCount();
  lpClient->Static.dwOnlineTime = (DWORD) time((time_t*)NULL) ;
  lpClient->Static.Uid    = -1;
  lpClient->Static.ulClientIp  = pConnectionInfo->ClientAddress.sin_addr.s_addr;
  lpClient->Static.usClientPort  = pConnectionInfo->ClientAddress.sin_port;
  _tcsncpy(lpClient->Static.tszServiceName, pConnectionInfo->lpService->tszName, _MAX_NAME);
  strncpy(lpClient->Static.szHostName, szHostName, MAX_HOSTNAME - 1);
  strncpy(lpClient->Static.szIdent, szIdent, MAX_IDENT - 1);

  //  Pop list
  EnterCriticalSection(&csClientArray);
  lpSlot  = lpClientSlotList;
  if (lpSlot) lpClientSlotList  = lpClientSlotList->lpNext;
  if (lpSlot && lpSlot->dwId > dwActualMaxClientId)
  {
	  dwActualMaxClientId = lpSlot->dwId;
	  // fudge value to use for all loops
	  if (dwActualMaxClientId < MAX_CLIENTS-20)
	  {
		  dwMaxClientId = lpSlot->dwId + 20;
	  }
	  else
	  {
		  dwMaxClientId = MAX_CLIENTS;
	  }
  }

  //  Verify slot allocation
  if (! lpSlot)
  {
	  LeaveCriticalSection(&csClientArray);
	  Free(lpClient);
	  return (DWORD)-1;
  }
  lpClient->lClientCount = InterlockedIncrement(&lClientCounter);

  //  Update array
  lpClientSlot[lpSlot->dwId].lDataLock = TRUE;
  lpClientArray[lpSlot->dwId]  = lpClient;

  LeaveCriticalSection(&csClientArray);

  pConnectionInfo->lpClient = lpClient;
  return lpSlot->dwId;
}





LPCLIENT UnregisterClient(DWORD dwClientId)
{
  LPCLIENT  lpClient;

  // need to lock client because SetJobFilter when canceling jobs can end up closing the
  // control connection which triggers the cleanup routine which calls this.  If we were
  // to free it immediately like in the past it might be too soon as SetJobFilter might
  // not have actually completed.

  EnterCriticalSection(&csClientArray);

  while (InterlockedExchange(&lpClientSlot[dwClientId].lDataLock, TRUE)) SwitchToThread();
  lpClient  = lpClientArray[dwClientId];

  // Don't clear the client array and the lock until after the client is cleaned up.
  // During shutdown we want to make sure we don't start cleaning everything else up
  // until the last client is really finished.

  //  Push client slot back to list
  lpClientSlot[dwClientId].lpNext  = lpClientSlotList;
  lpClientSlotList  = &lpClientSlot[dwClientId];
  //  Update client counters
  if (lpClient->Static.Uid != -1)
  {
	  InterlockedDecrement(&lpClient->lpHostInfo->lClients);
	  if (lpClient->lpClass)
	  {
		  InterlockedDecrement(&lpClient->lpClass->lClients);
	  }
	  InterlockedDecrement(&lpClient->lpService->lClients);
  }

  //  Free resources
  UnregisterNetworkAddress(lpClient->lpHostInfo, 1);
  FreeShared(lpClient->Static.tszRealPath);
  FreeShared(lpClient->Static.tszRealDataPath);

  lpClientArray[dwClientId]  = NULL;
  InterlockedExchange(&lpClientSlot[dwClientId].lDataLock, FALSE);

  LeaveCriticalSection(&csClientArray);
  return lpClient;
}




LRESULT ReleasePath(LPTSTR tszRealPath, LPTSTR tszVirtualPath)
{
  LPCLIENT  lpClient;
  LPTSTR    sCompare[2];
  DWORD    n;
  INT      iReturn;

  if ((! tszRealPath && ! tszVirtualPath) ||
    (tszRealPath && tszVirtualPath)) return -1;

  iReturn  = 0;
  sCompare[0]  = (tszRealPath ? tszRealPath : tszVirtualPath);

  //  Go through all users
  for (n = 0;n <= dwMaxClientId ; n++)
  {
    if (! (lpClient = LockClient(n))) continue;

    sCompare[1]  = (tszRealPath ? lpClient->Static.tszRealDataPath : lpClient->Static.tszVirtualDataPath);
    //  Compare
    if (sCompare[1] &&
      lpClient->Static.bTransferStatus &&
      ! iCompare(sCompare[0], sCompare[1]))
    {
      //  Terminate all jobs
      SetJobFilter(n, PRIMARY|SECONDARY|TERTIARY);
      lpClient->Static.dwFlags  |= S_DEAD;
      iReturn++;
    }
    UnlockClient(n);
  }
  return iReturn;
}





LRESULT GetOnlineData(ONLINEDATA *lpOnlineData, DWORD dwClientId)
{
  LPCLIENT    lpClient;
  DWORD       dwTickCount, dwLast;
  PONLINEDATA lpOnline;

  lpClient  = LockClient(dwClientId);
  if (! lpClient) return FALSE;

    //  Copy data
  CopyMemory(lpOnlineData, &lpClient->Static, sizeof(ONLINEDATA));
  //  Shared memory (dummy allocations)
  AllocateShared(lpClient->Static.tszRealPath, NULL, 0);
  AllocateShared(lpClient->Static.tszRealDataPath, NULL, 0);
  dwLast = lpClient->dwTransferLastUpdated;
  UnlockClient(dwClientId);

  // now fix up the transfer speed if necessary
  lpOnline = (PONLINEDATA) lpOnlineData;
  if (lpOnline->bTransferStatus)
  {
	  dwTickCount = GetTickCount();
	  dwTickCount = Time_DifferenceDW32(dwLast, dwTickCount);
	  if (dwTickCount > ZERO_SPEED_DELAY)
	  {
		  lpOnline->dwIntervalLength  = 1; // so bytes/time doesn't generate an error
		  lpOnline->dwBytesTransfered = 0;
	  }
  }
  return TRUE;
}



LRESULT SeekOnlineData(ONLINEDATA *lpOnlineData, DWORD dwClientId)
{
  LPCLIENT    lpClient;
  DWORD       dwTickCount, dwLast;
  PONLINEDATA lpOnline;

  //  Find available user
  for ( ; dwClientId <= dwMaxClientId ; dwClientId++)
  {
    lpClient  = LockClient(dwClientId);
    if (lpClient)
    {
      //  Copy data
      CopyMemory(lpOnlineData, &lpClient->Static, sizeof(ONLINEDATA));
      //  Shared memory (dummy allocations)
      AllocateShared(lpClient->Static.tszRealPath, NULL, 0);
      AllocateShared(lpClient->Static.tszRealDataPath, NULL, 0);
	  dwLast = lpClient->dwTransferLastUpdated;
	  UnlockClient(dwClientId);

	  // now fix up the transfer speed if necessary
	  lpOnline = (PONLINEDATA) lpOnlineData;
	  if (lpOnline->bTransferStatus)
	  {
		  dwTickCount = GetTickCount();
		  dwTickCount = Time_DifferenceDW32(dwLast, dwTickCount);
		  if (dwTickCount > ZERO_SPEED_DELAY)
		  {
			 lpOnline->dwIntervalLength  = 1; // so bytes/time doesn't generate an error
			 lpOnline->dwBytesTransfered = 0;
		  }
	  }
      return dwClientId + 1;
    }
  }
  return -1;
}



LRESULT KillUser(UINT32 dwClientId)
{
  LPCLIENT  lpClient;

  if (! (lpClient = LockClient(dwClientId))) return TRUE;
  //  Terminate all jobs
  SetJobFilter(dwClientId, PRIMARY|SECONDARY|TERTIARY);
  lpClient->Static.dwFlags  |= S_DEAD;
  UnlockClient(dwClientId);
  return FALSE;
}



// If lpService is NULL kill everything, we're shutting down...
INT KillService(LPIOSERVICE lpService)
{
	DWORD    n, dwNumClients, dwTickCount, dwLoops;
	LPCLIENT  lpClient;

	if (bKillShutdownDone)
	{
		// we already tried to kill everything as part of shutdown, don't try again
		return 0;
	}
	if (!lpService)
	{
		bKillShutdownDone = TRUE;
	}

	//  Go through all users
	dwNumClients = 0;
	for (n = 0; n < MAX_CLIENTS ;n++)
	{
		if ((lpClient = LockClient(n)))
		{
			//  Compare service
			if (!lpService || (lpClient->lpService == lpService))
			{
				//  Set filter
				dwNumClients++;
				SetJobFilter(n, PRIMARY|SECONDARY|TERTIARY);
				lpClient->Static.dwFlags  |= S_DEAD;
				// OK, this is a bit tricky to follow...
				// The SetJobFilter will cancel all jobs for this cid and run any associated
				// cancelProc's along the way.  This should result in FTP_Cancel being called
				// which closes the control connection.  The next time EndClientJob() is
				// called it will realize there are no jobs and will call the services' close
				// proc which is where the real cleanup is done.  If the client is awaiting user
				// input or attempting to send data the closed control connection will be
				// noticed immediately.  However scripts or other long running commands that don't
				// produce output for a while may not notice the change right away...
			}
			UnlockClient(n);
		}
	}

	// now give a maximum of 60 seconds for outstanding jobs to terminate since
	// they are running in different threads and haven't yet had a chance to
	// recognize they should die.  
	dwLoops = 0;
	dwTickCount = GetTickCount() + 60000;
	while (dwNumClients && (GetTickCount() < dwTickCount))
	{
		SleepEx(100, TRUE);
		dwNumClients = 0;
		for (n = 0 ; n <  MAX_CLIENTS ; n++)
		{
			if ((lpClient = LockClient(n)))
			{
				//  Compare service
				if (!lpService || (lpClient->lpService == lpService))
				{
				  dwNumClients++;
				}
				UnlockClient(n);
			}
		}
		if(dwLoops++ > 100) // 10 seconds
		{
			dwLoops = 0;
			if (hRestartHeartbeat != INVALID_HANDLE_VALUE)
			{
				// report that we are still alive...
				SetEvent(hRestartHeartbeat);
			}
		}
	}

	if (!dwNumClients) return 0;

	if (!lpService)
	{
		Putlog(LOG_ERROR, _T("%d clients failed to finish cleanly during shutdown grace period.\r\n"), dwNumClients);
	}
	else
	{
		Putlog(LOG_ERROR, _T("%d clients of service '%s' failed are still active.\r\n"), dwNumClients, lpService->tszName);
	}
	return dwNumClients;
}



LRESULT KickUser(INT Uid)
{
  LPCLIENT  lpClient;
  DWORD    dwKicked, n;

  dwKicked  = 0;
  //  Go through all users
  for (n = 0 ; n <= dwMaxClientId ; n++)
  {
    if ((lpClient = LockClient(n)))
    {
      //  Compare uid
      if (lpClient->Static.Uid == Uid)
      {
        //  Set filter
        SetJobFilter(n, PRIMARY|SECONDARY|TERTIARY);
        lpClient->Static.dwFlags  |= S_DEAD;
        dwKicked++;
      }
      UnlockClient(n);
    }
  }
  return dwKicked;
}






BOOL UpdateClientData(DWORD dwDataType, DWORD dwClientId, ...)
{
  PVIRTUALPATH  pVirtualPath;
  LPUSERFILE    lpUserFile;
  PDATACHANNEL  lpData;
  LPHOSTINFO    lpHostInfo;
  LPIOSERVICE    lpService;
  LPCLASS      lpClass;
  LPCLIENT    lpClient, lpClient2;
  va_list      Arguments;
  LPSTR      szHostName, szIdent;
  INT        iCount;
  DWORD      dwBytesTransferred, dwType, n;
  BOOL      bReturn, bTransferStatus;

  bReturn  = FALSE;
  //  Get offset for arguments
  va_start(Arguments, dwClientId);
  //  Lock client
  if (! (lpClient = LockClient(dwClientId))) return TRUE;

  switch (dwDataType)
  {
  case DATA_TRANSFER_UPDATE:
	  //  Update transfer stats
	  dwBytesTransferred  = va_arg(Arguments, UINT32);
	  lpClient->Static.dwBytesTransfered      = dwBytesTransferred;
	  lpClient->Static.dwIntervalLength      = va_arg(Arguments, DWORD);
	  lpClient->Static.i64TotalBytesTransfered  += dwBytesTransferred;
	  lpClient->dwTransferLastUpdated = va_arg(Arguments, DWORD);
	  break;
  case DATA_TRANSFER:
    if ((bTransferStatus = va_arg(Arguments, DWORD)) == (DWORD)-1)
    {
      //  Transfer end
      if (lpClient->Static.bTransferStatus)
      {
        //  Decrease transfer count
        lpClient->Static.bTransferStatus  = 0;
		lpClient->Static.ulDataClientIp   = 0;
		lpClient->Static.usDataClientPort = 0;
		lpClient->Static.usDeviceNum      = 0;
		lpClient->Static.dwBytesTransfered = 0;
		lpClient->Static.dwIntervalLength  = 1;
		lpClient->Static.i64TotalBytesTransfered  = 0;
        InterlockedDecrement(&lpClient->lpService->lTransfers);
      }
      break;
    }
    //  Update transfer stats
    lpClient->Static.bTransferStatus      = bTransferStatus;
    lpClient->Static.dwIntervalLength      = 1;
    lpClient->Static.dwBytesTransfered      = 0;
    lpClient->Static.i64TotalBytesTransfered  = 0;
	lpClient->dwTransferLastUpdated = GetTickCount();
    //  Increase transfer count
    InterlockedIncrement(&lpClient->lpService->lTransfers);

	lpData = va_arg(Arguments, PDATACHANNEL);
    pVirtualPath  = &lpData->File;
    //  Copy virtual path
    CopyMemory(lpClient->Static.tszVirtualDataPath, pVirtualPath->pwd, (pVirtualPath->len + 1) * sizeof(TCHAR));
    //  Unshare old real path
    FreeShared(lpClient->Static.tszRealDataPath);
    //  Share new real path
    lpClient->Static.dwRealDataPath  = pVirtualPath->l_RealPath + 1;
    lpClient->Static.tszRealDataPath  = (LPSTR)AllocateShared(pVirtualPath->RealPath, NULL, 0);

	lpClient->Static.ulDataClientIp   = (ULONG) lpData->Address.sin_addr.s_addr;
	lpClient->Static.usDataClientPort = lpData->Address.sin_port;

	if (lpData->ioSocket.lpDevice && FtpSettings.bOnlineDataExtraFields)
	{
		lpClient->Static.usDeviceNum = (USHORT) lpData->ioSocket.lpDevice->lDeviceNum;
	}
	else
	{
		lpClient->Static.usDeviceNum = 0;
	}

    break;

  case DATA_DEAUTHENTICATE:
	  lpUserFile  = va_arg(Arguments, LPUSERFILE);
	  dwType      = lpClient->lpService->dwType;
	  lpHostInfo  = lpClient->lpHostInfo;
	  lpService   = lpClient->lpService;
	  lpClass     = lpClient->lpHostInfo->ClassInfo.lpClass;

	  EnterCriticalSection(&csClientArray);
	  InterlockedDecrement(&lpService->lClients);
	  InterlockedDecrement(&lpHostInfo->lClients);
	  if (lpClass)
	  {
		  InterlockedDecrement(&lpClass->lClients);
	  }
	  UserFile_DecrementLoginCount(lpUserFile, dwType);
	  LeaveCriticalSection(&csClientArray);
	  lpClient->Static.Uid  = -1;
	  break;

  case DATA_AUTHENTICATE:
    //  Get variables
    lpUserFile  = va_arg(Arguments, LPUSERFILE);
    dwType    = lpClient->lpService->dwType;

    lpHostInfo  = lpClient->lpHostInfo;
    lpService  = lpClient->lpService;
    lpClass  = lpClient->lpHostInfo->ClassInfo.lpClass;

    EnterCriticalSection(&csClientArray);
    //  Check user logins
	if (HasFlag(lpUserFile, _TEXT("ML")))
	{
		if ( ( lpService->lMaxClients != -1 ) && ( lpService->lClients >= lpService->lMaxClients ) )
		{
			bReturn  = -1;
		}
		else if (lpClass && ( lpClass->lMaxClients != -1 ) && (lpClass->lClients >= lpClass->lMaxClients ) )
		{
			bReturn  = -2;
		}
		else if ( ( lpHostInfo->ClassInfo.lConnectionsPerHost != -1 ) && ( lpHostInfo->lClients >= lpHostInfo->ClassInfo.lConnectionsPerHost ) )
		{
			bReturn  = -3;
		}
		else if ( ( lpUserFile->Limits[dwType + 2] != -1 ) && ( UserFile_GetLoginCount(lpUserFile, dwType) >= lpUserFile->Limits[dwType + 2] ) )
		{
			bReturn  = -4;
		}
		else if (lpUserFile->LimitPerIP > 0)
		{
			iCount = 0;
			for (n = 0 ; n <= dwMaxClientId ; n++)
			{
				if (n == dwClientId) continue;

				if ((lpClient2 = LockClient(n)))
				{
					//  Compare uid and host address which means hostinfo is the same
					if ( ( lpClient2->Static.Uid == lpUserFile->Uid ) && ( lpClient2->lpHostInfo == lpClient->lpHostInfo ) )
					{
						iCount++;
					}
					UnlockClient(n);
				}
			}

			if (iCount >= lpUserFile->LimitPerIP)
			{
				bReturn = -5;
			}
		}
	}

	//  Increase counters
    if (! bReturn)
    {
		InterlockedIncrement(&lpService->lClients);
		InterlockedIncrement(&lpHostInfo->lClients);
		if (HasFlag(lpUserFile, _TEXT("A")))
		{
			// if a non-anonymous account reset connection auto-ban count
			lpHostInfo->dwAttemptCount = 1;
		}
		if (lpClass)
		{
			InterlockedIncrement(&lpClass->lClients);
		}
		UserFile_IncrementLoginCount(lpUserFile, dwType);
    }
    LeaveCriticalSection(&csClientArray);
    //  Abort?
    if (bReturn) break;
    //  Update user id
    lpClient->Static.Uid  = lpUserFile->Uid;
	// fallthrough
  case DATA_CHDIR:
    pVirtualPath  = va_arg(Arguments, PVIRTUALPATH);
    //  Copy virtual path
    CopyMemory(lpClient->Static.tszVirtualPath, pVirtualPath->pwd, (pVirtualPath->len + 1) * sizeof(TCHAR));
    //  Unshare old real path
    FreeShared(lpClient->Static.tszRealPath);
    //  Share new real path
    lpClient->Static.dwRealPath  = pVirtualPath->l_RealPath + 1;
    lpClient->Static.tszRealPath  = (LPSTR)AllocateShared(pVirtualPath->RealPath, NULL, 0);
    break;
  case DATA_ACTION:
    //  Update Last Action
	  _tcsncpy(lpClient->Static.tszAction, (LPSTR)va_arg(Arguments, LPSTR), 64);
    lpClient->Static.dwIdleTickCount  = va_arg(Arguments, DWORD);
    break;
  case DATA_NOOP:
	//  Just update timestamp
	  if (!lpClient->Static.bTransferStatus)
	  {
		  // not transfering, so record the NOOP
		  _tcsncpy(lpClient->Static.tszAction, (LPSTR)va_arg(Arguments, LPSTR), 64);
	  }
	  lpClient->Static.dwIdleTickCount  = va_arg(Arguments, DWORD);
	  break;
  case DATA_IDENT:
    //  Update client's identity
    szIdent    = (LPSTR)va_arg(Arguments, LPSTR);
    szHostName  = (LPSTR)va_arg(Arguments, LPSTR);
    //  Replace null variables
    if (! szHostName) szHostName  = "";
    if (! szIdent) szIdent  = "*";

    CopyString(lpClient->Static.szHostName, szHostName);
    CopyString(lpClient->Static.szIdent, szIdent);
    lpClient->Static.ulClientIp  = (ULONG)va_arg(Arguments, ULONG);
    break;
  default:
    bReturn  = TRUE;
    break;
  }
  UnlockClient(dwClientId);
  va_end(Arguments);

  return bReturn;
}




DWORD GetCurrentClassListId(VOID)
{
  return dwClassListId;
}


VOID GetHostClass(LPHOSTINFO lpHostInfo)
{
  LPHOSTCLASS lpHostClass;
  LPSTR       szHostName;
  LPRULE      lpRule;
  DWORD      dwHostName, dwTicks;
  struct in_addr  InetAddress;
  INT        iResult;
  CHAR        szObscuredHost[MAX_HOSTNAME];
  CHAR        szObscuredIP[MAX_HOSTNAME];


  szHostName = (lpHostInfo->szHostName ? lpHostInfo->szHostName : "");
  dwHostName  = strlen(szHostName);
  InetAddress.S_un.S_addr  = ((PULONG)lpHostInfo->NetworkAddress)[0];
  Obscure_IP(szObscuredIP, &InetAddress);
  Obscure_Host(szObscuredHost, szHostName);

  lpHostClass = &lpHostInfo->ClassInfo;

  if (!UserIpHostMaskKnown(szHostName, &InetAddress))
  {
	  if (!IpHasKnocked(lpHostInfo->NetworkAddress, lpHostInfo->dwNetworkAddress))
	  {
		  dwTicks = GetTickCount();
		  if (Time_DifferenceDW32(lpHostInfo->dwLastUnMatchedLogTime, dwTicks) > lpHostInfo->dwLastUnMatchedLogDelay * 60 * 1000)
		  {
			  if (lpHostInfo->dwLastUnMatchedLogDelay < dwMaxLogSuppression)
			  {
				  lpHostInfo->dwLastUnMatchedLogDelay += dwLogSuppressionIncrement;
				  if (lpHostInfo->dwLastUnMatchedLogDelay > dwMaxLogSuppression)
				  {
					  lpHostInfo->dwLastUnMatchedLogDelay = dwMaxLogSuppression;
				  }
			  }
			  lpHostInfo->dwLastUnMatchedLogTime = dwTicks;
			  Putlog(LOG_ERROR, _TEXT("Rejected unmatched client %s (%s).\r\n"),
				  szObscuredIP, szObscuredHost);
		  }
		  lpHostClass->lpClass  = NULL;
		  lpHostClass->lConnectionsPerHost  = 0;
		  lpHostClass->dwClassListId = 0;
		  return;
	  }
  }

  AcquireSharedLock(&loRuleList);
  //  Browse through rule list
  for (lpRule = lpRuleList;lpRule;lpRule = lpRule->lpNext)
  {
    if (lpRule->dwType & HOSTNAME)
    {
      //  Rule sanity check
      if (lpRule->dwContext > dwHostName) continue;

      iResult  = memicmp(lpRule->lpContext,
        &szHostName[dwHostName - lpRule->dwContext], lpRule->dwContext);
    }
    else
    {
      //  Compare
      iResult  = memcmp(lpRule->lpContext, lpHostInfo->NetworkAddress, lpRule->dwContext);
    }
    if (! iResult) break;
  }

  //  Choose proper handler
  if (! lpRule)
  {
    lpHostClass->lpClass  = NULL;
    //  Use default policy
    if (! iDefaultPolicy)
    {
      lpHostClass->lConnectionsPerHost  = iMaxConnectionsPerIp;
    }
    else lpHostClass->lConnectionsPerHost  = 0;
  }
  else if (lpRule->dwType & ACCEPT)
  {
    lpHostClass->lpClass  = ((LPACCEPT_RULE)lpRule)->lpClass;
    lpHostClass->lConnectionsPerHost  = ((LPACCEPT_RULE)lpRule)->lConnectionsPerHost;
  }
  else
  {
	  //  Log event
	  dwTicks = GetTickCount();
	  if (Time_DifferenceDW32(lpHostInfo->dwLastBadHostLogTime, dwTicks) > lpHostInfo->dwLastBadHostLogDelay * 60 * 1000)
	  {
		  if (lpHostInfo->dwLastBadHostLogDelay < dwMaxLogSuppression)
		  {
			  lpHostInfo->dwLastBadHostLogDelay += dwLogSuppressionIncrement;
			  if (lpHostInfo->dwLastBadHostLogDelay > dwMaxLogSuppression)
			  {
				  lpHostInfo->dwLastBadHostLogDelay = dwMaxLogSuppression;
			  }
		  }
		  lpHostInfo->dwLastBadHostLogTime = dwTicks;
		  Putlog(LOG_ERROR, _TEXT("Rejected client from %s: %s\r\n"),
			  szObscuredIP, (((LPDENY_RULE)lpRule)->tszLog) ? ((LPDENY_RULE)lpRule)->tszLog : _T(""));
	  }
	  lpHostClass->lpClass  = NULL;
	  lpHostClass->lConnectionsPerHost  = 0;
  }
  lpHostClass->dwClassListId  = dwClassListId;
  ReleaseSharedLock(&loRuleList);
}





VOID StripSpaces(LPSTR szString)
{
  PCHAR  pTarget, pSource, pQuote;

  pQuote  = NULL;
  //  Loop through string
  for (pTarget = pSource = szString;;)
  {
    switch (pTarget[0] = (pSource++)[0])
    {
    case '\0':
      return;
    case '"':
      //  Find next quote
      pQuote  = strchr(pSource, '"');
      //  Store new offset
      if (pQuote++)
      {
        CopyMemory(++pTarget, pSource, pQuote - pSource);
        pTarget  += pQuote - pSource;
        pSource  = pQuote;
      }
      break;
    case '\t':
      //  Replace tab with space
      pTarget[0]  = ' ';
    case ' ':
      //  Check previous character
      if (pTarget > szString && pTarget[-1] == ' ') break;
    default:
      pTarget++;
    }
  }
}






BOOL ReadConnectionClasses(LPTSTR tszFileName)
{
  IO_STRING    String;
  LPACCEPT_RULE  lpAcceptRule;
  LPDENY_RULE    lpDenyRule;
  LPVOID      lpContext;
  LPRULE      lpFirstRule, lpLastRule, lpRule;
  LPCLASS      lpFirstClass, lpLastClass, lpClass, lpSeek;
  HANDLE      hFile;
  LPTSTR      tszLine, tszType, tszLimit, tszClass, tszIp, tszArg, tszLog;
  TCHAR      *tpBuffer, *tpNewline, *tpEnd, *tpCheck, *tpLineEnd;
  BYTE      pHostName[MAX_HOSTNAME + 1];
  DWORD      dwTokens, dwClass, dwLog, dwContext, dwFileSize, dwBytesRead, dwType;
  LONG      lLimit;
  BOOL      bReturn, bResult;
  INT        iPolicyConnections, iPolicy;

  //  Open rule file
  hFile  = CreateFile(tszFileName, GENERIC_READ, FILE_SHARE_READ,
    NULL, OPEN_EXISTING, 0, NULL);
  if (hFile == INVALID_HANDLE_VALUE) return FALSE;

  iPolicy  = -1;
  bResult  = FALSE;
  bReturn  = FALSE;
  tpBuffer  = NULL;
  lpFirstRule  = NULL;
  lpLastRule  = NULL;
  lpLastClass  = NULL;
  lpFirstClass  = NULL;
  //  Buffer file to memory
  dwFileSize  = GetFileSize(hFile, NULL);
  if (dwFileSize != INVALID_FILE_SIZE)
  {
    tpBuffer  = (PCHAR)Allocate("Policies", dwFileSize + sizeof(TCHAR));
    if (tpBuffer)
    {
      bResult  = ReadFile(hFile, tpBuffer, dwFileSize, &dwBytesRead, NULL);
    }
  }

  if (bResult)
  {
    dwBytesRead  /= sizeof(TCHAR);
    tpBuffer[dwBytesRead++]  = _TEXT('\n');
    tszLine  = tpBuffer;
    tpEnd  = &tpBuffer[dwBytesRead];

    //  Parse buffer
    for (;tpNewline = (TCHAR *)_tmemchr(tszLine, _TEXT('\n'), tpEnd - tszLine);tszLine = &tpNewline[1])
    {
      //  Remove unwanted characters
      tpLineEnd  = &tpNewline[(tpNewline[-1] == _TEXT('\r') ? -1 : 0)];
      for (;tpLineEnd > tszLine && _istspace(tpLineEnd[-1]);tpLineEnd--);
      for (;tszLine < tpLineEnd && _istspace(tszLine[0]);tszLine++);

      tpLineEnd[0]  = '\0';
      StripSpaces(tszLine);

      if (isalpha(tszLine[0]) &&
        ! SplitString(tszLine, &String))
      {
        dwTokens  = GetStringItems(&String);
        tszArg  = GetStringIndexStatic(&String, 0);

        if (dwTokens > 1 && ! _tcsicmp(tszArg, _TEXT("policy")))
        {
          //  Determinate default policy
          tszType  = GetStringIndexStatic(&String, 1);
          if (dwTokens == 3 && ! _tcsicmp(tszType, _TEXT("accept")))
          {
            tszLimit  = GetStringIndexStatic(&String, 2);
            lLimit  = _tcstol(tszLimit, &tpCheck, 10);
            if (tpCheck > tszLimit && tpCheck[0] == _TEXT('\0'))
            {
              iPolicy  = 0;
              iPolicyConnections  = lLimit;
            }
          }
          else if (dwTokens == 2 && ! _tcsicmp(tszType, _TEXT("deny")))
          {
            iPolicy  = 1;
            iPolicyConnections = 0;
          }
        }
        else if (dwTokens == 3 && ! _tcsicmp(tszArg, _TEXT("class")))
        {
          tszLimit  = GetStringIndexStatic(&String, 2);
          lLimit  = _tcstol(tszLimit, &tpCheck, 10);
          if (tpCheck > tszLimit && tpCheck[0] == _TEXT('\0'))
          {
            //  Add new service class
            tszClass  = GetStringIndexStatic(&String, 1);
            dwClass  = GetStringIndexLength(&String, 1) * sizeof(TCHAR);

            lpClass  = (LPCLASS)Allocate("Rule:ClassName", sizeof(CLASS) + dwClass);
            if (lpClass)
            {
              //  Copy contents
              lpClass->lpNext  = NULL;
              lpClass->lClients    = 0;
              lpClass->lMaxClients  = lLimit;
              CopyMemory(lpClass->tszName, tszClass, dwClass + sizeof(TCHAR));

              //  Append class to list
              if (! lpFirstClass)
              {
                lpFirstClass  = lpClass;
              }
              else lpLastClass->lpNext  = lpClass;
              lpLastClass  = lpClass;
            }
          }
        }
        else if (dwTokens > 3)
        {
          //  Get hostname type
          tszType  = GetStringIndexStatic(&String, 1);
          lpContext  = NULL;

          switch (_totupper(tszType[0]))
          {
          case _TEXT('H'):
            //  Get hostname and length
#ifdef _UNICODE
            lpContext  = pHostName;
            dwContext  = sprintf((PCHAR)pHostName, "%.*S", sizeof(pHostName) - 1, GetStringIndexStatic(&String, 2));
#else
            lpContext  = GetStringIndexStatic(&String, 2);
            dwContext  = GetStringIndexLength(&String, 2);
#endif
            dwType    = HOSTNAME;
            break;
          case _TEXT('I'):
            //  Get ipv4 string
            tszIp  = GetStringIndexStatic(&String, 2);

            for (dwContext = 0;dwContext < 4;dwContext++)
            {
              //  Convert string to integer
              pHostName[dwContext]  = (BYTE)_tcstoul(tszIp, &tpCheck, 10);
              if (tpCheck == tszIp) break;
              tszIp  = tpCheck;
              if (tszIp[0] == _TEXT('.')) tszIp++;

              if (tszIp[0] == _TEXT('\0'))
              {
                lpContext  = (LPVOID)pHostName;
                dwType  = IP;
                dwContext++;
                break;
              }
            }
            break;
          }

          if (lpContext)
          {
            lpRule  = NULL;
            if (! _tcsicmp(tszArg, _TEXT("deny")))
            {
                if (GetStringItems(&String) > 3)
                {
                  tszLog  = GetStringRange(&String, 3, STR_END);
                  dwLog  = (_tcslen(tszLog) + 1) * sizeof(TCHAR);
                }
                else dwLog  = 0;

                //  Allocate rule
                lpDenyRule  = (LPDENY_RULE)Allocate("Rule:Deny", sizeof(DENY_RULE) + dwLog + dwContext);
                if (lpDenyRule)
                {
                  //  Copy log string
                  if (dwLog)
                  {
                    lpDenyRule->tszLog  = (LPSTR)((ULONG)&lpDenyRule[1] + dwContext);
                    CopyMemory(lpDenyRule->tszLog, tszLog, dwLog);
                  }
                  else lpDenyRule->tszLog  = NULL;
                  //  Convert to LPRULE
                  lpRule  = (LPRULE)lpDenyRule;
                  lpRule->lpContext  = (LPVOID)&lpDenyRule[1];
                  lpRule->dwType    = dwType|DENY;
                }
            }
            else if (dwTokens == 5 && ! _tcsicmp(tszArg, _TEXT("accept")))
            {
              tszLimit  = GetStringIndex(&String, 4);
              lLimit  = _tcstol(tszLimit, &tpCheck, 10);
              if (tpCheck > tszLimit && tpCheck[0] == _TEXT('\0'))
              {
                tszClass  = GetStringIndex(&String, 3);
                for (lpClass = lpFirstClass;lpClass;lpClass = lpClass->lpNext)
                {
                  if (! _tcsicmp(lpClass->tszName, tszClass)) break;
                }

                if (lpClass)
                {
                  //  Allocate rule
                  lpAcceptRule  = (LPACCEPT_RULE)Allocate("Rule:Accept", sizeof(ACCEPT_RULE) + dwContext);
                  if (lpAcceptRule)
                  {
                    lpAcceptRule->lConnectionsPerHost  = lLimit;
                    lpAcceptRule->lpClass  = lpClass;
                    lpRule  = (LPRULE)lpAcceptRule;
                    lpRule->lpContext  = (LPVOID)&lpAcceptRule[1];
                    lpRule->dwType  = dwType|ACCEPT;
                  }
                }
              }
            }
            if (lpRule)
            {
              lpRule->lpNext    = NULL;
              lpRule->dwContext  = dwContext;
              CopyMemory(lpRule->lpContext, lpContext, dwContext);
              //  Append rule to rule list
              if (! lpFirstRule)
              {
                lpFirstRule  = lpRule;
              }
              else lpLastRule->lpNext  = lpRule;
              lpLastRule  = lpRule;
            }
          }
        }
        FreeString(&String);
      }
    }
  }
  //  Free resources
  Free(tpBuffer);
  CloseHandle(hFile);

  if (iPolicy != -1)
  {
    AcquireExclusiveLock(&loRuleList);

    if (! ++dwClassListId) dwClassListId  = 1;
    iDefaultPolicy  = iPolicy;
    iMaxConnectionsPerIp  = iPolicyConnections;
    //  Append classes to global list and update rule class references
    for (;lpClass = lpFirstClass;)
    {
      lpFirstClass  = lpFirstClass->lpNext;
      for (lpSeek = lpClassList;lpSeek;lpSeek = lpSeek->lpNext)
      {
        if (! _tcsicmp(lpSeek->tszName, lpClass->tszName)) break;
      }

      if (lpSeek)
      {
        for (lpRule = lpFirstRule;lpRule;lpRule = lpRule->lpNext)
        {
          if (lpRule->dwType & ACCEPT &&
            ((LPACCEPT_RULE)lpRule)->lpClass == lpClass)
          {
            ((LPACCEPT_RULE)lpRule)->lpClass  = lpSeek;
          }
        }        
        lpSeek->lMaxClients  = lpClass->lMaxClients;
        Free(lpClass);
      }
      else
      {
        lpClass->lpNext  = lpClassList;
        lpClassList  = lpClass;
      }
    }
    //  Update global rule list
    lpRule  = lpRuleList;
    lpRuleList  = lpFirstRule;
    lpFirstRule  = lpRule;

    ReleaseExclusiveLock(&loRuleList);
    bReturn  = TRUE;
  }
  else
  {
    //  Free resources
    for (;lpClass = lpFirstClass;)
    {
      lpFirstClass  = lpFirstClass->lpNext;
      Free(lpClass);
    }
  }

  for (;lpRule = lpFirstRule;)
  {
    lpFirstRule  = lpFirstRule->lpNext;
    Free(lpRule);
  }

  return bReturn;
}






BOOL Client_Init(BOOL bFirstInitialization)
{
  LPTSTR  tszFileName;
  DWORD  n;
  BOOL  bReturn;

  if (bFirstInitialization)
  {
    //  Initialize local variables
    lpClassList  = NULL;
    lpRuleList  = NULL;
    dwClassListId  = 0;
    lpClientSlotList  = &lpClientSlot[0];

    InitializeCriticalSectionAndSpinCount(&csClientArray, 100);
    InitializeLockObject(&loRuleList);
    //  Allocate client slot list
    for (n = MAX_CLIENTS;n--;)
    {
      lpClientArray[n]      = NULL;
      lpClientSlot[n].dwId    = n;
      lpClientSlot[n].lJobLock  = FALSE;
      lpClientSlot[n].lDataLock  = FALSE;
      lpClientSlot[n].lpNext    = (n == (MAX_CLIENTS - 1) ? NULL : &lpClientSlot[n + 1]);
    }
	lClientCounter = 0;
	dwMaxClientId  = 0;
	dwActualMaxClientId = 0;
  }

  //  Read connection classes
  tszFileName  = Config_Get(&IniConfigFile, _TEXT("Locations"), _TEXT("Hosts_Rules"), NULL, NULL);
  if (! tszFileName) return FALSE;
  bReturn  = ReadConnectionClasses(tszFileName);
  Free(tszFileName);

  return bReturn;
}



VOID Client_DeInit(VOID)
{
  LPCLASS  lpClass;
  LPRULE  lpRule;

  for (;lpRule = lpRuleList;)
  {
    lpRuleList  = lpRuleList->lpNext;
    Free(lpRule);
  }
  for (;lpClass = lpClassList;)
  {
    lpClassList  = lpClassList->lpNext;
    Free(lpClass);
  }

  //  Free resources
  DeleteLockObject(&loRuleList);
  DeleteCriticalSection(&csClientArray);
}











VOID SetJobFilter(DWORD hClient, DWORD dwFlags)
{
  LPCLIENTJOB        lpJobQueue[3];
  register LPCLIENTJOB  lpJob;
  volatile LONG *pLock;
  LPCLIENT       lpClient;

  lpJobQueue[0]  = NULL;
  lpJobQueue[2]  = NULL;

  pLock    = &lpClientSlot[hClient].lJobLock;
  lpClient  = lpClientArray[hClient];

  if (lpClient->dwJobFilter == dwFlags) return;
  while (InterlockedExchange(pLock, TRUE)) SwitchToThread();
  if (lpClient->dwJobFilter != dwFlags)
  {
	  //  Cancel active jobs
	  for (lpJob = lpClient->lpActiveJob;lpJob;lpJob = lpJob->lpNext)
	  {
		  if (lpJob->dwJobFlags & dwFlags)
		  {
			  if (lpJob->lpCancelProc) lpJob->lpCancelProc(lpJob->lpContext);
			  // NOTE: extended jobs have never been started and the threads trying to
			  // are blocked...  Should we call the cancel function, or let them start
			  // when EndClientJob gets called? For the moment, just going to let them
			  // start one by one as they will end up getting errors and abort soon
			  // enough...
#if 0
			  for (lpJob2 = lpJob->lpExtended ; lpJob2 ; lpJob2 = lpJob2->lpExtended )
			  {
				  if (lpJob2->lpCancelProc) lpJob2->lpCancelProc(lpJob2->lpContext);
			  }
#endif
		  }
	  }

	  //  Cancel pending jobs
	  for (lpJob = lpClient->lpJobQueue[HEAD];lpJob;lpJob = lpJob->lpNext)
	  {
		  if (lpJob->dwJobFlags & dwFlags &&
			  ! (lpJob->dwJobFlags & CJOB_EXCLUSIVE))
		  {
			  if (! lpJobQueue[HEAD])
			  {
				  lpJobQueue[HEAD]  = lpJob;
			  }
			  else lpJobQueue[TAIL]->lpNext  = lpJob;
			  lpJobQueue[TAIL]  = lpJob;
		  }
		  else
		  {
			  if (! lpJobQueue[2])
			  {
				  lpClient->lpJobQueue[HEAD]  = lpJob;
			  }
			  else lpJobQueue[2]->lpNext  = lpJob;
			  lpJobQueue[2]  = lpJob;
		  }
	  }

	  if (lpJobQueue[2])
	  {
		  lpJobQueue[2]->lpNext  = NULL;
		  lpClient->lpJobQueue[TAIL]  = lpJobQueue[2];
	  }
	  else lpClient->lpJobQueue[HEAD]  = NULL;
	  lpClient->dwJobFilter  = dwFlags;
  }
  InterlockedExchange(pLock, FALSE);

  //  Free all inactive canceled jobs
  if (lpJob = lpJobQueue[HEAD])
  {
	  lpJobQueue[TAIL]->lpNext  = NULL;
	  do
	  {
		  lpJobQueue[HEAD]  = lpJob->lpNext;
		  Free(lpJob);

	  } while (lpJob = lpJobQueue[HEAD]);
  }
}





BOOL AddClientJob(DWORD hClient, DWORD dwJobId, DWORD dwJobFlags, DWORD dwJobTimeOut, BOOL (* lpJobProc)(LPVOID), VOID (*lpCancelProc)(LPVOID), DWORD (*lpTimerProc)(LPVOID, LPTIMER), LPVOID lpContext)
{
  volatile LONG *pLock;
  LPCLIENTJOB    lpJob;
  LPCLIENT    lpClient;
  BOOL      bReturn;

  pLock    = &lpClientSlot[hClient].lJobLock;
  lpClient  = lpClientArray[hClient];

  if (dwJobFlags & lpClient->dwJobFilter &&
    ! (dwJobFlags & CJOB_EXCLUSIVE)) return FALSE;
  //  Initialize new job structure
  lpJob  = (LPCLIENTJOB)Allocate("Client:Job", sizeof(CLIENTJOB));
  if (! lpJob) return FALSE;

  // no sense starting a timer if the timeout was never
  if (lpTimerProc && (dwJobFlags & CJOB_INSTANT) && (dwJobTimeOut != -1))
  {
    lpJob->lpTimer  = StartIoTimer(NULL, lpTimerProc, lpContext, dwJobTimeOut);
  }
  else lpJob->lpTimer  = NULL;

  lpJob->lpJobProc    = lpJobProc;
  lpJob->lpCancelProc = lpCancelProc;
  lpJob->lpTimerProc  = NULL;
  lpJob->dwTimeOut    = dwJobTimeOut;
  lpJob->lpContext    = lpContext;
  lpJob->dwJobId      = dwJobId;
  lpJob->dwJobFlags   = dwJobFlags;
  lpJob->lpExtended   = NULL;

  while (InterlockedExchange(pLock, TRUE)) SwitchToThread();
  if (! (dwJobFlags & lpClient->dwJobFilter) || dwJobFlags & CJOB_EXCLUSIVE)
  {
    if (! (lpClient->dwActiveFlags & dwJobFlags) ||
      dwJobFlags & CJOB_INSTANT)
    {
      //  Job can be activated now
      lpJob->lpNext  = lpClient->lpActiveJob;
      lpClient->lpActiveJob  = lpJob;
      lpClient->dwActiveFlags  |= (dwJobFlags & (0xFFFFFFF - (CJOB_EXCLUSIVE|CJOB_INSTANT)));
    }
    else
    {
      //  Append job to pending list
      if (! lpClient->lpJobQueue[HEAD])
      {
        lpClient->lpJobQueue[HEAD]  = lpJob;
      }
      else lpClient->lpJobQueue[TAIL]->lpNext  = lpJob;
      lpClient->lpJobQueue[TAIL]  = lpJob;
      lpJob->lpNext  = NULL;
      lpJob  = NULL;
    }
    bReturn  = FALSE;
  }
  else bReturn  = TRUE;
  InterlockedExchange(pLock, FALSE);

  if (! bReturn)
  {
    //  Queue active job
    if (lpJobProc && lpJob) QueueJob(lpJobProc, lpContext, JOB_PRIORITY_NORMAL);
    return TRUE;
  }
  StopIoTimer(lpJob->lpTimer, FALSE);
  Free(lpJob);
  return FALSE;
}




BOOL IsClientJobActive(DWORD hClient, DWORD dwJobId)
{
	volatile LONG *pLock;
	LPCLIENTJOB    lpJob;
	BOOL           bReturn;

	pLock   = &lpClientSlot[hClient].lJobLock;
	bReturn = FALSE;
	while (InterlockedExchange(pLock, TRUE)) SwitchToThread();
	for (lpJob = lpClientArray[hClient]->lpActiveJob ; lpJob ; lpJob = lpJob->lpNext)
	{
		if (lpJob->dwJobId == dwJobId)
		{
			bReturn = TRUE;
			break;
		}
	}
	InterlockedExchange(pLock, FALSE);
	return bReturn;
}


BOOL AddClientJobTimer(DWORD hClient, DWORD dwJobId, DWORD dwJobTimeOut, DWORD (*lpTimerProc)(LPVOID, LPTIMER))
{
  volatile LONG *pLock;
  LPCLIENTJOB    lpJob;
  BOOL           bReturn;

  pLock   = &lpClientSlot[hClient].lJobLock;
  bReturn = TRUE;
  while (InterlockedExchange(pLock, TRUE)) SwitchToThread();
  for (lpJob = lpClientArray[hClient]->lpActiveJob;lpJob;lpJob = lpJob->lpNext)
  {
    if (lpJob->dwJobId == dwJobId && ! lpJob->lpTimer)
    {
      lpJob->lpTimer  = StartIoTimer(NULL, lpTimerProc, lpJob->lpContext, dwJobTimeOut);
	  bReturn = FALSE;
	  break;
    }
  }
  InterlockedExchange(pLock, FALSE);
  return bReturn;
}


// specialized function to deal with outputting data to the control connection, but in theory
// could be used for all client jobs though only job #2 (output to control connection) uses it.
// extended jobs have their own timers that start when they actually get to run and they must
// be INSTANT jobs (although they may be forced to wait to start).
BOOL AddClientJobOrExtendJob(DWORD hClient, DWORD dwJobId, DWORD dwJobFlags, DWORD dwJobTimeOut, BOOL (* lpJobProc)(LPVOID), VOID (*lpCancelProc)(LPVOID), DWORD (*lpTimerProc)(LPVOID, LPTIMER), LPVOID lpContext)
{
	volatile LONG *pLock;
	LPCLIENTJOB    lpJob, lpOldJob;
	LPCLIENT    lpClient;
	BOOL      bReturn;

	pLock    = &lpClientSlot[hClient].lJobLock;
	lpClient  = lpClientArray[hClient];

	if (dwJobFlags & lpClient->dwJobFilter &&
		! (dwJobFlags & CJOB_EXCLUSIVE)) return FALSE;
	//  Initialize new job structure
	lpJob  = (LPCLIENTJOB)Allocate("Client:Job", sizeof(CLIENTJOB));
	if (! lpJob) return FALSE;

	lpJob->lpJobProc    = lpJobProc;
	lpJob->lpCancelProc = lpCancelProc;
	lpJob->lpTimerProc  = lpTimerProc;
	lpJob->dwTimeOut    = dwJobTimeOut;
	lpJob->lpContext    = lpContext;
	lpJob->dwJobId      = dwJobId;
	lpJob->dwJobFlags   = dwJobFlags;
	lpJob->lpExtended   = NULL;


	while (InterlockedExchange(pLock, TRUE)) SwitchToThread();
	for (lpOldJob = lpClient->lpActiveJob ; lpOldJob ; lpOldJob = lpOldJob->lpNext)
	{
		if (lpOldJob->dwJobId == dwJobId) break;
	}
	if (!lpOldJob)
	{
		for (lpOldJob = lpClient->lpJobQueue[HEAD] ; lpOldJob ; lpOldJob = lpOldJob->lpNext)
		{
			if (lpOldJob->dwJobId == dwJobId) break;
		}
	}
	if (lpOldJob)
	{
		lpJob->lpTimer = NULL;

		// we found a match, so append job onto end of extended list
		for ( ; lpOldJob->lpExtended ; lpOldJob = lpOldJob->lpExtended );
		lpOldJob->lpExtended = lpJob;
		bReturn = TRUE;
		lpJobProc = NULL;
	}
	else
	{
		// just treat it as a new job

		// no sense starting a timer if the timeout was never
		if (lpTimerProc && (dwJobFlags & CJOB_INSTANT) && (dwJobTimeOut != -1))
		{
			lpJob->lpTimer  = StartIoTimer(NULL, lpTimerProc, lpContext, dwJobTimeOut);
		}
		else lpJob->lpTimer  = NULL;


		if (! (dwJobFlags & lpClient->dwJobFilter) || dwJobFlags & CJOB_EXCLUSIVE)
		{
			if (! (lpClient->dwActiveFlags & dwJobFlags) ||
				dwJobFlags & CJOB_INSTANT)
			{
				//  Job can be activated now
				lpJob->lpNext  = lpClient->lpActiveJob;
				lpClient->lpActiveJob  = lpJob;
				lpClient->dwActiveFlags  |= (dwJobFlags & (0xFFFFFFF - (CJOB_EXCLUSIVE|CJOB_INSTANT)));
			}
			else
			{
				//  Append job to pending list
				if (! lpClient->lpJobQueue[HEAD])
				{
					lpClient->lpJobQueue[HEAD]  = lpJob;
				}
				else lpClient->lpJobQueue[TAIL]->lpNext  = lpJob;
				lpClient->lpJobQueue[TAIL]  = lpJob;
				lpJob->lpNext  = NULL;
				lpJob  = NULL;
			}
			bReturn  = TRUE;
		}
		else bReturn  = FALSE;
	}
	InterlockedExchange(pLock, FALSE);

	if (bReturn)
	{
		//  Queue active job
		if (lpJobProc && lpJob) QueueJob(lpJobProc, lpContext, JOB_PRIORITY_NORMAL);
		return TRUE;
	}
	StopIoTimer(lpJob->lpTimer, FALSE);
	Free(lpJob);
	return FALSE;
}


// specialized function to deal with outputting data to the control connection, but in theory
// could be used for all client jobs though only job #2 (output to control connection) uses it.
// extended jobs have their own timers (that start when the job actually starts) and they must
// be INSTANT jobs...
BOOL AddExclusiveClientJob(DWORD hClient, DWORD dwJobId, DWORD dwJobFlags, DWORD dwJobTimeOut, BOOL (* lpJobProc)(LPVOID), VOID (*lpCancelProc)(LPVOID), DWORD (*lpTimerProc)(LPVOID, LPTIMER), LPVOID lpContext)
{
	volatile LONG *pLock;
	LPCLIENTJOB    lpJob, lpOldJob;
	LPCLIENT    lpClient;
	BOOL      bReturn;

	pLock    = &lpClientSlot[hClient].lJobLock;
	lpClient  = lpClientArray[hClient];

	if (dwJobFlags & lpClient->dwJobFilter &&
		! (dwJobFlags & CJOB_EXCLUSIVE)) return FALSE;
	//  Initialize new job structure
	lpJob  = (LPCLIENTJOB)Allocate("Client:Job", sizeof(CLIENTJOB));
	if (! lpJob) return FALSE;

	lpJob->lpJobProc    = lpJobProc;
	lpJob->lpCancelProc = lpCancelProc;
	lpJob->lpTimerProc  = lpTimerProc;
	lpJob->dwTimeOut    = dwJobTimeOut;
	lpJob->lpContext    = lpContext;
	lpJob->dwJobId      = dwJobId;
	lpJob->dwJobFlags   = dwJobFlags;
	lpJob->lpExtended   = NULL;


	while (InterlockedExchange(pLock, TRUE)) SwitchToThread();
	for (lpOldJob = lpClient->lpActiveJob ; lpOldJob ; lpOldJob = lpOldJob->lpNext)
	{
		if (lpOldJob->dwJobId == dwJobId) break;
	}
	if (!lpOldJob)
	{
		for (lpOldJob = lpClient->lpJobQueue[HEAD] ; lpOldJob ; lpOldJob = lpOldJob->lpNext)
		{
			if (lpOldJob->dwJobId == dwJobId) break;
		}
	}
	if (lpOldJob)
	{
		lpJob->lpTimer = NULL;

		for ( ; lpOldJob->lpExtended ; lpOldJob = lpOldJob->lpExtended );
		lpOldJob->lpExtended = lpJob;
		// we found a match, so append job onto end of extended list
		lpJob->hEvent = GetThreadEvent();
		ResetEvent(lpJob->hEvent);

		// NOW WE WAIT FOR IT TO FINISH!
		InterlockedExchange(pLock, FALSE);
		SetBlockingThreadFlag();
		WaitForSingleObject(lpJob->hEvent, INFINITE);
		SetNonBlockingThreadFlag();
		return TRUE;
	}
	else
	{
		// just treat it as a new job

		// no sense starting a timer if the timeout was never
		if (lpTimerProc && (dwJobFlags & CJOB_INSTANT) && (dwJobTimeOut != -1))
		{
			lpJob->lpTimer  = StartIoTimer(NULL, lpTimerProc, lpContext, dwJobTimeOut);
		}
		else lpJob->lpTimer  = NULL;

		if (! (dwJobFlags & lpClient->dwJobFilter) || dwJobFlags & CJOB_EXCLUSIVE)
		{
			if (! (lpClient->dwActiveFlags & dwJobFlags) ||
				dwJobFlags & CJOB_INSTANT)
			{
				//  Job can be activated now
				lpJob->lpNext  = lpClient->lpActiveJob;
				lpClient->lpActiveJob  = lpJob;
				lpClient->dwActiveFlags  |= (dwJobFlags & (0xFFFFFFF - (CJOB_EXCLUSIVE|CJOB_INSTANT)));
			}
			else
			{
				//  Append job to pending list
				if (! lpClient->lpJobQueue[HEAD])
				{
					lpClient->lpJobQueue[HEAD]  = lpJob;
				}
				else lpClient->lpJobQueue[TAIL]->lpNext  = lpJob;
				lpClient->lpJobQueue[TAIL]  = lpJob;
				lpJob->lpNext  = NULL;
				lpJob  = NULL;
			}
			bReturn  = TRUE;
		}
		else bReturn  = FALSE;
	}
	InterlockedExchange(pLock, FALSE);

	if (bReturn)
	{
		//  Queue active job
		if (lpJobProc && lpJob) QueueJob(lpJobProc, lpContext, JOB_PRIORITY_NORMAL);
		return TRUE;
	}
	StopIoTimer(lpJob->lpTimer, FALSE);
	Free(lpJob);
	return FALSE;
}


BOOL EndClientJob(DWORD hClient, DWORD dwJobId)
{
  volatile LONG *pLock;
  DWORD      dwActiveFlags, dwJobsToStart;
  LPCLIENTJOB    lpJob, lpJobToStop, lpJobToStart[256], lpPreviousJob, lpNextJob, lpExtended;
  LPCLIENT    lpClient;
  BOOL      bReturn, bLastJob;

  dwActiveFlags  = 0;
  lpJobToStop    = NULL;
  lpPreviousJob  = NULL;
  lpExtended     = NULL;
  dwJobsToStart  = 0;

  pLock    = &lpClientSlot[hClient].lJobLock;
  lpClient  = lpClientArray[hClient];
  while (InterlockedExchange(pLock, TRUE)) SwitchToThread();
  if (lpJob = lpClient->lpActiveJob)
  {
	  if (lpJob->dwJobId == dwJobId)
	  {  
		  if (lpJob->lpExtended)
		  {
			  lpExtended = lpJob;
		  }
		  else
		  {
			  lpJobToStop = lpJob;
			  lpClient->lpActiveJob  = lpJob->lpNext;
		  }
	  }
	  else if (lpJob->lpNext)
	  {
		  dwActiveFlags  |= lpJob->dwJobFlags;
		  do
		  {
			  if (lpJob->lpNext->dwJobId == dwJobId)
			  {
				  if (lpJob->lpNext->lpExtended)
				  {
					  lpExtended = lpJob->lpNext;
				  }
				  else
				  {
					  lpJobToStop  = lpJob->lpNext;
					  lpJob->lpNext  = lpJobToStop->lpNext;
				  }
				  break;
			  }
			  lpJob  = lpJob->lpNext;
			  dwActiveFlags  |= lpJob->dwJobFlags;

		  } while (lpJob->lpNext);
	  }

	  if (lpExtended)
	  {
		  lpJob = lpExtended;
		  lpExtended = lpJob->lpExtended;

		  lpExtended->lpTimer = lpJob->lpTimer;
		  lpExtended->lpNext = lpJob->lpNext;
		  // copy extended job onto current job, keeping next field...
		  CopyMemory(lpJob, lpExtended, sizeof(*lpJob));

		  // release waiting thread.
		  SetEvent(lpJob->hEvent);

		  // create timer
		  if (lpJob->lpTimerProc && (lpJob->dwTimeOut != -1))
		  {
			  lpJob->lpTimer  = StartIoTimer(NULL, lpJob->lpTimerProc, lpJob->lpContext, lpJob->dwTimeOut);
		  }
		  else
		  {
			  lpJob->lpTimer = NULL;
		  }
		  lpJobToStop = lpExtended;
		  lpJobToStart[dwJobsToStart++] = lpJob;
	  }
	  else if (lpJobToStop)
	  {
		  while (lpJob = lpJob->lpNext) dwActiveFlags  |= lpJob->dwJobFlags;
		  dwActiveFlags  &= (0xFFFFFFF - (CJOB_EXCLUSIVE|CJOB_INSTANT));
		  //  Find jobs to start
		  for (lpJob = lpClient->lpJobQueue[HEAD];lpJob;)
		  {
			  if (! (lpJob->dwJobFlags & dwActiveFlags))
			  {
				  lpNextJob  = lpJob->lpNext;
				  lpJobToStart[dwJobsToStart++]  = lpJob;
				  if (lpPreviousJob)
				  {
					  lpPreviousJob->lpNext  = lpNextJob;
				  }
				  else lpClient->lpJobQueue[HEAD]  = lpNextJob;

				  lpJob->lpNext  = lpClient->lpActiveJob;
				  lpClient->lpActiveJob  = lpJob;
				  dwActiveFlags  |= (lpJob->dwJobFlags & (0xFFFFFFFF - (CJOB_EXCLUSIVE|CJOB_INSTANT)));
				  lpJob  = lpNextJob;
			  }
			  else lpJob  = (lpPreviousJob = lpJob)->lpNext;
		  }
		  lpClient->lpJobQueue[TAIL]  = lpPreviousJob;
		  lpClient->dwActiveFlags  = dwActiveFlags;
	  }
	  bLastJob  = (! lpClient->lpActiveJob && ! lpClient->lpJobQueue[HEAD] ? TRUE : FALSE);
  }
  InterlockedExchange(pLock, FALSE);

  //  Stop current job
  if (lpJobToStop)
  {
	  bReturn  = StopIoTimer(lpJobToStop->lpTimer, FALSE);
	  Free(lpJobToStop);
	  if (bLastJob) {
		  QueueJob(lpClient->lpService->lpCloseProc, lpClient->lpUser, JOB_PRIORITY_NORMAL);
	  }
  }
  else bReturn  = FALSE;
  //  Start new jobs
  while (dwJobsToStart--) QueueJob(lpJobToStart[dwJobsToStart]->lpJobProc, lpJobToStart[dwJobsToStart]->lpContext, JOB_PRIORITY_NORMAL);

  return bReturn;
}

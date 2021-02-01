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

/* Q = QueueJob, A = AddClientJob, [job IDs], 
 *
 * Service_AcceptClient (called on new connections)
 *   |
 *   +-RegisterNetworkAddress (creates/looks up HostInfos, handles auto-bans)
 *   |
 *   +-IdentifyClient (resolves IP via lookup or resolve thread)
 *       Q | Q
 *       | | ResolveThread
 *       | |    Q   |
 *       | +--------GetHostClass (apply Reject_Unknown_Ips, Hosts.Rules)
 *       |      |
 *       +----ProcessClient (creates client structures)
 *            A
 *            Ident_Read [10000] (requests ident response, Ends [10000])
 *              |  A  A
 *              |  |  Ident_Copy [10004] (prepares request, Ends [10000] or [10004])
 *              |  |    A
 *              |  +----lpAcceptProc [10005] (new client with valid or cached ident)
 *              |
 *              +-Ident_SendQuery (Ends [10000], starts timer for ident timeout (Ident_TimerProc))
 *                  |
 *                  +-TransmitPackages (low level transmit function)
 *                      |
 *                      Ident_ReceiveResponse (input received)
 *                        | |
 *                        | Receive_Line
 *                        |   |
 *                        +---Ident_ParseResponse (Ends [10000], stops ident timeout)
 *
 *
 * Ident_TimerProc just closes the socket used to resolve ident
 *
 * FTP_New_Client (*lpAcceptProc, Ends [10005])
 * 
 */

#include <ioFTPD.h>

#define IDENT_PORT				    113
#define DEFAULT_RESOLVER_THREADS	2
#define MAX_KNOCK_HOSTS             100
#define MAX_KNOCK_PORTS             5
#define MAX_KNOCK_SEPARATION        60

//	Local declarations
static BOOL ProcessClient(LPNEWCLIENT lpClient);
static BOOL Ident_Copy(LPIDENTCLIENT lpIdentClient);
static BOOL Ident_Read(PCONNECTION_INFO pConnection);

static LPHOSTINFO			*lpHostArray, lpFreeHostList[2];
static LPRESOLVE			lpResolveList[2];;
static CRITICAL_SECTION	csResolveList;
static CRITICAL_SECTION	csHostArray;
static DWORD				dwResolveThreads, dwHostArraySize, dwHostArrayItems;
static DWORD volatile		dwMaxResolverThreads, dwIdentCache;
DWORD volatile              dwIdentTimeOut;
static DWORD volatile		dwTickInterval, dwMaxTicks, dwBanDuration;
static BOOL           bRejectUnknownIps;
static DWORD          dwHostNameCache;
static DWORD          dwObscureIP;
static DWORD          dwObscureHost;
static BOOL           bReverseResolveIPs;

DWORD volatile dwRandomLoginDelay;
DWORD volatile dwDynamicDnsLookup;
DWORD volatile dwMaxLogSuppression;
DWORD volatile dwLogSuppressionIncrement;
BOOL  volatile bIgnoreHostmaskIdents;


typedef struct _KNOCKINFO {
	struct in_addr      InetAddr;
	DWORD               dwKnockTicks;
	DWORD               dwStatus;
} KNOCKINFO, *LPKNOCKINFO;


static DWORD volatile       dwNumKnockPorts;
static DWORD volatile       lpdwKnockPorts[MAX_KNOCK_PORTS];
static SOCKET               lpKnockSocket[MAX_KNOCK_PORTS];

static CRITICAL_SECTION	csKnockHosts;
static DWORD            dwNumKnockHosts;
static LPKNOCKINFO      KnockHosts[MAX_KNOCK_HOSTS];


static USERMASKSET UserIpHostMasks;
static USERMASKSET ImmuneMasks;

static VOID UserMaskSetInit(LPUSERMASKSET lpSet);
static VOID UserMaskSetReset(LPUSERMASKSET lpSet);
static BOOL UserMaskSetAdd(LPUSERMASKSET lpSet, INT32 Uid, char *szIpHost);
static BOOL UserMaskSetAddUserFile(LPUSERMASKSET lpSet, LPUSERFILE lpUserFile);
static VOID UserMaskSetRemoveUser(LPUSERMASKSET lpSet, INT32 Uid);
static BOOL UserMaskKnown(LPUSERMASKSET lpSet, char *szItem, BOOL bNumeric);



BOOL
IsNumericIP(char *szIpHost)
{
	for ( ; *szIpHost ; szIpHost++ )
	{
		if (!isdigit(*szIpHost) && *szIpHost != '.' && *szIpHost != '*')
		{
			return FALSE;
		}
	}
	return TRUE;
}



static VOID
UserMaskSetInit(LPUSERMASKSET lpSet)
{
	lpSet->dwLock   = 0;
	lpSet->dwCount  = 0;
	lpSet->dwMax    = 0;
	lpSet->lppUserIpHostMasks = 0;
}


// lock must be held
static VOID
UserMaskSetReset(LPUSERMASKSET lpSet)
{
    DWORD i;

	for (i=0 ; i < lpSet->dwCount ; i++)
	{
		if (lpSet->lppUserIpHostMasks[i])
		{
			Free(lpSet->lppUserIpHostMasks[i]);
			lpSet->lppUserIpHostMasks[i] = 0;
		}
	}

	lpSet->dwCount = 0;
}



// lock must be held
static BOOL
UserMaskSetAdd(LPUSERMASKSET lpSet, INT32 Uid, char *szIpHost)
{
	LPUSERIPHOSTMASK lpMask;

	if (lpSet->dwCount >= lpSet->dwMax)
	{
		lpSet->dwMax += 100;
		lpSet->lppUserIpHostMasks = ReAllocate(lpSet->lppUserIpHostMasks, "UserIpHostMasks",
			lpSet->dwMax*sizeof(LPUSERIPHOSTMASK));
		if (!lpSet->lppUserIpHostMasks)
		{
			lpSet->dwCount = lpSet->dwMax = 0;
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			return TRUE;
		}
	}

	lpMask = Allocate("UserIpHostMask", sizeof(USERIPHOSTMASK));
	if (!lpMask)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return TRUE;
	}
	lpSet->lppUserIpHostMasks[lpSet->dwCount++] = lpMask;

	lpMask->Uid = Uid;
	lpMask->bNumeric = IsNumericIP(szIpHost);
	ZeroMemory(lpMask->Ip, sizeof(lpMask->Ip));

	strcpy_s(lpMask->Ip, sizeof(lpMask->Ip), szIpHost);
	return FALSE;
}


// lock must be held
static BOOL
UserMaskSetAddUserFile(LPUSERMASKSET lpSet, LPUSERFILE lpUserFile)
{
	int     i;
	LPTSTR   tszAt;

	for (i=0 ; i < MAX_IPS && lpUserFile->Ip[i][0] != 0 ; i++)
	{
		tszAt = strchr(lpUserFile->Ip[i], _T('@'));
		if (!tszAt)
		{
			// this shouldn't really happen since it needs an @ in it
			continue;
		}
		tszAt++;
		if (UserMaskSetAdd(lpSet, lpUserFile->Uid, tszAt))
		{
			return TRUE;
		}
	}
	return FALSE;
}




// lock must be held
static VOID
UserMaskSetRemoveUser(LPUSERMASKSET lpSet, INT32 Uid)
{
	DWORD i;

	for (i=0 ; i < lpSet->dwCount ; i++)
	{
		if (!lpSet->lppUserIpHostMasks[i] || lpSet->lppUserIpHostMasks[i]->Uid != Uid)
		{
			continue;
		}

		Free(lpSet->lppUserIpHostMasks[i]);
		MoveMemory(&lpSet->lppUserIpHostMasks[i], &lpSet->lppUserIpHostMasks[i+1],
			(lpSet->dwCount-i-1)*sizeof(LPUSERIPHOSTMASK));
		lpSet->lppUserIpHostMasks[lpSet->dwCount--] = 0;
	}
}


// lock must be held
static BOOL
UserMaskKnown(LPUSERMASKSET lpSet, char *szItem, BOOL bNumeric)
{
	DWORD i;

	for(i=0 ; i<lpSet->dwCount ; i++)
	{
		if (bNumeric && !lpSet->lppUserIpHostMasks[i]->bNumeric)
		{
			continue;
		}
		if (! iCompare(lpSet->lppUserIpHostMasks[i]->Ip, szItem))
		{
			return TRUE;
		}
	}

	return FALSE;
}



static VOID
UserIpHostMaskInit()
{
	DWORD            i, dwUserCount;
	PINT32           lpUidArray;
	LPUSERFILE       lpUserFile;

	while (InterlockedExchange(&UserIpHostMasks.dwLock, TRUE)) SwitchToThread();

	UserMaskSetReset(&UserIpHostMasks);

	dwUserCount = 0;
	lpUidArray = GetUsers(&dwUserCount);

	if (!lpUidArray)
	{
		InterlockedExchange(&UserIpHostMasks.dwLock, FALSE);
		return;
	}

	for(i=0 ; i < dwUserCount ; i++)
	{
		lpUserFile = 0;
		if (!UserFile_OpenPrimitive(lpUidArray[i], &lpUserFile, 0) && lpUserFile)
		{
			if (UserMaskSetAddUserFile(&UserIpHostMasks, lpUserFile))
			{
				UserFile_Close(&lpUserFile, 0);
				break;
			}
			UserFile_Close(&lpUserFile, 0);
		}
	}

	InterlockedExchange(&UserIpHostMasks.dwLock, FALSE);
	Free(lpUidArray);
}


VOID
UserIpHostMaskRemove(INT32 Uid)
{
	while (InterlockedExchange(&UserIpHostMasks.dwLock, TRUE)) SwitchToThread();

	UserMaskSetRemoveUser(&UserIpHostMasks, Uid);

	InterlockedExchange(&UserIpHostMasks.dwLock, FALSE);
}


BOOL
UserIpHostMaskUpdate(LPUSERFILE lpUserFile)
{
	BOOL          bResult;

	while (InterlockedExchange(&UserIpHostMasks.dwLock, TRUE)) SwitchToThread();

	UserMaskSetRemoveUser(&UserIpHostMasks, lpUserFile->Uid);

	bResult = UserMaskSetAddUserFile(&UserIpHostMasks, lpUserFile);

	InterlockedExchange(&UserIpHostMasks.dwLock, FALSE);

	return bResult;
}


BOOL
UserIpHostMaskKnown(LPTSTR tszHostName, struct in_addr *InetAddress)
{
	TCHAR    tszIp[32];

	if (!bRejectUnknownIps || InetAddress->s_addr == 0x0100007f)
	{
		return TRUE;
	}

	_stprintf_s(tszIp, sizeof(tszIp)/sizeof(*tszIp), "%hs", inet_ntoa(*InetAddress));

	while (InterlockedExchange(&UserIpHostMasks.dwLock, TRUE)) SwitchToThread();

	if (UserMaskKnown(&UserIpHostMasks, tszIp, TRUE) ||
		(tszHostName && tszHostName[0] && UserMaskKnown(&UserIpHostMasks, tszHostName, FALSE)))
	{
		InterlockedExchange(&UserIpHostMasks.dwLock, FALSE);
		return TRUE;
	}

	InterlockedExchange(&UserIpHostMasks.dwLock, FALSE);
	return FALSE;
}


INT
UserIpHostMaskMatches(LPBUFFER lpBuffer, LPTSTR tszMultilinePrefix, LPTSTR tszHost)
{
	DWORD i, dwMatches;
	BOOL  bFirst;
	LPUSERIPHOSTMASK lpMask;
	INT32 iUid;

	while (InterlockedExchange(&UserIpHostMasks.dwLock, TRUE)) SwitchToThread();

	dwMatches = 0;
	bFirst = TRUE;

	for(i=0 ; i < UserIpHostMasks.dwCount ; i++)
	{
		lpMask = UserIpHostMasks.lppUserIpHostMasks[i];
		if (! iCompare(lpMask->Ip, tszHost))
		{
			if (bFirst || (iUid != lpMask->Uid))
			{
				dwMatches++;
				if (bFirst)
				{
					bFirst = FALSE;
				}
				else
				{
					FormatString(lpBuffer, _T("\r\n"));
				}
				FormatString(lpBuffer, _T("%s%s: %s"), tszMultilinePrefix, Uid2User(lpMask->Uid), lpMask->Ip);
			}
			else
			{
				FormatString(lpBuffer, _T(" ; %s"), lpMask->Ip);
			}
			iUid = lpMask->Uid;
		}
	}
	if (!bFirst)
	{
		FormatString(lpBuffer, _T("\r\n"));
	}

	InterlockedExchange(&UserIpHostMasks.dwLock, FALSE);
	FormatString(lpBuffer, "%sTotal matches: %u\r\n", tszMultilinePrefix, dwMatches);
	return dwMatches;
}


INT
ImmuneMaskList(LPBUFFER lpBuffer, LPTSTR tszMultilinePrefix)
{
	DWORD i, dwMatches;
	BOOL  bFirst;
	LPUSERIPHOSTMASK lpMask;
	INT32 iUid;
	LPTSTR tszUser;

	while (InterlockedExchange(&ImmuneMasks.dwLock, TRUE)) SwitchToThread();

	dwMatches = 0;
	bFirst = TRUE;

	for(i=0 ; i < ImmuneMasks.dwCount ; i++)
	{
		lpMask = ImmuneMasks.lppUserIpHostMasks[i];
		if (bFirst || (iUid != lpMask->Uid))
		{
			dwMatches++;
			if (bFirst)
			{
				bFirst = FALSE;
			}
			else
			{
				FormatString(lpBuffer, _T("\r\n"));
			}
			if (lpMask->Uid == -1)
			{
				tszUser = _T("[.ini]");
			}
			else
			{
				tszUser = Uid2User(lpMask->Uid);
			}

			FormatString(lpBuffer, _T("%s%s: %s"), tszMultilinePrefix, tszUser, lpMask->Ip);
		}
		else
		{
			FormatString(lpBuffer, _T(" ; %s"), lpMask->Ip);
		}
		iUid = lpMask->Uid;
	}
	if (!bFirst)
	{
		FormatString(lpBuffer, _T("\r\n"));
	}

	InterlockedExchange(&ImmuneMasks.dwLock, FALSE);
	FormatString(lpBuffer, "%sTotal: %u\r\n", tszMultilinePrefix, dwMatches);
	return dwMatches;
}

BOOL
ImmuneMatch(LPTSTR tszHostName, struct in_addr *InetAddress)
{
	TCHAR    tszIp[32];

	_stprintf_s(tszIp, sizeof(tszIp)/sizeof(*tszIp), "%hs", inet_ntoa(*InetAddress));

	while (InterlockedExchange(&ImmuneMasks.dwLock, TRUE)) SwitchToThread();

	if (UserMaskKnown(&ImmuneMasks, tszIp, TRUE) ||
		(tszHostName && tszHostName[0] && UserMaskKnown(&ImmuneMasks, tszHostName, FALSE)))
	{
		InterlockedExchange(&ImmuneMasks.dwLock, FALSE);
		return TRUE;
	}

	InterlockedExchange(&ImmuneMasks.dwLock, FALSE);
	return FALSE;
}

INT __cdecl
CompareHostInfo(LPHOSTINFO *lpHostInfo1, LPHOSTINFO *lpHostInfo2)
{
	//	Compare network address
	return memcmp(lpHostInfo1[0]->NetworkAddress, lpHostInfo2[0]->NetworkAddress, 8);
}


BOOL FreeHostInfo(LPHOSTINFO lpHostInfo)
{
	LPHOSTINFO	lpSeek;

	while (lpSeek = lpHostInfo)
	{
		lpHostInfo	= lpHostInfo->lpNext;
		FreeShared(lpSeek->szHostName);
		FreeShared(lpSeek->szIdent);
		Free(lpSeek);
	}
	return FALSE;
}





LPBANINFO GetNetworkBans(VOID)
{
	LPHOSTINFO	*lpHostInfo;
	LPBANINFO	lpBanInfo[2];
	DWORD		n, dwTickCount, dwDiff;

	dwTickCount	= GetTickCount();
	lpBanInfo[HEAD]	= NULL;
	lpBanInfo[TAIL]	= NULL;
	EnterCriticalSection(&csHostArray);
	lpHostInfo	= lpHostArray;
	for (n = dwHostArrayItems;n--;lpHostInfo++)
	{
		if (lpHostInfo[0]->dwAttemptCount >= dwMaxTicks &&
			(dwDiff = Time_DifferenceDW32(lpHostInfo[0]->dwLastOccurance, dwTickCount)) < dwBanDuration)
		{
			//	Add ban to list
			if (lpBanInfo[HEAD])
			{
				lpBanInfo[TAIL]->lpNext	= (LPBANINFO)Allocate("BanInfo", sizeof(BANINFO));
				lpBanInfo[TAIL]	= lpBanInfo[TAIL]->lpNext;
			}
			else
			{
				lpBanInfo[HEAD]	= (LPBANINFO)Allocate("BanInfo", sizeof(BANINFO));
				lpBanInfo[TAIL]	= lpBanInfo[HEAD];
			}
			if (! lpBanInfo[TAIL]) break;

			lpBanInfo[TAIL]->dwBanDuration	= dwBanDuration - dwDiff;
			lpBanInfo[TAIL]->dwNetworkAddress	= lpHostInfo[0]->dwNetworkAddress;
			lpBanInfo[TAIL]->dwConnectionAttempts	= lpHostInfo[0]->dwAttemptCount;
			CopyMemory(lpBanInfo[TAIL]->pNetworkAddress, lpHostInfo[0]->NetworkAddress, 8);
		}
	}
	LeaveCriticalSection(&csHostArray);
	if (lpBanInfo[TAIL]) lpBanInfo[TAIL]->lpNext	= NULL;

	return lpBanInfo[HEAD];
}





VOID UnbanNetworkAddress(LPTSTR tszNetworkAddressMask, LPBUFFER lpBuffer, LPTSTR tszMultilinePrefix)
{
	LPHOSTINFO lpHostInfo;
	IN_ADDR    InetAddress;
	LPSTR      tszAddress;
	DWORD      n, dwDiff, dwTickCount;

	//	Binary search
	EnterCriticalSection(&csHostArray);
	
	dwTickCount = GetTickCount();
	for (n=0 ; n<dwHostArrayItems ; n++)
	{
		lpHostInfo = lpHostArray[n];
		if (!lpHostInfo) continue;

		if ((lpHostInfo->dwAttemptCount >= dwMaxTicks) &&
			((dwDiff = Time_DifferenceDW32(lpHostInfo->dwLastOccurance, dwTickCount)) < dwBanDuration))
		{
			// it's banned so now check IP
			InetAddress.s_addr = ((PULONG)lpHostInfo->NetworkAddress)[0];
			tszAddress = inet_ntoa(InetAddress);

			if (!iCompare(tszNetworkAddressMask, tszAddress))
			{
				if (lpBuffer && tszMultilinePrefix)
				{
					FormatString(lpBuffer, _TEXT("%sRemoved banned IP: %s (had %d seconds remaining with %d total attempt(s)).\r\n"),
						tszMultilinePrefix, tszAddress, (dwBanDuration - dwDiff) / 1000, lpHostInfo->dwAttemptCount);
				}
				lpHostInfo->dwAttemptCount = 0;
			}
		}
	}
	LeaveCriticalSection(&csHostArray);
}




LPHOSTINFO
RegisterNetworkAddress(PBYTE pNetworkAddress, DWORD dwNetworkAddress)
{
	LPHOSTINFO	lpHostInfo, lpOldHostInfo, lpSeek;
	LPVOID		lpMemory;
	DWORD		dwDifference, dwTickCount, n;
	BOOL		bReturn, bReject;
	INT			iResult;
	BYTE        LocalHost[8];
	struct      in_addr addr;
	CHAR        szHostName[MAX_HOSTNAME];
	CHAR        szObscuredHost[MAX_HOSTNAME];
	CHAR        szObscuredIP[MAX_HOSTNAME];


	ZeroMemory(LocalHost, sizeof(LocalHost));
	LocalHost[0] = 127;
	LocalHost[3] = 1;
	bReturn		= FALSE;
	bReject     = FALSE;
	dwTickCount	= GetTickCount();
	//	Allocate memory for hostinfo structure
	lpHostInfo	= (LPHOSTINFO)Allocate("Identify:HostInfo", sizeof(HOSTINFO));
	lpOldHostInfo	= NULL;
	if (! lpHostInfo) return NULL;

	//	Initialize structure
	ZeroMemory(lpHostInfo, sizeof(HOSTINFO));
	lpHostInfo->dwNetworkAddress	= dwNetworkAddress;
	lpHostInfo->dwShareCount		= 1;
	lpHostInfo->dwAttemptCount			= 1;
	lpHostInfo->dwLastOccurance		= dwTickCount;
	lpHostInfo->dwHostNameCacheTime	= (dwTickCount > dwHostNameCache ? dwTickCount - dwHostNameCache : 1);
	CopyMemory(lpHostInfo->NetworkAddress, pNetworkAddress, dwNetworkAddress);

	EnterCriticalSection(&csHostArray);
	//	Check usable size
	if (dwHostArraySize == dwHostArrayItems)
	{
		//	Determinate cheapest method
		lpSeek	= lpFreeHostList[HEAD];
		for (n = 0;n < 128 && lpSeek && Time_DifferenceDW32(lpSeek->dwLastOccurance, dwTickCount) > dwBanDuration;n++)
		{
			QuickDelete(lpHostArray, dwHostArrayItems--, lpSeek, (QUICKCOMPAREPROC) CompareHostInfo, NULL);
			lpSeek	= lpSeek->lpNext;
		}

		if (n)
		{
			lpOldHostInfo	= lpFreeHostList[HEAD];
			if ((lpFreeHostList[HEAD] = lpSeek))
			{
				lpSeek->lpPrev->lpNext	= NULL;
				lpSeek->lpPrev	= NULL;
			}
			else lpFreeHostList[TAIL]	= NULL;
		}
		else
		{
			//	Grow array
			lpMemory	= ReAllocate(lpHostArray, "Identify:HostArray",
				sizeof(LPHOSTINFO) * (dwHostArraySize + 1024));
			if (! lpMemory)
			{
				LeaveCriticalSection(&csHostArray);
				Free(lpHostInfo);
				return NULL;
			}
			//	Store new offset
			lpHostArray	= (LPHOSTINFO *)lpMemory;
			//	Increase array size
			dwHostArraySize	+= 1024;
			lpMemory	= NULL;
		}
	}

	//	Insert item to array
	iResult	= QuickInsert(lpHostArray, dwHostArrayItems, lpHostInfo, (QUICKCOMPAREPROC) CompareHostInfo);

	if (iResult--)
	{
		//	Store pointer to allocated memory
		lpMemory	= (LPVOID)lpHostInfo;
		//	Retrieve new pointer from HostArray
		lpHostInfo	= lpHostArray[iResult];
		//	Get time difference
		dwDifference	= Time_DifferenceDW32(lpHostInfo->dwLastOccurance, dwTickCount);
		//	Update tickcount
		lpHostInfo->dwLastOccurance	= dwTickCount;
		//	Check current flags
		if (lpHostInfo->dwAttemptCount < dwMaxTicks ||
			dwDifference > dwBanDuration ||
			!memcmp(lpHostInfo->NetworkAddress, LocalHost, sizeof(LocalHost)) ||
			ImmuneMatch(lpHostInfo->szHostName, (struct in_addr *) lpHostInfo->NetworkAddress))
		{
			//	Increase share count
			if (! lpHostInfo->dwShareCount++)
			{
				//	Update head & tail
				DELETELIST(lpHostInfo, lpFreeHostList);

				lpHostInfo->lpPrev	= NULL;
				lpHostInfo->lpNext	= NULL;
			}

			//	Compare time difference against tick interval
			if (lpHostInfo->dwAttemptCount >= dwMaxTicks ||
				dwDifference > dwTickInterval)
			{
				//	Reset tick count
				lpHostInfo->dwAttemptCount	= 1;
			}
			else lpHostInfo->dwAttemptCount++;
		}
		else
		{
			if (lpHostInfo->dwAttemptCount < (DWORD)-1) lpHostInfo->dwAttemptCount++;
			szHostName[0] = 0;
			if (lpHostInfo->szHostName)
			{
				strcpy_s(szHostName, sizeof(szHostName), lpHostInfo->szHostName);
			}
			memcpy(&addr, lpHostInfo->NetworkAddress, sizeof(addr));
			// moved putlog to be done outside the critical section lock
			if (Time_DifferenceDW32(lpHostInfo->dwLastAutoBanLogTime, dwTickCount) >= lpHostInfo->dwLastAutoBanLogDelay * 60 * 1000)
			{
				if (lpHostInfo->dwLastAutoBanLogDelay < dwMaxLogSuppression)
				{
					lpHostInfo->dwLastAutoBanLogDelay += dwLogSuppressionIncrement;
					if (lpHostInfo->dwLastAutoBanLogDelay > dwMaxLogSuppression)
					{
						lpHostInfo->dwLastAutoBanLogDelay = dwMaxLogSuppression;
					}
				}
				lpHostInfo->dwLastAutoBanLogTime = dwTickCount;
				bReject = TRUE;
			}
			bReturn	= TRUE;
		}
	}
	else dwHostArrayItems++;
	LeaveCriticalSection(&csHostArray);

	if (bReturn && bReject)
	{
		Putlog(LOG_ERROR, _TEXT("Rejected auto-banned IP %s (%s).\r\n"),
			Obscure_IP(szObscuredIP, &addr),
			Obscure_Host(szObscuredHost, szHostName));
	}
	//	Free memory
	if (iResult != -1) Free(lpMemory);
	if (lpOldHostInfo) QueueJob(FreeHostInfo, lpOldHostInfo, JOB_PRIORITY_HIGH);

	return (! bReturn ? lpHostInfo : NULL);
}



VOID UnregisterNetworkAddress(LPHOSTINFO lpHostInfo, DWORD dwCount)
{
	EnterCriticalSection(&csHostArray);
	//	Decrease share count
	if (! (lpHostInfo->dwShareCount -= dwCount))
	{
		//	Append item to-be freed list
		APPENDLIST(lpHostInfo, lpFreeHostList);
	}
	LeaveCriticalSection(&csHostArray);
}






BOOL ResolveThread(LPVOID lpNull)
{
	LPNEWCLIENT	lpNewClient, lpNextClient;
	LPRESOLVE	lpResolve;
	LPHOSTINFO	lpHostInfo;
	PHOSTENT	pHostEnt;
	LPSTR		szHostName, szOldHostName;
	DWORD		dwHostName, dwCount;
	SOCKET      Socket;

	for (;;)
	{
		//	Pop item from resolve list
		EnterCriticalSection(&csResolveList);
		if ((lpResolve = lpResolveList[HEAD]))
		{
			lpResolveList[HEAD]	= lpResolveList[HEAD]->lpNext;
		}
		else dwResolveThreads--;
		LeaveCriticalSection(&csResolveList);

		if (! lpResolve) return FALSE;

		//	Get hostinfo structure
		lpHostInfo	= lpResolve->lpHostInfo;
		Free(lpResolve);

		//	Resolve host
		pHostEnt	= gethostbyaddr((PCHAR)lpHostInfo->NetworkAddress, lpHostInfo->dwNetworkAddress, AF_INET);

		if (pHostEnt && pHostEnt->h_name &&
			(dwHostName = strlen(pHostEnt->h_name)) > 0)
		{
			if (dwHostName > MAX_HOSTNAME - 1) dwHostName	= MAX_HOSTNAME - 1;
			//	Allocate shared memory
			szHostName	= (LPSTR)AllocateShared(NULL, "HostName", dwHostName + 1);

			if (szHostName)
			{
				//	Store string
				CopyMemory(szHostName, pHostEnt->h_name, dwHostName);
				szHostName[dwHostName]	= '\0';
				//	Allocate once more to ensure validity
				AllocateShared(szHostName, NULL, 0);
			}
		}
		else szHostName	= NULL;

		while (InterlockedExchange(&lpHostInfo->lLock, TRUE)) SwitchToThread();
		//	Get old hostname
		szOldHostName	= lpHostInfo->szHostName;

		//	Update hostinfo structure
		lpHostInfo->szHostName			= szHostName;
		lpHostInfo->dwHostNameCacheTime	= GetTickCount();

		GetHostClass(lpHostInfo);

		//	Get pending clients
		lpNewClient	= (LPNEWCLIENT)lpHostInfo->lpNext;
		lpHostInfo->lpNext	= NULL;
		InterlockedExchange(&lpHostInfo->lLock, FALSE);

		dwCount	= 0;
		//	Release pending clients
		for (;lpNewClient;lpNewClient = lpNextClient)
		{
			//	Get next client
			lpNextClient	= lpNewClient->lpNext;

			if (lpHostInfo->ClassInfo.lConnectionsPerHost)
			{
				lpNewClient->szHostName	= (szHostName ? (LPSTR)AllocateShared(szHostName, NULL, 0) : NULL);
				//	Queue clients to next thread
				QueueJob(ProcessClient, lpNewClient, JOB_PRIORITY_NORMAL);
			}
			else
			{
				//	Free client resources
				Socket  = (SOCKET)InterlockedExchange(&lpNewClient->Socket, INVALID_SOCKET);
				if (Socket != INVALID_SOCKET) {
					closesocket(Socket);
				}
				Free(lpNewClient);
				dwCount++;
			}
		}
		//	Free resources
		if (dwCount) UnregisterNetworkAddress(lpHostInfo, dwCount);
		FreeShared(szOldHostName);
		FreeShared(szHostName);
	}
}


VOID QueueHostResolve(LPRESOLVE lpResolve)
{
	BOOL bNewThread = FALSE;

	lpResolve->lpNext		= NULL;

	//	Push item to list
	EnterCriticalSection(&csResolveList);
	if (! lpResolveList[HEAD])
	{
		lpResolveList[HEAD]	= lpResolve;
	}
	else lpResolveList[TAIL]->lpNext	= lpResolve;
	lpResolveList[TAIL]	= lpResolve;
	//	Require new resolver thread?
	if (dwResolveThreads < dwMaxResolverThreads)
	{
		dwResolveThreads++;
		bNewThread	= TRUE;
	}
	LeaveCriticalSection(&csResolveList);

	//	Queue job to new thread
	if (bNewThread) QueueJob(ResolveThread, NULL, JOB_PRIORITY_HIGH);
}


BOOL IdentifyClient(LPNEWCLIENT lpNewClient)
{
	LPRESOLVE	lpResolve;
	LPHOSTINFO	lpHostInfo;
	BOOL		bError, bSkipResolve;
	SOCKET      Socket;

	bError			= TRUE;
	bSkipResolve	= FALSE;

	lpHostInfo = lpNewClient->lpHostInfo;

	if (!bReverseResolveIPs)
	{
		bSkipResolve = TRUE;

		//	Perform host check
		if (lpHostInfo->ClassInfo.dwClassListId != GetCurrentClassListId())
		{
			GetHostClass(lpHostInfo);
		}
	}
	else if (lpHostInfo)
	{
		while (InterlockedExchange(&lpHostInfo->lLock, TRUE)) SwitchToThread();
		//	Check if we are supposed to be added to resolve queue
		if (lpHostInfo->lpNext)
		{
			//	Append to queue
			lpNewClient->lpNext	= (LPNEWCLIENT)lpHostInfo->lpNext;
			lpHostInfo->lpNext	= (LPHOSTINFO)lpNewClient;
			bError	= FALSE;
		}
		else if (Time_DifferenceDW32(lpHostInfo->dwHostNameCacheTime, GetTickCount()) >= dwHostNameCache)
		{
			//	Allocate memory for resolve item
			lpResolve = (LPRESOLVE)Allocate("Identify:Resolve", sizeof(RESOLVE));

			if (lpResolve)
			{
				lpNewClient->lpNext		= NULL;
				lpResolve->lpHostInfo	= lpHostInfo;
				lpHostInfo->lpNext		= (LPHOSTINFO)lpNewClient;

				QueueHostResolve(lpResolve);
				bError	= FALSE;
			}
		}
		else
		{
			//	Perform host check
			if (lpHostInfo->ClassInfo.dwClassListId != GetCurrentClassListId())
			{
				GetHostClass(lpHostInfo);
			}

			if (lpHostInfo->ClassInfo.lConnectionsPerHost)
			{
				//	Copy hostname
				lpNewClient->szHostName	= (lpHostInfo->szHostName ?
					(LPSTR)AllocateShared(lpHostInfo->szHostName, NULL, 0) : NULL);
				bSkipResolve	= TRUE;
			}
		}
		InterlockedExchange(&lpHostInfo->lLock, FALSE);
	}

	if (bSkipResolve)
	{
		//	Queue client to next thread
		QueueJob(ProcessClient, lpNewClient, JOB_PRIORITY_NORMAL);
	}
	else if (bError)
	{
		//	Free client resources
		Socket  = (SOCKET)InterlockedExchange(&lpNewClient->Socket, INVALID_SOCKET);
		if (Socket != INVALID_SOCKET) {
			closesocket(Socket);
		}
		Free(lpNewClient);
		if (lpHostInfo) UnregisterNetworkAddress(lpHostInfo, 1);
	}

	return FALSE;
}






static BOOL ProcessClient(LPNEWCLIENT lpNewClient)
{
	LPFTPUSER			lpUser;
	PCONNECTION_INFO	pConnection;
	LPIOSOCKET			lpSocket;
	DWORD				dwClientId;
	SOCKET              Socket;

	//	Allocate memory
	if (lpUser = Allocate("FtpUser", sizeof(*lpUser)))
	{
		ZeroMemory(lpUser, sizeof(*lpUser));
		pConnection	= &lpUser->Connection;
		lpSocket    = &lpUser->CommandChannel.Socket;

		//	Update connection structure
		pConnection->lpService	= lpNewClient->lpService;
		pConnection->lpHostInfo	= lpNewClient->lpHostInfo;
		pConnection->szHostName	= lpNewClient->szHostName;
		pConnection->tLogin		= time((time_t*)NULL);
		//	Copy remote address
		CopyMemory(&pConnection->LocalAddress, lpNewClient->lpLocalSockAddress, sizeof(struct sockaddr_in));
		CopyMemory(&pConnection->ClientAddress, lpNewClient->lpRemoteSockAddress, sizeof(struct sockaddr_in));
		//	Get client id
		dwClientId	= RegisterClient(pConnection, lpUser);
		//	Check result
		if (dwClientId != (DWORD)-1)
		{
			lpSocket->Socket	= lpNewClient->Socket;
			IoSocketInit(lpSocket);
			lpSocket->dwClientId = dwClientId;
			BindSocketToDevice(lpNewClient->lpService, lpSocket, NULL, NULL, BIND_FAKE);
			pConnection->lpDevice   = lpSocket->lpDevice;
			pConnection->dwUniqueId	= dwClientId;

			UnlockClient(dwClientId);

			//	Queue new job
			if (!dwIdentTimeOut)
			{
				// 0 timeout so why bother even trying just queue accept proc
				AddClientJob(pConnection->dwUniqueId, 10005, TERTIARY, INFINITE, pConnection->lpService->lpAcceptProc, NULL, NULL, pConnection);
			}
			else
			{
				AddClientJob(dwClientId, 10000, PRIMARY|EXCLUSIVE, INFINITE, Ident_Read, NULL, NULL, pConnection);
			}
			//	Free memory
			Free(lpNewClient);
			return FALSE;
		}
		Free(lpUser);
	}
	//	Free resources
	Socket  = (SOCKET)InterlockedExchange(&lpNewClient->Socket, INVALID_SOCKET);
	if (Socket != INVALID_SOCKET) {
		closesocket(Socket);
	}
	UnregisterNetworkAddress(lpNewClient->lpHostInfo, 1);
	FreeShared(lpNewClient->szHostName);
	Free(lpNewClient);

	return FALSE;
}




LPSTR PushSpaces(LPSTR StrIn)
{
	while (StrIn[0] == ' ') StrIn++;
	return StrIn;
}





DWORD Ident_TimerProc(LPIOSOCKET lpSocket, LPTIMER lpTimer)
{
	//	Close socket
	CloseSocket(lpSocket, TRUE);
	return 0;
}


VOID Ident_ParseResponse(LPIDENTCLIENT lpClient, LPSTR szLine, DWORD dwLastError, ULONG ulSslError)
{	
	StopIoTimer(lpClient->lpTimer, FALSE);
	if (dwLastError == NO_ERROR)
	{
		lpClient->szIdentReply	= szLine;
	}
	else lpClient->dwLastError	= dwLastError;
	EndClientJob(lpClient->dwClientId, 10000);
}



VOID Ident_ReceiveResponse(LPIDENTCLIENT lpClient, DWORD dwLastError, INT64 i64Total, ULONG ulSslError)
{
	if (dwLastError == NO_ERROR)
	{
		ReceiveLine(&lpClient->ioSocket, 512, Ident_ParseResponse, lpClient);
	}
	else Ident_ParseResponse(lpClient, NULL, dwLastError, ulSslError);
}




BOOL Ident_SendQuery(LPIDENTCLIENT lpClient)
{
	IOBUFFER	Buffer[1];

	//	Check for connection error
	if (lpClient->dwLastError == NO_ERROR)
	{
		lpClient->lpTimer	= StartIoTimer(NULL, Ident_TimerProc, &lpClient->ioSocket, dwIdentTimeOut);
		if (lpClient->lpTimer)
		{
			Buffer[0].dwType		= PACKAGE_BUFFER_SEND;
			Buffer[0].len			= lpClient->dwBuffer;
			Buffer[0].buf			= lpClient->pBuffer;
			Buffer[0].dwTimerType	= NO_TIMER;

			TransmitPackages(&lpClient->ioSocket,	Buffer, 1, NULL, Ident_ReceiveResponse, lpClient);
			return FALSE;
		}
		lpClient->dwLastError	= GetLastError();
		if (lpClient->lpTimer) StopIoTimer(lpClient->lpTimer, FALSE);
	}
	EndClientJob(lpClient->dwClientId, 10000);
	return FALSE;
}








static BOOL Ident_Read(PCONNECTION_INFO pConnection)
{
	struct sockaddr_in	ServerAddress;
	LPIDENTCLIENT		lpIdentClient;
	LPHOSTINFO			lpHostInfo;
	DWORD				dwDifference, dwClientId, dwLimit;
	BOOL				bCopyIdent;
	SOCKET				Socket;
	INT					iResult;

	bCopyIdent	= FALSE;
	dwLimit		= 1;
	dwClientId	= pConnection->dwUniqueId;
	lpHostInfo	= pConnection->lpHostInfo;

	while (InterlockedExchange(&lpHostInfo->lLock, TRUE)) SwitchToThread();
	//	Check if there is already a queue
	if (lpHostInfo->lpPrev)
	{
		pConnection->lpHostInfo	= lpHostInfo->lpPrev;
		lpHostInfo->lpPrev		= (LPHOSTINFO)pConnection;
	}
	else
	{
		dwDifference	= Time_DifferenceDW32(lpHostInfo->dwIdentCacheTime, GetTickCount());
		//	Check ident validity
		if (dwDifference >= dwIdentCache)
		{
			//	Allocate memory for new client
			lpIdentClient	= (LPIDENTCLIENT)Allocate("Identify:IdentClient", sizeof(IDENTCLIENT));

			if (lpIdentClient)
			{
				pConnection->lpHostInfo	= NULL;
				lpHostInfo->lpPrev		= (LPHOSTINFO)pConnection;
				InterlockedExchange(&lpHostInfo->lLock, FALSE);

				ZeroMemory(lpIdentClient, sizeof(IDENTCLIENT));
				ZeroMemory(&ServerAddress.sin_zero, sizeof(ServerAddress.sin_zero));
				//	Queue new job for client
				AddClientJob(dwClientId, 10004, PRIMARY|EXCLUSIVE, INFINITE, Ident_Copy, NULL, NULL, lpIdentClient);
				//	Read Client Ident
				lpIdentClient->wClientPort	= ntohs(pConnection->ClientAddress.sin_port);
				lpIdentClient->wServerPort	= ntohs(pConnection->LocalAddress.sin_port);

				//	Create socket
				Socket	= OpenSocket();
				if (Socket != INVALID_SOCKET)
				{
					//	Initialize identity store
					lpIdentClient->dwClientId		= dwClientId;
					lpIdentClient->lpHostInfo		= lpHostInfo;
					lpIdentClient->dwBuffer			= sprintf(lpIdentClient->pBuffer, "%u,%u\r\n", lpIdentClient->wClientPort, lpIdentClient->wServerPort);
					//	Initialize remote server address
					ServerAddress.sin_port			= htons(IDENT_PORT);
					ServerAddress.sin_family		= AF_INET;
					ServerAddress.sin_addr.s_addr	= pConnection->ClientAddress.sin_addr.s_addr;

					//	Bind socket to local address (do not care about result)
					BindSocket(Socket, pConnection->LocalAddress.sin_addr.s_addr, 0, FALSE);
					//	Update socket
					lpIdentClient->ioSocket.Socket	= Socket;
					lpIdentClient->ioSocket.lpDevice	= pConnection->lpDevice;

					IoSocketInit(&lpIdentClient->ioSocket);

					BindCompletionPort((HANDLE)Socket);
					SetSocketOption(&lpIdentClient->ioSocket, IO_SOCKET, SEND_LIMIT, (PCHAR)&dwLimit, sizeof(DWORD));
					SetSocketOption(&lpIdentClient->ioSocket, IO_SOCKET, RECEIVE_LIMIT, (PCHAR)&dwLimit, sizeof(DWORD));

					//	Create async select event
					if (!WSAAsyncSelectWithTimeout(&lpIdentClient->ioSocket, dwIdentTimeOut, FD_CONNECT, &lpIdentClient->dwLastError))
					{
						//	Connect to server
						iResult	= WSAConnect(Socket, (struct sockaddr *)&ServerAddress,
							sizeof(struct sockaddr_in), NULL, NULL, NULL, NULL);
						//	Check result
						if (iResult != SOCKET_ERROR ||
							WSAGetLastError() == WSAEWOULDBLOCK)
						{
							//	Send continue message
							WSAAsyncSelectContinue(&lpIdentClient->ioSocket, Ident_SendQuery, lpIdentClient);
							return FALSE;
						}
						WSAAsyncSelectCancel(&lpIdentClient->ioSocket);
					}
					closesocket(Socket);
				}
				lpIdentClient->ioSocket.Socket	= INVALID_SOCKET;

				EndClientJob(dwClientId, 10000);
				return FALSE;
			}
		}
		bCopyIdent	= TRUE;
	}
	//	Copy identity
	if (bCopyIdent)
	{
		pConnection->szIdent	=
			(lpHostInfo->szIdent ? (LPSTR)AllocateShared(lpHostInfo->szIdent, NULL, 0) : NULL);
	}
	InterlockedExchange(&lpHostInfo->lLock, FALSE);

	//	Queue new job
	if (bCopyIdent)
	{
		AddClientJob(pConnection->dwUniqueId, 10005, TERTIARY, INFINITE, pConnection->lpService->lpAcceptProc, NULL, NULL, pConnection);
		EndClientJob(dwClientId, 10000);
	}
	return FALSE;
}






static BOOL Ident_Copy(LPIDENTCLIENT lpIdentClient)
{
	PCONNECTION_INFO	pConnection;
	LPHOSTINFO			lpHostInfo;
	LPVOID				lpMemory;
	LPSTR				szIdent;
	DWORD				dwIdent, dwClientId;

	//	Get hostinfo
	lpHostInfo	= lpIdentClient->lpHostInfo;
	lpMemory	= NULL;

	//	Parse ident request
	if (lpIdentClient->szIdentReply &&
		(szIdent = PushSpaces(lpIdentClient->szIdentReply)) &&
		(DWORD)strtol(szIdent, &szIdent, 10) == lpIdentClient->wClientPort &&
		(szIdent = PushSpaces(szIdent)) && szIdent[0] == ',' &&
		(szIdent = PushSpaces(&szIdent[1])) &&
		(DWORD)strtol(szIdent, &szIdent, 10) == lpIdentClient->wServerPort &&
		(szIdent = PushSpaces(szIdent)) && szIdent[0] == ':' &&
		(szIdent = PushSpaces(&szIdent[1])) && ! strncmp(szIdent, "USERID", 6) &&
		(szIdent = PushSpaces(&szIdent[6])) && szIdent[0] == ':' &&
		(szIdent = strchr(&szIdent[1], ':')))
	{
		while ((++szIdent)[0] == ' ');

		//	Calculate ident length
		dwIdent	= &lpIdentClient->pBuffer[lpIdentClient->dwBuffer] - szIdent;
		if (dwIdent > MAX_IDENT - 1) dwIdent	= MAX_IDENT - 1;

		//	Allocate memory for new variable
		if ((lpMemory = (dwIdent ? AllocateShared(NULL, "Ident", dwIdent + 1) : NULL)))
		{
			//	Copy ident to memory
			lpMemory	= AllocateShared(lpMemory, NULL, 0);
			CopyMemory(lpMemory, szIdent, dwIdent);
			((PCHAR)((ULONG)lpMemory + dwIdent))[0]	= '\0';
		}
	}

	while (InterlockedExchange(&lpHostInfo->lLock, TRUE)) SwitchToThread();
	//	Get old ident
	szIdent	= lpHostInfo->szIdent;
	lpHostInfo->szIdent	= (LPSTR)lpMemory;
	lpHostInfo->dwIdentCacheTime	= GetTickCount();
	//	Get pending connections
	pConnection	= (PCONNECTION_INFO)lpHostInfo->lpPrev;
	//	Reset pointer
	lpHostInfo->lpPrev	= NULL;
	InterlockedExchange(&lpHostInfo->lLock, FALSE);

	//	Free resources
	lpIdentClient->ioSocket.lpDevice	= NULL;
	ioDeleteSocket(&lpIdentClient->ioSocket, TRUE);
	Free(lpIdentClient);
	FreeShared(szIdent);

	szIdent	= (LPSTR)lpMemory;
	//	Release pending clients
	for (;pConnection;pConnection = (PCONNECTION_INFO)lpMemory)
	{
		dwClientId	= pConnection->dwUniqueId;
		lpMemory	= pConnection->lpHostInfo;
		pConnection->szIdent	= (szIdent ? (LPSTR)AllocateShared(szIdent, NULL, 0) : NULL);
		pConnection->lpHostInfo = lpHostInfo;

		UpdateClientData(DATA_IDENT, dwClientId, szIdent, lpHostInfo->szHostName, pConnection->ClientAddress.sin_addr.s_addr);
		//	Queue new job for client
		AddClientJob(dwClientId, 10005, TERTIARY, INFINITE, pConnection->lpService->lpAcceptProc, NULL, NULL, pConnection);
		EndClientJob(dwClientId, (lpMemory ? 10000 : 10004));
	}
	FreeShared(szIdent);

	return FALSE;
}


INT __cdecl
CompareKnockInfo(LPKNOCKINFO *lpKnockInfo1, LPKNOCKINFO *lpKnockInfo2)
{
	//	Compare network address
	return memcmp(&lpKnockInfo1[0]->InetAddr, &lpKnockInfo2[0]->InetAddr, sizeof(lpKnockInfo1[0]->InetAddr));
}



BOOL
IpHasKnocked(PBYTE pInetAddress, DWORD dwAddrLen)
{
	KNOCKINFO KnockInfo, *lpKnock;
	int       iMatch;

	if (dwAddrLen > sizeof(KnockInfo.InetAddr))
	{
		return FALSE;
	}
	CopyMemory(&KnockInfo.InetAddr, pInetAddress, dwAddrLen);
	EnterCriticalSection(&csKnockHosts);
	iMatch = QuickFind((LPVOID *)KnockHosts, dwNumKnockHosts, &KnockInfo, (QUICKCOMPAREPROC) CompareKnockInfo);
	if (!iMatch)
	{
		// not there
		LeaveCriticalSection(&csKnockHosts);
		return FALSE;
	}
	lpKnock = KnockHosts[iMatch-1];
	if (lpKnock && lpKnock->dwStatus >= dwNumKnockPorts)
	{
		LeaveCriticalSection(&csKnockHosts);
		return TRUE;
	}
	LeaveCriticalSection(&csKnockHosts);
	return FALSE;
}


static int CALLBACK
Knock_Reject(LPWSABUF lpCallerId, LPWSABUF lpCallerData, LPQOS lpSQOS, LPQOS lpGQOS,
			 LPWSABUF lpCalleeId, LPWSABUF lpCalleeData, GROUP *g, DWORD_PTR dwCallbackData)
{
	struct sockaddr_in *pAddr = (struct sockaddr_in *) dwCallbackData;

	if (lpCallerId && lpCallerId->len == sizeof(struct sockaddr_in))
	{
		CopyMemory(pAddr, lpCallerId->buf, lpCallerId->len);
	}
	return CF_REJECT;
}


static LRESULT
Knock_Accept(WPARAM wParam, LPARAM lParam)
{
	DWORD  n, dwIndex, dwPort, dwTicks;
	SOCKET Socket = (SOCKET) wParam;
	SOCKET NewSocket;
	int    iSize, iMatch;
	struct sockaddr_in addrA, addrR;
	KNOCKINFO KnockInfo, *lpKnock;
	BOOL   bDone;
	CHAR szObscuredHost[MAX_HOSTNAME];

	// dwEvent = WSAGETSELECTEVENT(lParam);
	// dwError = WSAGETSELECTERROR(lParam);

	for(dwIndex=0 ; dwIndex<dwNumKnockPorts ; dwIndex++)
	{
		if (lpKnockSocket[dwIndex] == Socket) break;
	}
	if ( dwIndex >= dwNumKnockPorts )
	{
		return FALSE;
	}

	dwPort  = lpdwKnockPorts[dwIndex];

	iSize	= sizeof(addrA);
	ZeroMemory(&addrR, sizeof(addrR));
	AcquireHandleLock();
	NewSocket = WSAAccept(Socket, (struct sockaddr *) &addrA, &iSize, Knock_Reject, (DWORD_PTR) &addrR);
	if (NewSocket != INVALID_SOCKET) SetHandleInformation((HANDLE)NewSocket, HANDLE_FLAG_INHERIT, 0);
	ReleaseHandleLock();

	if ((NewSocket == INVALID_SOCKET) && (WSAGetLastError() != WSAECONNREFUSED))
	{
		// something wrong... abort
		return FALSE;
	}
	if (NewSocket != INVALID_SOCKET)
	{
		// we accepted it somehow?
		closesocket(NewSocket);
		return FALSE;
	}

	bDone = TRUE;
	if (dwNumKnockPorts > 1)
	{
		// keeping state only useful if more than 1 port used

		EnterCriticalSection(&csKnockHosts);
		dwTicks = GetTickCount();
		CopyMemory(&KnockInfo.InetAddr, &addrR.sin_addr, sizeof(addrR.sin_addr));
		KnockInfo.dwStatus     = 0;
		KnockInfo.dwKnockTicks = dwTicks;

		if (dwNumKnockHosts == MAX_KNOCK_HOSTS)
		{
			// time to clean house!
			for (n=0 ; n < dwNumKnockHosts ; )
			{
				if (Time_DifferenceDW32(dwTicks, KnockHosts[n]->dwKnockTicks) > MAX_KNOCK_SEPARATION)
				{
					QuickDeleteIndex(KnockHosts, dwNumKnockHosts--, n+1);
					continue;
				}
				n++;
			}
			if (dwNumKnockHosts == MAX_KNOCK_HOSTS)
			{
				// getting flooded, try a search for the current entry
				iMatch = QuickFind(KnockHosts, dwNumKnockHosts, &KnockInfo, (QUICKCOMPAREPROC) CompareKnockInfo);
				if (!iMatch)
				{
					// not there, so just abort
					LeaveCriticalSection(&csKnockHosts);
					return FALSE;
				}
			}
		}

		// room now, or matching entry will be found, so try to add the stack version to find a match.  if
		// it gets added we will replace it with a new allocated version
		iMatch = QuickInsert2(KnockHosts, dwNumKnockHosts, &KnockInfo, (QUICKCOMPAREPROC) CompareKnockInfo);

		if (iMatch > 0)
		{
			// it's an existing entry
			lpKnock = KnockHosts[iMatch-1];
		}
		else
		{
			// it's a new entry
			// iMatch is negative value whose absolute value representing offset from 1...
			iMatch = -iMatch;
			lpKnock = Allocate("KnockInfo", sizeof(KNOCKINFO));
			if (!lpKnock)
			{
				// cleanup time!
				QuickDeleteIndex(KnockHosts, dwNumKnockHosts, iMatch);
				LeaveCriticalSection(&csKnockHosts);
				return FALSE;
			}
			dwNumKnockHosts++;
			CopyMemory(lpKnock, &KnockInfo, sizeof(KnockInfo));
			KnockHosts[iMatch-1] = lpKnock;
		}


		// now test to see if sequence is OK
		if (lpKnock->dwStatus != dwIndex)
		{
			// out of sequence, reset and return
			lpKnock->dwStatus = 0;
			LeaveCriticalSection(&csKnockHosts);
			return FALSE;
		}
		lpKnock->dwStatus++;

		if (lpKnock->dwStatus < dwNumKnockPorts)
		{
			bDone = FALSE;
		}
		else
		{
			// we are done... remove the entry from the list
			QuickDeleteIndex((LPVOID *)KnockHosts, dwNumKnockHosts--, iMatch);
			KnockHosts[iMatch-1] = 0;
		}
	}

	LeaveCriticalSection(&csKnockHosts);

	if (!bDone)
	{
		return FALSE;
	}

	Putlog(LOG_GENERAL, _TEXT("KNOCK: \"%s\"\r\n"),	Obscure_IP(szObscuredHost, &KnockInfo.InetAddr));

	return TRUE;
}


LPSTR
Obscure_IP(LPSTR szBuffer, struct in_addr *pAddr)
{
	switch (dwObscureIP)
	{
	case 0:
		sprintf(szBuffer, "%d.%d.%d.%d",
			pAddr->S_un.S_un_b.s_b1,
			pAddr->S_un.S_un_b.s_b2,
			pAddr->S_un.S_un_b.s_b3,
			pAddr->S_un.S_un_b.s_b4);
		return szBuffer;
	case 1:
		sprintf(szBuffer, "%d.%d.%d.#",
			pAddr->S_un.S_un_b.s_b1,
			pAddr->S_un.S_un_b.s_b2,
			pAddr->S_un.S_un_b.s_b3);
		return szBuffer;
	case 2:
		sprintf(szBuffer, "%d.%d.#.#",
			pAddr->S_un.S_un_b.s_b1,
			pAddr->S_un.S_un_b.s_b2);
		return szBuffer;
	case 3:
		sprintf(szBuffer, "%d.#.#.#",
			pAddr->S_un.S_un_b.s_b1);
		return szBuffer;
	default:
		sprintf(szBuffer, "#.#.#.#");
		return szBuffer;
	}
}


LPSTR
Obscure_Host(LPSTR szBuffer, LPSTR szHost)
{
	LPSTR szTemp, szOut;
	DWORD dwDots, n;
	BOOL  bField;

	if (dwObscureHost == 0 || szHost[0] == 0)
	{
		// don't obscure at all, or empty string to start with
		strcpy(szBuffer, szHost);
		return szBuffer;
	}

	szOut  = szBuffer;
	szTemp = szHost;
	n = MAX_HOSTNAME-1;
	dwDots = 0;
	bField = FALSE;
	while (n && *szTemp && dwDots < dwObscureHost)
	{
		if (!bField && *szTemp == '*' && (szTemp[1] == '.' || !szTemp[1]))
		{
			dwDots++;
			*szOut++ = '*';
			n--;
			szTemp++;
			if (!n || !*szTemp)
			{
				*szOut = 0;
				return szBuffer;
			}
			*szOut++ = '.';
			n--;
		}
		else if (*szTemp == '.')
		{
			dwDots++;
			if (bField)
			{
				*szOut++ = '#';
				n--;
				if (!n)
				{
					*szOut = 0;
					return szBuffer;
				}
			}
			bField = FALSE;
			*szOut++ = '.';
			n--;
		}
		else
		{
			bField = TRUE;
		}
		szTemp++;
	}
	if (n && dwDots < dwObscureHost && bField)
	{
		*szOut++ = '#';
		n--;
	}
	while (n && *szTemp)
	{
		*szOut++ = *szTemp++;
		n--;
	}
	*szOut = 0;
	return szBuffer;
}


LPSTR
Obscure_Mask(LPSTR szBuffer, LPSTR szMask)
{
	LPSTR szTemp, szOut;
	DWORD dwDots;


	szTemp = szMask;
	szOut = szBuffer;

	for ( ; *szTemp && *szTemp != '@' ; szTemp++)
	{
		*szOut++ = *szTemp;
	}
	if (!*szTemp)
	{
		*szOut = 0;
		return szBuffer;
	}
	
	*szOut++ = *szTemp++;
	// szTemp now points to the host/IP section of the mask
	if (!IsNumericIP(szTemp))
	{
		Obscure_Host(szOut, szTemp);
		return szBuffer;
	}
	if (!dwObscureIP)
	{
		strcpy(szOut, szTemp);
		return szBuffer;
	}

	dwDots = 0;
	while (*szTemp && dwDots < 4 - dwObscureIP)
	{
		if (*szTemp == '.')
		{
			dwDots++;
		}
		*szOut++ = *szTemp++;
	}

	// only bother obscuring if mask includes the fields
	for( ; *szTemp && dwDots < 4 ; dwDots++ )
	{
		if (*szTemp == '*' && (!*(szTemp+1) || *(szTemp+1) == '.'))
		{
			*szOut++ = '*';
		}
		else
		{
			*szOut++ = '#';
		}
		for ( ; *szTemp && *szTemp != '.' ; szTemp++);

		if (*szTemp)
		{
			*szOut++ = *szTemp++;
		}
	}
	*szOut = 0;

	return szBuffer;
}


BOOL Identify_Init(BOOL bFirstInitialization)
{
	DWORD	dwIdentTimeOutT, dwBanDurationT, dwMaxTicksT;
	DWORD	dwTickIntervalT, dwIdentCacheT, dwHostNameCacheT;
	DWORD   n, m, dwPort, dwUserIds;
	LPTSTR  tszTemp, tszTemp2, tszEnd;
	LPUSERFILE lpUserFile;
	TCHAR   tszKnock[_INI_LINE_LENGTH + 1];
	SOCKET  Socket;
	PINT32  lpUserIds;
	INT32   Uid;

	if (bFirstInitialization)
	{
		//	Reset local variables
		lpHostArray			= NULL;
		lpFreeHostList[HEAD]	= NULL;
		lpFreeHostList[TAIL]	= NULL;
		lpResolveList[HEAD]	= NULL;
		lpResolveList[TAIL]	= NULL;
		dwResolveThreads	= 0;
		dwHostArraySize		= 0;
		dwHostArrayItems	= 0;
		dwHostNameCacheT	= 1800;			//	Default to 30minutes
		dwIdentCacheT		= 1800;			//	Default to 30minutes
		dwTickIntervalT		= 60;			//	Default to 60seconds
		dwMaxTicksT			= 5;			//	Default to 5ticks
		dwBanDurationT		= 600;			//	Default to 10minutes
		dwIdentTimeOutT		= 10;			//	Default to 10seconds
		UserMaskSetInit(&UserIpHostMasks);
		UserMaskSetInit(&ImmuneMasks);

		//	Initialize critical sections
		InitializeCriticalSectionAndSpinCount(&csResolveList, 100);
		InitializeCriticalSectionAndSpinCount(&csHostArray, 100);
		InitializeCriticalSectionAndSpinCount(&csKnockHosts, 100);

		InstallMessageHandler(WM_KNOCK, Knock_Accept, TRUE, FALSE);

		bRejectUnknownIps    = FALSE;
		dwMaxResolverThreads = DEFAULT_RESOLVER_THREADS;

		dwNumKnockHosts = 0;
		ZeroMemory(KnockHosts, sizeof(KnockHosts));

		dwObscureIP = 0;
		dwObscureHost = 0;
		dwRandomLoginDelay = 0;
		dwDynamicDnsLookup = 0;
		dwMaxLogSuppression     = 10;
		dwLogSuppressionIncrement = 1;

		bReverseResolveIPs = TRUE;
		bIgnoreHostmaskIdents = FALSE;
		dwNumKnockPorts     = 0;

		for (n=0; n < MAX_KNOCK_PORTS ; n++)
		{
			lpdwKnockPorts[n] = 0;
			lpKnockSocket[n] = 0;
		}

	}
	else
	{
		dwHostNameCacheT	= dwHostNameCache / 1000;
		dwIdentCacheT		= dwIdentCache / 1000;
		dwTickIntervalT		= dwTickInterval / 1000;
		dwMaxTicksT			= dwMaxTicks;
		dwBanDurationT		= dwBanDuration / 1000;
		dwIdentTimeOutT		= dwIdentTimeOut / 1000;
		dwRandomLoginDelay  = dwRandomLoginDelay / 1000;
	}
	//	Get values from config
	Config_Get_Bool(&IniConfigFile, _TEXT("Network"), _TEXT("Reverse_Resolve_IPs"), &bReverseResolveIPs);
	Config_Get_Bool(&IniConfigFile, _TEXT("Network"), _TEXT("Ignore_Hostmask_Idents"), (PINT) &bIgnoreHostmaskIdents);
	Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("Ident_TimeOut"), (PINT) &dwIdentTimeOutT);
	Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("HostName_Cache_Duration"), (PINT)&dwHostNameCacheT);
	Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("Ident_Cache_Duration"), (PINT)&dwIdentCacheT);
	Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("Temporary_Ban_Duration"), (PINT)&dwBanDurationT);
	Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("Connections_To_Ban"), (PINT)&dwMaxTicksT);
	Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("Ban_Counter_Reset_Interval"), (PINT)&dwTickIntervalT);

	Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("Max_Log_Suppression"), (PINT)&dwMaxLogSuppression);
	Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("Log_Suppression_Increment"), (PINT)&dwLogSuppressionIncrement);
	Config_Get_Bool(&IniConfigFile, _TEXT("Network"), _TEXT("Reject_Unknown_Ips"), &bRejectUnknownIps);
	Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("Max_Resolver_Threads"), (PINT)&dwMaxResolverThreads);

	Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("Obscure_IP"), (PINT)&dwObscureIP);
	if (dwObscureIP > 4) dwObscureIP = 4;
	Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("Obscure_Host"), (PINT)&dwObscureHost);

	Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("Random_Login_Delay"), (PINT)&dwRandomLoginDelay);

	dwMaxTicks		= dwMaxTicksT;
	dwTickInterval	= dwTickIntervalT * 1000;
	dwHostNameCache	= dwHostNameCacheT * 1000;
	dwBanDuration	= dwBanDurationT * 1000;
	dwIdentCache	= dwIdentCacheT * 1000;
	dwIdentTimeOut	= dwIdentTimeOutT * 1000;
	dwRandomLoginDelay = dwRandomLoginDelay * 1000;

	if (tszTemp = Config_Get(&IniConfigFile, _TEXT("Network"), _TEXT("Dynamic_DNS_Lookup"), NULL, NULL))
	{
		if (!_tcsnicmp(tszTemp, _T("KNOCK"), 5))
		{
			dwDynamicDnsLookup = 1;
		}
		else if (!_tcsnicmp(tszTemp, _T("ALWAYS"), 6))
		{
			dwDynamicDnsLookup = 2;
		}
		else
		{
			dwDynamicDnsLookup = 0;
		}
		Free(tszTemp);
	}
	else
	{
		dwDynamicDnsLookup = 0;
	}


	UserIpHostMaskInit();

	while (InterlockedExchange(&ImmuneMasks.dwLock, TRUE)) SwitchToThread();

	UserMaskSetReset(&ImmuneMasks);

	if (tszTemp2 = tszTemp = Config_Get(&IniConfigFile, _TEXT("Network"), _TEXT("Immune_Hosts"), NULL, NULL))
	{
		while (tszTemp != NULL && *tszTemp != _T('\0'))
		{
			if (*tszTemp == _T(' ') || *tszTemp == _T('\t'))
			{
				tszTemp++;
				continue;
			}
			tszEnd = _tcspbrk(tszTemp, _T(" \t"));
			if (tszEnd != NULL)
			{
				// terminate string at separator
				*tszEnd++ = 0;
			}
			UserMaskSetAdd(&ImmuneMasks, -1, tszTemp);
			tszTemp = tszEnd;
		}
		Free(tszTemp2);
	}

	if (tszTemp = Config_Get(&IniConfigFile, _TEXT("Network"), _TEXT("Immune_Users"), NULL, NULL))
	{
		lpUserIds = GetUsers(&dwUserIds);

		if (lpUserIds)
		{
			for (n=0 ; n < dwUserIds; n++)
			{
				Uid = lpUserIds[n];
				if (UserFile_OpenPrimitive(Uid, &lpUserFile, 0))
				{
					continue;
				}
				if (!HavePermission(lpUserFile, tszTemp))
				{
					UserMaskSetAddUserFile(&ImmuneMasks, lpUserFile);
				}
				UserFile_Close(&lpUserFile, 0);
			}
			Free(lpUserIds);
		}
		Free(tszTemp);
	}

	InterlockedExchange(&ImmuneMasks.dwLock, FALSE);

	m=0;
	for (n=1 ; n <= MAX_KNOCK_PORTS ; n++)
	{
		_sntprintf_s(tszKnock, sizeof(tszKnock)/sizeof(*tszKnock), _TRUNCATE, _T("Knock_%d"), n);
		if ( Config_Get_Int(&IniConfigFile, _T("Network"), tszKnock, &dwPort) || (dwPort == 0) )
		{
			break;
		}
		if (lpdwKnockPorts[m] == dwPort)
		{
			// already initialized
			m++;
		}
		else
		{
			// new or moved ports... moving could be bad, just sorta ignore it right now
			if (lpKnockSocket[m] != 0)
			{
				closesocket(lpKnockSocket[m]);
				lpKnockSocket[m] = 0;
			}
			Socket = OpenSocket();
			if (Socket != INVALID_SOCKET)
			{
				if (! BindSocket(Socket, (ULONG) 0, (USHORT) dwPort, TRUE) &&
					! listen(Socket, SOMAXCONN))
				{
					if (WSAAsyncSelect(Socket, GetMainWindow(), WM_KNOCK, FD_ACCEPT) != SOCKET_ERROR)
					{
						lpdwKnockPorts[m] = dwPort;
						lpKnockSocket[m++] = Socket;
					}
					else
					{
						closesocket(Socket);
					}
				}
				else
				{
					closesocket(Socket);
				}
			}
		}
	}

	// close any extra's
	dwNumKnockPorts = m;
	for( ; m < MAX_KNOCK_PORTS ; m++)
	{
		if (lpKnockSocket[m])
		{
			closesocket(lpKnockSocket[m]);
		}
		lpdwKnockPorts[m] = 0;
		lpKnockSocket[m] = 0;
	}
	return TRUE;
}

VOID Identify_Shutdown(VOID)
{
	DWORD       n;

	for(n=0 ; n < MAX_KNOCK_PORTS ; n++)
	{
		if (lpKnockSocket[n])
		{
			closesocket(lpKnockSocket[n]);
		}
		lpdwKnockPorts[n] = 0;
		lpKnockSocket[n] = 0;
	}
}



VOID Identify_DeInit(VOID)
{
	LPHOSTINFO	lpHostInfo;

	while (InterlockedExchange(&UserIpHostMasks.dwLock, TRUE)) SwitchToThread();
	UserMaskSetReset(&UserIpHostMasks);
	UserIpHostMasks.dwMax = 0;
	Free(UserIpHostMasks.lppUserIpHostMasks);
	UserIpHostMasks.lppUserIpHostMasks = 0;
	InterlockedExchange(&UserIpHostMasks.dwLock, FALSE);

	while (InterlockedExchange(&ImmuneMasks.dwLock, TRUE)) SwitchToThread();
	UserMaskSetReset(&ImmuneMasks);
	ImmuneMasks.dwMax   = 0;
	Free(ImmuneMasks.lppUserIpHostMasks);
	ImmuneMasks.lppUserIpHostMasks = 0;
	InterlockedExchange(&ImmuneMasks.dwLock, FALSE);

	//	Delete critical sections
	DeleteCriticalSection(&csResolveList);
	DeleteCriticalSection(&csHostArray);
	DeleteCriticalSection(&csKnockHosts);

	//	Free memory
	while (lpHostInfo = lpFreeHostList[HEAD])
	{
		lpFreeHostList[HEAD]	= lpHostInfo->lpNext;
		FreeShared(lpHostInfo->szHostName);
		FreeShared(lpHostInfo->szIdent);
		Free(lpHostInfo);		
	}
	Free(lpHostArray);
}


LPTSTR Admin_Knock(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPBUFFER       lpBuffer;
	DWORD          n;

	lpBuffer	= &lpUser->CommandChannel.Out;

	if (!dwNumKnockPorts)
	{
		FormatString(lpBuffer, _T("%sIP port KNOCKing is disabled.\r\n"), tszMultilinePrefix);
		return NULL;
	}

	FormatString(lpBuffer, _T("%sIP port KNOCK sequence:\r\n"), tszMultilinePrefix);
	for(n=0;n<dwNumKnockPorts;n++)
	{
		FormatString(lpBuffer, _T("%s  #%d: Port %d.\r\n"), tszMultilinePrefix, n, lpdwKnockPorts[n]);
	}
	return NULL;
}



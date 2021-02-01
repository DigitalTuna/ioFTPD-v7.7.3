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

static LPTSTR  ioFTPD_shared_string;
FTP_SETTINGS  FtpSettings;


BOOL Download_Complete(LPUSERFILE *hUserFile, INT64 Credits,
					   INT64 Bytes, DWORD dwTime, INT CreditSection,
					   INT StatsSection, INT ShareSection)
{
	LPUSERFILE	lpUserFile;

	Bytes			/= 1024;
	StatsSection	*= 3;

	//	Lock userfile
	if (UserFile_Lock(hUserFile, 0)) return TRUE;
	//	Get pointer to data
	lpUserFile	= hUserFile[0];
	//	Make sure that right amount of credits was taken
	if (Credits) lpUserFile->Credits[ShareSection]	+= Credits;

	if (StatsSection >= 0)
	{
		//	Increase files downloaded
		lpUserFile->DayDn[StatsSection]++;
		lpUserFile->WkDn[StatsSection]++;
		lpUserFile->MonthDn[StatsSection]++;
		lpUserFile->AllDn[StatsSection]++;

		//	Add to bytes (kilo) downloaded
		lpUserFile->DayDn[StatsSection + 1]		+= Bytes;
		lpUserFile->WkDn[StatsSection + 1]		+= Bytes;
		lpUserFile->MonthDn[StatsSection + 1]	+= Bytes;
		lpUserFile->AllDn[StatsSection + 1]		+= Bytes;

		//	Add to time (seconds) downloaded
		lpUserFile->DayDn[StatsSection + 2]		+= dwTime;
		lpUserFile->WkDn[StatsSection + 2]		+= dwTime;
		lpUserFile->MonthDn[StatsSection + 2]	+= dwTime;
		lpUserFile->AllDn[StatsSection + 2]		+= dwTime;
	}
	//	Unlock Userfile
	UserFile_Unlock(hUserFile, 0);

	return FALSE;
}




BOOL Upload_Complete(LPUSERFILE *hUserFile, INT64 Bytes,
					 DWORD dwTime, INT CreditSection,
					 INT StatsSection, INT ShareSection)
{
	LPUSERFILE	lpUserFile;

	//	Bytes to kilobytes
	Bytes			/= 1024;
	StatsSection	*= 3;

	//	Lock userfile
	if (UserFile_Lock(hUserFile, 0)) return TRUE;
	//	Get pointer to data
	lpUserFile	= hUserFile[0];
	//	Update credits
	lpUserFile->Credits[ShareSection]	+= lpUserFile->Ratio[CreditSection] * Bytes;

	if (StatsSection >= 0)
	{
		//	Increase files uploaded
		lpUserFile->DayUp[StatsSection]++;
		lpUserFile->WkUp[StatsSection]++;
		lpUserFile->MonthUp[StatsSection]++;
		lpUserFile->AllUp[StatsSection]++;

		//	Add to bytes (kilo) uploaded
		lpUserFile->DayUp[StatsSection + 1]		+= Bytes;
		lpUserFile->WkUp[StatsSection + 1]		+= Bytes;
		lpUserFile->MonthUp[StatsSection + 1]	+= Bytes;
		lpUserFile->AllUp[StatsSection + 1]		+= Bytes;

		//	Add to time (seconds) uploaded
		lpUserFile->DayUp[StatsSection + 2]		+= dwTime;
		lpUserFile->WkUp[StatsSection + 2]		+= dwTime;
		lpUserFile->MonthUp[StatsSection + 2]	+= dwTime;
		lpUserFile->AllUp[StatsSection + 2]		+= dwTime;
	}
	//	Unlock Userfile
	UserFile_Unlock(hUserFile, 0);

	return FALSE;
}



/*

  FTP_Close_Connection() - Closes FTP Connection

  */
BOOL FTP_Close_Connection(LPFTPUSER lpUser)
{
	LPCLIENT lpClient;
	LPSTR	szHostName, szIdent;
	CHAR    szObscuredHost[MAX_HOSTNAME];
	INT     i;
	EVENT_COMMAND Event;
	DWORD   dwCid;

	dwCid = lpUser->Connection.dwUniqueId;

	if ( lpUser->Connection.dwStatus & U_IDENTIFIED )
	{
		// U_IDENTIFIED implies userfile is valid, but double checking
		if ( lpUser->UserFile )
		{
			//  Run logout event, but do it with no output socket!
			ZeroMemory(&Event, sizeof(Event));
			Event.lpDataSource	= lpUser;
			Event.dwDataSource	= DT_FTPUSER;
			for (i = 0 ; (Event.tszCommand = Config_Get(&IniConfigFile, _T("Events"), _T("OnFtpLogOut"), NULL, &i)) ; )
			{
				RunEvent(&Event);
				Free(Event.tszCommand);
			}

			if (!FtpSettings.tQuietLoginFlag || !_tcschr(lpUser->UserFile->Flags, FtpSettings.tQuietLoginFlag))
			{
				//	Get ident & hostname strings
				szIdent		= (lpUser->Connection.szIdent ? lpUser->Connection.szIdent : "*");
				szHostName  = (lpUser->Connection.szHostName ?
					Obscure_Host(szObscuredHost, lpUser->Connection.szHostName) : Obscure_IP(szObscuredHost, &lpUser->Connection.ClientAddress.sin_addr));

				// Spit logout message to log
				Putlog(LOG_GENERAL, "LOGOUT: \"%s\" \"%s\" \"%s\" \"%s\" \"%s@%s\"\r\n",
					lpUser->Connection.lpService->tszName, Uid2User(lpUser->UserFile->Uid),
					Gid2Group(lpUser->UserFile->Gid), lpUser->UserFile->Tagline,
					szIdent, szHostName);
			}
		}
		else
		{
			Putlog(LOG_ERROR, _T("Supposedly logged in user missing userfile!\r\n"));
		}
	}

	// Moved this earlier so if we can lock the client in another thread we can steal info
	// from the still valid lpFtpUser which wasn't guaranteed if we started freeing it but
	// left the client accessible.
	lpClient = UnregisterClient(dwCid);

	//	Free resources
	FreeShared(lpUser->Connection.szIdent);
	FreeShared(lpUser->Connection.szHostName);
	ioDeleteSocket(&lpUser->CommandChannel.Socket, TRUE);
	ioDeleteSocket(&lpUser->DataChannel.ioSocket, TRUE);
	MountFile_Close(lpUser->hMountFile);
	PWD_Free(&lpUser->CommandChannel.Path);
	PWD_Free(&lpUser->DataChannel.File);
	if (lpUser->CommandChannel.Out.buf)    Free(lpUser->CommandChannel.Out.buf);
	PWD_Free(&lpUser->FtpVariables.vpRenameFrom);
	PWD_Free(&lpUser->FtpVariables.vpLastUpload);
	if (lpUser->FtpVariables.tszClientType) Free(lpUser->FtpVariables.tszClientType);
	FreeShared(lpUser->FtpVariables.lpTheme);
	if (lpUser->FtpVariables.lpDenyPortAddressList)
	{
		ioAddressListFree(lpUser->FtpVariables.lpDenyPortAddressList);
	}
	// clear out old matching info
	if (lpUser->FtpVariables.lpUidList)	   Free(lpUser->FtpVariables.lpUidList);
	if (lpUser->FtpVariables.lpUidMatches) Free(lpUser->FtpVariables.lpUidMatches);

	for(i=0 ; i<MAX_MESSAGES ; i++)
	{
		FreeShared(lpUser->FtpVariables.tszMsgStringArray[i]);
	}

	if (lpClient && (lpClient->Static.Uid != -1))
	{
		if (!lpUser->UserFile)
		{
			Putlog(LOG_ERROR, _T("Client structure identifies user as logged in but no userfile: %d!\r\n"), lpClient->Static.Uid);
		}
		else
		{
			UserFile_DecrementLoginCount(lpUser->UserFile, C_FTP);
		}
	}

	UserFile_Close(&lpUser->UserFile, 0);
	Free(lpUser);
	Free(lpClient);

	return FALSE;
}




VOID FTP_Cancel(LPFTPUSER lpUser)
{
	CloseSocket(&lpUser->CommandChannel.Socket, TRUE);
}



VOID FTP_ReceiveLine(LPFTPUSER lpUser, LPSTR szCommand, DWORD dwLastError, ULONG ulSslError)
{
	if (szCommand)
	{
		//	Queue job
		lpUser->CommandChannel.Command	= szCommand;
		AddClientJob(lpUser->Connection.dwUniqueId,	5, CJOB_PRIMARY|CJOB_SECONDARY, INFINITE, FTP_Command, NULL, NULL, lpUser);
	}
	else SetJobFilter(lpUser->Connection.dwUniqueId, CJOB_PRIMARY|CJOB_SECONDARY|CJOB_TERTIARY);

	EndClientJob(lpUser->Connection.dwUniqueId, 1);
}



VOID FTP_AcceptInput(LPFTPUSER lpUser)
{
	DWORD   dwTimeOut;
	time_t  lTime;

	if (! (lpUser->Connection.dwStatus & U_LOGOFF))
	{
		if (lpUser->DataChannel.bActive)
		{
			dwTimeOut  = (DWORD)-1; //  No timer required
		}
		else if (lpUser->Connection.dwStatus & U_IDENTIFIED)
		{
			// first test to see if it's worth acquiring the lock to actually use the value...
			if (FtpSettings.tszIdleExempt)
			{
				while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();
				if (FtpSettings.tszIdleExempt && !HavePermission(lpUser->UserFile, FtpSettings.tszIdleExempt))
				{
					InterlockedExchange(&FtpSettings.lStringLock, FALSE);
					dwTimeOut = (DWORD)-1;
				}
				else
				{
					InterlockedExchange(&FtpSettings.lStringLock, FALSE);
					//  Calculate time left
					lTime=Time_DifferenceDW32(lpUser->CommandChannel.Idle,
						GetTickCount());
					if (lTime > FtpSettings.dwIdleTimeOut )
					{
						dwTimeOut = 0;
					}
					else
					{
						dwTimeOut = (DWORD) (FtpSettings.dwIdleTimeOut - lTime);
					}
				}
			}
			else
			{
				dwTimeOut = FtpSettings.dwIdleTimeOut;
			}
		}
		else
		{
			//  Calculate time left
			lTime  = (time((time_t*)NULL) - lpUser->Connection.tLogin) * 1000;
			if (lTime > FtpSettings.dwLoginTimeOut )
			{
				dwTimeOut = 0;
			}
			else
			{
				dwTimeOut = (DWORD) (FtpSettings.dwLoginTimeOut - lTime);
			}
		}

		// long line, but want to turn it up in searches and see it all...
		if(AddClientJob(lpUser->Connection.dwUniqueId, 1, CJOB_PRIMARY|CJOB_INSTANT, dwTimeOut,	NULL, FTP_Cancel, (lpUser->DataChannel.bActive ? NULL : FTP_TimerProc),	lpUser))
		{
			ReceiveLine(&lpUser->CommandChannel.Socket,	1024, FTP_ReceiveLine, lpUser);
		}
	}
}



VOID FTP_SendReply(LPFTPUSER lpUser, DWORD dwLastError, INT64 i64Total, ULONG ulSslError)
{
	lpUser->CommandChannel.Out.len	= 0;
	if (dwLastError != NO_ERROR)
	{
		SetJobFilter(lpUser->Connection.dwUniqueId, CJOB_PRIMARY|CJOB_SECONDARY|CJOB_TERTIARY);
	}
	EndClientJob(lpUser->Connection.dwUniqueId, 2);
}



VOID FTP_SSLAccept(LPFTPUSER lpUser, DWORD dwLastError, INT64 i64Total, ULONG ulSslError)
{
	lpUser->CommandChannel.Out.len	= 0;

	if (dwLastError != NO_ERROR)
	{
		SetJobFilter(lpUser->Connection.dwUniqueId, CJOB_PRIMARY|CJOB_SECONDARY|CJOB_TERTIARY);
	}
	else
	{
		FTP_AcceptInput(lpUser);
	}

	EndClientJob(lpUser->Connection.dwUniqueId, 2);
}




BOOL FTP_New_Client(PCONNECTION_INFO lpConnection)
{
	LPIOSERVICE lpService;
	IOBUFFER	Buffer[2];
	LPFTPUSER	lpUser;
	LPTSTR		tszFileName;
	BOOL		bResult;

	// NOTE: The reason errors in here are so harsh (KillUser) is because failures that don't
	// print responses or don't setup input jobs means the server will just hang this connection.
	// Only happens when out of memory so all hell breaking loose already...

	lpUser	= (LPFTPUSER )((ULONG)lpConnection - offsetof(FTP_USER, Connection));

	//	Allocate transfer buffer
	lpUser->CommandChannel.Out.size	= DEFAULT_BUF_SIZE;
	lpUser->CommandChannel.Out.buf	= (PCHAR)Allocate("FTP:Output:Buffer", DEFAULT_BUF_SIZE);

	// need to initialize data socket so the critical section is valid...
	IoSocketInit(&lpUser->DataChannel.ioSocket);
	lpUser->DataChannel.IoFile.FileHandle	= INVALID_HANDLE_VALUE;
	lpUser->DataChannel.ioSocket.Socket		= INVALID_SOCKET;

	//	Setup Timestamps
	lpUser->CommandChannel.Idle		= GetTickCount();

	// don't allow this to switch for already logged in users...
	lpUser->FtpVariables.bKeepLinksInPath = FtpSettings.bKeepLinksInPath;

	if (!lpUser->CommandChannel.Out.buf)
	{
		KillUser(lpUser->Connection.dwUniqueId);
		return TRUE;
	}

	//	Bind socket completion port
	BindCompletionPort((HANDLE)lpUser->CommandChannel.Socket.Socket);
	//	Set socket options
	SetSocketOption(&lpUser->CommandChannel.Socket, SOL_SOCKET, SO_SNDBUF, (PCHAR)&FtpSettings.dwSocketBuffer[0], sizeof(DWORD));
	SetSocketOption(&lpUser->CommandChannel.Socket, SOL_SOCKET, SO_RCVBUF, (PCHAR)&FtpSettings.dwSocketBuffer[1], sizeof(DWORD));

	bResult	    = TRUE;
	tszFileName = NULL;
	lpService   = lpUser->Connection.lpService;

	AcquireSharedLock(&lpService->loLock);
	if (lpService->lpPortDeniedAddresses)
	{
		InterlockedIncrement(&lpService->lpPortDeniedAddresses->lReferenceCount);
		lpUser->FtpVariables.lpDenyPortAddressList = lpService->lpPortDeniedAddresses;
	}
	// set to 1 minus current value so we force a refresh of settings in FTP_Command() the first time through.
	lpUser->FtpVariables.dwConfigCounter = dwConfigCounter - 1;
	if (lpService->tszMessageLocation)
	{
		//	Show server hello
		if (!aswprintf(&tszFileName, _TEXT("%s\\LogIn"), lpService->tszMessageLocation))
		{
			tszFileName = NULL;
		}
	}
	ReleaseSharedLock(&lpService->loLock);

	if (tszFileName)
	{
		bResult	= MessageFile_Show(tszFileName, 
			&lpUser->CommandChannel.Out, lpUser, DT_FTPUSER, _TEXT("220-"), _TEXT("220 "));
		Free(tszFileName);
	}

	//	Show default login message
	if (bResult) FormatString(&lpUser->CommandChannel.Out, _TEXT("220 FTP Server ready.\r\n"));

	if (lpUser->Connection.lpService->pSecureCtx &&
		! lpUser->Connection.lpService->bExplicitEncryption)
	{
		//	Init secure socket
		if (! Secure_Init_Socket(&lpUser->CommandChannel.Socket, lpUser->Connection.lpService, SSL_ACCEPT))
		{
			if (AddClientJob(lpUser->Connection.dwUniqueId, 2, CJOB_PRIMARY|CJOB_SECONDARY|CJOB_INSTANT, INFINITE, NULL, FTP_Cancel, NULL, lpUser))
			{
				//	Initialize transfer structures
				Buffer[0].dwType		= PACKAGE_SSL_ACCEPT;
				Buffer[0].dwTimeOut		= SSL_TIMEOUT;
				Buffer[0].dwTimerType	= TIMER_PER_PACKAGE;
				Buffer[1].dwType		= PACKAGE_BUFFER_SEND;
				Buffer[1].len			= lpUser->CommandChannel.Out.len;
				Buffer[1].buf			= lpUser->CommandChannel.Out.buf;
				Buffer[1].dwTimerType	= TIMER_PER_PACKAGE;
				Buffer[1].dwTimeOut		= SEND_TIMEOUT;

				TransmitPackages(&lpUser->CommandChannel.Socket, Buffer, 2, NULL, FTP_SSLAccept, lpUser);

				EndClientJob(lpUser->Connection.dwUniqueId, 10005);

				return FALSE;
			}
		}

		// This is bad... just bail
		KillUser(lpUser->Connection.dwUniqueId);
		return TRUE;
	}

	if (AddClientJob(lpUser->Connection.dwUniqueId, 2, CJOB_SECONDARY|CJOB_INSTANT, INFINITE, NULL, FTP_Cancel, NULL, lpUser))
	{
		//	Initialize transfer structures
		Buffer[0].dwType		= PACKAGE_BUFFER_SEND;
		Buffer[0].len			= lpUser->CommandChannel.Out.len;
		Buffer[0].buf			= lpUser->CommandChannel.Out.buf;
		Buffer[0].dwTimerType	= TIMER_PER_PACKAGE;
		Buffer[0].dwTimeOut		= SEND_TIMEOUT;

		TransmitPackages(&lpUser->CommandChannel.Socket, Buffer, 1, NULL, FTP_SendReply, lpUser);

		if (AddClientJob(lpUser->Connection.dwUniqueId, 1, CJOB_PRIMARY|CJOB_INSTANT, FtpSettings.dwLoginTimeOut, NULL, FTP_Cancel, FTP_TimerProc, lpUser))
		{
			ReceiveLine(&lpUser->CommandChannel.Socket, 1024, FTP_ReceiveLine, lpUser);

			EndClientJob(lpUser->Connection.dwUniqueId, 10005);
			return FALSE;
		}
	}

	KillUser(lpUser->Connection.dwUniqueId);
	return TRUE;
}


BOOL LoadLockedString(LPTSTR tszSection, LPTSTR tszName, LPTSTR volatile *ptszString, DWORD volatile *pdwLen)
{
	DWORD  dwLen;
	LPTSTR tszTemp, tszShared;

	if (tszTemp = Config_Get(&IniConfigFile, tszSection, tszName, NULL, NULL))
	{
		dwLen = _tcslen(tszTemp);
		if (!(tszShared = (LPTSTR) AllocateShared(NULL, tszName, (dwLen+1)*sizeof(TCHAR))))
		{
			Free(tszTemp);
			if (pdwLen) *pdwLen = 0;
			return FALSE;
		}
		CopyMemory(tszShared, tszTemp, (dwLen+1)*sizeof(TCHAR));
		Free(tszTemp);
		tszTemp = *ptszString;
		*ptszString = tszShared;
		FreeShared(tszTemp);
		if (pdwLen) *pdwLen = dwLen;
		return TRUE;
	}

	tszTemp = *ptszString;
	*ptszString = NULL;
	FreeShared(tszTemp);
	if (pdwLen) *pdwLen = 0;
	return TRUE;
}


BOOL FTP_Init(BOOL bFirstInitialization)
{
	LPTSTR  tszTemp;
	BOOL	bNagle;
	INT     iTemp;
	TCHAR   pBuffer[_INI_LINE_LENGTH+1];

	if (bFirstInitialization)
	{
		ZeroMemory(&FtpSettings, sizeof(FtpSettings));

		FtpSettings.dwSocketBuffer[0]	= 4096;
		FtpSettings.dwSocketBuffer[1]	= 8192;
		FtpSettings.dwDataSocketBuffer[0]	= 16384;
		FtpSettings.dwDataSocketBuffer[1]	= 16384;
		FtpSettings.dwTransferBuffer	= 65536;
		FtpSettings.dwIdleTimeOut	= 180 * 1000;
		FtpSettings.dwLoginTimeOut	= 30 * 1000;
		FtpSettings.dwLoginAttempts	= 3;
		FtpSettings.bComputeCRCs    = TRUE;

		FtpSettings.dwShutdownCID = -1;
		FtpSettings.ShutdownUID = -1;

		FtpSettings.iSingleCloseUID = -1;

		FtpSettings.bKeepLinksInPath = TRUE;
		FtpSettings.bOnlineDataExtraFields = TRUE;

		FtpSettings.dwFtpDataTimeout = FTP_DATA_TIMEOUT;

		// this prevents users from logging in until server start scripts finish
		FtpSettings.bServerStartingUp = TRUE;
		FtpSettings.iServerSingleExemptUID = -1;

		ioFTPD_shared_string = AllocateShared(NULL, _T("ioFTPD_shared_string"), 7*sizeof(TCHAR));
		if (!ioFTPD_shared_string)
		{
			return FALSE;
		}
		_tcscpy(ioFTPD_shared_string, _T("ioFTPD"));
	}

	Config_Get_Int(&IniConfigFile, _TEXT("FTP"), _TEXT("Socket_Send_Buffer"), (PINT)&FtpSettings.dwSocketBuffer[0]);
	Config_Get_Int(&IniConfigFile, _TEXT("FTP"), _TEXT("Socket_Recv_Buffer"), (PINT)&FtpSettings.dwSocketBuffer[1]);
	Config_Get_Int(&IniConfigFile, _TEXT("FTP"), _TEXT("DataSocket_Send_Buffer"), (PINT)&FtpSettings.dwDataSocketBuffer[0]);
	Config_Get_Int(&IniConfigFile, _TEXT("FTP"), _TEXT("DataSocket_Recv_Buffer"), (PINT)&FtpSettings.dwDataSocketBuffer[1]);
	Config_Get_Int(&IniConfigFile, _TEXT("FTP"), _TEXT("Transfer_Buffer"), (PINT)&FtpSettings.dwTransferBuffer);
	if (FtpSettings.dwTransferBuffer < 32768) FtpSettings.dwTransferBuffer = 32768;
	if (! Config_Get_Bool(&IniConfigFile, _TEXT("FTP"), _TEXT("DataSocket_Nagle"), &bNagle)) FtpSettings.bNagle = (bNagle ? FALSE : TRUE);

	if (! Config_Get_Int(&IniConfigFile, _TEXT("FTP"), _TEXT("Idle_TimeOut"), &iTemp)) FtpSettings.dwIdleTimeOut	= iTemp * 1000;
	if (! Config_Get_Int(&IniConfigFile, _TEXT("FTP"), _TEXT("Login_TimeOut"), &iTemp)) FtpSettings.dwLoginTimeOut	= iTemp * 1000;

	Config_Get_Int(&IniConfigFile, _TEXT("FTP"), _TEXT("Login_Attempts"), (PINT)&FtpSettings.dwLoginAttempts);
	Config_Get_Bool(&IniConfigFile, _TEXT("FTP"), _TEXT("Hide_Xfer_Host"), (PINT)&FtpSettings.bHideXferHost);
	Config_Get_Bool(&IniConfigFile, _TEXT("FTP"), _TEXT("No_SubDir_Sizing"), (PINT)&FtpSettings.bNoSubDirSizing);
	Config_Get_Bool(&IniConfigFile, _TEXT("FTP"), _TEXT("Compute_CRC"), (PINT)&FtpSettings.bComputeCRCs);
	Config_Get_Bool(&IniConfigFile, _TEXT("FTP"), _TEXT("Show_HostMask_Error"), (PINT)&FtpSettings.bShowHostMaskError);
	Config_Get_Bool(&IniConfigFile, _TEXT("FTP"), _TEXT("Keep_Links_In_Paths"), (PINT) &FtpSettings.bKeepLinksInPath);
	Config_Get_Bool(&IniConfigFile, _TEXT("FTP"), _TEXT("OnlineData_Extra_Fields"), (PINT) &FtpSettings.bOnlineDataExtraFields);
	Config_Get_Bool(&IniConfigFile, _TEXT("FTP"), _TEXT("Enable_TimeStamp_On_Last_Upload"), (PINT) &FtpSettings.bEnableTimeStampOnLastUpload);
	Config_Get_Bool(&IniConfigFile, _TEXT("FTP"), _TEXT("Who_Sort_Output"), (PINT) &FtpSettings.bWhoSortOutput);


	if (tszTemp = Config_Get(&IniConfigFile, _TEXT("FTP"), _TEXT("Banned_User_Flag"), pBuffer, NULL))
	{
		FtpSettings.tBannedFlag = ((!tszTemp[1] || _istspace(tszTemp[1])) ? *tszTemp : 0);
	}
	else
	{
		FtpSettings.tBannedFlag = 0;
	}

	if (tszTemp = Config_Get(&IniConfigFile, _TEXT("FTP"), _TEXT("Quiet_Login_Flag"), pBuffer, NULL))
	{
		FtpSettings.tQuietLoginFlag = ((!tszTemp[1] || _istspace(tszTemp[1])) ? *tszTemp : 0);
	}
	else
	{
		FtpSettings.tQuietLoginFlag = 0;
	}

	if (tszTemp = Config_Get(&IniConfigFile, _TEXT("FTP"), _TEXT("Chmod_Check"), pBuffer, NULL))
	{
		if (!_tcsicmp(tszTemp, _T("WriteOnly")))
		{
			FtpSettings.dwChmodCheck = 1;
		}
		else if (!_tcsicmp(tszTemp, _T("NoChecks")))
		{
			FtpSettings.dwChmodCheck = 2;
		}
		else
		{
			FtpSettings.dwChmodCheck = 0;
		}
	}

	if (tszTemp = Config_Get(&IniConfigFile, _TEXT("FTP"), _TEXT("Single_Closed_Exempt_Name"), pBuffer, NULL))
	{
		FtpSettings.iServerSingleExemptUID = User2Uid(pBuffer);
	}

	if (! Config_Get_Int(&IniConfigFile, _TEXT("FTP"), _TEXT("Data_Timeout"), &iTemp)) FtpSettings.dwFtpDataTimeout = iTemp * 1000;

	// must hold this lock to examine strings, but can use allocateshared to up
	// ref count and then release lock
	while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();

	LoadLockedString(_T("FTP"), _T("Allowed_Recursive"), &FtpSettings.tszAllowedRecursive, NULL);

	LoadLockedString(_T("FTP"), _T("Idle_Ignore"), &FtpSettings.tszIdleIgnore, NULL);

	LoadLockedString(_T("FTP"), _T("Idle_Exempt"), &FtpSettings.tszIdleExempt, NULL);

	LoadLockedString(_T("FTP"), _T("Close_Exempt"), &FtpSettings.tszCloseExempt, NULL);

	LoadLockedString(_T("FTP"), _T("Site_Name"), &FtpSettings.tszSiteName, NULL);

	if (!FtpSettings.tszSiteName)
	{
		AllocateShared(ioFTPD_shared_string, NULL, 0);
		FtpSettings.tszSiteName = ioFTPD_shared_string;
	}

	LoadLockedString(_T("FTP"), _T("Site_Box_Theme"), &FtpSettings.tszSiteBoxTheme, NULL);

	LoadLockedString(_T("FTP"), _T("Site_Box_Header"), &FtpSettings.tszSiteBoxHeader, &FtpSettings.dwSiteBoxHeader);

	LoadLockedString(_T("FTP"), _T("Site_Box_Footer"), &FtpSettings.tszSiteBoxFooter, &FtpSettings.dwSiteBoxFooter);

	LoadLockedString(_T("FTP"), _T("Help_Box_Theme"), &FtpSettings.tszHelpBoxTheme, NULL);

	LoadLockedString(_T("FTP"), _T("Help_Box_Header"), &FtpSettings.tszHelpBoxHeader, &FtpSettings.dwHelpBoxHeader);

	LoadLockedString(_T("FTP"), _T("Help_Box_Footer"), &FtpSettings.tszHelpBoxFooter, &FtpSettings.dwHelpBoxFooter);

	// release string lock
	InterlockedExchange(&FtpSettings.lStringLock, FALSE);

	return TRUE;
}


VOID FTP_DeInit(VOID)
{
	LPTSTR tszTemp;

	while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();

	tszTemp = FtpSettings.tszAllowedRecursive;
	FtpSettings.tszAllowedRecursive = NULL;
	FreeShared(tszTemp);

	tszTemp = FtpSettings.tszIdleIgnore;
	FtpSettings.tszIdleIgnore = NULL;
	FreeShared(tszTemp);

	tszTemp = FtpSettings.tszIdleExempt;
	FtpSettings.tszIdleExempt = NULL;
	FreeShared(tszTemp);

	tszTemp = FtpSettings.tszCloseExempt;
	FtpSettings.tszCloseExempt = NULL;
	FreeShared(tszTemp);

	tszTemp = FtpSettings.tszSiteName;
	FtpSettings.tszSiteName = NULL;
	FreeShared(tszTemp);

	FreeShared(ioFTPD_shared_string);

	tszTemp = FtpSettings.tszSiteBoxTheme;
	FtpSettings.tszSiteBoxTheme = NULL;
	FreeShared(tszTemp);

	tszTemp = FtpSettings.tszSiteBoxHeader;
	FtpSettings.tszSiteBoxHeader = NULL;
	FreeShared(tszTemp);

	tszTemp = FtpSettings.tszSiteBoxFooter;
	FtpSettings.tszSiteBoxFooter = NULL;
	FreeShared(tszTemp);

	tszTemp = FtpSettings.tszHelpBoxTheme;
	FtpSettings.tszHelpBoxTheme = NULL;
	FreeShared(tszTemp);

	tszTemp = FtpSettings.tszHelpBoxHeader;
	FtpSettings.tszHelpBoxHeader = NULL;
	FreeShared(tszTemp);

	tszTemp = FtpSettings.tszHelpBoxFooter;
	FtpSettings.tszHelpBoxFooter = NULL;
	FreeShared(tszTemp);

	InterlockedExchange(&FtpSettings.lStringLock, FALSE);
}

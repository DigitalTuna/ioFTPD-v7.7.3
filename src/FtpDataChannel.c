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


//	Local Declatarions
static BOOL	FTPData_TransferComplete(LPFTPUSER lpUser);




static BOOL FTP_CancelSocketJob(LPFTPUSER lpUser)
{
	EndClientJob(lpUser->Connection.dwUniqueId, 201);
	return FALSE;
}


VOID FTP_CancelData(LPFTPUSER lpUser)
{
	PDATACHANNEL lpData;
	BOOL         bAborted;

	lpData = &lpUser->DataChannel;

	bAborted = FALSE;
	EnterCriticalSection(&lpData->ioSocket.csLock);
	// force the socket closed...
	if (lpData->ioSocket.lpSelectEvent)
	{
		// we are still trying to connect or listen for a connection...
		if (WSAAsyncSelectCancel(&lpData->ioSocket))
		{
			// we canceled the event/timer which means no failure indication will happen...
			bAborted = TRUE;
		}
	}
	CloseSocket(&lpData->ioSocket, TRUE);
	LeaveCriticalSection(&lpData->ioSocket.csLock);
	if (bAborted)
	{
		lpData->dwLastError	= WSAECONNABORTED;
		// we can't call EndClientJob here because we can be called from SetJobFilter or EndClientJob
		// and already hold the job lock, so setup another thread to end the job...
		QueueJob(FTP_CancelSocketJob, lpUser, JOB_PRIORITY_NORMAL);
	}
}



VOID FTP_TransferComplete(LPFTPUSER lpUser, DWORD dwLastError, INT64 i64Total, ULONG ulSslError)
{
	lpUser->DataChannel.dwLastError	= dwLastError;
	lpUser->DataChannel.ulSslError  = ulSslError;
	lpUser->DataChannel.Size = i64Total;
	EndClientJob(lpUser->Connection.dwUniqueId, 201);
}



VOID FTP_TransferRate(LPFTPUSER lpUser, DWORD dwBytesTransferred, DWORD dwTransferInterval, DWORD dwTickCount)
{
	UpdateClientData(DATA_TRANSFER_UPDATE,
		lpUser->Connection.dwUniqueId, dwBytesTransferred, dwTransferInterval, dwTickCount);
}





BOOL FTP_Data_Start(LPFTPUSER lpUser)
{
	IOBUFFER		Buffer[4];
	DWORD			dwBuffers, dwBufferSize;
	PDATACHANNEL	lpData;
	SOCKET			Socket;
	INT				iSize;
	struct linger   Linger;

	dwBuffers	= 0;
	ZeroMemory(&Buffer, sizeof(Buffer));

	lpData	= &lpUser->DataChannel;

	// we'll use the socket lock as the data lock...
	EnterCriticalSection(&lpData->ioSocket.csLock);

	if (lpData->dwLastError == NO_ERROR &&
		lpData->bTransferMode == PASSIVE)
	{

		//	Passive transfer mode, must accept new connection
		iSize	= sizeof(struct sockaddr_in);
		AcquireHandleLock();
		if (lpData->ioSocket.Socket != INVALID_SOCKET)
		{
			Socket	= WSAAccept(lpData->ioSocket.Socket, (struct sockaddr *)&lpData->Address, &iSize, 0, 0);
			if (Socket != INVALID_SOCKET)
			{
				SetHandleInformation((HANDLE)Socket, HANDLE_FLAG_INHERIT, 0);
			}
		}
		ReleaseHandleLock();
		if (lpData->ioSocket.Socket == INVALID_SOCKET)
		{
			// this shouldn't happen
			lpData->dwLastError	= WSAEBADF;
		}
		else if (Socket != INVALID_SOCKET) 		//	Update socket structure
		{
			// close the listening socket, and use the new accepted socket...
			CloseSocket(&lpData->ioSocket, TRUE);

			// initialize the new data socket
			lpData->ioSocket.Socket  = Socket;

			Linger.l_onoff  = 1;
			Linger.l_linger  = 0;
			//  Set for hard close
			setsockopt(Socket, SOL_SOCKET, SO_LINGER, (PCHAR)&Linger, sizeof(struct linger));
		}
		else
		{
			// close the listening socket
			lpData->dwLastError	= WSAGetLastError();
			CloseSocket(&lpData->ioSocket, TRUE);
		}
	}

	if (lpData->dwLastError != NO_ERROR)
	{
		goto FAIL;
	}

	//	Check for FXP permission
	if (memcmp(&lpData->Address.sin_addr, &lpUser->Connection.ClientAddress.sin_addr, sizeof(lpData->Address.sin_addr)))
	{
		if (lpData->bDirection == RECEIVE &&
			(! HasFlag(lpUser->UserFile, "F") ||
			! PathCheck(lpUser->UserFile, lpData->File.pwd, "NoFxpIn")))
		{
			//	FXP is not allowed for user
			lpData->dwLastError	= IO_NO_ACCESS_FXP_IN;

			goto FAIL;
		}

		if (lpData->bDirection == SEND &&
			(! HasFlag(lpUser->UserFile, "f") ||
			! PathCheck(lpUser->UserFile, lpData->File.pwd, "NoFxpOut")))
		{
			//	FXP is not allowed for user
			lpData->dwLastError	= IO_NO_ACCESS_FXP_OUT;

			goto FAIL;
		}
	}

	// we'll use this for cert caching
	sprintf(lpData->ioSocket.szPrintedIP, "%d.%d.%d.%d",
		lpData->Address.sin_addr.S_un.S_un_b.s_b1,
		lpData->Address.sin_addr.S_un.S_un_b.s_b2,
		lpData->Address.sin_addr.S_un.S_un_b.s_b3,
		lpData->Address.sin_addr.S_un.S_un_b.s_b4);

	//	SSL Handshake
	if (lpData->bProtected)
	{
	  // determine role as client or server for SSL
	  if ( ((lpUser->Connection.dwStatus & U_FXPSSLCLIENT) ||
		    (lpUser->DataChannel.bProtectedConnect)) &&
	       (lpData->bSpecial != LIST) )
	  {
		  lpUser->DataChannel.bProtectedConnect = FALSE;
		  // we're supposed to be acting as a client
		  Secure_Init_Socket(&lpData->ioSocket,
			  lpUser->Connection.lpService,
			  SSL_CONNECT|SSL_LARGE_BUFFER);
		  Buffer[0].dwType = PACKAGE_SSL_CONNECT;
	  }
	  else
	  {
		  Secure_Init_Socket(&lpData->ioSocket,
			  lpUser->Connection.lpService,
			  SSL_ACCEPT|SSL_LARGE_BUFFER);
		  Buffer[0].dwType = PACKAGE_SSL_ACCEPT;
	  }
	  Buffer[0].dwTimerType	= TIMER_PER_PACKAGE;
	  Buffer[0].dwTimeOut		= SSL_TIMEOUT;
	  dwBuffers++;
	}
	else if (! Service_RequireSecureData(lpUser->Connection.lpService, lpUser->UserFile))
	{
		lpData->dwLastError	= IO_ENCRYPTION_REQUIRED;

		goto FAIL;
	}

	//	Data transfer
	if (lpData->bSpecial == LIST)
	{
		if (lpUser->Listing)
		{
			// we have more work to do after buffer sent
			Buffer[dwBuffers].dwType		= PACKAGE_LIST_BUFFER_SEND;
		}
		else
		{
			Buffer[dwBuffers].dwType		= PACKAGE_BUFFER_SEND;
		}
	}
	else if (lpData->bDirection == SEND)
	{
		Buffer[dwBuffers].dwType		= PACKAGE_FILE_SEND;
		Buffer[dwBuffers].ioFile	    = &lpData->IoFile;
	}
	else
	{
		Buffer[dwBuffers].dwType		= PACKAGE_FILE_RECEIVE;
		Buffer[dwBuffers].ioFile	    = &lpData->IoFile;
	}
	Buffer[dwBuffers].buf			= lpData->Buffer.buf;
	Buffer[dwBuffers].size			= lpData->Buffer.size;
	Buffer[dwBuffers].len			= lpData->Buffer.len;
	Buffer[dwBuffers].dwTimerType	= TIMER_PER_TRANSMIT;
	Buffer[dwBuffers++].dwTimeOut	= FtpSettings.dwFtpDataTimeout;

	dwBufferSize	= 1024;
	//	TCP Shutdown
	if (lpData->bDirection == SEND)
	{
		Buffer[dwBuffers].dwType		= PACKAGE_SHUTDOWN;
		Buffer[dwBuffers].dwTimerType	= TIMER_PER_PACKAGE;
		Buffer[dwBuffers++].dwTimeOut	= 5000;

		//	Set socket options
		SetSocketOption(&lpData->ioSocket, SOL_SOCKET, SO_SNDBUF, (PCHAR)&FtpSettings.dwDataSocketBuffer[0], sizeof(DWORD));
		SetSocketOption(&lpData->ioSocket, SOL_SOCKET, SO_RCVBUF, (PCHAR)&dwBufferSize, sizeof(DWORD));
	}
	else
	{
		SetSocketOption(&lpData->ioSocket, SOL_SOCKET, SO_SNDBUF, (PCHAR)&dwBufferSize, sizeof(DWORD));
		SetSocketOption(&lpData->ioSocket, SOL_SOCKET, SO_RCVBUF, (PCHAR)&FtpSettings.dwDataSocketBuffer[1], sizeof(DWORD));
	}

	SetSocketOption(&lpData->ioSocket, IO_SOCKET, SEND_LIMIT, (PCHAR)&lpUser->UserFile->Limits[0], sizeof(DWORD));
	SetSocketOption(&lpData->ioSocket, IO_SOCKET, RECEIVE_LIMIT, (PCHAR)&lpUser->UserFile->Limits[1], sizeof(DWORD));
	SetSocketOption(&lpData->ioSocket, IPPROTO_TCP, TCP_NODELAY, (PCHAR)&FtpSettings.bNagle, sizeof(BOOL));

	//	Bind socket to completion port
	BindCompletionPort((HANDLE)lpData->ioSocket.Socket);

	//	Begin transfer
	UpdateClientData(DATA_TRANSFER, lpUser->Connection.dwUniqueId,
		(lpData->bDirection != SEND ? 2 : (lpData->bSpecial == LIST ? 3 : 1)), lpData);

	LeaveCriticalSection(&lpData->ioSocket.csLock);
	TransmitPackages(&lpData->ioSocket, Buffer, dwBuffers, FTP_TransferRate, FTP_TransferComplete, lpUser);
	return FALSE;

FAIL:
	LeaveCriticalSection(&lpData->ioSocket.csLock);
	EndClientJob(lpUser->Connection.dwUniqueId, 201);
	return FALSE;
}









/*

  FTP_Data_InitializeTransfer() - Initializes FTP data transfer

  */
BOOL FTPData_InitializeTransfer(LPFTPUSER lpUser, PDATACHANNEL lpData)
{
	DWORD	dwSizeHigh, dwSizeLow;
	INT64	Charge;
	BOOL	bError;

	if (! lpData->bInitialized)
	{
		//	Socket not initialized
		lpData->dwLastError	= WSAENOTSOCK;
		return TRUE;
	}

	bError	= TRUE;

	//	Initialize transfer information
	lpData->bAbort		 = FALSE;
	lpData->dwLastError	 = NO_ERROR;
	lpData->ulSslError   = 0;
	lpData->Charged		 = 0;
	lpData->Size		 = 0;
	lpData->dwDuration   = 0;
	lpData->bActive		 = TRUE;
	lpData->bFree		 = FALSE;
	lpData->Buffer.len	    = 0;
	lpData->Buffer.size	    = FtpSettings.dwTransferBuffer;
	// || ((lpData->bDirection == SEND) && (lpData->bSpecial != LIST) && lpUser->FtpVariables.bShowDownloadCrcs) )
	if ( ((lpData->bDirection == RECEIVE) && FtpSettings.bComputeCRCs || lpUser->FtpVariables.bComputeCrc))
	{
		lpData->IoFile.Overlapped.bDoCrc = TRUE;
		lpData->IoFile.Overlapped.Crc32	 = 0xFFFFFFFF;
	}
	else
	{
		lpData->IoFile.Overlapped.bDoCrc = FALSE;
	}

	//	Allocate transfer buffer
	if (! (lpData->Buffer.buf = (PCHAR)Allocate("FTPData:Init:Buffer", lpData->Buffer.size)))
	{
		lpData->dwLastError	= ERROR_NOT_ENOUGH_MEMORY;
		return TRUE;
	}

	//	Set Timestamps
	Time_Read(&lpData->Start);
	ZeroMemory(&lpData->Stop, sizeof(TIME_STRUCT));

	if (lpData->bSpecial != LIST)
	{
		//	Set resume offset
		if (ioSeekFile(&lpData->IoFile, lpData->dwResumeOffset, (lpData->bDirection == RECEIVE ? TRUE : FALSE)))
		{
			lpData->dwLastError	= GetLastError();
			return TRUE;
		}

		//	Credit check, if user is downloading
		if (lpData->bDirection == SEND)
		{
			if (UserFile_Lock(&lpUser->UserFile, 0))
			{
				lpData->dwLastError	= GetLastError();
				return TRUE;
			}

			if ((dwSizeLow = ioGetFileSize(&lpData->IoFile, &dwSizeHigh)) != INVALID_FILE_SIZE ||
				(lpData->dwLastError = GetLastError()) == NO_ERROR)
			{
				lpData->FileSize = dwSizeHigh                * 0x100000000 + dwSizeLow;
				Charge           = lpData->dwResumeOffset[0] * 0x100000000 + lpData->dwResumeOffset[1];

				if (Charge > lpData->FileSize)
				{
					lpData->dwLastError = ERROR_FILESEEK;
				}
				else if (! lpUser->UserFile->Ratio[lpData->File.CreditSection])
				{
					bError	        = FALSE;
					lpData->bFree   = TRUE;
					lpData->Charged	= Charge;
				}
				else if (lpUser->UserFile->Credits[lpData->File.ShareSection] > Charge)
				{
					//	Sufficient amount of credits
					lpUser->UserFile->Credits[lpData->File.ShareSection]	-= Charge;
					lpData->Charged	= Charge;
					bError	= FALSE;
				}
				else lpData->dwLastError	= IO_NO_CREDITS;
			}
			UserFile_Unlock(&lpUser->UserFile, 0);

			if (bError) return TRUE;
		}
	}
	return FALSE;
}










/*

  FTP_Data_Close() - Closes ftp transfer

  */
BOOL FTP_Data_Close(LPFTPUSER lpUser)
{
	PDATACHANNEL    lpData;
	EVENT_COMMAND	Event;
	TCHAR			pBuffer[_INI_LINE_LENGTH + 1];
	LPSTR			szFileName, szHostName, szUserName, szIdent, szEvent;
	CHAR			cEncoding, cDirection;
	BOOL			bUpdateDirectory, bError, bRealCrc;
	DWORD			dwFileName, dwDuration, dwCrc;
	INT				n;
	UINT64          u64Len;
	HANDLE          hFile;
	CHAR            szObscuredHost[MAX_HOSTNAME];
	LPFILEINFO      lpFileInfo;
	VFSUPDATE       UpdateData;

	lpData = &lpUser->DataChannel;

	// close socket here in case we resumed a large file and it takes a while to CRC it
	ioCloseSocket(&lpData->ioSocket, TRUE);

	bRealCrc = FALSE;
	if (lpData->bActive && lpData->IoFile.Overlapped.bDoCrc && (lpData->dwLastError == NO_ERROR))
	{
		// we're computing the CRC for an incoming complete file
		if (lpData->dwResumeOffset[0] || lpData->dwResumeOffset[1])
		{
			// but we didn't start at byte 0, so get the complete checksum
			u64Len = lpData->dwResumeOffset[0] * 0x100000000 + lpData->dwResumeOffset[1];
			dwCrc = 0xFFFFFFFF;

			hFile = CreateFile(lpData->File.RealPath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				if (FileCrc32(hFile, 0L, u64Len, &dwCrc))
				{
					lpData->IoFile.Overlapped.Crc32 = crc32_combine(~dwCrc, lpData->IoFile.Overlapped.Crc32, lpData->Size);
					bRealCrc = TRUE;
				}
				CloseHandle(hFile);
			}
			else
			{
				lpData->IoFile.Overlapped.Crc32 = ~0;
			}
		}
	}
	else
	{
		// we had an error or aren't computing so set it to 0
		lpData->IoFile.Overlapped.Crc32 = 0;
	}

	//	Close file
	ioCloseFile(&lpData->IoFile, TRUE);

	if (lpData->bActive)
	{
		//	Mark transfer as completed
		UpdateClientData(DATA_TRANSFER, lpUser->Connection.dwUniqueId, (UINT32)-1);
		//	Get timestamp
		Time_Read(&lpData->Stop);

		if (lpData->bSpecial != LIST)
		{
			if (lpData->bDirection == RECEIVE)
			{
				UserFile_UploadEnd(lpUser->UserFile);
			}
			else
			{
				UserFile_DownloadEnd(lpUser->UserFile);
			}
		}

		if ((lpData->bSpecial != LIST) && (lpData->bDirection == RECEIVE) && !(lpData->IoFile.dwFlags & IOFILE_VALID))
		{
			// the upload failed to connect at all
			if (lpData->File.RealPath && (lpData->IoFile.dwFlags & IOFILE_CREATED))
			{
				// delete newly created file
				IoDeleteFile(lpData->File.RealPath, lpData->File.l_RealPath);
			}
		}
		else if (lpData->bSpecial != LIST)  //	Handle non-list transfers
		{
			//	Get hostname
			if (lpUser->Connection.szHostName &&
				! memcmp(&lpData->Address.sin_addr,
				&lpUser->Connection.ClientAddress.sin_addr, sizeof(lpData->Address.sin_addr)))
			{
				szHostName = Obscure_Host(szObscuredHost, lpUser->Connection.szHostName);
			}
			else
			{
				szHostName = Obscure_IP(szObscuredHost, &lpData->Address.sin_addr);
			}
			//	Get identity
			szUserName	= Uid2User(lpUser->UserFile->Uid);
			szIdent		= (lpUser->Connection.szIdent ? lpUser->Connection.szIdent : "*");
			//	Get filename
			szFileName	= lpData->File.RealPath;
			dwFileName	= lpData->File.l_RealPath;
			//	Get encoding
			cEncoding	= (lpData->bEncoding == BINARY ? 'b' : 'a');
			//	Get direction
			cDirection	= (lpData->bDirection == SEND ? 'o' : 'i');
			//	Get duration
			lpData->dwDuration = (DWORD) (Time_Difference(&lpData->Start, &lpData->Stop) * 1000.0);
			dwDuration = (lpData->dwDuration + 500) / 1000;

			//	Transfer log entry
			Putlog(LOG_TRANSFER,
				"%u %s %I64i %s %c _ %c r %s ftp 1 %s l\r\n",
				dwDuration, (FtpSettings.bHideXferHost ? _T("[hidden]") : szHostName), lpData->Size,
				lpData->File.pwd, cEncoding, cDirection, szUserName, szIdent);

			//	Get prefix
			Event.tszOutputPrefix	= (lpData->dwLastError == NO_ERROR ? _TEXT("226-") : _TEXT("426-"));

			if (cDirection == 'i')
			{
				//	Get event
				szEvent	= (lpData->dwLastError == NO_ERROR ? "OnUploadComplete" : "OnUploadError");
				bUpdateDirectory	= TRUE;

				//	Format arguments
				Event.tszParameters	= (LPSTR)Allocate("FTPData:Close:Args", dwFileName + 15 + lpData->File.len);
				if (Event.tszParameters)
				{
					wsprintf(Event.tszParameters, _TEXT("\"%s\" %08X \"%s\""), szFileName, ~lpData->IoFile.Overlapped.Crc32, lpData->File.pwd);
				}
				else szEvent	= NULL;
			}
			else
			{
				//	Get event
				szEvent	= (lpData->dwLastError == NO_ERROR ? "OnDownloadComplete" : "OnDownloadError");
				bUpdateDirectory	= FALSE;

				//	Allocate arguments
				Event.tszParameters	= (LPSTR)Allocate("FTPData:Close:Args", dwFileName + 6 + lpData->File.len);
				//	Format arguments
				if (Event.tszParameters)
				{
					wsprintf(Event.tszParameters, _TEXT("\"%s\" \"%s\""), szFileName, lpData->File.pwd);
				}
				else szEvent	= NULL;
			}

			bError	= FALSE;
			if (szEvent)
			{
				//	Execute events
				Event.lpDataSource	= lpUser;
				Event.dwDataSource	= DT_FTPUSER;
				Event.tszCommand	= pBuffer;
				Event.lpOutputBuffer	= &lpUser->CommandChannel.Out;
				Event.lpOutputSocket	= &lpUser->CommandChannel.Socket;

				for (n = 0;Config_Get(&IniConfigFile, _TEXT("Events"), szEvent, pBuffer, &n) && ! bError;)
				{
					bError	= RunEvent(&Event);
				}
			}

			//	Update directory
			if (bUpdateDirectory)
			{
				if (! bError)
				{
					if (GetFileInfoNoCheck(szFileName, &lpFileInfo))
					{
						// need to load the fileinfo here in case the script changed something above...
						UpdateData.Uid  = lpFileInfo->Uid;
						UpdateData.Gid  = lpFileInfo->Gid;
						UpdateData.dwFileMode  = lpFileInfo->dwFileMode;
						UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
						UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs + lpData->dwDuration;
						UpdateData.Context.lpData   = lpFileInfo->Context.lpData;
						UpdateData.Context.dwData   = lpFileInfo->Context.dwData;
						// don't really care if this works or not...
						UpdateFileInfo(szFileName, &UpdateData);

						// since we updated the fileinfo we need to get the new comparison value
						lpUser->FtpVariables.dwLastUploadUpTime = UpdateData.dwUploadTimeInMs;
						CloseFileInfo(lpFileInfo);
					}
					else
					{
						lpUser->FtpVariables.dwLastUploadUpTime = lpData->dwDuration;
					}

					Upload_Complete(&lpUser->UserFile, lpData->Size,
						dwDuration, lpData->File.CreditSection,
						(! PathCheck(lpUser->UserFile, lpData->File.pwd, "NoStats") ? -1 : lpData->File.StatsSection),
						lpData->File.ShareSection);

					PWD_Copy(&lpData->File, &lpUser->FtpVariables.vpLastUpload, TRUE);

					if ((lpData->dwLastError == NO_ERROR) && (lpData->IoFile.Overlapped.bDoCrc) && (lpData->bDirection == RECEIVE) &&
						(bRealCrc || (!lpData->dwResumeOffset[0] && !lpData->dwResumeOffset[1])))
					{
						// we computed the crc over the whole file, so stash the answer away in case user
						// issues a XCRC on this just transferred file we don't have to compute it again...
						// Also enables the user to verify the upload was correct even though a zipscript
						// may have just modified the file which means it would fail the check!
						lpUser->FtpVariables.dwLastCrc32 = ~lpData->IoFile.Overlapped.Crc32;
						lpUser->FtpVariables.bValidCRC = TRUE;
					}
					else
					{
						lpUser->FtpVariables.bValidCRC = FALSE;
					}
				}
				else
				{
					IoDeleteFile(szFileName, dwFileName);
					PWD_Reset(&lpUser->FtpVariables.vpLastUpload);
					lpUser->FtpVariables.bValidCRC = FALSE;
					lpUser->FtpVariables.dwLastUploadUpTime = 0;
				}
				// Update vfs
				MarkParent(szFileName, TRUE);
				MarkVirtualDir(&lpData->File, lpUser->hMountFile);
			}
			else
			{
				// Update download stats
				Download_Complete(&lpUser->UserFile, (lpData->bFree ? 0 : lpData->Charged - (lpData->Size / 1024)),
					lpData->Size, dwDuration, lpData->File.CreditSection,
					(! PathCheck(lpUser->UserFile, lpData->File.pwd, "NoStats") ? -1 : lpData->File.StatsSection),
					lpData->File.ShareSection);
			}
			if (Event.tszParameters) Free(Event.tszParameters);
		}
	}

	if (lpData->IoFile.lpFileLock)
	{
		UnlockPath(lpData->IoFile.lpFileLock);
		lpData->IoFile.lpFileLock = NULL;
	}

	if (lpUser->Listing)
	{
		FTP_AbortListing(lpUser);
	}

	//	Free transfer buffer
	if (lpData->Buffer.buf)
	{
		Free(lpData->Buffer.buf);
		lpData->Buffer.buf = NULL;
	}

	//	Reset data
	lpData->Buffer.buf			= NULL;
	lpData->bInitialized		= FALSE;
	lpData->bSpecial			= FALSE;
	lpData->bFree				= FALSE;
	lpData->bActive				= FALSE;
	lpData->dwResumeOffset[0]	= 0;
	lpData->dwResumeOffset[1]	= 0;

	return FALSE;
}




/*

  FTP_Data_Init_Transfer() - Initializes transfer

  */
BOOL FTP_Data_Init_Transfer(LPFTPUSER lpUser, LPSTR szFileName)
{
	if (FTPData_InitializeTransfer(lpUser, &lpUser->DataChannel))
	{
		//	Could not initialize transfer
		FTP_Data_Close(lpUser);
		//	Show Error
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("425 %2TCan't build data connection: %E.%0T\r\n"), lpUser->DataChannel.dwLastError);
		return TRUE;
	}

	// Print success message - annoying because of color controls and the overhead of FormatString...
	if (lpUser->DataChannel.bEncoding == BINARY)
	{
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("150 Opening %6TBINARY%0T mode data connection for "));
	}
	else
	{
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("150 Opening %7TASCII%0T mode data connection for "));
	}

	if (lpUser->DataChannel.bSpecial == LIST)
	{
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("%s"), szFileName);
	}
	else if (lpUser->DataChannel.bDirection == SEND)
	{
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("%8T%s%0T (%9T%I64i%0T bytes)"), szFileName, lpUser->DataChannel.FileSize);
	}
	else
	{
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("%8T%s%0T"), szFileName);
	}

	if (lpUser->DataChannel.bProtected)
	{
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT(" using %10TSSL/TLS%0T.\r\n"));
	}
	else
	{
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT(".\r\n"));
	}
	return FALSE;
}







BOOL FTP_Data_Begin_Transfer(LPFTPUSER lpUser)
{
	//	Queue job (no start / no timeout)
	if (AddClientJob(lpUser->Connection.dwUniqueId, 201, CJOB_TERTIARY|CJOB_INSTANT, INFINITE, NULL, FTP_CancelData, NULL, lpUser))
	{
		WSAAsyncSelectContinue(&lpUser->DataChannel.ioSocket, FTP_Data_Start, lpUser);
	}
	else
	{
		WSAAsyncSelectCancel(&lpUser->DataChannel.ioSocket);
	}

	AddClientJob(lpUser->Connection.dwUniqueId, 203, CJOB_TERTIARY|CJOB_SECONDARY|CJOB_EXCLUSIVE, INFINITE, FTPData_TransferComplete, NULL, NULL, lpUser);

	return FALSE;
}






/*

  */
static BOOL FTPData_TransferComplete(LPFTPUSER lpUser)
{
	IOBUFFER		Buffer[1];
	PDATACHANNEL	lpData;
	BOOL			bShowDefault, bSpecial;
	LPTSTR			tszFileName, tszBasePath;
	DWORD           dwTimeOut;
	CHAR            szBuf[512];
	INT64           i64Raw;

	lpData	= &lpUser->DataChannel;

	// we'll use the socket lock as the data lock...
	EnterCriticalSection(&lpData->ioSocket.csLock);

	i64Raw = lpData->Size;

	//	Calculate transferred bytes
	if ( !(lpData->bActive && (lpData->bSpecial == LIST)) )
	{
		lpData->Size	= (lpData->IoFile.Overlapped.OffsetHigh - lpData->dwResumeOffset[0]) * 0x100000000 +
			(lpData->IoFile.Overlapped.Offset - lpData->dwResumeOffset[1]);
	}

	if (lpData->bAbort && (lpData->dwLastError == NO_ERROR))
	{
		// need to adjust the error
		lpData->dwLastError = IO_TRANSFER_ABORTED;
	}

	bSpecial = lpData->bSpecial;

	//	Close transfer
	FTP_Data_Close(lpUser);

	if ((lpData->dwLastError == NO_ERROR) && lpData->IoFile.Overlapped.bDoCrc && (lpData->bDirection == SEND) && (bSpecial != LIST) )
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT("226-File Size: %I64i - CRC32: 0x%08x.\r\n"), lpData->Size, ~lpData->IoFile.Overlapped.Crc32);
	}

	// this function run directly via a job on a worker thread, and not through FTP_Command so the
	// theme won't be set
	if (lpUser->FtpVariables.lpTheme)
	{
		SetTheme(lpUser->FtpVariables.lpTheme);
	}

	if (lpData->dwLastError == NO_ERROR)
	{
		// Show status message
		MaybeDisplayStatus(lpUser, _T("226-"));

		//	Show completion message
		bShowDefault	= TRUE;
		tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
		if (tszBasePath)
		{
			if (aswprintf(&tszFileName, _TEXT("%s\\TransferComplete"), tszBasePath))
			{
				if (! MessageFile_Show(tszFileName,
					&lpUser->CommandChannel.Out, lpUser, DT_FTPUSER, _TEXT("226-"), _TEXT("226 ")))
				{
					bShowDefault	= FALSE;
				}
				Free(tszFileName);
			}
			FreeShared(tszBasePath);
		}
		if (bShowDefault)
		{
			FormatString(&lpUser->CommandChannel.Out,
				_TEXT("%s"), _TEXT("226 Transfer complete.\r\n"));
		}
	}
	else
	{
		// Show status message
		MaybeDisplayStatus(lpUser, _T("426-"));

		if (lpData->dwLastError == ERROR_SEM_TIMEOUT)
		{
			// For some reason WinSock returns this non-winsock error, make our response more informative
			lpData->dwLastError = ERROR_TRANSFER_TIMEOUT;
		}

		if ((lpData->dwLastError == IO_SSL_FAIL) && lpData->ulSslError && ERR_error_string(lpData->ulSslError, szBuf))
		{
			FormatString(&lpUser->CommandChannel.Out,
				_TEXT("426 %2TConnection closed: SSL library failure (%s).%0T\r\n"), szBuf);
		}
		else
		{
			FormatString(&lpUser->CommandChannel.Out,
				_TEXT("426 %2TConnection closed: %E.%0T\r\n"), lpData->dwLastError);
		}
	}

	//	Transfer aborted?
	if (lpData->bAbort)
	{
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("226 %4TABOR command successful.%0T\r\n"));
		lpData->bAbort = FALSE;
	}

	LeaveCriticalSection(&lpData->ioSocket.csLock);

	//	Activate idle timeout
	lpUser->CommandChannel.Idle	= GetTickCount();

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
			dwTimeOut = FtpSettings.dwIdleTimeOut;
		}
	}
	else
	{
		dwTimeOut = FtpSettings.dwIdleTimeOut;
	}
	AddClientJobTimer(lpUser->Connection.dwUniqueId, 1, dwTimeOut, FTP_TimerProc);

	if (lpUser->CommandChannel.Out.len)
	{
		Buffer[0].dwType		= PACKAGE_BUFFER_SEND;
		Buffer[0].len			= lpUser->CommandChannel.Out.len;
		Buffer[0].buf			= lpUser->CommandChannel.Out.buf;
		Buffer[0].dwTimerType	= TIMER_PER_PACKAGE;
		Buffer[0].dwTimeOut		= SEND_TIMEOUT;

		if (AddExclusiveClientJob(lpUser->Connection.dwUniqueId, 2,	CJOB_SECONDARY|CJOB_INSTANT, INFINITE, NULL, FTP_Cancel, NULL, lpUser))
		{
			TransmitPackages(&lpUser->CommandChannel.Socket, Buffer, 1, NULL, FTP_SendReply, lpUser);
		}
	}
	EndClientJob(lpUser->Connection.dwUniqueId, 203);
	return FALSE;
}

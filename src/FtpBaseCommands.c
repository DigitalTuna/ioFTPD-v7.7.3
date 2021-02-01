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


// Internal FTP commands
static BOOL FTP_Authentication(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_FileDelete(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_DirectoryChange(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_DirectoryChangeUp(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_DirectoryCreate(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_DirectoryRemove(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_DirectoryPrint(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_Size(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_FileTime(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_Identify(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_LoginPassword(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_LoginUser(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_Noop(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_Protection(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_ProtectionBufferSize(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_Quit(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_RenameFrom(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_RenameTo(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_System(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_TransferAbort(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_TransferActive(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_TransferAppend(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_TransferPassive(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_TransferRestart(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_TransferRetrieve(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_TransferSecurePassive(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_TransferStore(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_TransferType(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_Features(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_SecureFxpConfig(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_XCRC(LPFTPUSER lpUser, IO_STRING *Args);
static BOOL FTP_ClientType(LPFTPUSER lpUser, IO_STRING *Args);



#define LOGIN_CMD 1
#define OTHER_CMD 2
#define XFER_CMD  4

static FTPCOMMAND FtpCommand[] = {

  // Authentication

  _TEXT("AUTH"),  LOGIN_CMD, 1, 1,        FTP_Authentication,
  _TEXT("IDNT"),  LOGIN_CMD, 0, -1,      FTP_Identify,
  _TEXT("USER"),  LOGIN_CMD, 1, 1,        FTP_LoginUser,
  _TEXT("PASS"),  LOGIN_CMD, 1, -1,      FTP_LoginPassword,

  // Socket operations

  _TEXT("PASV"),  OTHER_CMD, 0, 0,  FTP_TransferPassive,
  _TEXT("CPSV"),  OTHER_CMD, 0, 0,  FTP_TransferSecurePassive,
  _TEXT("SSCN"),  OTHER_CMD, 0, 1,  FTP_SecureFxpConfig,
  _TEXT("PORT"),  OTHER_CMD, 1, 1,  FTP_TransferActive,

  // Transfer operations

  _TEXT("LIST"),  OTHER_CMD, -1, -1,  FTP_List,
  _TEXT("NLST"),  OTHER_CMD, -1, -1,  FTP_Nlist,
  _TEXT("MLSD"),  OTHER_CMD, -1, -1,  FTP_MLSD,
  _TEXT("STAT"),  OTHER_CMD, 1, -1,   FTP_Stat,
  _TEXT("STOR"),  OTHER_CMD, 1, -1,   FTP_TransferStore,
  _TEXT("RETR"),  OTHER_CMD, 1, -1,   FTP_TransferRetrieve,
  _TEXT("APPE"),  OTHER_CMD, 1, -1,   FTP_TransferAppend,
  _TEXT("TYPE"),  OTHER_CMD, 1, 1,    FTP_TransferType,
  _TEXT("REST"),  OTHER_CMD, 1, 1,    FTP_TransferRestart,
  _TEXT("ABOR"),  OTHER_CMD|XFER_CMD, -1, -1,           FTP_TransferAbort,

  // Directory operations

  _TEXT("CDUP"), OTHER_CMD, 0, 0,         FTP_DirectoryChangeUp,
  _TEXT("CWD"),  OTHER_CMD, 1, -1,        FTP_DirectoryChange,
  _TEXT("PWD"),  OTHER_CMD, 0, 0,         FTP_DirectoryPrint,
  _TEXT("MKD"),  OTHER_CMD, 1, -1,        FTP_DirectoryCreate,
  _TEXT("RMD"),  OTHER_CMD, 1, -1,        FTP_DirectoryRemove,

  // File operations

  _TEXT("DELE"),  OTHER_CMD, 1, -1,      FTP_FileDelete,
  _TEXT("RNFR"),  OTHER_CMD, 1, -1,      FTP_RenameFrom,
  _TEXT("RNTO"),  OTHER_CMD, 1, -1,      FTP_RenameTo,
  _TEXT("SIZE"),  OTHER_CMD, 1, -1,      FTP_Size,
  _TEXT("MDTM"),  OTHER_CMD, 1, -1,      FTP_FileTime,
  _TEXT("XCRC"),  OTHER_CMD, 1, -1,      FTP_XCRC,

  // Other
  _TEXT("PROT"), OTHER_CMD|LOGIN_CMD, 1, 1,  FTP_Protection,
  _TEXT("PBSZ"), OTHER_CMD|LOGIN_CMD, 1, 1,  FTP_ProtectionBufferSize,
  _TEXT("FEAT"), OTHER_CMD|LOGIN_CMD, 0, 0,  FTP_Features,
  _TEXT("SITE"), OTHER_CMD, 1, -1,           FTP_AdminCommands,
  _TEXT("HELP"), OTHER_CMD, 0, -1,           FTP_Help,
  _TEXT("NOOP"), OTHER_CMD|XFER_CMD, 0, 0,   FTP_Noop,
  _TEXT("SYST"), OTHER_CMD, 0, 0,            FTP_System,
  _TEXT("CLNT"), OTHER_CMD, 1, -1,           FTP_ClientType,
  _TEXT("QUIT"), OTHER_CMD|LOGIN_CMD, 0, 0,  FTP_Quit
};


VOID MaybeDisplayStatus(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix)
{
	LPCLIENT lpClient;
	LPTSTR tszBasePath, tszMsg;
	TCHAR  tszFileName[_MAX_PATH+1];
	INT32  iLen;
	DWORD  n;

	tszBasePath = NULL;

	if (FtpSettings.dwShutdownTimeLeft > 0)
	{
		tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
		if (!tszBasePath)
		{
			// won't be able to locate any message files... just bail
			return;
		}
		_tcsncpy_s(tszFileName, sizeof(tszFileName)/sizeof(*tszFileName), tszBasePath, _TRUNCATE);
		iLen = _tcslen(tszFileName);

		_tcsncpy_s(&tszFileName[iLen], sizeof(tszFileName)/sizeof(*tszFileName) - iLen, _T("\\ServerShutdown"), _TRUNCATE);
		MessageFile_Show(tszFileName, &lpUser->CommandChannel.Out, lpUser, DT_FTPUSER, tszMultilinePrefix, NULL);
	}

	if (FtpSettings.tmSiteClosedOn)
	{
		if (!tszBasePath)
		{
			if ( !(tszBasePath = Service_MessageLocation(lpUser->Connection.lpService)) )
			{
				// won't be able to locate any message files... just bail
				return;
			}
			_tcsncpy_s(tszFileName, sizeof(tszFileName)/sizeof(*tszFileName), tszBasePath, _TRUNCATE);
			iLen = _tcslen(tszFileName);
		}

		_tcsncpy_s(&tszFileName[iLen], sizeof(tszFileName)/sizeof(*tszFileName) - iLen, _T("\\ServerClosing"), _TRUNCATE);
		MessageFile_Show(tszFileName, &lpUser->CommandChannel.Out, lpUser, DT_FTPUSER, tszMultilinePrefix, NULL);
	}

	for (n=0 ; n<MAX_MESSAGES ; n++)
	{
		// we'll test but not use the value outside of locking the client since it's ourself
		if (tszMsg = lpUser->FtpVariables.tszMsgStringArray[n])
		{
			if (!tszBasePath)
			{
				if ( !(tszBasePath = Service_MessageLocation(lpUser->Connection.lpService)) )
				{
					// won't be able to locate any message files... just bail
					break;
				}
				_tcsncpy_s(tszFileName, sizeof(tszFileName)/sizeof(*tszFileName), tszBasePath, _TRUNCATE);
				iLen = _tcslen(tszFileName);
			}

			if (lpClient = LockClient(lpUser->Connection.dwUniqueId))
			{
				// retest the value here
				if (tszMsg = lpUser->FtpVariables.tszMsgStringArray[n])
				{
					_snprintf_s(&tszFileName[iLen], sizeof(tszFileName)/sizeof(*tszFileName) - iLen, _TRUNCATE, _T("\\Msg%u"), n+1);
					// we need to undo the lock here since %[MSG()] needs to acquire it as well
					UnlockClient(lpUser->Connection.dwUniqueId);
					MessageFile_Show(tszFileName, &lpUser->CommandChannel.Out, lpUser, DT_FTPUSER, tszMultilinePrefix, NULL);
				}
				else
				{
					UnlockClient(lpUser->Connection.dwUniqueId);
				}
			}
		}
	}

	FreeShared(tszBasePath);
}



static BOOL FTP_Features(LPFTPUSER lpUser, IO_STRING *Args)
{
	LPTSTR tszDeviceName, tszSuppressed;

	if (lpUser->CommandChannel.Socket.lpDevice && (tszDeviceName = lpUser->CommandChannel.Socket.lpDevice->tszName))
	{
		tszSuppressed = Config_Get(&IniConfigFile, tszDeviceName, "Feature_Suppression", NULL, NULL);
	}
	else
	{
		tszSuppressed = 0;
	}

	FormatString(&lpUser->CommandChannel.Out, _TEXT("211-Extensions supported:\r\n"));
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("AUTH SSL")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" AUTH SSL\r\n"));
	}
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("AUTH TLS")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" AUTH TLS\r\n"));
	}
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("CLNT")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" CLNT\r\n"));
	}
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("CPSV")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" CPSV\r\n"));
	}
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("LIST -")))
	{
		if (HasFlag(lpUser->UserFile, _T("VM")))
		{
			// regular user
			FormatString(&lpUser->CommandChannel.Out, _TEXT(" LIST -1aAdflLRsTU\r\n"));
		}
		else
		{
			// VFS admin or Master
			FormatString(&lpUser->CommandChannel.Out, _TEXT(" LIST -1aAdflLRsTUVZ\r\n"));
		}
	}
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("MDTM--")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" MDTM\r\n"));
	}
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("MDTM YYYYMMDDHHMMSS filename")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" MDTM YYYYMMDDHHMMSS filename\r\n"));
	}

  // we don't support OPTS command yet, and FlashFXP sucks with MLSD at the moment, so
  // accept the MLSD command, but don't advertise we have it
  // FormatString(&lpUser->CommandChannel.Out, _TEXT(" MLST type*;size*;modify*;UNIX.mode*;UNIX.owner*;UNIX.group*\r\n"));

	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("PBSZ")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" PBSZ\r\n"));
	}
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("PROT")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" PROT\r\n"));
	}
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("REST STREAM")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" REST STREAM\r\n"));
	}
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("SIZE")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" SIZE\r\n"));
	}
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("SSCN")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" SSCN\r\n"));
	}
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("STAT -")))
	{
		if (HasFlag(lpUser->UserFile, _T("VM")))
		{
			// regular user
			FormatString(&lpUser->CommandChannel.Out, _TEXT(" STAT -1aAdflLRsTU\r\n"));
		}
		else
		{
			// VFS admin or Master
			FormatString(&lpUser->CommandChannel.Out, _TEXT(" STAT -1aAdflLRsTUVZ\r\n"));
		}
	}
	if (!tszSuppressed || !_tcsstr(tszSuppressed, _T("TVFS")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" TVFS\r\n"));
	}

	if (!tszSuppressed || _tcsstr(tszSuppressed, _T("XCRC filename;start;end")))
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT(" XCRC filename;start;end\r\n"));
	}
  FormatString(&lpUser->CommandChannel.Out, _TEXT("211 END\r\n"));
  return FALSE;
}



static BOOL FTP_ClientType(LPFTPUSER lpUser, IO_STRING *Args)
{
	LPTSTR tszClient;
	int    iLen;

	if (lpUser->FtpVariables.tszClientType)
	{
		Free(lpUser->FtpVariables.tszClientType);
		lpUser->FtpVariables.tszClientType = NULL;
	}

	tszClient  = GetStringIndex(Args, STR_ALL);
	iLen = _tcslen(tszClient)+1;

	if (!(lpUser->FtpVariables.tszClientType = Allocate("ClientType", iLen*sizeof(TCHAR))))
	{
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("550 %5TCLNT%0T: %2T%E.%0T\r\n"), GetLastError());
		return TRUE;
	}

	strcpy_s(lpUser->FtpVariables.tszClientType, iLen, tszClient);
	FormatString(&lpUser->CommandChannel.Out, _TEXT("200 Noted.\r\n"));
	return FALSE;
}



static BOOL FTP_XCRC(LPFTPUSER lpUser, IO_STRING *Args)
{
	VIRTUALPATH          Path;
	LPFILEINFO           lpFileInfo;
	LPTSTR               tszFileName, tszPath;
	TCHAR                tChar;
	DWORD                dwError, dwCrc;
	BOOL                 bResult;
	UINT64               u64Start, u64End;
	int                  iScan;
	HANDLE               hFile;
	MOUNT_DATA           MountData;

	u64Start = 0;
	u64End = 0;
	dwError = NO_ERROR;
	bResult = FALSE;
	dwCrc = 1;

	// this will force all subsequent transfers to compute
	// the CRC on the fly since it's likely they will also
	// be requested...
	lpUser->FtpVariables.bComputeCrc = TRUE;

	switch (GetStringItems(Args))
	{
	case 3:
		tszFileName = GetStringIndexStatic(Args, 1);
		iScan = sscanf_s(tszFileName, "%I64u%c", &u64Start, &tChar);
		if (iScan != 1)
		{
			dwError = ERROR_INVALID_ARGUMENTS;
			break;
		}
		dwCrc = 2;
	case 2:
		tszFileName = GetStringIndexStatic(Args, dwCrc);
		iScan = sscanf_s(tszFileName, "%I64u%c", &u64End, &tChar);
		if (iScan != 1 || u64Start > u64End)
		{
			dwError = ERROR_INVALID_ARGUMENTS;
			break;
		}
	case 1:
		tszFileName  = GetStringIndexStatic(Args, 0);
		if (!tszFileName || !*tszFileName)
		{
			dwError = ERROR_INVALID_ARGUMENTS;
			tszFileName = _T("XCRC");
		}
		//  Remove quotes
		if (tszFileName[0] == _T('"') && (tszPath = _tcsrchr(&tszFileName[1], _T('"'))) && !tszPath[1])
		{
			*tszPath = 0;
			tszFileName++;
		}
		break;
	default:
	case 0:
		// shouldn't be able to call this function with no arguments...
		dwError = ERROR_INVALID_ARGUMENTS;
		tszFileName = _T("XCRC");
	}
	
	if (dwError == NO_ERROR)
	{
		//  Copy virtual path
		PWD_Reset(&Path);
		PWD_Copy(&lpUser->CommandChannel.Path, &Path, FALSE);

		if ((tszPath = PWD_CWD2(lpUser->UserFile, &Path, tszFileName, lpUser->hMountFile, &MountData, EXISTS|VIRTUAL_PWD, lpUser, _T("XCRC"), Args)))
		{
			//  Get file attributes
			if (GetFileInfo(tszPath, &lpFileInfo))
			{
				if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					dwError = IO_INVALID_FILENAME;
				}
				else if (u64End && u64End > lpFileInfo->FileSize)
				{
					dwError = ERROR_INVALID_ARGUMENTS;
				}
				else
				{
					if (!u64End) u64End = lpFileInfo->FileSize;

					if ((lpFileInfo->FileSize == u64End) && !u64Start && lpUser->FtpVariables.bValidCRC &&
						(Path.l_RealPath == lpUser->FtpVariables.vpLastUpload.l_RealPath) &&
						(lpFileInfo->dwUploadTimeInMs == lpUser->FtpVariables.dwLastUploadUpTime) &&
						lpUser->FtpVariables.vpLastUpload.RealPath && 
						!stricmp(Path.RealPath, lpUser->FtpVariables.vpLastUpload.RealPath))
					{
						// we have a cached answer!
						dwCrc = lpUser->FtpVariables.dwLastCrc32;
						bResult = TRUE;
					}
					else
					{
						hFile = CreateFile(tszPath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
							0, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
						if (hFile != INVALID_HANDLE_VALUE)
						{
							dwCrc = ~0;
							if (FileCrc32(hFile, u64Start, u64End-u64Start, &dwCrc))
							{
								dwCrc = ~dwCrc;
								bResult = TRUE;
							}
							CloseHandle(hFile);
						}
						else
						{
							dwError = GetLastError();
						}
					}
				}
				CloseFileInfo(lpFileInfo);
			}
			else
			{
				dwError = GetLastError();
			}
			PWD_Free(&Path);
		}
		else
		{
			dwError = GetLastError();
		}

		// invalidate the cached answer.  Either we just used it, or the user requested
		// a different file in which case we should give the current value if the last
		// uploaded file is requested later on.
		lpUser->FtpVariables.bValidCRC = FALSE;
	}

	if (bResult)
	{
		FormatString(&lpUser->CommandChannel.Out, _TEXT("250 %08x\r\n"), dwCrc);
		return FALSE;
	}

	//  Error
	if (tszFileName )
	FormatString(&lpUser->CommandChannel.Out,
		_TEXT("550 %5T%s%0T: %2T%E%0T.\r\n"), tszFileName, dwError);
	return TRUE;
}



// SSCN [on|off]
static BOOL FTP_SecureFxpConfig(LPFTPUSER lpUser, IO_STRING *Args)
{
	LPTSTR  tszType;

	switch(GetStringItems(Args)) {
		case 1: // set status
			tszType = GetStringIndex(Args, STR_ALL);
			if (!_tcsicmp(tszType, "ON")) {
				lpUser->Connection.dwStatus |= U_FXPSSLCLIENT;
			}
			else if (!_tcsicmp(tszType, "OFF")) {
				lpUser->Connection.dwStatus &= ~U_FXPSSLCLIENT;
			}
			else break;
			// fall through

		case 0: // print current status
			if (lpUser->Connection.dwStatus & U_FXPSSLCLIENT) {
				FormatString(&lpUser->CommandChannel.Out,
					_TEXT("200 SSCN:CLIENT METHOD\r\n"));
			}
			else {
				FormatString(&lpUser->CommandChannel.Out,
					_TEXT("200 SSCN:SERVER METHOD\r\n"));
			}
			return FALSE;
	}
	FormatString(&lpUser->CommandChannel.Out,
		_TEXT("501 '%5TSSCN%0T': %2T%E.%0T\r\n"),
		ERROR_INVALID_ARGUMENTS);

	return TRUE;
}




static BOOL FTP_Event_CreateDirectory(LPFTPUSER lpUser, CHAR *Arguments, DWORD dwError)
{
	if (dwError == NO_ERROR)
	{
		return Event_Run("Events", "OnNewDir", Arguments, lpUser, DT_FTPUSER, "550-");
	}
	return Event_Run("Events", "OnFailedDir", Arguments, lpUser, DT_FTPUSER, "550-");
}


static BOOL FTP_Event_DeleteDirectory(LPFTPUSER lpUser, CHAR *Arguments)
{
  return Event_Run("Events", "OnDelDir", Arguments, lpUser, DT_FTPUSER, "250-");
}


static BOOL FTP_Event_OnUploadResume(LPFTPUSER lpUser, CHAR *Arguments)
{
  return Event_Run("Events", "OnResume", Arguments, lpUser, DT_FTPUSER, "550-");
}



static BOOL FTP_Event_OnUpload(LPFTPUSER lpUser, CHAR *Arguments)
{
  return Event_Run("Events", "OnUpload", Arguments, lpUser, DT_FTPUSER, "550-");
}






static BOOL FTP_System(LPFTPUSER lpUser, IO_STRING *Args)
{
  //  Show system type
  FormatString(&lpUser->CommandChannel.Out,
    _TEXT("%s"), _TEXT("215 UNIX Type: L8\r\n"));
  return FALSE;
}



static BOOL FTP_Protection(LPFTPUSER lpUser, IO_STRING *Args)
{
  LPTSTR  tszProtection;

  //  Get protection string
  tszProtection  = GetStringIndexStatic(Args, 0);

  switch (tszProtection[0])
  {
  case _TEXT('P'):
    //  Private (TLS/SSL)
    FormatString(&lpUser->CommandChannel.Out,
      _TEXT("%s"), _TEXT("200 Protection set to: Private.\r\n"));
    lpUser->DataChannel.bProtected  = TRUE;
    return FALSE;

  case 'C':
    //  Clear type (RAW)
    FormatString(&lpUser->CommandChannel.Out,
      _TEXT("%s"), _TEXT("200 Protection set to: Clear.\r\n"));
    lpUser->DataChannel.bProtected  = FALSE;
    return FALSE;
  }
  //  Unsupported protection mode
  FormatString(&lpUser->CommandChannel.Out, _TEXT("504 %5TPROT %s%0T %2Tunsupported.%0T\r\n"), tszProtection);
  return TRUE;
}





/*
 * FTP: PBSZ <size>
 */
static BOOL
FTP_ProtectionBufferSize(LPFTPUSER lpUser,
                         IO_STRING *Args)
{
  LPTSTR  tszSize;
  TCHAR  *tpCheck;
  
  //  Get protection buffer size
  tszSize = GetStringIndexStatic(Args, 0);

  if (! _tcstol(tszSize, &tpCheck, 10) &&
    tpCheck != tszSize && tpCheck[0] == _TEXT('\0'))
  {
    //  Protection buffer set to zero
    FormatString(&lpUser->CommandChannel.Out,
      _TEXT("%s"), _TEXT("200 PBSZ 0 successful.\r\n"));
    return FALSE;
  }

  if (*tpCheck)
  {
	  // the number couldn't be converted correctly
	  FormatString(&lpUser->CommandChannel.Out, _T("501 %5TPBSZ%0T %2T%E.%0T\r\n"), ERROR_INVALID_ARGUMENTS);
	  return TRUE;
  }

  // tell client we only accept a buffer size of '0'.
  FormatString(&lpUser->CommandChannel.Out, _T("200 PSZS=0\r\n"));
  return FALSE;
}


/*
 * FTP_Identify() - FTP protocol has support for bouncers
 */
static BOOL
FTP_Identify(LPFTPUSER lpUser,
             IO_STRING *Args)
{
	LPIOSERVICE lpService;
	TCHAR szIp[MAX_HOSTNAME];
	PCHAR  pAt, pSemicolon;
	DWORD  dwIdent, dwHostName, n;
	LPSTR  szLine, szIdent, szHostName, szAddress;
	IN_ADDR addr, addr2;
	BOOL   bFound;
	LONG   lAddress;

	szIdent     = NULL;
	lpService   = lpUser->Connection.lpService;

	if (!lpService) goto logoff;

	szHostName  = lpUser->Connection.szHostName;
	sprintf(szIp, "%s", inet_ntoa(lpUser->Connection.ClientAddress.sin_addr));
	bFound = FALSE;

	AcquireSharedLock(&lpService->loLock);

	for (n=0 ; n<10 ; n++, szAddress++)
	{
		szAddress = lpService->tszBncAddressArray[n];
		if (!szAddress) break;
		if (IsNumericIP(szAddress))
		{
			// compare as IP
			if (!iCompare(szAddress, szIp))
			{
				bFound = TRUE;
				break;
			}
		}
		else
		{
			// compare as HOST
			if (!iCompare(szAddress, szHostName))
			{
				bFound = TRUE;
				break;
			}
		}
	}

	ReleaseSharedLock(&lpUser->Connection.lpService->loLock);
	if (!bFound) goto logoff;

  // connection is from a registered BNC address here

  //  Get line
  if (! (szLine = GetStringRange(Args, STR_BEGIN, STR_END))) goto logoff;
  //  Find At
  if (! (pAt = strchr(szLine, '@'))) goto logoff;
  if (! (pSemicolon = strchr(&pAt[1], ':'))) goto logoff;

  //  Zero padding
  pAt[0]      = '\0';
  pSemicolon[0]  = '\0';

  //  String length
  dwHostName  = strlen(&pSemicolon[1]);
  dwIdent    = pAt - szLine;

  if (dwHostName > MAX_HOSTNAME - 1) dwHostName  = MAX_HOSTNAME - 1;
  if (dwIdent > MAX_IDENT - 1) dwIdent  = MAX_IDENT - 1;

  //  Allocate memory
  if (dwIdent && ! (szIdent = (LPSTR)AllocateShared(NULL, "Ident", dwIdent + 1))) goto logoff;
  if (dwHostName && ! (szHostName = (LPSTR)AllocateShared(NULL, "Hostname", dwHostName + 1)))
  {
    FreeShared(szIdent);
	goto logoff;
  }

  if (szIdent) szIdent[dwIdent]  = '\0';
  if (szHostName) szHostName[dwHostName]  = '\0';
  //  Copy strings
  CopyMemory(szIdent, szLine, dwIdent);
  CopyMemory(szHostName, &pSemicolon[1], dwHostName);

  addr.s_addr = lAddress  = inet_addr(&pAt[1]);

  // You can never IDNT the loopback address or a local non-routable address
  if ((addr.s_net == 10) || (addr.s_net == 127) ||
	  ((addr.s_net == 172) && (addr.s_host == 16)) ||
	  ((addr.s_net == 192) && (addr.s_host == 168)))
  {
	  goto logoff;
  }

  // catch BNC's that don't reverse resolve and just use IP...
  if (szHostName && IsNumericIP(szHostName))
  {
	  addr2.s_addr = inet_addr(szHostName);
	  if (!memcmp(&addr, &addr2, sizeof(addr)))
	  {
		  FreeShared(szHostName);
		  szHostName = NULL;
	  }
  }

  //  Free old entries
  FreeShared(lpUser->Connection.szHostName);
  FreeShared(lpUser->Connection.szIdent);

  //  Update data
  lpUser->Connection.ClientAddress.sin_addr.s_addr  = lAddress;
  lpUser->Connection.szIdent     = szIdent;
  lpUser->Connection.szHostName  = szHostName;
  lpUser->Connection.dwStatus   |= U_IDENT;

  //  Update data
  UpdateClientData(DATA_IDENT, lpUser->Connection.dwUniqueId, szIdent, szHostName, lAddress);

  return FALSE;

logoff:
  // set the logoff flag, if we return from here with this set it will throw the
  // user off which is what we want for non-authorized hosts.
  lpUser->Connection.dwStatus  |= U_LOGOFF;
  return TRUE;
}


/*
 * FTP: AUTH TLS/SSL
 */
static BOOL
FTP_Authentication(LPFTPUSER lpUser,
                   IO_STRING *Args)
{
	LPIOSERVICE lpService;
	LPTSTR  tszType;

	tszType  = GetStringIndexStatic(Args, 0);
	lpService = lpUser->Connection.lpService;

	if (lpService->pSecureCtx && ! _tcsicmp(tszType, _TEXT("TLS")) && lpService->bTlsSupported)
	{
		//  AUTH TLS
		if (! Secure_Init_Socket(&lpUser->CommandChannel.Socket, lpService, SSL_ACCEPT))
		{
			//  Secure socket initialization successful
			FormatString(&lpUser->CommandChannel.Out,
				_TEXT("%s"), _TEXT("234 AUTH TLS successful.\r\n"));
			lpUser->Connection.dwStatus  |= U_MAKESSL;
			return FALSE;
		}
		//  Secure socket intialization failed
		FormatString(&lpUser->CommandChannel.Out, _TEXT("534 %2TError initializing secure socket: %E.%0T\r\n"), GetLastError());
		return TRUE;
	}
	else if (lpService->pSecureCtx && ! _tcsicmp(tszType, "SSL") && lpService->bSslSupported)
	{
		//  AUTH SSL
		if (! Secure_Init_Socket(&lpUser->CommandChannel.Socket, lpService, SSL_ACCEPT))
		{
			//  Secure socket initialization successful
			FormatString(&lpUser->CommandChannel.Out,
				_TEXT("%s"),_TEXT("234 AUTH SSL successful.\r\n"));
			lpUser->Connection.dwStatus    |= U_MAKESSL;
			return FALSE;
		}
		//  Secure socket intialization failed
		FormatString(&lpUser->CommandChannel.Out, _TEXT("%2T534 Error initializing secure socket: %E.%0T\r\n"), GetLastError());
		return TRUE;
	}
	//  Unknown AUTH mode
	FormatString(&lpUser->CommandChannel.Out,
		_TEXT("504 %5TAUTH %s%0T %2Tunsupported.%0T\r\n"), tszType);
	return TRUE;
}


/*
 * FTP: RNFR <filename>
 */
static BOOL
FTP_RenameFrom(LPFTPUSER lpUser,
               IO_STRING *Args)
{
  LPTSTR  tszFileName, tszPath;
  LPFILEINFO lpFileInfo;

  //  Get source filename
  tszFileName  = GetStringIndex(Args, STR_ALL);

  PWD_Free(&lpUser->FtpVariables.vpRenameFrom);
  PWD_Copy(&lpUser->CommandChannel.Path, &lpUser->FtpVariables.vpRenameFrom, FALSE);

  lpUser->FtpVariables.bNoPath = ( _tcschr(tszFileName, _T('/')) ? 0 : 1);

  if (tszPath = Rename_Prepare_From(lpUser, &lpUser->FtpVariables.vpRenameFrom,
	  lpUser->UserFile, lpUser->hMountFile, tszFileName, &lpUser->CommandChannel.ErrorCode))
  {
	if (GetFileInfo(tszPath, &lpFileInfo))
	{
		if (lpFileInfo->dwFileAttributes && FILE_ATTRIBUTE_DIRECTORY)
		{
			lpUser->FtpVariables.bRenameDir = TRUE;
		}
		else
		{
			lpUser->FtpVariables.bRenameDir = FALSE;
		}
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("350 %s exists, ready for destination name.\r\n"),
			(lpUser->FtpVariables.bRenameDir ? _T("Directory") : _T("File")));
		CloseFileInfo(lpFileInfo);
		if (lpUser->FtpVariables.bNoPath)
		{
			tszPath = _tcsrchr(lpUser->FtpVariables.vpRenameFrom.RealPath, _T('\\'));
			if (!tszPath || stricmp(&tszPath[1], tszFileName))
			{
				lpUser->FtpVariables.bNoPath = 0;
			}
		}
		return FALSE;
	}
	lpUser->CommandChannel.ErrorCode = GetLastError();
	PWD_Free(&lpUser->FtpVariables.vpRenameFrom);
  }
  //  Failed
  FormatString(&lpUser->CommandChannel.Out,
    _TEXT("550 %5T%s%0T: %2T%E.%0T\r\n"), tszFileName, lpUser->CommandChannel.ErrorCode);
  return TRUE;
}


/*
 * FTP: RNTO <filename>
 * - Rename to
 */
static BOOL
FTP_RenameTo(LPFTPUSER lpUser,
             IO_STRING *Args)
{
  LPTSTR    tszNewName, tszCwdName, tszName;
  BOOL    bResult;
  // LPFILEINFO lpFileInfo;
  DWORD   dwIndex, dwSlashes;
  ADMIN_SIZE AdminSize;
  UINT64 u64Free, u64Size;
  TCHAR tcSave, *tcTemp;
  TCHAR *tszOldName;
  DWORD dwInitialTicks;
  TCHAR tszPath[MAX_PATH+1];

  //  Get source filename
  tszOldName = lpUser->FtpVariables.vpRenameFrom.RealPath;
  if ( !tszOldName )
  {
	  //  Output error
	  FormatString(&lpUser->CommandChannel.Out,
		  _TEXT("503 %2TBad sequence of commands.%0T\r\n"));
	  return TRUE;
  }

  tszName  = GetStringIndex(Args, STR_ALL);
  //  Get destination filename
  tszCwdName = tszNewName  = Rename_Prepare_To(lpUser, &lpUser->CommandChannel.Path,
	  lpUser->UserFile, lpUser->hMountFile, tszName, &lpUser->CommandChannel.ErrorCode);
  if (tszNewName)
  {
	  if (lpUser->FtpVariables.bNoPath && (!_tcschr(tszName, _T('/'))))
	  {
		  // since neither the FROM nor the TO specified a path it must a simple rename!
		  // use the source path as the destination path in case we are dealing with
		  // merged directories or something, but first verify the simple name is the
		  // final path components.
		  _tcscpy_s(tszPath, sizeof(tszPath), tszOldName);
		  if (tcTemp = _tcsrchr(tszPath, _T('\\')))
		  {
			  *++tcTemp = 0;
			  _tcsncat_s(tszPath, sizeof(tszPath), tszName, _tcslen(tszName));
			  tszNewName = tszPath;
		  }
	  }

	  bResult = FALSE;
	  if (lpUser->FtpVariables.bRenameDir)
	  {
		  dwSlashes = 0;

		  // ok, let's find the end of the disk or UNC for
		  // the new filename while also seeing if it matches
		  // the source.
		  for (dwIndex = 0; tszNewName[dwIndex] ; dwIndex++)
		  {
			  if (tszOldName)
			  {
				  if (*tszOldName == tszNewName[dwIndex])
				  {
					  tszOldName++;
				  }
				  else
				  {
					  tszOldName = NULL;
				  }
			  }
			  if (tszNewName[dwIndex] == _T('\\'))
			  {
				  if (++dwSlashes > 3)
				  {
					  break;
				  }
			  }
			  if (tszNewName[dwIndex] == _T(':'))
			  {
				  if (tszNewName[dwIndex+1] == _T('\\'))
				  {
					  dwIndex++;
					  break;
				  }
			  }
		  }
		  // the two strings matched for the comparison length if tszOldName != NULL
		  bResult = (tszOldName ? TRUE : FALSE);
		  tszOldName = lpUser->FtpVariables.vpRenameFrom.RealPath;

		  if (bResult)
		  {
			  // it's just a rename operation
			  bResult = IoMoveFile(tszOldName, tszNewName);
			  if (!bResult) lpUser->CommandChannel.ErrorCode  = GetLastError();
		  }
		  else
		  {
			  // it's a real move across filesystems...
			  // need drive letter or \\computer\share\ 
			  dwIndex++;
			  tcSave = tszNewName[dwIndex];
			  tszNewName[dwIndex] = 0;
			  bResult = GetDiskFreeSpaceEx(tszNewName, (PULARGE_INTEGER) &u64Free,
				  (PULARGE_INTEGER) &u64Size, NULL);
			  tszNewName[dwIndex] = tcSave;
			  if (bResult)
			  {
				  // this could take a while...
				  SetBlockingThreadFlag();

				  // calculate disk space of directory tree to be moved, and the free space
				  // on the target filesystem to make sure there is enough room.  Also look
				  // for any permission issues...
				  ZeroMemory(&AdminSize, sizeof(AdminSize));
				  AdminSize.lpUserFile = lpUser->UserFile;
				  AdminSize.Progress.lpCommand = &lpUser->CommandChannel;
				  AdminSize.Progress.tszMultilinePrefix = _T("250-");
				  AdminSize.Progress.dwDelay  = 10000;
				  dwInitialTicks = GetTickCount() + AdminSize.Progress.dwDelay;
				  AdminSize.Progress.dwTicks  = dwInitialTicks;
				  AdminSize.Progress.tszFormatString = _T("Still sizing move... %u dirs, %u files processed, %u access errors.\r\n");
				  AdminSize.dwDirCount++;

				  RecursiveAction(lpUser->UserFile, lpUser->hMountFile,
					  lpUser->FtpVariables.vpRenameFrom.pwd, TRUE, FALSE, -1, Admin_SizeAdd, &AdminSize);
				  // lop off the added \ at end
				  if (AdminSize.dwNoAccess != 0)
				  {
					  lpUser->CommandChannel.ErrorCode = IO_NO_ACCESS_VFS;
				  }
				  else if (u64Free < AdminSize.u64Size)
				  {
					  lpUser->CommandChannel.ErrorCode = ERROR_DISK_FULL;
				  }
				  else
				  {
					  AdminSize.Progress.dwArg1 = 0;
					  AdminSize.Progress.dwArg2 = AdminSize.dwDirCount;
					  AdminSize.Progress.dwArg3 = 0;
					  AdminSize.Progress.tszFormatString = _T("Moving directories... %u of %u done. (%d files so far).\r\n");
					  bResult = IoMoveDirectory(tszOldName, tszNewName, &AdminSize.Progress);
					  if (!bResult) lpUser->CommandChannel.ErrorCode  = GetLastError();
				  }

				  SetNonBlockingThreadFlag();
			  }
		  }
	  }
	  else
	  {
		  // it's not a directory
		  bResult  = IoMoveFile(tszOldName, tszNewName);
		  if (!bResult) lpUser->CommandChannel.ErrorCode  = GetLastError();
	  }
	  if (!bResult)
	  {
		  if ((lpUser->CommandChannel.ErrorCode == ERROR_SHARING_VIOLATION) ||
			  (lpUser->CommandChannel.ErrorCode == ERROR_NOT_EMPTY))
		  {
			  // The source file/directory wasn't moved because it was in use,
			  // often happens with directory moves where another user or process
			  // is in the directory tree or actively transferring a file...
			  // Or the directory wasn't deleted...
			  // Changing the name of the error output so it makes more sense.
			  tszName = lpUser->FtpVariables.vpRenameFrom.pwd;
		  }
	  }
	  PWD_Free(&lpUser->FtpVariables.vpRenameFrom);
	  FreeShared(tszCwdName);

	  MarkVirtualDir(NULL, lpUser->hMountFile);
	  if (bResult)
	  {
		  FormatString(&lpUser->CommandChannel.Out,
			  _TEXT("%s"), _TEXT("250 RNTO command successful.\r\n"));
		  return FALSE;
	  }
  }

  //  Output error
  FormatString(&lpUser->CommandChannel.Out,
    _TEXT("550 %5T%s%0T: %2T%E.%0T\r\n"), tszName, lpUser->CommandChannel.ErrorCode);
  return TRUE;
}


static BOOL
FTP_TransferAbort(LPFTPUSER lpUser,
                  IO_STRING *Args)
{
	PDATACHANNEL lpData;
	BOOL         bAborted;

	bAborted = FALSE;
	lpData = &lpUser->DataChannel;

	// first make sure we at least initialized the data socket so we can grab it's lock...
	if (lpData->ioSocket.bInitialized)
	{
		EnterCriticalSection(&lpData->ioSocket.csLock);
		if (lpData->bActive)
		{
			// mark transfer as aborted...
			lpData->bAbort = TRUE;
			// force the socket closed...
			if (lpData->ioSocket.lpSelectEvent)
			{
				// we are still trying to connect or listen for a connection...
				if (WSAAsyncSelectCancel(&lpData->ioSocket))
				{
					// we canceled the event/timer which means no failure indication will happen...
					bAborted = TRUE;
					if (lpData->dwLastError == NO_ERROR)
					{
						lpData->dwLastError = IO_TRANSFER_ABORTED;
					}
				}
			}

			CloseSocket(&lpData->ioSocket, TRUE);
			LeaveCriticalSection(&lpData->ioSocket.csLock);
			// we'll report the result after transfer fails/finishes...
			if (bAborted)
			{
				FTP_Data_Start(lpUser);
			}
			return FALSE;
		}
		LeaveCriticalSection(&lpData->ioSocket.csLock);
	}
	FormatString(&lpUser->CommandChannel.Out, _TEXT("226 %4TABOR command successful.%0T\r\n"));
	return FALSE;
}


static BOOL
FTP_DirectoryCreate(LPFTPUSER lpUser,
                    IO_STRING *Args)
{
  LPTSTR  tszFileName;

  tszFileName  = GetStringIndex(Args, STR_ALL);

  if (! Create_Directory(lpUser, &lpUser->CommandChannel.Path, lpUser->UserFile, lpUser->hMountFile,
    tszFileName, FTP_Event_CreateDirectory, lpUser, &lpUser->CommandChannel.ErrorCode))
  {
    //  Directory successfully created
    FormatString(&lpUser->CommandChannel.Out, _TEXT("257 \"%s\" created.\r\n"), tszFileName);
    return FALSE;
  }
  //  Error while creating directory
  FormatString(&lpUser->CommandChannel.Out, _TEXT("550 %5T%s%0T: %2T%E.%0T\r\n"), tszFileName, lpUser->CommandChannel.ErrorCode);
  return TRUE; 
}


static BOOL
FTP_DirectoryRemove(LPFTPUSER lpUser,
                    IO_STRING *Args)
{
  LPTSTR  tszFileName;

  tszFileName  = GetStringIndex(Args, STR_ALL);

  if (! Remove_Directory(lpUser, &lpUser->CommandChannel.Path, lpUser->UserFile, lpUser->hMountFile,
    tszFileName, FTP_Event_DeleteDirectory, lpUser, &lpUser->CommandChannel.ErrorCode))
  {
    //  Directory successfully removed
    FormatString(&lpUser->CommandChannel.Out,
      _TEXT("%s"), _TEXT("250 RMD command successful.\r\n"));
    return FALSE;
  }

  //  Error while removing directory
  FormatString(&lpUser->CommandChannel.Out,
    _TEXT("550 %5T%s%0T: %2T%E.%0T\r\n"), tszFileName, lpUser->CommandChannel.ErrorCode);
  return TRUE;
}


/*
 * FTP: REST <offset>
 */
static BOOL
FTP_TransferRestart(LPFTPUSER lpUser,
                    IO_STRING *Args)
{
  LPTSTR  tszOffset;
  INT64  Offset;

  tszOffset  = GetStringIndexStatic(Args, 0);
  Offset    = _ttoi64(tszOffset);
  //  Set resume offset
  lpUser->DataChannel.dwResumeOffset[0]  = (DWORD)(Offset >> 32);
  lpUser->DataChannel.dwResumeOffset[1]  = (DWORD)(Offset & 0xFFFFFFFF);

  FormatString(&lpUser->CommandChannel.Out,
    "350 Restarting at %I64i. Send STORE or RETRIEVE to initiate transfer.\r\n", Offset);

  return FALSE;
}


/*
 * FTP: NOOP
 */
static BOOL
FTP_Noop(LPFTPUSER lpUser,
         IO_STRING *Args)
{
	//  NOOP Always succeeds

	// don't display server status during a transfer, just when idle
	if (!lpUser->DataChannel.bActive && (lpUser->Connection.dwStatus & U_IDENTIFIED))
	{
		MaybeDisplayStatus(lpUser, _T("200-"));
	}

	FormatString(&lpUser->CommandChannel.Out, _TEXT("200 NOOP command successful.\r\n"));
	return FALSE;
}





/*

  FTP: SIZE <filename>

  */
static BOOL FTP_Size(LPFTPUSER lpUser, IO_STRING *Args)
{
  VIRTUALPATH          Path;
  LPFILEINFO           lpFileInfo;
  LPTSTR               tszFileName, tszPath;
  DWORD                dwError;
  BOOL                 bResult = FALSE;
  UINT64               u64Size;
  MOUNT_DATA           MountData;

  tszFileName  = GetStringIndex(Args, STR_ALL);
  //  Copy virtual path
  PWD_Reset(&Path);
  PWD_Copy(&lpUser->CommandChannel.Path, &Path, FALSE);

  if ((tszPath = PWD_CWD2(lpUser->UserFile, &Path, tszFileName, lpUser->hMountFile, &MountData, EXISTS|VIRTUAL_PWD, lpUser, _T("SIZE"), Args)))
  {
	  //  Get file attrbiutes
	  if (GetFileInfo(tszPath, &lpFileInfo))
	  {
		  if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		  {
			  dwError = IO_INVALID_FILENAME;
		  }
		  else
		  {
			  bResult = TRUE;
			  u64Size = lpFileInfo->FileSize;
		  }
		  CloseFileInfo(lpFileInfo);
	  }
	  else
	  {
		  dwError = GetLastError();
	  }
	  PWD_Free(&Path);
  }
  else
  {
	  dwError = GetLastError();
  }

  if (bResult)
  {
	  FormatString(&lpUser->CommandChannel.Out, _TEXT("213 %I64u\r\n"), u64Size);
	  return FALSE;
  }

  //  Error reading file size
  FormatString(&lpUser->CommandChannel.Out,
	  _TEXT("550 %5T%s%0T: %2T%E.%0T\r\n"), tszFileName, dwError);
  return TRUE;
}



/*

  FTP: MDTM <filename>
       MDTM YYYYMMDDHHMMSS filename
*/
static BOOL FTP_FileTime(LPFTPUSER lpUser, IO_STRING *Args)
{
  WIN32_FILE_ATTRIBUTE_DATA  FileAttributes;
  VIRTUALPATH                Path;
  LPTSTR                     tszTime, tszFileName, tszPath, tszSlash;
  DWORD                      dwError, dwLen;
  SYSTEMTIME                 SystemTime;
  LPFILEINFO                 lpFileInfo, lpParentInfo;
  FILETIME                   FileTime;
  HANDLE                     hFile;
  MOUNT_DATA                 MountData;


  // can't have 0 arguments since MDTM requires at least 1, checked in FTP_Command()
  if (GetStringItems(Args) == 1)
  {
	  tszTime = NULL;
	  tszFileName = GetStringIndex(Args, 0);
  }
  else
  {
	  tszTime = GetStringIndexStatic(Args, 0);
	  // validate time format: YYYYMMDDHHMMSS filename
	  if ((_tcsnlen(tszTime, 15) != 14) ||
		  (6 != _stscanf_s(tszTime, "%4d%2d%2d%2d%2d%2d",
		  &SystemTime.wYear, &SystemTime.wMonth, &SystemTime.wDay,
		  &SystemTime.wHour, &SystemTime.wMinute, &SystemTime.wSecond)) ||
		  (!SystemTimeToFileTime(&SystemTime, &FileTime)))
	  {
		  // not a valid format... assume it's referring to a file...
		  tszTime = NULL;
		  tszFileName = GetStringRange(Args, 0, STR_END);
	  }
	  else
	  {
		  // it's a valid time format
		  tszFileName = GetStringRange(Args, 1, STR_END);
	  }
  }


  //  Copy virtual path
  PWD_Reset(&Path);
  PWD_Copy(&lpUser->CommandChannel.Path, &Path, FALSE);
	  
  // resolve the relative path to an absolute path, this updates
  // the Path variable as well
  dwError = NO_ERROR;
  if (tszPath = PWD_CWD2(lpUser->UserFile, &Path, tszFileName, lpUser->hMountFile, &MountData, EXISTS|VIRTUAL_PWD, lpUser, _T("MDTM"), Args))
  {
	  if (!tszTime)
	  {
		  // just read and return the time
		  if (!GetFileAttributesEx(tszPath, GetFileExInfoStandard, &FileAttributes) ||
			  !FileTimeToSystemTime(&FileAttributes.ftLastWriteTime, &SystemTime))
		  {
			  dwError = GetLastError();
		  }
		  else
		  {
			  FormatString(&lpUser->CommandChannel.Out,
				  _TEXT("213 %4d%02d%02d%02d%02d%02d\r\n"),
				  SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay,
				  SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);
			  // early exit!
			  PWD_Free(&Path);
			  return FALSE;
		  }
	  }
	  else if (GetVfsParentFileInfo(lpUser->UserFile, lpUser->hMountFile, &Path, &lpParentInfo, FALSE))
	  {
		  if (GetFileInfo(tszPath, &lpFileInfo))
		  {
			  do {
				  if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				  {
					  // not allowed to set filetime on directories...
					  dwError = IO_INVALID_ARGUMENTS;
					  break;
				  }
				  if (FtpSettings.bEnableTimeStampOnLastUpload &&
					  (lpUser->FtpVariables.vpLastUpload.l_RealPath == Path.l_RealPath) &&
					  (lpFileInfo->dwUploadTimeInMs == lpUser->FtpVariables.dwLastUploadUpTime) &&
					  lpUser->FtpVariables.vpLastUpload.RealPath &&
					  !stricmp(lpUser->FtpVariables.vpLastUpload.RealPath, Path.RealPath))
				  {
					  // special exception case!
					  dwError = NO_ERROR;
					  break;
				  }
				  if (Access(lpUser->UserFile, lpParentInfo, _I_WRITE) &&
					  Access(lpUser->UserFile, lpFileInfo, _I_WRITE) &&
					  ((Access(lpUser->UserFile, lpFileInfo, _I_OWN) &&
					  ! PathCheck(lpUser->UserFile, Path.pwd, "TimeStampOwn")) ||
					  ! PathCheck(lpUser->UserFile, Path.pwd, "TimeStamp")))
				  {
					  dwError = NO_ERROR;
					  break;
				  }
				  dwError = GetLastError();
			  } while (0);
			  CloseFileInfo(lpFileInfo);
		  }
		  else dwError = GetLastError();
		  CloseFileInfo(lpParentInfo);
	  }
	  else dwError = GetLastError();
  }
  else dwError = GetLastError();

  if (!dwError)
  {
	  // try to set the time
	  hFile = CreateFile(tszPath, GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ,
		  NULL, OPEN_EXISTING, 0, NULL);
	  if (hFile == INVALID_HANDLE_VALUE)
	  {
		  dwError = GetLastError();
	  }
	  else
	  {
		  if (!SetFileTime(hFile, NULL, NULL, &FileTime))
		  {
			  dwError = GetLastError();
		  }
		  else
		  {
			  // mark directory as changed
			  dwLen = _tcslen(tszPath);
			  tszSlash = _tcsrchr(tszPath, _T('\\'));

			  if (tszSlash && dwLen > 3)
			  {
				  *tszSlash = 0;
			  }
			  MarkDirectory(tszPath);
			  MarkVirtualDir(&Path, lpUser->hMountFile);

			  FormatString(&lpUser->CommandChannel.Out,
				  _TEXT("253 Date/time changed okay.\r\n"));
		  }
		  CloseHandle(hFile);
	  }
  }

  if (tszPath)
  {
	  PWD_Free(&Path);
  }

  if (!dwError) return FALSE;

  FormatString(&lpUser->CommandChannel.Out,
	  _TEXT("550 %5T%s%0T: %2T%E.%0T\r\n"), tszFileName, dwError);
  return TRUE;
}



/*

  FTP: TYPE <type>

  */
static BOOL FTP_TransferType(LPFTPUSER lpUser, IO_STRING *Args)
{
  LPTSTR  tszEncoding;

  tszEncoding  = GetStringIndexStatic(Args, 0);

  switch (_totupper(tszEncoding[0]))
  {
  case _TEXT('A'):
    //  ASCII (7bit, translation - not supported due inefficiency, just emulated)
    FormatString(&lpUser->CommandChannel.Out,
      _TEXT("%s"), _TEXT("200 Type set to A.\r\n"));
    lpUser->DataChannel.bEncoding  = ASCII;
    return FALSE;

  case _TEXT('I'):
    //  Binary (Image, no translation)
    FormatString(&lpUser->CommandChannel.Out,
      _TEXT("%s"), _TEXT("200 Type set to I.\r\n"));
    lpUser->DataChannel.bEncoding  = BINARY;
    return FALSE;
  }
  //  Unknown translation
  FormatString(&lpUser->CommandChannel.Out,
	  _TEXT("504 %5TTYPE%0T: %2TCommand not implemented for that parameter%0T.\r\n"));
  return TRUE;
}





static BOOL FTP_FileDelete(LPFTPUSER lpUser, IO_STRING *Args)
{
  LPTSTR  tszFileName;
  BOOL  bResult;

  tszFileName  = GetStringIndex(Args, STR_ALL);

  bResult  = Delete_File(lpUser, &lpUser->CommandChannel.Path, lpUser->UserFile,
    lpUser->hMountFile, tszFileName, &lpUser->CommandChannel.ErrorCode);
  if (! bResult)
  {
    //  File successfully deleted
    FormatString(&lpUser->CommandChannel.Out,
      _TEXT("%s"), _TEXT("250 DELE command successful.\r\n"));
    return FALSE;
  }
  //  Error while deleting file
  FormatString(&lpUser->CommandChannel.Out,
    _TEXT("550 %5T%s%0T: %2T%E.%0T\r\n"), tszFileName, lpUser->CommandChannel.ErrorCode);
  return TRUE;
}





/*

  FTP: QUIT

  */
static BOOL FTP_Quit(LPFTPUSER lpUser, IO_STRING *Args)
{
  LPTSTR  tszBasePath, tszFileName;
  BOOL  bShowGoodBye;

  bShowGoodBye  = TRUE;
  tszBasePath  = Service_MessageLocation(lpUser->Connection.lpService);
  //  Show logout message
  if (tszBasePath)
  {
    if (aswprintf(&tszFileName, _TEXT("%s\\LogOut"), tszBasePath))
    {
      bShowGoodBye  = MessageFile_Show(tszFileName, &lpUser->CommandChannel.Out,
        lpUser, DT_FTPUSER, "221-", "221 ");
      Free(tszFileName);
    }
    FreeShared(tszBasePath);
  }
  if (bShowGoodBye) FormatString(&lpUser->CommandChannel.Out, _TEXT("%s"), _TEXT("221 Goodbye\r\n"));
  lpUser->Connection.dwStatus  |= U_LOGOFF;

  return FALSE;
}






/*

  FTP: RETR <filename>

  */
static BOOL FTP_TransferRetrieve(LPFTPUSER lpUser, IO_STRING *Args)
{
	LPTSTR     tszFileName, tszRealName;
	MOUNT_DATA MountData;
	LPFILEINFO lpFileInfo;
	BOOL       bOK, bCount;

	bOK = FALSE;
	bCount = FALSE;
	lpFileInfo = NULL;
	tszFileName = GetStringIndex(Args, STR_ALL);

	do {
		//  Initialize path
		PWD_Copy(&lpUser->CommandChannel.Path, &lpUser->DataChannel.File, FALSE);

		//  Resolve file
		tszRealName = PWD_CWD2(lpUser->UserFile, &lpUser->DataChannel.File, tszFileName, lpUser->hMountFile, &MountData, EXISTS|TYPE_FILE|VIRTUAL_PWD,
			lpUser, _T("RETR"), Args);
		if (!tszRealName) break;

		if (UserFile_DownloadBegin(lpUser->UserFile)) break;
		bCount = TRUE;

		//  Check permissions
		if (!GetFileInfo(tszRealName, &lpFileInfo)) break;

		if (!Access(lpUser->UserFile, lpFileInfo, _I_READ)) break;

		if (PathCheck(lpUser->UserFile, lpUser->DataChannel.File.pwd, "Download")) break;

		//  Open file
		if (ioOpenFile(&lpUser->DataChannel.IoFile, lpUser->Connection.dwUniqueId, tszRealName, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE, OPEN_EXISTING)) break;

		bOK = TRUE;
	} while (0);

	if (!bOK) lpUser->DataChannel.dwLastError  = GetLastError();

	if (lpFileInfo) CloseFileInfo(lpFileInfo);

	if (!bOK)
	{
		//  Deinitialize transfer
		FTP_Data_Close(lpUser);
		if (bCount) UserFile_DownloadEnd(lpUser->UserFile);
		//  Show error message
		FormatString(&lpUser->CommandChannel.Out, 
			_TEXT("550 %5T%s%0T: %2T%E.%0T\r\n"), tszFileName, lpUser->DataChannel.dwLastError);
		PWD_Free(&lpUser->DataChannel.File);
		return TRUE;
	}

	lpUser->DataChannel.bDirection  = SEND;
	//  Initialize transfer
	if (! FTP_Data_Init_Transfer(lpUser, _tcsrchr(lpUser->DataChannel.File.pwd, _TEXT('/')) + 1))
	{
		return FTP_Data_Begin_Transfer(lpUser);
	}
	return TRUE;
}






/*

  FTP: STOR <filename>

  */
static BOOL FTP_TransferStore(LPFTPUSER lpUser, IO_STRING *Args)
{
	LPTSTR       tszUploadFileName, tszFileName;
	BOOL         bResult, bBegin;
	MOUNT_DATA   MountData;

	tszUploadFileName  = GetStringIndex(Args, STR_ALL);

	PWD_Copy(&lpUser->CommandChannel.Path, &lpUser->DataChannel.File, FALSE);

	tszFileName  = PWD_CWD2(lpUser->UserFile, &lpUser->DataChannel.File, tszUploadFileName, lpUser->hMountFile, &MountData, VIRTUAL_PWD, lpUser, _T("STOR"), NULL);
	if ( ! tszFileName || UserFile_UploadBegin(lpUser->UserFile) )
	{
		lpUser->DataChannel.dwLastError = GetLastError();
		bResult = TRUE;
		bBegin  = FALSE;
	}
	else 
	{
		bBegin = TRUE;
		if (lpUser->DataChannel.dwResumeOffset[0] || lpUser->DataChannel.dwResumeOffset[1])
		{
			//  Resume has been requested
			bResult  = Upload_Resume(lpUser, &lpUser->CommandChannel.Path, &lpUser->DataChannel, lpUser->Connection.dwUniqueId, lpUser->UserFile,
				lpUser->hMountFile, tszUploadFileName, FTP_Event_OnUploadResume, lpUser);
		}
		else
		{
			//  Upload/Overwrite
			bResult  = Upload(lpUser, &lpUser->CommandChannel.Path, &lpUser->DataChannel, lpUser->Connection.dwUniqueId, lpUser->UserFile,
				lpUser->hMountFile, tszUploadFileName, FTP_Event_OnUpload, lpUser);
		}
	}

	//  Initialize transfer
	if (!bResult)
	{
		if ( ! FTP_Data_Init_Transfer(lpUser, _tcsrchr(lpUser->DataChannel.File.pwd, _TEXT('/')) + 1) )
		{
			FTP_Data_Begin_Transfer(lpUser);
			return FALSE;
		}
		// we've already closed the data connection and reported an error at this point...
	}
	else
	{
		//  Could not initialize upload
		FTP_Data_Close(lpUser);
	}

	if (bBegin)
	{
		// we acquired the lock and need to release it since we closed the data connection without it being listed as active...
		UserFile_UploadEnd(lpUser->UserFile);
	}

	if (bResult)
	{
		// only append an error condition if one not already done...
		FormatString(&lpUser->CommandChannel.Out, _TEXT("550 %5T%s%0T: %2T%E.%0T\r\n"), tszUploadFileName, lpUser->DataChannel.dwLastError);
	}
	return TRUE;
}




/*

  FTP: APPE <filename>

  */
static BOOL FTP_TransferAppend(LPFTPUSER lpUser, IO_STRING *Args)
{
  //  Resume from eof
  lpUser->DataChannel.dwResumeOffset[0]  = (DWORD)-1;
  lpUser->DataChannel.dwResumeOffset[1]  = (DWORD)-1;

  return FTP_TransferStore(lpUser, Args);
}






/*

  FTP: CWD <path>

  */
static BOOL FTP_DirectoryChange(LPFTPUSER lpUser, IO_STRING *Args)
{
  LPTSTR  tszPath, tszFileName;
  DWORD   dwRealPath;

  tszFileName  = GetStringIndex(Args, STR_ALL);

  tszPath = PWD_CWD2(lpUser->UserFile, &lpUser->CommandChannel.Path, tszFileName, lpUser->hMountFile, NULL, EXISTS|TYPE_DIRECTORY|VIRTUAL_PWD, lpUser, _T("CWD"), Args);

  if (tszPath || lpUser->CommandChannel.Path.lpVirtualDirEvent)
  {
	  // update shared memory
	  UpdateClientData(DATA_CHDIR, lpUser->Connection.dwUniqueId, &lpUser->CommandChannel.Path);

	  if (tszPath && ! lpUser->FtpVariables.bSingleLineMode && (dwRealPath = lpUser->CommandChannel.Path.l_RealPath))
	  {
		  if ((tszFileName = (LPSTR)Allocate("FTP:DirectoryChange:Message", dwRealPath + 17)))
		  {
			  CopyMemory(tszFileName, tszPath, dwRealPath);
			  tszFileName[dwRealPath++]  = _TEXT('\\');

			  //  Show .ioFTPD.cwd message file which can contain cookies
			  _tcscpy(&tszFileName[dwRealPath], _TEXT(".ioFTPD.cwd"));
			  MessageFile_Show(tszFileName, &lpUser->CommandChannel.Out, lpUser, DT_FTPUSER, _TEXT("250-"), NULL);

			  //  Show plain text file
			  _tcscpy(&tszFileName[dwRealPath], _TEXT(".ioFTPD.message"));
			  TextFile_Show(tszFileName, &lpUser->CommandChannel.Out, _TEXT("250-"));
			  Free(tszFileName);
		  }
	  }

	  MaybeDisplayStatus(lpUser, _T("250-"));

	  //  Success message
	  FormatString(&lpUser->CommandChannel.Out,
		  _TEXT("%s"), _TEXT("250 CWD command successful.\r\n"));
	  return FALSE;
  }

  //  Show error message
  FormatString(&lpUser->CommandChannel.Out,
	  _TEXT("550 %5T%s%0T: %2T%E.%0T\r\n"), tszFileName, GetLastError());
  return TRUE;
}






/*

  FTP: CDUP

  */
static BOOL FTP_DirectoryChangeUp(LPFTPUSER lpUser, IO_STRING *Args)
{
  LPTSTR  tszPath;

  tszPath = PWD_CWD2(lpUser->UserFile, &lpUser->CommandChannel.Path, _T(".."), lpUser->hMountFile, NULL, EXISTS|TYPE_DIRECTORY|VIRTUAL_PWD, lpUser, _T("CDUP"), Args);

  if (tszPath || lpUser->CommandChannel.Path.lpVirtualDirEvent)
  {
	  //  Update shared memory
	  UpdateClientData(DATA_CHDIR, lpUser->Connection.dwUniqueId, &lpUser->CommandChannel.Path);
	  //  Success message
	  FormatString(&lpUser->CommandChannel.Out,
		  _TEXT("%s"), _TEXT("250 CWD command successful.\r\n"));
	  return FALSE;
  }
  //  Show error message (only fails in this case)
  FormatString(&lpUser->CommandChannel.Out,
	  _TEXT("550 %5T..%0T: %2TNo such file or directory.%0T\r\n"));
  return TRUE;
}





/*

  FTP: PWD

  */
static BOOL FTP_DirectoryPrint(LPFTPUSER lpUser, IO_STRING *Args)
{
	LPTSTR tszPath;
	DWORD  dwLen;
	TCHAR tszBuf[_MAX_PATH+1];

	if (lpUser->FtpVariables.bKeepLinksInPath)
	{
		tszPath = lpUser->CommandChannel.Path.Symbolic;
		dwLen   = lpUser->CommandChannel.Path.Symlen;
	}
	else
	{
		tszPath = lpUser->CommandChannel.Path.pwd;
		dwLen   = lpUser->CommandChannel.Path.len;
	}

	_tcsncpy_s(tszBuf, sizeof(tszBuf), tszPath, _TRUNCATE);
	if ((dwLen > 1) && (tszBuf[dwLen-1] == '/'))
	{
		tszBuf[dwLen-1] = 0;
	}

	//  Show current working directory
	FormatString(&lpUser->CommandChannel.Out, _TEXT("257 \"%s\" is current directory.\r\n"), tszBuf);
	return FALSE;
}







/*

  FTP: PORT <x1,x2,x3,x4,y1,y2>

  */
static BOOL FTP_TransferActive(LPFTPUSER lpUser, IO_STRING *Args)
{
	struct sockaddr_in  SockConnectAddr;
	ULONG        lConnectAddress;
	SOCKET       Socket;
	LPTSTR       tszHost;
	TCHAR       *tpComma[2], *tpNull;
	ULONG        ulPort[2], ulIp[4];
	BOOL         bResult;
	UINT         RetryCount;
	UINT         uFlags;
	INT          iErr;

	//  Get argument string
	tszHost    = GetStringIndexStatic(Args, 0);
	Socket     = INVALID_SOCKET;
	//  Read string using memchr, strtol and basic comparison
	if ((ulIp[0] = _tcstoul(tszHost, &tpComma[0], 10)) < 256 &&
		tpComma[0] != tszHost && tpComma[0][0] == _TEXT(',') &&
		(ulIp[1] = _tcstoul(++tpComma[0], &tpComma[1], 10)) < 256 &&
		tpComma[1] != tpComma[0] && tpComma[1][0] == _TEXT(',') &&
		(ulIp[2] = _tcstoul(++tpComma[1], &tpComma[0], 10)) < 256 &&
		tpComma[0] != tpComma[1] && tpComma[0][0] == _TEXT(',') &&
		(ulIp[3] = _tcstoul(++tpComma[0], &tpComma[1], 10)) < 256 &&
		tpComma[1] != tpComma[0] && tpComma[1][0] == _TEXT(',') &&
		(ulPort[0] = _tcstoul(++tpComma[1], &tpComma[0], 10)) < 256 &&
		tpComma[0] != tpComma[1] && tpComma[0][0] == _TEXT(',') &&
		(ulPort[1] = _tcstoul(++tpComma[0], &tpNull, 10)) < 256 &&
		tpNull != tpComma[0] && tpNull[0] == _TEXT('\0'))
	{
		//  Check if datachannel has been already initialized
		if (lpUser->DataChannel.bInitialized)
		{
			//  Close socket
			ioCloseSocket(&lpUser->DataChannel.ioSocket, TRUE);
			lpUser->DataChannel.bInitialized  = FALSE;
		}
		lConnectAddress  = (((ulIp[0] * 256 + ulIp[1]) * 256) + ulIp[2]) * 256 + ulIp[3];
		//  Set connect address
		ZeroMemory(&SockConnectAddr, sizeof(struct sockaddr_in));
		SockConnectAddr.sin_addr.s_addr  = htonl(lConnectAddress);
		SockConnectAddr.sin_port    = htons((USHORT)(256 * ulPort[0] + ulPort[1]));
		SockConnectAddr.sin_family    = AF_INET;

		if (lpUser->FtpVariables.lpDenyPortAddressList && ioAddressFind(lpUser->FtpVariables.lpDenyPortAddressList, SockConnectAddr.sin_addr))
		{
			WSASetLastError(IO_BAD_FXP_ADDR);
		}
		else if ((Socket = OpenSocket()) != INVALID_SOCKET)  		  //  Create socket
		{
			//  Update socket structure
			lpUser->DataChannel.ioSocket.Socket  = Socket;
			ZeroMemory(&lpUser->DataChannel.ioSocket.Overlapped, 2 * sizeof(SOCKETOVERLAPPED));
			IoSocketInit(&lpUser->DataChannel.ioSocket);

			uFlags = 0;
			for (RetryCount = 5;RetryCount > 0;RetryCount--)
			{
				//  Bind socket to device
				bResult  = BindSocketToDevice(lpUser->Connection.lpService,
					&lpUser->DataChannel.ioSocket, NULL, NULL, BIND_DATA|BIND_REUSE|BIND_MINUS_PORT|uFlags);
				uFlags = BIND_TRY_AGAIN;

				if (! bResult)
				{
					//  Asynchronous select with timeout
					if (WSAAsyncSelectWithTimeout(&lpUser->DataChannel.ioSocket,
						FTP_DATA_CONNECT_TIMEOUT, FD_CONNECT, &lpUser->DataChannel.dwLastError))
					{
						break;
					}

					//  Connect socket
					if (WSAConnect(Socket, (struct sockaddr *)&SockConnectAddr,
						sizeof(struct sockaddr_in), NULL, NULL, NULL, NULL) == SOCKET_ERROR)
					{
						iErr = WSAGetLastError();
						if (iErr == WSAEADDRINUSE)
						{
							WSAAsyncSelectCancel(&lpUser->DataChannel.ioSocket);
							continue;
						}
						if (iErr != WSAEWOULDBLOCK) break;
					}

					//  Initialize data channel structures
					CopyMemory(&lpUser->DataChannel.Address, &SockConnectAddr, sizeof(struct sockaddr_in));
					lpUser->DataChannel.bTransferMode  = ACTIVE;
					lpUser->DataChannel.bInitialized   = TRUE;

					FormatString(&lpUser->CommandChannel.Out,
						_TEXT("%s"), _TEXT("200 PORT command successful.\r\n"));
					return FALSE;
				}
			}
		}
	}
	else WSASetLastError(IO_INVALID_ARGUMENTS);

	//  Free resources
	lpUser->DataChannel.bInitialized  = FALSE;
	lpUser->DataChannel.dwLastError    = WSAGetLastError();
	ioCloseSocket(&lpUser->DataChannel.ioSocket, TRUE);

	FormatString(&lpUser->CommandChannel.Out,
		_TEXT("501 %5TPORT%0T %2Tcommand failed: %E.%0T\r\n"), lpUser->DataChannel.dwLastError);
	return TRUE;
}








/*

  FTP: PASV

  */
static INT
FTP_TransferPassive(LPFTPUSER lpUser, IO_STRING *Args)
{
	SOCKET    Socket;
	USHORT    usPort;
	ULONG     ulAddress;
	BOOL      bError, bResult;
	INT       Try;

	if (lpUser->DataChannel.bInitialized)
	{
		//  DeInitialize data socket
		ioCloseSocket(&lpUser->DataChannel.ioSocket, TRUE);
		lpUser->DataChannel.bInitialized  = FALSE;
	}

	//  Set variables
	ZeroMemory(&lpUser->DataChannel.Address, sizeof(struct sockaddr_in));

	Try    = 30;
	bError  = TRUE;
	Socket  = INVALID_SOCKET;

	for (;--Try;)
	{
		//  Create new socket
		if (Socket == INVALID_SOCKET)
		{
			Socket  = OpenSocket();
			//  Check newly created socket
			if (Socket == INVALID_SOCKET) break;
		}
		lpUser->DataChannel.ioSocket.Socket  = Socket;
		//  Bind socket to device
		bResult  = BindSocketToDevice(lpUser->Connection.lpService,
			&lpUser->DataChannel.ioSocket, &ulAddress, &usPort, BIND_DATA|BIND_PORT);

		if (! bResult)
		{
			//  Listen socket
			if (! listen(Socket, 1))
			{
				bError  = FALSE;
				break;
			}
			//  Close socket
			ioCloseSocket(&lpUser->DataChannel.ioSocket, TRUE);
			Socket  = INVALID_SOCKET;
		}
	}

	if (! bError)
	{
		// initialize overlapped and the critical section
		ZeroMemory(&lpUser->DataChannel.ioSocket.Overlapped, 2 * sizeof(SOCKETOVERLAPPED));
		IoSocketInit(&lpUser->DataChannel.ioSocket);

		//  Asynchronous select with timeout
		if (WSAAsyncSelectWithTimeout(&lpUser->DataChannel.ioSocket, FTP_DATA_ACCEPT_TIMEOUT, FD_ACCEPT, &lpUser->DataChannel.dwLastError))
		{
			bError = TRUE;
		}
	}

	if (bError)
	{
		//  Handle error
		lpUser->DataChannel.dwLastError     = WSAGetLastError();
		//  Close socket?
		ioCloseSocket(&lpUser->DataChannel.ioSocket, TRUE);
		//  Display error to client
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("527 %5TPASV%0T %2Tcommand failed: %E.%0T\r\n"), lpUser->DataChannel.dwLastError);
		return TRUE;
	}

	if (ulAddress == INADDR_ANY) ulAddress  = lpUser->Connection.LocalAddress.sin_addr.s_addr;
	//  Initialize data channel structures
	lpUser->DataChannel.bTransferMode   = PASSIVE;
	lpUser->DataChannel.bInitialized    = TRUE;
	//  Convert address to host byte order
	ulAddress  = ntohl(ulAddress);

	//  Display success message to client
	FormatString(&lpUser->CommandChannel.Out, _TEXT("227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)\r\n"),
		ulAddress >> 24, (ulAddress >> 16) & 0xFF,
		(ulAddress >> 8) & 0xFF, ulAddress & 0xFF,
		usPort >> 8, usPort & 0xFF);
	return FALSE;
}





/*

  FTP_TransferSecurePassive() - Enables SSL Connect

  */
static BOOL FTP_TransferSecurePassive(LPFTPUSER lpUser, IO_STRING *Args)
{
  lpUser->DataChannel.bProtectedConnect  = TRUE;
  //  Passive mode
  return FTP_TransferPassive(lpUser, Args);
}


/*
 * FTP: PASS <password>
 */
static BOOL
FTP_LoginPassword(LPFTPUSER lpUser,
                  IO_STRING *arg)
{
	LPTSTR  passwd, filename, tszBasePath, tszAdmin;
	LPSTR  szEventName;
	INT    iErrCode;
	INT32  Uid;
	CHAR   szObscuredHost[MAX_HOSTNAME];
	CHAR   szObscuredIP[MAX_HOSTNAME];


	passwd=GetStringIndexStatic(arg, 0);

	//  MuISTA
	if ((lpUser->CommandChannel.ErrorCode != NO_ERROR) ||
		Login_Second(&lpUser->UserFile,
		             passwd,
					 &lpUser->CommandChannel,
					 &lpUser->Connection,
					 &lpUser->hMountFile,
					 &lpUser->CommandChannel.ErrorCode,
					 lpUser))
	{
		if (lpUser->Connection.uLoginAttempt >= FtpSettings.dwLoginAttempts)
		{
			lpUser->Connection.dwStatus|=U_LOGOFF;
			if (LogLoginErrorP(lpUser->Connection.lpHostInfo, 8))
			{
				Putlog(LOG_ERROR,
					"Host '%s@%s' (%s) had %d failed login attempts - last user was '%s'.\r\n",
					(!lpUser->Connection.szIdent || !lpUser->Connection.szIdent[0] ? "*" : lpUser->Connection.szIdent),
					Obscure_IP(szObscuredIP, &lpUser->Connection.ClientAddress.sin_addr),
					(lpUser->Connection.szHostName ? Obscure_Host(szObscuredHost, lpUser->Connection.szHostName) : ""),
					lpUser->Connection.uLoginAttempt,
					lpUser->FtpVariables.tszUserName);
			}
		}

		szEventName = 0;
		iErrCode = lpUser->CommandChannel.ErrorCode;

		switch(lpUser->CommandChannel.ErrorCode)
		{
		case IO_INVALID_ARGUMENTS:
			if (!lpUser->FtpVariables.bHaveUserCmd)
			{
				FormatString(&lpUser->CommandChannel.Out,
					_TEXT("503 Login with USER first.\r\n"));
				return TRUE;
			}
			iErrCode = ERROR_PASSWORD;
			break;

		case ERROR_USER_BANNED:
			szEventName = "OnBannedLogin";
			// fallthrough
		case ERROR_USER_EXPIRED:
			if (!szEventName) szEventName = "OnExpiredLogin";
			// fallthrough
		case ERROR_USER_DELETED:
			if (!szEventName) szEventName = "OnDeletedLogin";

			if (lpUser->UserFile)
			{
				if (!User_CheckPassword(passwd, lpUser->UserFile->Password))
				{
					// user had the right password, so run event on the userfile
					if (Event_Run("Events", szEventName, NULL, lpUser->UserFile, DT_USERFILE, "530-"))
					{
						iErrCode = ERROR_PASSWORD;
					}
				}
				else
				{
					iErrCode = ERROR_PASSWORD;
				}

			}
			break;

		case ERROR_SERVER_SINGLE:
			Uid = FtpSettings.iSingleCloseUID;
			if ( ( Uid < 0 ) || ! ( tszAdmin = Uid2User(Uid) ) )  tszAdmin = _T("<unknown>");
			FormatString(&lpUser->CommandChannel.Out, _TEXT("530-ADMIN MODE CLOSURE BY '%s'.\r\n"), tszAdmin);

		case ERROR_SERVER_CLOSED:
			Event_Run("Events", "OnClosedLogin", NULL, lpUser, DT_FTPUSER, "530-");
			break;

			//  Login failed
		case ERROR_SHUTTING_DOWN:
		case ERROR_STARTING_UP:
		case ERROR_SERVICE_LOGINS:
		case ERROR_USER_LOGINS:
		case ERROR_IP_LOGINS:
		case ERROR_USER_IP_LOGINS:
		case ERROR_CLASS_LOGINS:
		case ERROR_HOME_DIR:
		case ERROR_VFS_FILE:
			//  Fatal error, force logout
			lpUser->Connection.dwStatus|=U_LOGOFF;
			break;

		case ERROR_PASSWORD:
			break;

		case ERROR_IDENT_FAILURE:
		case ERROR_CLIENT_HOST:
			if (!FtpSettings.bShowHostMaskError)
			{
				iErrCode = ERROR_PASSWORD;
			}
			break;

		default:
			// case IO_NO_ACCESS: not authorized for service
			// case ERROR_USER_NOT_FOUND:
			// Return generic bad password to not leak any info...
			iErrCode = ERROR_PASSWORD;
			break;
		}

		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("530 Login failed: %E.\r\n"),
			iErrCode);

		lpUser->CommandChannel.ErrorCode = NO_ERROR;
		lpUser->FtpVariables.bHaveUserCmd = 0;

		if (lpUser->UserFile)
		{
			UserFile_Close(&lpUser->UserFile, 0);
		}
		if (lpUser->hMountFile)
		{
			MountFile_Close(lpUser->hMountFile);
			lpUser->hMountFile = NULL;
		}

		return TRUE;
    }

	// enable theme here
	if (lpUser->UserFile->Theme > 0)
	{
		if (lpUser->FtpVariables.lpTheme = LookupTheme(lpUser->UserFile->Theme))
		{
			lpUser->FtpVariables.iTheme = lpUser->UserFile->Theme;
			SetTheme(lpUser->FtpVariables.lpTheme);
		}
	}

	// enable theme here
	if ( (lpUser->UserFile->Theme > 0) && ! lpUser->FtpVariables.bSingleLineMode )
	{
		if (lpUser->FtpVariables.lpTheme = LookupTheme(lpUser->UserFile->Theme))
		{
			lpUser->FtpVariables.iTheme = lpUser->UserFile->Theme;
			SetTheme(lpUser->FtpVariables.lpTheme);
		}
	}

	//  Run login event
	Event_Run("Events", "OnFtpLogIn", NULL, lpUser, DT_FTPUSER, "230-");

	//  Show welcome.msg
	tszBasePath=Service_MessageLocation(lpUser->Connection.lpService);
	if(tszBasePath) {
		if(aswprintf(&filename, _TEXT("%s\\Welcome"), tszBasePath))
		{
			MessageFile_Show(filename,
				&lpUser->CommandChannel.Out,
				lpUser,
				DT_FTPUSER,
				_TEXT("230-"),
				NULL);

			Free(filename);
		}

		FreeShared(tszBasePath);
	}

	//  Show welcome
	FormatString(&lpUser->CommandChannel.Out,
		_TEXT("230 User %s logged in.\r\n"),
		Uid2User(lpUser->UserFile->Uid));
	return FALSE;
}


/*
 * FTP: USER <login>
 */
static BOOL
FTP_LoginUser(LPFTPUSER user,
              IO_STRING *args)
{
  LPTSTR a0=GetStringIndexStatic(args, 0), b0;
  LPTSTR tszBasePath, tszFileName;
  int NegativeOne = -1;
  BOOL bResult;

  if (!user->FtpVariables.bLoginRetry)
  {
	  user->Connection.uLoginAttempt++;
  }

  Login_First(&user->UserFile,
              a0,
              &user->Connection,
			  (user->CommandChannel.Socket.lpSecure ? TRUE : FALSE),
              &user->CommandChannel.ErrorCode);

  b0 = a0;
  if (!b0) b0 = _T("");
  if (b0[0] == _T('!')) b0++;
  _tcsncpy_s(user->FtpVariables.tszUserName, sizeof(user->FtpVariables.tszUserName), b0, _TRUNCATE);

  switch (user->CommandChannel.ErrorCode)
  {
  case ERROR_USER_NOT_FOUND:
	  if (user->FtpVariables.bLoginRetry == 0)
	  {
		  if (Event_Run("Events", "OnUnknownLogin", b0, user, DT_FTPUSER, "530-"))
		  {
			  user->FtpVariables.bLoginRetry = 1;
			  // try again, in case we managed to create the user...
			  bResult = FTP_LoginUser(user, args);
			  user->FtpVariables.bLoginRetry = 0;
			  return bResult;
		  }
	  }
	  // fallthrough
  case ERROR_USER_BANNED:
  case ERROR_USER_EXPIRED:
  case ERROR_USER_DELETED:
	  // now figure out what the default secure requirement is
	  if (user->CommandChannel.Socket.lpSecure)
	  {
		  // it's a secure connection already
		  break;
	  }
	  AcquireSharedLock(&user->Connection.lpService->loLock);
	  if (!user->Connection.lpService->tszRequireSecureAuth ||
		  CheckPermissions("", &NegativeOne, "", user->Connection.lpService->tszRequireSecureAuth))
	  {
		  // it's a secure connection, or it is not required by default
		  ReleaseSharedLock(&user->Connection.lpService->loLock);
		  break;
	  }
	  ReleaseSharedLock(&user->Connection.lpService->loLock);
	  // fallthrough
  case IO_ENCRYPTION_REQUIRED:
	  tszBasePath=Service_MessageLocation(user->Connection.lpService);
	  if(tszBasePath) {
		  if(aswprintf(&tszFileName, _TEXT("%s\\SecureRequired"), tszBasePath)) {
			  MessageFile_Show(tszFileName,
				  &user->CommandChannel.Out,
				  user,
				  DT_FTPUSER,
				  _TEXT("530-"),
				  NULL);
			  Free(tszFileName);
		  }
		  FreeShared(tszBasePath);
	  }
	  FormatString(&user->CommandChannel.Out,
		  _TEXT("530 Login failed: %E.\r\n"), IO_ENCRYPTION_REQUIRED);
	  // unknown users won't have an open userfile, so this catches the expired/deleted cases
	  if (user->UserFile)
	  {
		  UserFile_Close(&user->UserFile, 0);
	  }
	  user->FtpVariables.bHaveUserCmd = FALSE;
	  user->CommandChannel.ErrorCode = NO_ERROR;
	  return FALSE;
  }

  user->FtpVariables.bHaveUserCmd = TRUE;
  FormatString(&user->CommandChannel.Out,
	  _TEXT("331 Password required for %s.\r\n"),
	  a0);
  return TRUE;
}


static VOID FTP_SendShutdown(LPFTPUSER lpUser, DWORD dwLastError, INT64 i64Total, ULONG ulSslError)
{
  if (dwLastError != NO_ERROR)
  {
    WSASendDisconnect(lpUser->CommandChannel.Socket.Socket, NULL);
    CloseSocket(&lpUser->CommandChannel.Socket, FALSE);
  }
  EndClientJob(lpUser->Connection.dwUniqueId, 2);
}


static BOOL FTP_TimedOut(LPFTPUSER lpUser)
{
  IOBUFFER  Buffer[1];  
  DWORD    dwTimeOut;

  dwTimeOut  = (lpUser->Connection.dwStatus & U_IDENTIFIED ? FtpSettings.dwIdleTimeOut : FtpSettings.dwLoginTimeOut);
  //  Add message to buffer
  FormatString(&lpUser->CommandChannel.Out,
    _TEXT("421 Timeout (%d seconds): closing control connection.\r\n"), dwTimeOut / 1000);
  lpUser->Connection.dwStatus  |= U_LOGOFF;

  //  Queue new job
  if (AddExclusiveClientJob(lpUser->Connection.dwUniqueId, 2, CJOB_SECONDARY|CJOB_INSTANT, INFINITE, NULL, FTP_Cancel, NULL, lpUser))
  {
    Buffer[0].dwType    = PACKAGE_BUFFER_SEND;
    Buffer[0].buf      = lpUser->CommandChannel.Out.buf;
    Buffer[0].len      = lpUser->CommandChannel.Out.len;
    Buffer[0].dwTimeOut    = 2000;
    Buffer[0].dwTimerType  = TIMER_PER_PACKAGE;

    TransmitPackages(&lpUser->CommandChannel.Socket, Buffer, 1, NULL, FTP_SendShutdown, lpUser);
  }
  EndClientJob(lpUser->Connection.dwUniqueId, 25);
  return FALSE;
}


static DWORD FTP_Kill(LPFTPUSER lpUser, LPTIMER lpTimer)
{
  CloseSocket(&lpUser->CommandChannel.Socket, TRUE);
  return 0;
}


DWORD FTP_TimerProc(LPFTPUSER lpUser, LPTIMER lpTimer)
{
  //  Set new timer procedure (context remains same)
  lpTimer->lpTimerProc  = FTP_Kill;
  //  Queue new job
  AddClientJob(lpUser->Connection.dwUniqueId, 25, CJOB_SECONDARY, INFINITE, FTP_TimedOut, NULL, NULL, lpUser);
  return FTP_LOGOUT_TIMEOUT;
}


/*

  Find FTP command

  */
__inline
LPFTPCOMMAND FTP_FindCommand(LPIO_STRING Args)
{
  LPTSTR  tszCommand;
  DWORD  n;

  //  Linear seek through command array
  switch (GetStringIndexLength(Args, 0))
  {
  case 4:
  case 3:
    tszCommand  = GetStringIndex(Args, 0);
    _tcsupr(tszCommand);

    for (n = 0;n < sizeof(FtpCommand) / sizeof(FTPCOMMAND);n++)
    {
      if (! memcmp(FtpCommand[n].tszName, tszCommand, 4 * sizeof(TCHAR))) return &FtpCommand[n];
    }
  }
  return NULL;
}



/*

  FTP_Command() - Execute FTP command

  */
BOOL FTP_Command(LPFTPUSER lpUser)
{
  IO_STRING    Args;
  LPIOSERVICE  lpService;
  LPFTPCOMMAND lpCommand;
  IOBUFFER     Buffer[2];
  LPTSTR       tszCommand;
  DWORD        dwLen, dwError;
  BOOL         bResult, bSync;

  tszCommand=lpUser->CommandChannel.Command;

  //  Remove OOB
  if (!_tcsncmp(tszCommand, _TEXT("ÿôÿÿ"), 4)) tszCommand+=4;

  //  Synchronize userfile
  bSync = UserFile_Sync(&lpUser->UserFile);

  if (lpUser->Connection.dwStatus & U_IDENTIFIED && lpUser->FtpVariables.lpTheme)
  {
	  SetTheme(lpUser->FtpVariables.lpTheme);
  }

  if (dwConfigCounter != lpUser->FtpVariables.dwConfigCounter)
  {
	  // the configuration might have changed, update whatever we need...

	  // TODO: stash pointers to the global ref-counted strings we use FtpSetting.lStringLock around so
	  // we don't have to grab the locks below...

	  lpService = lpUser->Connection.lpService;
	  AcquireSharedLock(&lpService->loLock);
	  if (lpUser->FtpVariables.lpDenyPortAddressList != lpService->lpPortDeniedAddresses)
	  {
		  ioAddressListFree(lpUser->FtpVariables.lpDenyPortAddressList);
		  if (lpService->lpPortDeniedAddresses)
		  {
			  InterlockedIncrement(&lpService->lpPortDeniedAddresses->lReferenceCount);
			  lpUser->FtpVariables.lpDenyPortAddressList = lpService->lpPortDeniedAddresses;
		  }
		  else
		  {
			  lpUser->FtpVariables.lpDenyPortAddressList = NULL;
		  }
	  }
	  ReleaseSharedLock(&lpService->loLock);
  }

  // if there is an active transfer in place, we want to save the fatal error
  // replies so don't waste it on a NOOP during transfer and thus prevent
  // an ABOR command from being sent...  Also, this stuff only applies to
  // logged in users.
  if (!(lpUser->Connection.dwStatus & U_LOGOFF) && !lpUser->DataChannel.bActive &&
	  (lpUser->Connection.dwStatus & U_IDENTIFIED) && lpUser->UserFile && _tcsicmp(tszCommand, _T("QUIT")))
  {
	  // testing these without the lock but that's ok
	  if ((FtpSettings.dwShutdownTimeLeft > 0) && (FtpSettings.dwShutdownCID != lpUser->Connection.dwUniqueId)
		  && HasFlag(lpUser->UserFile, _T("M")))
	  {
		  FormatString(&lpUser->CommandChannel.Out, _TEXT("421-\r\n421-\r\n421-%2TServer is shutting down.%0T\r\n421-\r\n421-\r\n"));
		  lpUser->Connection.dwStatus  |= U_LOGOFF;
	  }
	  else if (bSync || lpUser->UserFile->DeletedOn)
	  {
		  //  User has been deleted, how cool...
		  lpUser->Connection.dwStatus  |= U_LOGOFF;
		  Event_Run("Events", "OnDeletedKick", NULL, lpUser, DT_FTPUSER, "421-");
	  }
	  else if (lpUser->UserFile->ExpiresAt && (time((time_t) NULL) > lpUser->UserFile->ExpiresAt))
	  {
		  //  Account has expired...
		  lpUser->Connection.dwStatus  |= U_LOGOFF;
		  Event_Run("Events", "OnExpiredKick", NULL, lpUser, DT_FTPUSER, "421-");
	  }
	  // just testing tszCloseMsg for zero here which means server isn't closed, not using target...
	  else if (FtpSettings.tmSiteClosedOn)
	  {
		  while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();
		  if ( FtpSettings.tmSiteClosedOn
			   && (  (( FtpSettings.iSingleCloseUID != -1 ) && ( FtpSettings.iSingleCloseUID != lpUser->UserFile->Uid ) && 
			          ( FtpSettings.iServerSingleExemptUID != lpUser->UserFile->Uid ))
				   || ( FtpSettings.bKickNonExemptOnClose && HavePermission(lpUser->UserFile, FtpSettings.tszCloseExempt)) ) )
		  {
			  // server closed to all but user who closed site, or non-exempt user on normally closed site
			  InterlockedExchange(&FtpSettings.lStringLock, FALSE);
			  Event_Run("Events", "OnClosedKick", NULL, lpUser, DT_FTPUSER, "421-");
			  lpUser->Connection.dwStatus  |= U_LOGOFF;
		  }
		  else
		  {
			  InterlockedExchange(&FtpSettings.lStringLock, FALSE);
		  }
	  }

	  if (lpUser->Connection.dwStatus & U_LOGOFF)
	  {
		  FormatString(&lpUser->CommandChannel.Out, _TEXT("421 %2TService not available, closing control connection.%0T\r\n"));
	  }
  }

  if (!(lpUser->Connection.dwStatus & U_LOGOFF) &&
	  !SplitString(tszCommand, &Args))
  {
	  if(lpCommand=FTP_FindCommand(&Args))
	  {
		  if (lpCommand->lArgMax >= 0 &&
			  ((long) GetStringItems(&Args)) > lpCommand->lArgMax + 1)
		  {
			  //  Invalid arguments
			  FormatString(&lpUser->CommandChannel.Out,
				  _TEXT("501 '%5T%s%0T': %5TSyntax error in parameters or arguments%0T.\r\n"),
				  lpCommand->tszName);
			  lpCommand  = NULL;
		  }
		  else if(lpCommand->lArgMin > 0 &&
			  ((long) GetStringItems(&Args)) < lpCommand->lArgMin + 1)
		  {
			  //  Missing arguments
			  FormatString(&lpUser->CommandChannel.Out,
				  _TEXT("501 '%5T%s%0T': %2TNot enough parameters.%0T\r\n"),
				  lpCommand->tszName);
			  lpCommand  = NULL;
		  }
		  else if(!(lpUser->Connection.dwStatus & U_IDENTIFIED))
		  {
			  if (!(lpCommand->dwFlags & LOGIN_CMD))
			  {
				  //  Not a pre login command
				  FormatString(&lpUser->CommandChannel.Out,
					  _TEXT("%s"),
					  _TEXT("530 Please log in first.\r\n"));
				  lpCommand  = NULL;
			  }
		  }
		  else if(Config_Get_Permission(&IniConfigFile, _TEXT("FTP_Command_Permissions"),
			  lpCommand->tszName,
			  lpUser->UserFile))
		  {
			  //  No permission to command
			  FormatString(&lpUser->CommandChannel.Out,
				  _TEXT("500 '%5T%s%0T': %2TCommand not understood.%0T\r\n"),
				  GetStringIndexStatic(&Args, 0));
			  lpCommand  = NULL;
		  }
		  else
		  {
			  if (lpUser->DataChannel.bActive && !(lpCommand->dwFlags & XFER_CMD))
			  {
				  FormatString(&lpUser->CommandChannel.Out,
					  _TEXT("550 %2TActive transfer in progress, terminate transfer with ABOR before proceeding.%0T\r\n"));
				  lpCommand  = NULL;
			  }
			  else if (! (lpCommand->dwFlags & OTHER_CMD))
			  {
				  FormatString(&lpUser->CommandChannel.Out,
					  _TEXT("530 %2TAlready logged in.%0T\r\n"));
				  lpCommand  = NULL;
			  }
		  }

		  if (lpCommand)
		  {
			  PushString(&Args, 1);
			  //  Fire pre-command events
			  dwLen = lpUser->CommandChannel.Out.len;
			  bResult=Event_Run(_TEXT("FTP_Pre-Command_Events"),
				  lpCommand->tszName,
				  tszCommand,
				  lpUser,
				  DT_FTPUSER, NULL);
			  if(!bResult && !lpCommand->lpProc(lpUser, &Args))
			  {
				  // first test to see if it's worth acquiring the lock to actually use the value...
				  if (FtpSettings.tszIdleIgnore)
				  {
					  while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();
					  if (!FtpSettings.tszIdleIgnore || (!_tcsstr(FtpSettings.tszIdleIgnore, lpCommand->tszName)))
					  {
						  lpUser->CommandChannel.Idle  = GetTickCount();
					  }
					  InterlockedExchange(&FtpSettings.lStringLock, FALSE);
				  }
				  if (lpCommand->lpProc == FTP_LoginPassword)
				  {
					  UpdateClientData(DATA_ACTION,
						  lpUser->Connection.dwUniqueId,
						  _T("PASS ********"),
						  lpUser->CommandChannel.Idle);
				  }
				  else if (lpCommand->lpProc == FTP_Noop)
				  {
					  UpdateClientData(DATA_NOOP,
						  lpUser->Connection.dwUniqueId,
						  tszCommand,
						  lpUser->CommandChannel.Idle);
				  }
				  else
				  {
					  UpdateClientData(DATA_ACTION,
						  lpUser->Connection.dwUniqueId,
						  tszCommand,
						  lpUser->CommandChannel.Idle);
				  }

				  //  Fire post-command events
				  Event_Run(_TEXT("FTP_Post-Command_Events"),
					  lpCommand->tszName,
					  tszCommand,
					  lpUser,
					  DT_FTPUSER, NULL);
			  }
			  else if (dwLen == lpUser->CommandChannel.Out.len)
			  {
				  // the command failed but didn't print an error message
				  dwError = GetLastError();
				  if (dwError == IO_SCRIPT_FAILURE)
				  {
					  FormatString(&lpUser->CommandChannel.Out,
						  _TEXT("500 '%5T%s%0T': %2TCommand failed. (pre-cmd-event script)%0T\r\n"),
						  lpCommand->tszName);
				  }
				  else
				  {
					  FormatString(&lpUser->CommandChannel.Out,
						  _TEXT("500 '%5T%s%0T': %2TCommand failed.%0T\r\n"),
						  lpCommand->tszName);
				  }
			  }
			  PullString(&Args, 1);
		  }
	}
	else
    {
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("500 '%5T%s%0T': %2TCommand not understood%0T\r\n"),
			GetStringIndexStatic(&Args, 0));
	}

	FreeString(&Args);
  }

  if (lpUser->Connection.dwStatus & U_MAKESSL)
  {
	  lpUser->Connection.dwStatus  -= U_MAKESSL;
	  //  Initialize transfer structures
	  Buffer[0].dwType      = PACKAGE_BUFFER_SEND;
	  Buffer[0].len         = lpUser->CommandChannel.Out.len;
	  Buffer[0].buf         = lpUser->CommandChannel.Out.buf;
	  Buffer[0].dwTimerType = TIMER_PER_PACKAGE;
	  Buffer[0].dwTimeOut   = SEND_TIMEOUT;
	  Buffer[1].dwType      = PACKAGE_SSL_ACCEPT;
	  Buffer[1].dwTimeOut   = SSL_TIMEOUT;
	  Buffer[1].dwTimerType = TIMER_PER_PACKAGE;

	  if(AddExclusiveClientJob(lpUser->Connection.dwUniqueId, 2, CJOB_PRIMARY|CJOB_SECONDARY|CJOB_INSTANT, INFINITE, NULL, FTP_Cancel, NULL, lpUser))
	  {
		  TransmitPackages(&lpUser->CommandChannel.Socket,
			               Buffer,
						   2,
						   NULL,
						   FTP_SSLAccept,
						   lpUser);
	  }
  }
  else
  {
	  if (lpUser->CommandChannel.Out.len)
	  {
		  Buffer[0].dwType      = PACKAGE_BUFFER_SEND;
		  Buffer[0].len         = lpUser->CommandChannel.Out.len;
		  Buffer[0].buf         = lpUser->CommandChannel.Out.buf;
		  Buffer[0].dwTimerType = TIMER_PER_PACKAGE;
		  Buffer[0].dwTimeOut   = SEND_TIMEOUT;

		  if(AddExclusiveClientJob(lpUser->Connection.dwUniqueId, 2, CJOB_SECONDARY|CJOB_INSTANT, INFINITE, NULL, FTP_Cancel, NULL, lpUser))
		  {
			  TransmitPackages(&lpUser->CommandChannel.Socket,
				               Buffer,
							   1,
							   NULL,
							   FTP_SendReply,
							   lpUser);
		  }
	  }
	  // it's important that we only do this after we have output any response,
	  // otherwise we can end up blocking lots of threads if the client stops
	  // processing our output.
	  FTP_AcceptInput(lpUser);
  }

  EndClientJob(lpUser->Connection.dwUniqueId, 5);
  return FALSE;
}

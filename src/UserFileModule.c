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

//  Local declarations
static INT User_StandardOpen(LPTSTR tszUserName, LPUSERFILE lpUserFile);
static BOOL User_StandardLock(LPUSERFILE lpUserFile);
static BOOL User_StandardUnlock(LPUSERFILE lpUserFile);
static BOOL User_StandardWrite(LPUSERFILE lpUserFile);
static BOOL User_StandardClose(LPUSERFILE lpUserFile);
static BOOL User_StandardCreate(LPTSTR tszUserName, INT32 Gid);
static BOOL User_StandardDelete(LPTSTR tszUserName, INT32 Uid);
static BOOL User_StandardRename(LPTSTR tszUserName, INT32 Uid, LPTSTR tszNewUserName);
static INT User_StandardRead(LPTSTR tszFileName, LPUSERFILE lpUserFile, BOOL bCreate);


static DATAROW UserDataRow[] =
{
  "admingroups", offsetof(USERFILE, AdminGroups), DT_GROUPID, MAX_GROUPS, 0,
  "alldn", offsetof(USERFILE, AllDn), DT_INT64, MAX_SECTIONS * 3, 0,
  "allup", offsetof(USERFILE, AllUp), DT_INT64, MAX_SECTIONS * 3, 0,
  "CreatedOn", offsetof(USERFILE, CreatedOn), DT_INT64, 1, 0,
  "CreatorName", offsetof(USERFILE, CreatorName), DT_STRING, 1, _MAX_NAME,
  "CreatorUid", offsetof(USERFILE, CreatorUid), DT_INT32, 1, 0,
  "credits", offsetof(USERFILE, Credits), DT_INT64, MAX_SECTIONS, 0,
  "daydn", offsetof(USERFILE, DayDn), DT_INT64, MAX_SECTIONS * 3, 0,
  "dayup", offsetof(USERFILE, DayUp), DT_INT64, MAX_SECTIONS * 3, 0,
  "DeletedOn", offsetof(USERFILE, DeletedOn), DT_INT64, 1, 0,
  "DeletedBy", offsetof(USERFILE, DeletedBy), DT_INT32, 1, 0,
  "DeletedMsg", offsetof(USERFILE, DeletedMsg), DT_STRING, 1, MAX_DELETE_USER_MSG,
  "ExpiresAt", offsetof(USERFILE, ExpiresAt), DT_INT64, 1, 0,
  "flags", offsetof(USERFILE, Flags), DT_STRING, 1, 32,
  "groups", offsetof(USERFILE, Groups), DT_GROUPID, MAX_GROUPS, 0,
  "home", offsetof(USERFILE, Home), DT_STRING, 1, _MAX_PATH,
  "ips", offsetof(USERFILE, Ip), DT_STRING, MAX_IPS, _IP_LINE_LENGTH,
  "limits", offsetof(USERFILE, Limits), DT_INT32, 5, 0,
  "LimitPerIP", offsetof(USERFILE, LimitPerIP), DT_INT32, 1, 0,
  "LogonCount", offsetof(USERFILE, LogonCount), DT_INT32, 1, 0,
  "LogonLast", offsetof(USERFILE, LogonLast), DT_INT64, 1, 0,
  "MaxUploads", offsetof(USERFILE, MaxUploads), DT_INT32, 1, 0,
  "MaxDownloads", offsetof(USERFILE, MaxDownloads), DT_INT32, 1, 0,
  "monthdn", offsetof(USERFILE, MonthDn), DT_INT64, MAX_SECTIONS * 3, 0,
  "monthup", offsetof(USERFILE, MonthUp), DT_INT64, MAX_SECTIONS * 3, 0,
  "Opaque", offsetof(USERFILE, Opaque), DT_STRING, 1, 256,
  "password", offsetof(USERFILE, Password), DT_PASSWORD, 0, 20,
  "ratio", offsetof(USERFILE, Ratio), DT_INT32, MAX_SECTIONS, 0,
  "tagline", offsetof(USERFILE, Tagline), DT_STRING, 1, 128,
  "theme", offsetof(USERFILE, Theme), DT_INT32, 1, 0,
  "vfsfile", offsetof(USERFILE, MountFile), DT_STRING, 1, _MAX_PATH,
  "wkdn", offsetof(USERFILE, WkDn), DT_INT64, MAX_SECTIONS * 3, 0,
  "wkup", offsetof(USERFILE, WkUp), DT_INT64, MAX_SECTIONS * 3, 0

};


LPUSER_MODULE  lpUserModule;



BOOL User_StandardInit(LPUSER_MODULE lpModule)
{
  lpModule->tszModuleName  = _TEXT("STANDARD");
  lpModule->Open           = User_StandardOpen;
  lpModule->Lock           = User_StandardLock;
  lpModule->Unlock         = User_StandardUnlock;
  lpModule->Write          = User_StandardWrite;
  lpModule->Create         = User_StandardCreate;
  lpModule->Rename         = User_StandardRename;
  lpModule->Delete         = User_StandardDelete;
  lpModule->Close          = User_StandardClose;
  lpUserModule             = lpModule;
  return FALSE;
}


BOOL User_StandardDeInit(LPUSER_MODULE lpModule)
{
  lpModule->tszModuleName  = NULL;
  lpModule->Open           = NULL;
  lpModule->Lock           = NULL;
  lpModule->Unlock         = NULL;
  lpModule->Write          = NULL;
  lpModule->Create         = NULL;
  lpModule->Rename         = NULL;
  lpModule->Delete         = NULL;
  lpModule->Close          = NULL;
  return FALSE;
}



static INT32 User_StandardCreate(LPTSTR tszUserName, INT32 Gid)
{
  USERFILE  UserFile;
  LPTSTR    tszSourceFile, tszTargetFile, tszGroupName;
  DWORD    dwError;
  TCHAR    *tpOffset;
  TCHAR    tpBuffer[128];
  TCHAR    tszFileName[MAX_PATH+1];
  INT32    iReturn, iLen;

  //  Setup local variables
  ZeroMemory(&UserFile, sizeof(USERFILE));
  wsprintf(tpBuffer, _TEXT("%s.temporary"), tszUserName);

  UserFile.Groups[0]    = NOGROUP_ID;
  UserFile.Groups[1]    = -1;
  UserFile.AdminGroups[0]  = -1;

  //  Get location
  tszTargetFile = Config_Get_Path(&IniConfigFile, _TEXT("Locations"),
	                              _TEXT("User_Files"),
								  tpBuffer,
								  NULL);

  if (! tszTargetFile)
  {
	  if (GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
	  {
		  ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, -1);
	  }
	  Putlog(LOG_ERROR, _T("Unable locate User_Files setting of [Locations] section in config file\r\n"));
	  ERROR_RETURN(ERROR_NOT_FOUND, -1);
  }

  tszSourceFile = Config_Get_Path(&IniConfigFile, _TEXT("Locations"),
	                              _TEXT("User_Files"),
								  _TEXT(""),
								  tszFileName);

  if (! tszSourceFile)
  {
	  Free(tszTargetFile);
	  Putlog(LOG_ERROR, _T("Unable locate User_Files setting of [Locations] section in config file\r\n"));
	  ERROR_RETURN(ERROR_NOT_FOUND, -1);
  }

  //  Get position of '\'
  iLen = _tcslen(tszFileName);
  tpOffset = tszFileName + iLen;

  tszGroupName = NULL;
  if (Gid != -1 && (tszGroupName = Gid2Group(Gid)) && (iLen + 8 + _tcslen(tszSourceFile) < MAX_PATH))
  {
	  _stprintf(tpOffset, _T("Default=%s"), tszGroupName);
	  if (! CopyFile(tszSourceFile, tszTargetFile, FALSE))
	  {
		  tszGroupName = NULL;
	  }
  }

  if (!tszGroupName && (iLen + 12 < MAX_PATH))
  {
	  _stprintf(tpOffset, _T("Default.User"));

	  if (! CopyFile(tszSourceFile, tszTargetFile, FALSE))
	  {
		  dwError  = GetLastError();
		  if (dwError != ERROR_FILE_NOT_FOUND)
		  {
			  //  Free memory
			  Free(tszTargetFile);
			  ERROR_RETURN(dwError, -1);
		  }
	  }
  }

  //  Try to read file
  if (User_StandardRead(tszTargetFile, &UserFile, TRUE) != UM_SUCCESS)
  {
    dwError  = GetLastError();
    //  Delete file
    DeleteFile(tszTargetFile);
    //  Free memory
    Free(tszTargetFile);
    SetLastError(dwError);
    return -1;
  }
  //  Register user
  iReturn  = lpUserModule->Register((LPVOID)lpUserModule, tszUserName, &UserFile);
  //  Verify user registration
  if (iReturn == -1)
  {
    dwError  = GetLastError();
    //  Close userfile
    User_StandardClose(&UserFile);
    //  Delete file
    DeleteFile(tszTargetFile);
  }
  else
  {
    //  todo: really should verify there is room in string...
	  _stprintf(tpOffset, _T("%i"), iReturn);
	  //  Move file
	  if (! MoveFileEx(tszTargetFile, tszSourceFile, MOVEFILE_REPLACE_EXISTING))
	  {
		  dwError  = GetLastError();
		  //  Unregister user
		  if (! lpUserModule->Unregister((LPVOID)lpUserModule, tszUserName))
		  {
			  //  Close userfile
			  User_StandardClose(&UserFile);
      }
		  //  Delete file
		  DeleteFile(tszTargetFile);
		  iReturn  = -1;
	  }
	  else dwError  = NO_ERROR;
  }
  //  Free memory
  Free(tszTargetFile);
  //  Set error
  ERROR_RETURN(dwError, iReturn);
}


static BOOL User_StandardRename(LPTSTR tszUserName, INT32 Uid, LPSTR szNewUserName)
{
  //  Call register as
  return lpUserModule->RegisterAs((LPVOID)lpUserModule, tszUserName, szNewUserName);
}


static BOOL User_StandardDelete(LPSTR tszUserName, INT32 Uid)
{
  LPTSTR  tszFileName;
  DWORD  dwError;
  TCHAR  tpBuffer[32];

  wsprintf(tpBuffer, _TEXT("%i"), Uid);
  //  Get target file
  tszFileName  = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("User_Files"), tpBuffer, NULL);
  if (! tszFileName) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, TRUE);
  //  Delete file
  if (! DeleteFile(tszFileName) &&
    (dwError = GetLastError()) != ERROR_FILE_NOT_FOUND)
  {
    Free(tszFileName);
    ERROR_RETURN(dwError, TRUE);
  }
  Free(tszFileName);
  //  Unregister user (should not ever fail)
  return lpUserModule->Unregister((LPVOID)lpUserModule, tszUserName);
}



static BOOL User_StandardLock(LPUSERFILE lpUserFile)
{
  //  Lock should lock resource (exclusive lock), and synchronize contents of lpUserFile with database
  return FALSE;
}



static BOOL User_StandardUnlock(LPUSERFILE lpUserFile)
{
  //  Unlocks resource
  return FALSE;
}



BOOL Ascii2UserFile(PCHAR pBuffer, DWORD dwBuffer, LPUSERFILE lpUserFile)
{
	// for backward compatibility setting some of the new fields to default values here
	lpUserFile->CreatorUid = -1;
	lpUserFile->MaxUploads = -1;
	lpUserFile->MaxDownloads = -1;

	DataRow_ParseBuffer(pBuffer,
                        dwBuffer,
                        (LPVOID)lpUserFile,
                        UserDataRow,
                        sizeof(UserDataRow) / sizeof(DATAROW));

	return FALSE;
}

BOOL UserFile2Ascii(LPBUFFER lpBuffer, LPUSERFILE lpUserFile)
{
  DataRow_Dump(lpBuffer,
               (LPVOID)lpUserFile,
               UserDataRow,
               sizeof(UserDataRow) / sizeof(DATAROW));
  return FALSE;
}


static INT User_StandardRead(LPTSTR tszFileName, LPUSERFILE lpUserFile, BOOL bCreate)
{
  LPUSERFILE_CONTEXT  lpContext;
  DWORD        dwFileSize, dwBytesRead, dwError;
  PCHAR        pBuffer;
  INT          iReturn;

  pBuffer  = NULL;
  iReturn  = UM_FATAL;
  //  Allocate context
  if (! (lpContext = (LPUSERFILE_CONTEXT)Allocate(NULL, sizeof(USERFILE_CONTEXT)))) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, UM_FATAL);

  //  Open userfile
  lpContext->hFileHandle  = CreateFile(tszFileName, GENERIC_READ|GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL,
	(bCreate ? OPEN_ALWAYS : OPEN_EXISTING), 0, NULL);

  if (lpContext->hFileHandle == INVALID_HANDLE_VALUE)
  {
    iReturn  = (GetLastError() == ERROR_FILE_NOT_FOUND ? UM_DELETED : UM_FATAL);
    //  Free resources
    goto DONE;
  }

  //  Get filesize
  if ((dwFileSize = GetFileSize(lpContext->hFileHandle, NULL)) == INVALID_FILE_SIZE) goto DONE;
  //  Allocate read buffer
  if (! (pBuffer = (PCHAR)Allocate(NULL, dwFileSize + 1))) goto DONE;
  //  Read userfile to buffer
  if (! ReadFile(lpContext->hFileHandle, pBuffer, dwFileSize, &dwBytesRead, NULL)) goto DONE;
  if (dwBytesRead < 5 && !bCreate) goto DONE;
  //  Pad buffer with newline
  pBuffer[dwBytesRead++]  = '\n';
  //  Parse buffer
  Ascii2UserFile(pBuffer, dwBytesRead, lpUserFile);

  lpUserFile->Gid      = lpUserFile->Groups[0];
  lpUserFile->lpInternal  = (LPVOID)lpContext;
  iReturn          = UM_SUCCESS;
DONE:
  if (iReturn != UM_SUCCESS)
  {
    dwError  = GetLastError();
    if (lpContext->hFileHandle != INVALID_HANDLE_VALUE) CloseHandle(lpContext->hFileHandle);
    Free(lpContext);
    Free(pBuffer);
    SetLastError(dwError);
  }
  else Free(pBuffer);

  return iReturn;
}




static INT User_StandardOpen(LPTSTR tszUserName, LPUSERFILE lpUserFile)
{
  LPTSTR    tszFileName;
  TCHAR    tpIdBuffer[16];
  INT      iReturn;

  //  Print uid to buffer
  wsprintf(tpIdBuffer, _TEXT("%i"), lpUserFile->Uid);
  //  Get filename
  if (! (tszFileName = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("User_Files"), tpIdBuffer, NULL))) return UM_FATAL;
  //  Read userfile
  iReturn  = User_StandardRead(tszFileName, lpUserFile, FALSE);
  Free(tszFileName);

  return iReturn;
}




static BOOL User_StandardWrite(LPUSERFILE lpUserFile)
{
  LPUSERFILE_CONTEXT  lpContext;
  BUFFER        WriteBuffer;
  DWORD        dwBytesWritten, dwError;

  lpContext    = (LPUSERFILE_CONTEXT)lpUserFile->lpInternal;
  //  Allocate write buffer
  WriteBuffer.size  = 4096;
  WriteBuffer.dwType  = 0;
  WriteBuffer.len    = 0;
  WriteBuffer.buf    = (PCHAR)Allocate(NULL, WriteBuffer.size);
  if (! WriteBuffer.buf) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, TRUE);

  //  Dump to buffer
  UserFile2Ascii(&WriteBuffer, lpUserFile);

  //  Write buffer to file
  SetFilePointer(lpContext->hFileHandle, 0, 0, FILE_BEGIN);
  if (! WriteFile(lpContext->hFileHandle,
    WriteBuffer.buf, WriteBuffer.len, &dwBytesWritten, NULL))
  {
    dwError  = GetLastError();
    Free(WriteBuffer.buf);
    SetLastError(dwError);
    return TRUE;
  }
  SetEndOfFile(lpContext->hFileHandle);
  FlushFileBuffers(lpContext->hFileHandle);

  //  Free write buffer
  Free(WriteBuffer.buf);

  return FALSE;
}




static BOOL User_StandardClose(LPUSERFILE lpUserFile)
{
  LPUSERFILE_CONTEXT  lpContext;
  BOOL        bReturn;

  //  Get context
  lpContext  = (LPUSERFILE_CONTEXT)lpUserFile->lpInternal;
  if (! lpContext) return FALSE;
  //  Close filehandle
  bReturn  = CloseHandle(lpContext->hFileHandle);
  //  Free memory
  Free(lpContext);

  return bReturn;
}



BOOL
User_Default_Open(LPUSERFILE lpUserFile, INT32 id)
{
	TCHAR     tszFileName[MAX_PATH+1];
	LPTSTR    tszOffset, tszGroupName;
	INT       iReturn, iLen;

	// id = -1 -> Default.User         : uid = -2
	// id >= 0 -> Group Default File   : uid <= -3 

	// catch modifications to GID=1 'NoGroup' and treat it the same as /Default.User...
	if (id == NOGROUP_ID) id = -1;

	ZeroMemory(lpUserFile, sizeof(USERFILE));
	lpUserFile->Uid            = -3 - id;
	lpUserFile->Groups[0]      = -1;
	lpUserFile->AdminGroups[0] = -1;

	if (! (Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("User_Files"), _TEXT("Default"), tszFileName))) return TRUE;

	iLen = _tcslen(tszFileName);
	tszOffset = tszFileName + iLen;
	if (id >= 0)
	{
		if (!(tszGroupName = Gid2Group(id)))
		{
			SetLastError(ERROR_GROUP_NOT_FOUND);
			return TRUE;
		}
		if (iLen + _tcslen(tszGroupName) + 1 > MAX_PATH)
		{
			SetLastError(ERROR_BAD_PATHNAME);
			return TRUE;
		}
		*tszOffset = _T('=');
		_tcscpy(&tszOffset[1], tszGroupName);
		iReturn  = User_StandardRead(tszFileName, lpUserFile, FALSE);
		if (iReturn == UM_SUCCESS) return FALSE;
	}

	_tcscpy(tszOffset, _T(".User"));

	//  Read userfile
	iReturn  = User_StandardRead(tszFileName, lpUserFile, TRUE);

	if (iReturn != UM_SUCCESS) return TRUE;

	if (id >= 0)
	{
		// there wasn't a group specific file, so we just read the standard
		// default.user file but if we save any changes we want to do it
		// to the group and not user default file, so we clear close the
		// file handle, and clear the pointer which we test for later...
		User_Default_Close(lpUserFile);
		lpUserFile->lpInternal = NULL;
	}
	return FALSE;
}


BOOL
User_Default_Write(LPUSERFILE lpUserFile)
{
	LPUSERFILE_CONTEXT  lpContext;
	LPDATAROW    lpDataRow;
	DWORD        dwDataRow, dwTemp;
	CHAR         *pField, *pC;
	BUFFER       WriteBuffer;
	DWORD        dwBytesWritten, dwError;
	PINT32       p32;
	PINT64       p64;
	BOOL         bPrint;
	LPTSTR       tszGroupDefault, tszFileName;
	USERFILE     UserFile;

	//  Allocate write buffer
	WriteBuffer.size  = 4096;
	WriteBuffer.dwType  = 0;
	WriteBuffer.len    = 0;
	WriteBuffer.buf    = (PCHAR)Allocate(NULL, WriteBuffer.size);
	if (! WriteBuffer.buf) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, TRUE);


	dwDataRow = sizeof(UserDataRow) / sizeof(DATAROW);

	for (;dwDataRow--;)
	{
		lpDataRow  = &UserDataRow[dwDataRow];
		pField    = (LPVOID)((ULONG)lpUserFile + lpDataRow->dwOffset);
		bPrint = FALSE;

		switch (lpDataRow->dwType)
		{
		case DT_INT32:
			p32 = (PINT32) pField;
			for(dwTemp = 0 ; dwTemp < lpDataRow->dwMaxArgs && !*p32 ; dwTemp++, p32++);
			if (dwTemp < lpDataRow->dwMaxArgs)
			{
				bPrint = TRUE;
			}
			break;
		case DT_INT64:
			p64 = (PINT64) pField;
			for(dwTemp = 0 ; dwTemp < lpDataRow->dwMaxArgs && !*p64; dwTemp++, p64++);
			if (dwTemp < lpDataRow->dwMaxArgs)
			{
				bPrint = TRUE;
			}
			break;
		case DT_GROUPID:
			p32 = (PINT32) pField;
			for(dwTemp = 0 ; dwTemp < lpDataRow->dwMaxArgs && *p32 != -1 ; dwTemp++, p32++);
			if (dwTemp > 0)
			{
				bPrint = TRUE;
			}
			break;
		case DT_STRING:
		case DT_PASSWORD:
			pC = (PCHAR) pField;
			for(dwTemp = 0 ; dwTemp < lpDataRow->dwMaxLength && *pC; dwTemp++, pC++);
			if (dwTemp > 0)
			{
				bPrint = TRUE;
			}
			break;
		}

		if (bPrint)
		{
			// dump the line
			DataRow_Dump(&WriteBuffer, (LPVOID)lpUserFile, lpDataRow, 1);
		}
	}

	lpContext    = (LPUSERFILE_CONTEXT)lpUserFile->lpInternal;

	if (lpUserFile->Uid < -2)
	{
		// Default=Group case
		if (lpContext == NULL && WriteBuffer.len == 0)
		{
			// we are dealing with a non-existent Default=Group file and it would be empty anyway, just return
			Free(WriteBuffer.buf);
			return FALSE;
		}

		// check to see if it would be equal to Default.user
		if (!User_Default_Open(&UserFile, -1))
		{
			UserFile.Uid = lpUserFile->Uid;
			if (!memcmp(&UserFile, lpUserFile, sizeof(UserFile) - 2*sizeof(void *)))
			{
				// the group default is now the same as the user default... so delete the group default file
				if (tszGroupDefault = Uid2User(lpUserFile->Uid))
				{
					// the group still exists, which is good...
					if (tszFileName = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("User_Files"), tszGroupDefault, NULL))
					{
						User_Default_Close(&UserFile);
						User_Default_Close(lpUserFile);
						lpUserFile->lpInternal = NULL;
						if (!DeleteFile(tszFileName))
						{
							Putlog(LOG_ERROR, "Unable to delete default group file: %s\r\n", tszFileName);
						}
						Free(tszFileName);
						Free(WriteBuffer.buf);
						SetLastError(ERROR_USER_DELETED);
						return TRUE;
					}
				}
				// the group disappeared, or there was an error, just write an empty file or something...
			}
			User_Default_Close(&UserFile);
		}

		if (lpContext == NULL)
		{
			// we need to create the new file and get the handle into lpContext...
			if (!(tszGroupDefault = Uid2User(lpUserFile->Uid))) {
				ERROR_RETURN(ERROR_GROUP_NOT_FOUND, TRUE);
			}
			// we need to create a file handle to the proper group default file
			if (! (tszFileName = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("User_Files"), tszGroupDefault, NULL))) return TRUE;
			if (User_StandardRead(tszFileName, &UserFile, TRUE) != UM_SUCCESS)
			{
				Free(tszFileName);
				return TRUE;
			}
			Free(tszFileName);
			lpUserFile->lpInternal = UserFile.lpInternal;
			lpContext    = (LPUSERFILE_CONTEXT)lpUserFile->lpInternal;
			// everything the same at this point
		}
	}

	//  Write buffer to file
	SetFilePointer(lpContext->hFileHandle, 0, 0, FILE_BEGIN);
	if (! WriteFile(lpContext->hFileHandle,
		WriteBuffer.buf, WriteBuffer.len, &dwBytesWritten, NULL))
	{
		dwError  = GetLastError();
		Free(WriteBuffer.buf);
		SetLastError(dwError);
		return TRUE;
	}
	SetEndOfFile(lpContext->hFileHandle);
	FlushFileBuffers(lpContext->hFileHandle);

	//  Free write buffer
	Free(WriteBuffer.buf);

	return FALSE;
}


BOOL
User_Default_Close(LPUSERFILE lpUserFile)
{
	// lpInternal being null is OK here... just means non-existant group file
	if (lpUserFile->lpInternal == NULL) return TRUE;
	return User_StandardClose(lpUserFile);
}

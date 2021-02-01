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

//	Local declarations
static INT Group_StandardOpen(LPTSTR tszGroupName, LPGROUPFILE lpGroupFile);
static BOOL Group_StandardLock(LPGROUPFILE lpGroupFile);
static BOOL Group_StandardUnlock(LPGROUPFILE lpGroupFile);
static BOOL Group_StandardWrite(LPGROUPFILE lpGroupFile);
static BOOL Group_StandardClose(LPGROUPFILE lpGroupFile);
static INT32 Group_StandardCreate(LPTSTR tszGroupName);
static BOOL Group_StandardRename(LPTSTR tszGroupName, INT32 Gid, LPTSTR tszNewGroupName);
static BOOL Group_StandardDelete(LPTSTR tszGroupName, INT32 Gid);
static INT Group_StandardRead(LPTSTR tszFileName, LPGROUPFILE lpGroupFile, BOOL bCreate);

static DATAROW GroupDataRow[] =
{
	"description", offsetof(GROUPFILE, szDescription), DT_STRING, 1, 128,
	"users", offsetof(GROUPFILE, Users), DT_INT32, 1, 0,
	"slots", offsetof(GROUPFILE, Slots), DT_INT32, 2, 0,
	"vfsfile", offsetof(GROUPFILE, szVfsFile), DT_STRING, 1, _MAX_PATH
};


LPGROUP_MODULE	lpGroupModule;




BOOL Group_StandardInit(LPGROUP_MODULE lpModule)
{
	lpModule->tszModuleName	= _TEXT("STANDARD");
	lpModule->Open			= Group_StandardOpen;
	lpModule->Lock			= Group_StandardLock;
	lpModule->Unlock		= Group_StandardUnlock;
	lpModule->Write			= Group_StandardWrite;
	lpModule->Create		= Group_StandardCreate;
	lpModule->Rename		= Group_StandardRename;
	lpModule->Delete		= Group_StandardDelete;
	lpModule->Close			= Group_StandardClose;
	lpGroupModule			= lpModule;

	return FALSE;
}


BOOL Group_StandardDeInit(LPGROUP_MODULE lpModule)
{
	lpModule->tszModuleName	= NULL;
	lpModule->Open			= NULL;
	lpModule->Lock			= NULL;
	lpModule->Unlock		= NULL;
	lpModule->Write			= NULL;
	lpModule->Create		= NULL;
	lpModule->Rename		= NULL;
	lpModule->Delete		= NULL;
	lpModule->Close			= NULL;

	return FALSE;
}



static INT32 Group_StandardCreate(LPTSTR tszGroupName)
{
	GROUPFILE	GroupFile;
	LPTSTR		tszSourceFile, tszTargetFile;
	TCHAR		*tpOffset;
	DWORD		dwError;
	TCHAR		tpBuffer[128];
	INT32		iReturn;

	//	Setup local variables
	ZeroMemory(&GroupFile, sizeof(GROUPFILE));
	wsprintf(tpBuffer, _TEXT("%s.temporary"), tszGroupName);

	//	Get locations
	tszSourceFile	= Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Group_Files"), _TEXT("Default.Group"), NULL);
	tszTargetFile	= Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Group_Files"), tpBuffer, NULL);

	if (! tszSourceFile ||
		! tszTargetFile)
	{
		//	Free memory
		Free(tszSourceFile);
		Free(tszTargetFile);
		ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, -1);
	}
	//	Find last '\' (used later)
	tpOffset	= _tcsrchr(tszSourceFile, _TEXT('\\')) + 1;
	if (! tpOffset) tpOffset	= _tcsrchr(tszSourceFile, _TEXT('/')) + 1;
	//	Copy file
	if (! CopyFile(tszSourceFile, tszTargetFile, FALSE))
	{
		dwError	= GetLastError();
		if (dwError != ERROR_FILE_NOT_FOUND)
		{
			//	Free memory
			Free(tszSourceFile);
			Free(tszTargetFile);
			ERROR_RETURN(dwError, -1);
		}
	}
	//	Try to read file, create if missing
	if (Group_StandardRead(tszTargetFile, &GroupFile, TRUE) != GM_SUCCESS)
	{
		dwError	= GetLastError();
		//	Delete file
		DeleteFile(tszTargetFile);
		//	Free memory
		Free(tszSourceFile);
		Free(tszTargetFile);
		ERROR_RETURN(dwError, -1);
	}
	// it will always have zero members at this point
	GroupFile.Users = 0;

	//	Register user
	iReturn	= lpGroupModule->Register((LPVOID)lpGroupModule,
		tszGroupName, &GroupFile);

	if (iReturn == -1)
	{
		dwError	= GetLastError();
		//	Close userfile
		Group_StandardClose(&GroupFile);
		//	Delete file
		DeleteFile(tszTargetFile);
	}
	else
	{
		//	Update filename (int32 max/min, isn't longer as string than "default.user")
		wsprintf(tpOffset, _TEXT("%i"), iReturn);
		//	Move file
		if (! MoveFileEx(tszTargetFile, tszSourceFile, MOVEFILE_REPLACE_EXISTING))
		{
			dwError	= GetLastError();
			//	Unregister user
			if (! lpGroupModule->Unregister((LPVOID)lpGroupModule, tszGroupName))
			{
				//	Close userfile
				Group_StandardClose(&GroupFile);
			}
			//	Delete file
			DeleteFile(tszTargetFile);
			iReturn	= -1;
		}
		else dwError	= NO_ERROR;
	}
	//	Free memory
	Free(tszSourceFile);
	Free(tszTargetFile);
	//	Set error
	ERROR_RETURN(dwError, iReturn);
}


static BOOL Group_StandardRename(LPTSTR tszGroupName, INT32 Gid, LPTSTR tszNewGroupName)
{
	//	Call register as
	return lpGroupModule->RegisterAs((LPVOID)lpGroupModule, tszGroupName, tszNewGroupName);
}


static BOOL Group_StandardDelete(LPTSTR tszGroupName, INT32 Gid)
{
	LPSTR	tszFileName;
	DWORD	dwError;
	TCHAR	tpBuffer[32];

	wsprintf(tpBuffer, _TEXT("%i"), Gid);
	//	Get target file
	tszFileName	= Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Group_Files"), tpBuffer, NULL);
	if (! tszFileName) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, TRUE);
	//	Delete file
	if (! DeleteFile(tszFileName) &&
		(dwError = GetLastError()) != ERROR_FILE_NOT_FOUND)
	{
		Free(tszFileName);
		ERROR_RETURN(dwError, TRUE);
	}
	Free(tszFileName);
	//	Unregister user (should not ever fail)
	return lpGroupModule->Unregister((LPVOID)lpGroupModule, tszGroupName);
}



static BOOL Group_StandardLock(LPGROUPFILE lpGroupFile)
{
	return FALSE;
}



static BOOL Group_StandardUnlock(LPGROUPFILE lpGroupFile)
{
	return FALSE;
}



BOOL Ascii2GroupFile(PCHAR pBuffer, DWORD dwBuffer, LPGROUPFILE lpGroupFile)
{
	DataRow_ParseBuffer(pBuffer, dwBuffer,
		(LPVOID)lpGroupFile, GroupDataRow, sizeof(GroupDataRow) / sizeof(DATAROW));
	return FALSE;
}


BOOL GroupFile2Ascii(LPBUFFER lpBuffer, LPGROUPFILE lpGroupFile)
{
	DataRow_Dump(lpBuffer, (LPVOID)lpGroupFile,
		GroupDataRow, sizeof(GroupDataRow) / sizeof(DATAROW));
	return FALSE;
}



static INT Group_StandardRead(LPTSTR tszFileName, LPGROUPFILE lpGroupFile, BOOL bCreate)
{
	LPGROUPFILE_CONTEXT	lpContext;
	DWORD				dwFileSize, dwBytesRead;
	PCHAR				pBuffer;
	DWORD				dwError;
	INT					iReturn;

	pBuffer	= NULL;
	iReturn	= GM_FATAL;
	//	Allocate context
	lpContext = (LPGROUPFILE_CONTEXT)Allocate(NULL, sizeof(GROUPFILE_CONTEXT));
	if (! lpContext) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, GM_FATAL);

	//	Open userfile
	lpContext->hFileHandle	= CreateFile(tszFileName, GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL,
		(bCreate ? OPEN_ALWAYS : OPEN_EXISTING), 0, NULL);
	if (lpContext->hFileHandle == INVALID_HANDLE_VALUE)
	{
		iReturn	= (GetLastError() == ERROR_FILE_NOT_FOUND ? GM_DELETED : GM_FATAL);
		goto DONE;
	}

	//	Get filesize
	if ((dwFileSize = GetFileSize(lpContext->hFileHandle, NULL)) == INVALID_FILE_SIZE) goto DONE;

	//	Allocate read buffer
	pBuffer = (PCHAR)Allocate(NULL, dwFileSize + 1);
	if (! pBuffer) goto DONE;

	//	Read userfile to buffer
	if (! ReadFile(lpContext->hFileHandle, pBuffer, dwFileSize, &dwBytesRead, NULL)) goto DONE;
	if (dwFileSize < 5 && !bCreate) goto DONE;
	pBuffer[dwBytesRead++]	= '\n';

	//	Parse data
	if (Ascii2GroupFile(pBuffer, dwBytesRead, lpGroupFile)) goto DONE;

	lpGroupFile->lpInternal	= (LPVOID)lpContext;
	iReturn	= GM_SUCCESS;
DONE:
	if (iReturn != GM_SUCCESS)
	{
		dwError	= GetLastError();
		if (lpContext->hFileHandle != INVALID_HANDLE_VALUE) CloseHandle(lpContext->hFileHandle);
		Free(lpContext);
		Free(pBuffer);
		SetLastError(dwError);
	}
	else Free(pBuffer);

	return iReturn;
}




static INT Group_StandardOpen(LPTSTR tszGroupName, LPGROUPFILE lpGroupFile)
{
	LPTSTR				tszFileName;
	TCHAR				tpIdBuffer[16];
	INT					iReturn;

	//	Print uid to buffer
	wsprintf(tpIdBuffer, _TEXT("%i"), lpGroupFile->Gid);
	//	Get filename
	if (! (tszFileName = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Group_Files"), tpIdBuffer, NULL))) return GM_FATAL;
	//	Read userfile
	iReturn	= Group_StandardRead(tszFileName, lpGroupFile, FALSE);
	Free(tszFileName);

	return iReturn;
}



static BOOL Group_StandardWrite(LPGROUPFILE lpGroupFile)
{
	LPGROUPFILE_CONTEXT	lpContext;
	BUFFER				WriteBuffer;
	DWORD				dwBytesWritten, dwError;

	lpContext	= (LPGROUPFILE_CONTEXT)lpGroupFile->lpInternal;
	//	Allocate write buffer
	WriteBuffer.size	= 4096;
	WriteBuffer.dwType	= 0;
	WriteBuffer.len		= 0;
	WriteBuffer.buf		= (PCHAR)Allocate(NULL, WriteBuffer.size);
	if (! WriteBuffer.buf) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, TRUE);

	//	Dump buffer to test
	if (GroupFile2Ascii(&WriteBuffer, lpGroupFile))
	{
		dwError	= GetLastError();
		Free(WriteBuffer.buf);
		SetLastError(dwError);
		return TRUE;
	}

	//	Seek to beginning of file
	SetFilePointer(lpContext->hFileHandle, 0, 0, FILE_BEGIN);
	//	Write buffer to file
	if (! WriteFile(lpContext->hFileHandle,
		WriteBuffer.buf, WriteBuffer.len, &dwBytesWritten, NULL))
	{
		dwError	= GetLastError();
		Free(WriteBuffer.buf);
		SetLastError(dwError);
		return TRUE;
	}
	//	Set new end of file
	SetEndOfFile(lpContext->hFileHandle);
	FlushFileBuffers(lpContext->hFileHandle);
	//	Free write buffer
	Free(WriteBuffer.buf);

	return FALSE;
}




static BOOL Group_StandardClose(LPGROUPFILE lpGroupFile)
{
	LPGROUPFILE_CONTEXT	lpContext;
	BOOL				bReturn;

	//	Get context
	lpContext	= (LPGROUPFILE_CONTEXT)lpGroupFile->lpInternal;
	if (! lpContext) return FALSE;
	//	Close filehandle
	bReturn	= CloseHandle(lpContext->hFileHandle);
	Free(lpContext);

	return bReturn;
}


BOOL
Group_Default_Open(LPGROUPFILE lpGroupFile)
{
	LPTSTR    tszFileName;
	INT       iReturn;

	//  Get filename
	if (! (tszFileName = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Group_Files"), _T("Default.Group"), NULL))) return TRUE;

	ZeroMemory(lpGroupFile, sizeof(GROUPFILE));
	//  Read Groupfile
	iReturn  = Group_StandardRead(tszFileName, lpGroupFile, TRUE);
	Free(tszFileName);

	if (iReturn != UM_SUCCESS) return TRUE;
	return FALSE;
}



BOOL
Group_Default_Write(LPGROUPFILE lpGroupFile)
{
	LPUSERFILE_CONTEXT  lpContext;
	LPDATAROW    lpDataRow;
	DWORD        dwDataRow, dwTemp;
	CHAR         *pField, *pC;
	BUFFER       WriteBuffer;
	DWORD        dwBytesWritten, dwError;
	PINT32       p32;
	BOOL         bPrint;

	lpContext    = (LPUSERFILE_CONTEXT)lpGroupFile->lpInternal;
	//  Allocate write buffer
	WriteBuffer.size  = 4096;
	WriteBuffer.dwType  = 0;
	WriteBuffer.len    = 0;
	WriteBuffer.buf    = (PCHAR)Allocate(NULL, WriteBuffer.size);
	if (! WriteBuffer.buf) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, TRUE);


	dwDataRow = sizeof(GroupDataRow) / sizeof(DATAROW);

	for (;dwDataRow--;)
	{
		lpDataRow  = &GroupDataRow[dwDataRow];
		pField    = (LPVOID)((ULONG)lpGroupFile + lpDataRow->dwOffset);
		bPrint = FALSE;


		switch (lpDataRow->dwType)
		{
		case DT_INT32:
			// in the case of groups the users field is always 0 for the default file
			if (strcmp(lpDataRow->szName, "users"))
			{
				// it's not users!
				p32 = (PINT32) pField;
				for(dwTemp = 0 ; dwTemp < lpDataRow->dwMaxArgs && !*p32 ; dwTemp++, p32++);
				if (dwTemp < lpDataRow->dwMaxArgs)
				{
					bPrint = TRUE;
				}
			}
			break;
		case DT_STRING:
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
			DataRow_Dump(&WriteBuffer, (LPVOID)lpGroupFile, lpDataRow, 1);
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
Group_Default_Close(LPGROUPFILE lpGroupFile)
{
	return Group_StandardClose(lpGroupFile);
}



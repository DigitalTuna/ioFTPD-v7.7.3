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

static DWORD		dwMaxRequestsPerDevice;
static DEVICEINFO	DeviceInformation['Z' - 'C' + 2];

static LPFILELOCK FileLockList[2];
static CRITICAL_SECTION csFileLock;


// bExact allows you ignore wildcard matches
BOOL IsPathLocked(LPTSTR tszFileName, DWORD dwFileName, BOOL bExact)
{
	LPFILELOCK lpFileLock;

	EnterCriticalSection(&csFileLock);
	lpFileLock = FileLockList[HEAD];
	for ( ; lpFileLock ; lpFileLock = lpFileLock->lpNext )
	{
		if (bExact)
		{
			if (!_tcsicmp(lpFileLock->tszFileName, tszFileName))
			{
				LeaveCriticalSection(&csFileLock);
				SetLastError(ERROR_LOCK_FAILED);
				return TRUE;
			}
		}
		else
		{
			if (!spCompare(lpFileLock->tszFileName, tszFileName))
			{
				LeaveCriticalSection(&csFileLock);
				SetLastError(ERROR_LOCK_FAILED);
				return TRUE;
			}
		}
	}
	LeaveCriticalSection(&csFileLock);
	return FALSE;
}


// This just locks a "name".  Could be a VFS path or a real path or even
// a wildcard name to lock a directory tree.  If bWildLock isn't specified
// then c:\a\b followed by c:\a\* will be allowed.  If bWildLock is specified
// then it would return an error because something below it is already
// locked.  Thus specifying bExact is only meaningful when the path that
// you are requesting to be locked contains a wildcard.
LPFILELOCK LockPath(LPTSTR tszFileName, DWORD dwFileName, DWORD dwClientId, BOOL bWildLock)
{
	LPFILELOCK lpFileLock;

	EnterCriticalSection(&csFileLock);
	lpFileLock = FileLockList[HEAD];
	for ( ; lpFileLock ; lpFileLock = lpFileLock->lpNext )
	{
		// NOTE: spCompare used this way so you can lock trees via c:\foo\* type things
		if (!spCompare(lpFileLock->tszFileName, tszFileName))
		{
			LeaveCriticalSection(&csFileLock);
			SetLastError(ERROR_LOCK_FAILED);
			return NULL;
		}
	}
	if (bWildLock)
	{
		lpFileLock = FileLockList[HEAD];
		for ( ; lpFileLock ; lpFileLock = lpFileLock->lpNext )
		{
			// NOTE: spCompare used this way to check for things under a wildcard already locked
			if (!spCompare(tszFileName, lpFileLock->tszFileName))
			{
				LeaveCriticalSection(&csFileLock);
				SetLastError(ERROR_LOCK_FAILED);
				return NULL;
			}
		}
	}
	lpFileLock = (LPFILELOCK) Allocate("FileLock", sizeof(FILELOCK)+(dwFileName+1)*sizeof(TCHAR));
	if (!lpFileLock)
	{
		LeaveCriticalSection(&csFileLock);
		SetLastError(ERROR_OUTOFMEMORY);
		return NULL;
	}
	lpFileLock->tszFileName = (LPTSTR) &lpFileLock[1];
	_tcscpy_s(lpFileLock->tszFileName, dwFileName+1, tszFileName);
	lpFileLock->dwClientId = dwClientId;
	APPENDLIST(lpFileLock, FileLockList);
	LeaveCriticalSection(&csFileLock);
	return lpFileLock;
}


// Remove a lock returned via LockPath
BOOL UnlockPath(LPFILELOCK lpFileLock)
{
	BOOL bDelete = FALSE;

	EnterCriticalSection(&csFileLock);
	DELETELIST(lpFileLock, FileLockList);
	LeaveCriticalSection(&csFileLock);
	Free(lpFileLock);
	return FALSE;
}


DWORD GetDeviceID(LPSTR szFileName)
{
	while (szFileName[0] == '\\' || szFileName[0] == '/') szFileName++;
	if (toupper(szFileName[0]) < 'C' || toupper(szFileName[0]) > 'Z' ||
		(szFileName[1] != ':' && szFileName[1] != '\\' && szFileName[1] != '/'))
	{
		return ((BYTE)'Z' - (BYTE)'C' + 1);
	}
	return (BYTE)toupper(szFileName[0]) - (BYTE)'C';
}




BOOL BindCompletionPort(HANDLE Handle)
{
	//	Bind handle to completion port
	return (! CreateIoCompletionPort(Handle, hCompletionPort, 0, 0));
}




BOOL ioOpenFile(LPIOFILE lpFile, DWORD dwClientId, LPSTR szFileName,
				DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition)
{
	DWORD	dwFileType, dwLastError, dwFileName;

	//	Update file structure
	ZeroMemory(lpFile, sizeof(IOFILE));
	lpFile->lpDeviceInformation	= &DeviceInformation[GetDeviceID(szFileName)];
	lpFile->Overlapped.hFile	= lpFile;

	dwFileName = _tcslen(szFileName);
	if (dwDesiredAccess & GENERIC_WRITE)
	{
		// we want to be able to write to the file, so try to acquire a lock on the name
		lpFile->lpFileLock = LockPath(szFileName, dwFileName, dwClientId, FALSE);
		if (!lpFile->lpFileLock)
		{
			return TRUE;
		}
	}
	else
	{
		// we want to just read the file, so see if it's locked
		if (IsPathLocked(szFileName, dwFileName, FALSE))
		{
			return TRUE;
		}
	}

	//	Open file
	lpFile->FileHandle = CreateFile(szFileName, dwDesiredAccess, dwShareMode, NULL,
		dwCreationDisposition, FILE_FLAG_OVERLAPPED|FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (lpFile->FileHandle == INVALID_HANDLE_VALUE)
	{
		dwLastError = GetLastError();
		goto error;
	}

	//	Get filetype
	dwFileType	= GetFileType(lpFile->FileHandle);
	if (dwFileType == FILE_TYPE_CHAR ||
		dwFileType == FILE_TYPE_PIPE)
	{
		//	Invalid filetype
		dwLastError = ERROR_INVALID_NAME;
		goto error;
	}

	//	Bind to completion port
	if (! CreateIoCompletionPort(lpFile->FileHandle, hCompletionPort, (DWORD)-1, 0))
	{
		dwLastError	= GetLastError();
		goto error;
	}

	if (dwDesiredAccess & GENERIC_WRITE)
	{
		// we created a file, need to mark the directory as stale
		MarkParent(szFileName, FALSE);
	}
	return FALSE;

error:
	if (lpFile->FileHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(lpFile->FileHandle);
		lpFile->FileHandle	= INVALID_HANDLE_VALUE;
	}
	if (lpFile->lpFileLock)
	{
		UnlockPath(lpFile->lpFileLock);
		lpFile->lpFileLock = 0;
	}
	SetLastError(dwLastError);
	return TRUE;

}


DWORD ioGetFileSize(LPIOFILE lpFile, LPDWORD lpFileSizeHigh)
{
	return GetFileSize(lpFile->FileHandle, lpFileSizeHigh);
}


BOOL ioSeekFile(LPIOFILE lpFile, LPDWORD lpSeekOffset, BOOL bSetEndOfFile)
{
	DWORD	dwFileSize, dwFileSizeHigh;
	BOOL	bReturn;

	bReturn	= FALSE;

	//	Check filesize
	if (lpSeekOffset[0] || lpSeekOffset[1])
	{
		//	Update overlapped structure
		lpFile->Overlapped.Offset		= lpSeekOffset[1];
		lpFile->Overlapped.OffsetHigh	= lpSeekOffset[0];

		if ((dwFileSize = GetFileSize(lpFile->FileHandle, &dwFileSizeHigh)) == INVALID_FILE_SIZE &&
			GetLastError() != NO_ERROR) return TRUE;

		if (lpSeekOffset[0] == (DWORD)-1 &&
			lpSeekOffset[1] == (DWORD)-1)
		{
			lpSeekOffset[0]	= dwFileSizeHigh;
			lpSeekOffset[1]	= dwFileSize;

			lpFile->Overlapped.Offset		= lpSeekOffset[1];
			lpFile->Overlapped.OffsetHigh	= lpSeekOffset[0];
		}
		else if (lpSeekOffset[0] > dwFileSizeHigh ||
			(lpSeekOffset[0] == dwFileSizeHigh && lpSeekOffset[1] > dwFileSize))
		{
			ERROR_RETURN(ERROR_FILESEEK, TRUE);
		}
	}

	//	Set end file
	if (bSetEndOfFile)
	{
		if (SetFilePointer(lpFile->FileHandle, lpSeekOffset[1], (LPLONG)&lpSeekOffset[0], FILE_BEGIN) != INVALID_SET_FILE_POINTER ||
			GetLastError() == NO_ERROR)
		{
			bReturn	= (! SetEndOfFile(lpFile->FileHandle));
		}
		else bReturn	= TRUE;
	}
	return bReturn;
}



BOOL ioCloseFile(LPIOFILE lpFile, BOOL bKeepLock)
{
	BOOL	bReturn;

	bReturn	= FALSE;
	//	Close file
	if (lpFile->FileHandle &&
		lpFile->FileHandle != INVALID_HANDLE_VALUE)
	{
		bReturn	= CloseHandle(lpFile->FileHandle);
	}
	lpFile->FileHandle	= INVALID_HANDLE_VALUE;
	if (!bKeepLock && lpFile->lpFileLock)
	{
		UnlockPath(lpFile->lpFileLock);
		lpFile->lpFileLock = NULL;
	}
	return bReturn;
}






VOID IoReadFile(LPIOFILE lpFile, LPVOID lpBuffer, DWORD dwBuffer)
{
	LPDEVICEINFO	lpDeviceInformation;
	DWORD			dwBytesRead, dwResult;

	lpDeviceInformation	= lpFile->lpDeviceInformation;

	lpFile->Overlapped.lpBuffer		= lpBuffer;
	lpFile->Overlapped.dwBuffer		= dwBuffer;
	lpFile->Overlapped.Internal     = 0;
	lpFile->Overlapped.InternalHigh = 0;
	EnterCriticalSection(&lpDeviceInformation->CriticalSection);
	if (dwMaxRequestsPerDevice < lpDeviceInformation->dwRequests++)
	{
		//	Push item to queue
		if (! lpDeviceInformation->lpQueueHead)
		{
			lpDeviceInformation->lpQueueHead	= &lpFile->Overlapped;
		}
		else lpDeviceInformation->lpQueueTail->lpNext	= &lpFile->Overlapped;
		lpDeviceInformation->lpQueueTail	= &lpFile->Overlapped;
		lpFile->Overlapped.lpNext		= NULL;
		lpFile->Overlapped.dwCommand	= FILE_READ;
		lpFile	= NULL;
	}
	LeaveCriticalSection(&lpDeviceInformation->CriticalSection);
	//	Readfile
	if (lpFile &&
		! ReadFile(lpFile->FileHandle, lpBuffer, dwBuffer, &dwBytesRead, (LPOVERLAPPED)&lpFile->Overlapped) &&
		(dwResult = GetLastError()) != ERROR_IO_PENDING)
	{
		lpFile->Overlapped.Internal	= dwResult;
		PostQueuedCompletionStatus(hCompletionPort, 0, (DWORD)-5, (LPOVERLAPPED)&lpFile->Overlapped);
	}
}




VOID IoWriteFile(LPIOFILE lpFile, LPVOID lpBuffer, DWORD dwBuffer)
{
	LPDEVICEINFO	lpDeviceInformation;
	DWORD			dwBytesWritten;

	lpDeviceInformation	= lpFile->lpDeviceInformation;

	lpFile->Overlapped.lpBuffer		= lpBuffer;
	lpFile->Overlapped.dwBuffer		= dwBuffer;
	lpFile->Overlapped.Internal     = 0;
	lpFile->Overlapped.InternalHigh = 0;
	EnterCriticalSection(&lpDeviceInformation->CriticalSection);
	if (dwMaxRequestsPerDevice < lpDeviceInformation->dwRequests++)
	{
		//	Push item to queue
		if (! lpDeviceInformation->lpQueueHead)
		{
			lpDeviceInformation->lpQueueHead	= &lpFile->Overlapped;
		}
		else lpDeviceInformation->lpQueueTail->lpNext	= &lpFile->Overlapped;
		lpDeviceInformation->lpQueueTail	= &lpFile->Overlapped;
		lpFile->Overlapped.lpNext		= NULL;
		lpFile->Overlapped.dwCommand	= FILE_WRITE;
		lpFile	= NULL;
	}
	LeaveCriticalSection(&lpDeviceInformation->CriticalSection);

	//	Write file
	if (lpFile &&
		! WriteFile(lpFile->FileHandle, lpBuffer, dwBuffer, &dwBytesWritten, (LPOVERLAPPED)&lpFile->Overlapped) &&
		(dwBytesWritten = GetLastError()) != ERROR_IO_PENDING)
	{
		lpFile->Overlapped.Internal	= dwBytesWritten;
		PostQueuedCompletionStatus(hCompletionPort, 0, (DWORD)-5, (LPOVERLAPPED)&lpFile->Overlapped);
	}
}





VOID PopIOQueue(LPIOFILE hFile)
{
	LPDEVICEINFO		lpDeviceInformation;
	LPFILEOVERLAPPED	lpOverlapped;
	DWORD				dwResult;
	BOOL				bResult;

	lpDeviceInformation	= hFile->lpDeviceInformation;

	//	Pop item from queue
	EnterCriticalSection(&lpDeviceInformation->CriticalSection);
	if (--lpDeviceInformation->dwRequests > dwMaxRequestsPerDevice)
	{
		lpOverlapped	= lpDeviceInformation->lpQueueHead;
		lpDeviceInformation->lpQueueHead	= lpOverlapped->lpNext;
	}
	else lpOverlapped	= NULL;
	LeaveCriticalSection(&lpDeviceInformation->CriticalSection);

	if (lpOverlapped)
	{
		bResult	= (lpOverlapped->dwCommand == FILE_READ ? ReadFile : WriteFile)(
			lpOverlapped->hFile->FileHandle,
			lpOverlapped->lpBuffer, lpOverlapped->dwBuffer, &dwResult, (LPOVERLAPPED)lpOverlapped);

		if (! bResult && (dwResult = GetLastError()) != ERROR_IO_PENDING)
		{
			lpOverlapped->Internal	= dwResult;
			PostQueuedCompletionStatus(hCompletionPort, 0, (DWORD)-5, (LPOVERLAPPED)lpOverlapped);
		}
	}
}





BOOL ReadTextFile(LPTSTR tszFileName, TCHAR **lpTextBuffer, LPDWORD lpTextBufferSize)
{
	HANDLE	hFile;
	DWORD	dwFileSize, dwError, dwBytesRead;
	TCHAR	*lpBuffer;

	dwError	= NO_ERROR;
	//	Open file
	hFile	= CreateFile(tszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;
	if ((dwFileSize = GetFileSize(hFile, NULL)) == INVALID_FILE_SIZE)
	{
		dwError	= GetLastError();
		CloseHandle(hFile);
		ERROR_RETURN(dwError, FALSE);
	}

	//	Buffer file to memory
	lpBuffer	= (TCHAR *)Allocate("ReadBuffer", dwFileSize + sizeof(TCHAR));
	if (! lpBuffer)
	{
		CloseHandle(hFile);
		ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, FALSE);
	}
	if (! ReadFile(hFile, lpBuffer, dwFileSize, &dwBytesRead, NULL))
	{
		dwError	= GetLastError();
		CloseHandle(hFile);
		Free(lpBuffer);
		ERROR_RETURN(dwError, FALSE);
	}
	if (dwBytesRead < sizeof(TCHAR))
	{
		CloseHandle(hFile);
		Free(lpBuffer);
		lpTextBuffer	= NULL;
		lpTextBufferSize	= 0;
		return TRUE;
	}
	
	
	//	Convert buffer to tchar
#ifdef _UNICODE
	if (((WCHAR *)lpBuffer)[0] != 65279)
	{
		//	Buffer is not unicode encoded
		lpTextBuffer[0]	= (WCHAR *)Allocate("ReadBuffer", (dwBytesRead + 1) * sizeof(WCHAR));
		if (lpTextBuffer[0])
		{
			lpTextBufferSize[0]	= swprintf(lpTextBuffer[0], ((PCHAR)lpBuffer)[dwBytesRead - 1] == '\n' ? L"%.*S" : L"%.*S\n", dwBytesRead, lpBuffer);
		}
		else dwError	= ERROR_NOT_ENOUGH_MEMORY;
	}
	else
	{
		//	Buffer is unicode encoded
		dwBytesRead	= (dwBytesRead & (0xFFFFFFFF - (sizeof(WCHAR) - 1))) / sizeof(WCHAR);

		MoveMemory(lpBuffer, &lpBuffer[1], (--dwBytesRead) * sizeof(WCHAR));
		if (lpBuffer[dwBytesRead - 1] != L'\n') lpBuffer[dwBytesRead++]	= L'\n';
		lpTextBuffer[0]	= lpBuffer;
		lpTextBufferSize[0]	= dwBytesRead;
		lpBuffer	= NULL;
	}
#else
	if (((WCHAR *)lpBuffer)[0] == 65279)
	{
		//	Buffer is unicode encoded
		dwBytesRead	= (dwBytesRead & (0xFFFFFFFF - (sizeof(WCHAR) - 1))) / sizeof(WCHAR);
		lpTextBuffer[0]	= (PCHAR)Allocate("ReadBuffer", dwBytesRead + 1);
		if (lpTextBuffer[0])
		{
			lpTextBufferSize[0]	= sprintf(lpTextBuffer[0], ((WCHAR *)lpBuffer)[dwBytesRead - 1] == L'\n' ? "%.*S" : "%.*S\n", dwBytesRead - 1, lpBuffer + sizeof(WCHAR));
		}
		else dwError	= ERROR_NOT_ENOUGH_MEMORY;
	}
	else
	{
		//	Buffer is not unicode encoded
		if (lpBuffer[dwBytesRead - 1] == '\n') lpBuffer[dwBytesRead++]	= '\n';
		lpTextBuffer[0]	= lpBuffer;
		lpTextBufferSize[0]	= dwBytesRead;
		lpBuffer	= NULL;
	}
#endif
	CloseHandle(hFile);
	Free(lpBuffer);
	if (dwError) ERROR_RETURN(dwError, FALSE);
	return TRUE;
}



BOOL FileCrc32(HANDLE hFile, UINT64 u64StartPos, UINT64 u64Len, LPDWORD lpdwCRC)
{
	char Buffer[64*1024];
	DWORD dwBytesWanted, dwBytesRead, dwError;
	DWORD dwFileSize, dwFileSizeHigh;
	LONG lpStartPos[2];

	lpStartPos[0] = (DWORD) (u64StartPos >> 32);
	lpStartPos[1] = (DWORD) u64StartPos & 0xFFFFFFFF;

	if ((dwFileSize = GetFileSize(hFile, &dwFileSizeHigh)) == INVALID_FILE_SIZE &&
		GetLastError() != NO_ERROR)
	{
		return FALSE;
	}

	if (lpStartPos[0] > (LONG) dwFileSizeHigh ||
		(lpStartPos[0] == (LONG) dwFileSizeHigh && (DWORD) lpStartPos[1] > dwFileSize))
	{
		ERROR_RETURN(ERROR_FILESEEK, FALSE);
	}

	if (SetFilePointer(hFile, lpStartPos[1], &lpStartPos[0], FILE_BEGIN) == INVALID_SET_FILE_POINTER &&
		GetLastError() != NO_ERROR)
	{
		ERROR_RETURN(ERROR_FILESEEK, FALSE);
	}

	SetBlockingThreadFlag();
	dwBytesWanted = sizeof(Buffer);

	while (u64Len > 0)
	{
		if (u64Len < (UINT64) dwBytesWanted)
		{
			dwBytesWanted = (DWORD) u64Len;
		}
		if (!ReadFile(hFile, (LPVOID) Buffer, dwBytesWanted, &dwBytesRead, 0) ||
			(dwBytesWanted != dwBytesRead))
		{
			dwError = GetLastError();
			SetNonBlockingThreadFlag();
			return FALSE;
		}
		CalculateCrc32(Buffer, dwBytesRead, lpdwCRC);
		u64Len -= dwBytesRead;
	}
	SetNonBlockingThreadFlag();
	return TRUE;
}




BOOL File_Init(BOOL bFirstInitialization)
{
	DWORD	dwConfigValue, n;
	BOOL	bResult;

	//	Initialize structures
	if (bFirstInitialization)
	{
		ZeroMemory(&DeviceInformation, sizeof(DeviceInformation));
		for (n = 0;n < ('Z' - 'C' + 2);n++) 
		{
			bResult	= InitializeCriticalSectionAndSpinCount(&DeviceInformation[n].CriticalSection, 100);
			if (! bResult) return FALSE;
		}
		dwMaxRequestsPerDevice	= 5;

		if (! Config_Get_Int(&IniConfigFile, _TEXT("File"), _TEXT("Device_Concurrency"), (PINT)&dwConfigValue) &&
			dwConfigValue > 0) dwMaxRequestsPerDevice	= dwConfigValue;

		InitializeCriticalSectionAndSpinCount(&csFileLock, 100);
		FileLockList[HEAD] = FileLockList[TAIL] = NULL;
	}

	return TRUE;
}


VOID File_DeInit(VOID)
{
	DWORD	n;

	for (n = 0;n < ('Z' - 'C' + 2);n++)
	{
		DeleteCriticalSection(&DeviceInformation[n].CriticalSection);
	}
}


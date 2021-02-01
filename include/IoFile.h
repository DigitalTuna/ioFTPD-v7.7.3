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

typedef struct _DEVICEINFO
{
	CRITICAL_SECTION	CriticalSection;
	DWORD				dwRequests;
	LPFILEOVERLAPPED	lpQueueHead;
	LPFILEOVERLAPPED	lpQueueTail;

} DEVICEINFO, * LPDEVICEINFO;

#define IOFILE_CREATED 0x1  // Used by upload routine to indicate file didn't exist before
#define IOFILE_VALID   0x2  // Used by transfer logic to indicate a connection was established
                            // which means a 0 byte transfer indicates a 0 byte file


typedef struct _FILELOCK
{
	struct _FILELOCK  *lpPrev;
	struct _FILELOCK  *lpNext;

	DWORD   dwClientId;
	LPTSTR  tszFileName;

} FILELOCK, * LPFILELOCK;


typedef struct _IOFILE
{
	HANDLE			FileHandle;
	FILEOVERLAPPED	Overlapped;
	DWORD			dwFlags;
	LPDEVICEINFO	lpDeviceInformation;
	LPFILELOCK      lpFileLock;

} IOFILE, * LPIOFILE;


#define	FILE_READ			0
#define	FILE_WRITE			1
#define OVERLAPPED_INC(O, I) if ((O.Offset += I) < I) O.OffsetHigh++;


BOOL ioOpenFile(LPIOFILE lpFile, DWORD dwClientId, LPTSTR FileName,
				DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition);
BOOL ioCloseFile(LPIOFILE lpFile, BOOL bKeepLock);
BOOL ioSeekFile(LPIOFILE lpFile, LPDWORD lpSeekOffset, BOOL bSetEndOfFile);
DWORD ioGetFileSize(LPIOFILE lpFile, LPDWORD lpFileSizeHigh);
BOOL BindCompletionPort(HANDLE Handle);

BOOL File_Init(BOOL bFirstInitialization);
VOID File_DeInit(VOID);
VOID PopIOQueue(LPIOFILE hFile);
VOID IoReadFile(LPIOFILE lpFile, LPVOID lpBuffer, DWORD dwBufferSize);
VOID IoWriteFile(LPIOFILE lpFile, LPVOID lpBuffer, DWORD dwBufferSize);

BOOL FileCrc32(HANDLE hFile, UINT64 u64StartPos, UINT64 u64Len, LPDWORD lpdwCRC);

LPFILELOCK LockPath(LPTSTR tszFileName, DWORD dwFileName, DWORD dwClientId, BOOL bWildLock);
BOOL IsPathLocked(LPTSTR tszFileName, DWORD dwFileName, BOOL bExact);
BOOL UnlockPath(LPFILELOCK lpFileLock);


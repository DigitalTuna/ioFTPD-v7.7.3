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

static DIRECTORYTABLE  *DirectoryTable;  // Contains directories sorted by hash
UINT32 volatile  DefaultUid[2], DefaultGid[2];
DWORD volatile  dwDefaultFileMode[2], dwCacheBucketSize, dwCacheBuckets;

DWORD NtfsReparseMethod;
BOOL  bVfsExportedPathsOnly;

static BOOL  ValidFileContext(LPFILECONTEXT lpContext);
static DWORD GetFileModeContextFlags(LPFILECONTEXT lpContext);


UINT32 HashFileName(register LPTSTR tszFileName, register DWORD dwFileName)
{
  register UINT32  Crc32;

#if (CHAR == TCHAR)
  //  Calculate crc
  for (Crc32 = 0xFFFFFFFF;dwFileName--;)
  {
    Crc32  = (Crc32 >> 8) ^ crc32_table[(BYTE)((BYTE)Crc32 ^ tolower((tszFileName++)[0]))];
  }
#else
  abort();
#endif
  return Crc32;
}



INT __cdecl CompareDirectoryName(LPDIRECTORY *lpItem1, LPDIRECTORY *lpItem2)
{
  if (lpItem1[0]->Hash > lpItem2[0]->Hash) return 1;
  if (lpItem1[0]->Hash < lpItem2[0]->Hash) return -1;
  return _tcsicmp(lpItem1[0]->tszFileName, lpItem2[0]->tszFileName);
}



INT __cdecl CompareFileName(LPFILEINFO *lpItem1, LPFILEINFO *lpItem2)
{
	DWORD  dwFileAttributes[2];

  dwFileAttributes[0]  = lpItem1[0]->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
  dwFileAttributes[1]  = lpItem2[0]->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
  if (dwFileAttributes[0] > dwFileAttributes[1]) return -1;
  if (dwFileAttributes[0] < dwFileAttributes[1]) return 1;

  return _tcsicmp(lpItem1[0]->tszFileName, lpItem2[0]->tszFileName);
}


INT __cdecl SearchCompareFileName(LPFSEARCH lpItem1, LPFILEINFO *lpItem2)
{
  DWORD  dwFileAttributes[2];
  INT register  iResult;

  dwFileAttributes[0]  = lpItem1->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
  dwFileAttributes[1]  = lpItem2[0]->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
  if (dwFileAttributes[0] > dwFileAttributes[1]) return -1;
  if (dwFileAttributes[0] < dwFileAttributes[1]) return 1;

  iResult  = _tcsicmp(lpItem1->tszFileName, lpItem2[0]->tszFileName);
  if (iResult > 0) return 1;
  return (iResult < 0 ? -1 : 0);
}



DWORD TrimFileName(LPTSTR tszSource, LPTSTR tszTarget)
{
  LPTSTR  tszFileName;
  DWORD  dwBackSlash;

  dwBackSlash  = 0;
  tszFileName  = tszTarget;
  for (;;)
  {
    switch ((tszTarget++)[0] = (tszSource++)[0])
    {
    case _TEXT('\0'):
      if (--tszTarget - tszFileName != 3)
      {
        tszTarget[-(INT)dwBackSlash]  = _TEXT('\0');
        return tszTarget - tszFileName - dwBackSlash;
      }
      return tszTarget - tszFileName;
    case _TEXT('/'):
      tszTarget[-1]  = _TEXT('\\');
    case _TEXT('\\'):
      if (dwBackSlash &&
        tszTarget - tszFileName > 2)
      {
        tszTarget--;
      }
      else dwBackSlash++;
      break;
    default:
      dwBackSlash  = 0;
    }
  }
}


// return 0 on error
DWORD GetTailOfPath(LPTSTR tszPath, DWORD dwStart)
{
	DWORD dwPos;

	if (dwStart == 0)
	{
		dwStart = _tcslen(tszPath);
	}
	if (dwStart <= 2)
	{
		// this is an error, a real path can't be that short
		return 0;
	}
	if (tszPath[dwStart-1] == _T('\\'))
	{
		dwStart--;
	}
	for (dwPos = dwStart ; dwPos ; dwPos--)
	{
		if (tszPath[dwPos-1] == _T('\\'))
		{
			break;
		}
	}
	if (tszPath[1] == _T(':'))
	{
		// we have a D:\ type path
		return dwPos;
	}
	// we have a \\machine\ type path
	if ((dwPos <= 2) && (tszPath[0] == _T('\\')) && (tszPath[1] == _T('\\')))
	{
		// this is an error, we chopped off the machine name
		return 0;
	}
	return dwPos;
}


LPFILEINFO FindFileInfo(DWORD dwFileAttributes, LPTSTR tszFileName, LPDIRECTORYINFO lpDirectoryInfo)
{
  FSEARCH    SearchItem;
  LPFILEINFO  *lpResult;

  if (! lpDirectoryInfo) return NULL;
  SearchItem.dwFileAttributes  = dwFileAttributes;
  SearchItem.tszFileName  = tszFileName;

  lpResult  = (LPFILEINFO *)bsearch(&SearchItem, lpDirectoryInfo->lpFileInfo,
	                                lpDirectoryInfo->dwDirectorySize, sizeof(LPFILEINFO),
									(QUICKCOMPAREPROC) SearchCompareFileName);

  return (lpResult ? lpResult[0] : NULL);
}



BOOL FreeDirectoryInfo(LPDIRECTORYINFO lpDirectoryInfo, BOOL bDirty)
{
	DWORD  n;

	if (! lpDirectoryInfo) return TRUE;

	if (bDirty)
	{
		lpDirectoryInfo->lpRootEntry->dwFileAttributes |= FILE_ATTRIBUTE_DIRTY;
	}
	//  Frees directory info if reference count is zero
	if (! InterlockedDecrement(&lpDirectoryInfo->lReferenceCount))
	{
		if (lpDirectoryInfo->lpLinkedInfo)
		{
			// we are a sharing directory info so we need to release it
			FreeDirectoryInfo(lpDirectoryInfo->lpLinkedInfo, FALSE);
		}
		else
		{
			// release unshared directory contents
			for (n = lpDirectoryInfo->dwDirectorySize;n--;)
			{
				CloseFileInfo(lpDirectoryInfo->lpFileInfo[n]);
			}
			Free(lpDirectoryInfo->lpFileInfo);
		}
		CloseFileInfo(lpDirectoryInfo->lpRootEntry);
		Free(lpDirectoryInfo);
	}
	return FALSE;
}


// Must hold the directory table lock!
BOOL AcquireDirCacheLock(LPDIRECTORYTABLE lpDirectoryTable, LPDIRECTORY lpDirectory)
{
	HANDLE hEvent;
	BOOL  bDoUpdate;

	//  Increase directory reference count
	switch (lpDirectory->lReferenceCount++)
	{
	case 1:
		hEvent            = INVALID_HANDLE_VALUE;
		if (!lpDirectory->bLocked)
		{
			// This change means a refcount=1 which is a currently unopen item that should
			// be on the LRU list won't be because we need to keep it around so we can
			// see that it's been locked while a move/delete is under way.
			DELETELIST(lpDirectory, lpDirectoryTable->lpDirectoryList);
		}
		bDoUpdate = TRUE;
		break;
	case 2:
		hEvent = lpDirectory->hEvent;
		if (hEvent == INVALID_HANDLE_VALUE) 
		{
			hEvent  = CreateEvent(NULL, FALSE, FALSE, NULL);
			lpDirectory->hEvent  = hEvent;
		}
		bDoUpdate = FALSE;
		break;
	default:
		hEvent  = lpDirectory->hEvent;
		bDoUpdate = FALSE;
		break;
	}
	LeaveCriticalSection(&lpDirectoryTable->CriticalSection);

	//  Prevent race conditions
	if (hEvent != INVALID_HANDLE_VALUE)
	{
		WaitForSingleObject(hEvent, INFINITE);
		if (lpDirectory->lForceUpdate)
		{
			// make sure we catch late changes
			bDoUpdate = TRUE;
		}
	}
	return bDoUpdate;
}



// DirCache really should be a self balancing tree or heap so deleting the LRU
// item would be faster...

LPDIRECTORY InsertAndAcquireDirCacheLock(LPTSTR tszFileName, DWORD dwFileName, LPBOOL pbDoUpdate)
{
	LPDIRECTORY        lpOldDirectory, lpTempDir, *lpDirArray, lpDirectory;
	LPDIRECTORYTABLE   lpDirectoryTable;
	INT                iResult;
	BOOL               bDoUpdate;
	CHAR               pBuffer[sizeof(DIRECTORY) + MAX_PATH + 1];

	lpOldDirectory    = NULL;
	lpTempDir         = (LPDIRECTORY) pBuffer;

	lpTempDir->dwFileName = dwFileName;
	CopyMemory(lpTempDir->tszFileName, tszFileName, dwFileName*sizeof(TCHAR));
	lpTempDir->tszFileName[dwFileName] = 0;
	lpTempDir->Hash = HashFileName(tszFileName, dwFileName);

	lpDirectoryTable  = &DirectoryTable[lpTempDir->Hash % dwCacheBuckets];

	EnterCriticalSection(&lpDirectoryTable->CriticalSection);
	//  Trim directory cache size
	if ((lpDirectoryTable->dwDirectories+1) >= lpDirectoryTable->dwAllocated)
	{
		if (lpDirectoryTable->lpDirectoryList[HEAD])
		{
			//  Remove oldest item from cache
			lpOldDirectory  = lpDirectoryTable->lpDirectoryList[TAIL];
			DELETELIST(lpOldDirectory, lpDirectoryTable->lpDirectoryList);
			QuickDelete(lpDirectoryTable->lpDirectory, lpDirectoryTable->dwDirectories--,
				lpOldDirectory, (QUICKCOMPAREPROC) CompareDirectoryName, NULL);
		}
		else
		{
			//  Grow array size
			lpDirArray  = ReAllocate(lpDirectoryTable->lpDirectory,
				"Directory", (lpDirectoryTable->dwAllocated + dwCacheBucketSize) * sizeof(LPDIRECTORY));
			if (! lpDirArray)
			{
				LeaveCriticalSection(&lpDirectoryTable->CriticalSection);
				ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, NULL);
			}
			lpDirectoryTable->dwAllocated  += dwCacheBucketSize;
			lpDirectoryTable->lpDirectory  = lpDirArray;
		}
	}

	//  Insert new directory to database
	iResult = QuickInsert2(lpDirectoryTable->lpDirectory, lpDirectoryTable->dwDirectories,
		                   lpTempDir, (QUICKCOMPAREPROC) CompareDirectoryName);

	if (iResult < 0)
	{
		//  Directory was not found in cache, so replace the faked entry with a real one...
		lpDirectory = (LPDIRECTORY) Allocate("Directory", sizeof(DIRECTORY) + (dwFileName+1)*sizeof(TCHAR));
		if (!lpDirectory)
		{
			// yank out our temp entry since we can't make a real one...
			QuickDeleteIndex(lpDirectoryTable->lpDirectory, lpDirectoryTable->dwDirectories, -iResult);
			return NULL;
		}

		lpDirectoryTable->lpDirectory[-iResult-1] = lpDirectory;
		lpDirectoryTable->dwDirectories++;
		lpDirectory->hEvent           = INVALID_HANDLE_VALUE;
		lpDirectory->bPopulated       = FALSE;
		lpDirectory->lForceUpdate     = TRUE;
		lpDirectory->bHasFakeSubDirs  = FALSE;
		lpDirectory->bLocked          = FALSE;
		lpDirectory->lReferenceCount  = 2;
		lpDirectory->lpDirectoryInfo  = NULL;
		lpDirectory->dwFileName       = dwFileName;
		CopyMemory(lpDirectory->tszFileName, tszFileName, dwFileName*sizeof(TCHAR));
		lpDirectory->tszFileName[dwFileName] = 0;
		lpDirectory->Hash             = lpTempDir->Hash;
		bDoUpdate = TRUE;
		LeaveCriticalSection(&lpDirectoryTable->CriticalSection);
	}
	else
	{
		lpDirectory  = lpDirectoryTable->lpDirectory[iResult - 1];
		bDoUpdate = AcquireDirCacheLock(lpDirectoryTable, lpDirectory);
	}

	if (lpOldDirectory)
	{
		FreeDirectoryInfo(lpOldDirectory->lpDirectoryInfo, TRUE);
		Free(lpOldDirectory);
	}

	if (pbDoUpdate)
	{
		*pbDoUpdate = bDoUpdate;
	}

	return lpDirectory;
}


void ReleaseDirCacheLock2(LPDIRECTORYTABLE lpDirectoryTable, LPDIRECTORY lpDirectory, BOOL bDelete)
{
	BOOL               bFree;
	HANDLE             hEvent;

	if (lpDirectory->lReferenceCount == 1)
	{
		hEvent  = lpDirectory->hEvent;
		if (!bDelete)
		{
			lpDirectory->hEvent  = INVALID_HANDLE_VALUE;
			bFree = FALSE;
			if (!lpDirectory->bLocked)
			{
				// This change means a refcount=1 which is a currently unopen item that should
				// be on the LRU list won't be because we need to keep it around so we can
				// see that it's been locked while a move/delete is under way.
				INSERTLIST(lpDirectory, lpDirectoryTable->lpDirectoryList);
			}
		}
		else
		{
			bFree = TRUE;
			QuickDelete(lpDirectoryTable->lpDirectory, lpDirectoryTable->dwDirectories--, lpDirectory, (QUICKCOMPAREPROC) CompareDirectoryName, NULL);
		}
	}
	else
	{
		bFree = FALSE;
		hEvent  = INVALID_HANDLE_VALUE;
		SetEvent(lpDirectory->hEvent);
	}

	if (hEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hEvent);
	}

	if (bFree)
	{
		FreeDirectoryInfo(lpDirectory->lpDirectoryInfo, TRUE);
		Free(lpDirectory);
	}
}



void ReleaseDirCacheLock(LPDIRECTORY lpDirectory, BOOL bDelete)
{
	LPDIRECTORYTABLE   lpDirectoryTable;

	lpDirectoryTable  = &DirectoryTable[lpDirectory->Hash % dwCacheBuckets];

	EnterCriticalSection(&lpDirectoryTable->CriticalSection);
	//  Decrement directory reference count
	lpDirectory->lReferenceCount--;
	ReleaseDirCacheLock2(lpDirectoryTable, lpDirectory, bDelete);
	LeaveCriticalSection(&lpDirectoryTable->CriticalSection);
}



static BOOL UpdateDirectory(LPDIRECTORY lpDirectory, BOOL bRecursive, BOOL bFakeDirs)
{
  LPFILEINFO      *lpFileInfoArray, lpFile, lpParent;
  WIN32_FIND_DATA    FindData;
  WIN32_FILE_ATTRIBUTE_DATA DirData;
  LPTSTR        tszFileName;
  LPTSTR        tszTemp;
  FILETIME      ftRootEntry[3];
  LPDIRECTORYINFO    lpDirectoryInfo, lpPopulated;
  LPVOID        lpMemory;
  UINT64        FileSize, DirectorySize;
  HANDLE        hFind, hFile;
  DWORD        dwItems, dwSize, dwFileName, dwSubDirectories, dwError, dwBytesRead;
  BOOL        bReturn, bDirectory, bFakeEntry;
  UINT        pBuffer[1024];

  bFakeEntry = FALSE;
  bReturn  = FALSE;
  dwError  = NO_ERROR;
  if (! lpDirectory) return FALSE;
  // Fill with -1 since FILETIME's must be less than 0x8000000000000000.
  FillMemory(ftRootEntry, sizeof(FILETIME)*3,-1);

  //  Allocate memory area for tables
  lpDirectoryInfo  = (LPDIRECTORYINFO)Allocate("Directory:Info", sizeof(DIRECTORYINFO) + lpDirectory->dwFileName*sizeof(TCHAR));
  if (! lpDirectoryInfo) return FALSE;
  ZeroMemory(lpDirectoryInfo, sizeof(DIRECTORYINFO));
  lpDirectoryInfo->dwRealPath = lpDirectory->dwFileName;
  CopyMemory(lpDirectoryInfo->tszRealPath, lpDirectory->tszFileName, lpDirectory->dwFileName+1);

  dwItems  = 0;
  dwSize  = (lpDirectory->lpDirectoryInfo ? max(lpDirectory->lpDirectoryInfo->dwDirectorySize, 25) : 25);

  //  Allocate table
  lpFileInfoArray  = (LPFILEINFO *)Allocate("Directory:Info:File", sizeof(LPFILEINFO) * dwSize);
  if (! lpFileInfoArray)
  {
    Free(lpDirectoryInfo);
    return FALSE;
  }

  //  Allocate memory for new root entry, don't use old one as it is shared!
  if (! lpDirectory->lpDirectoryInfo)
  {
	  lpParent  = NULL;
	  //  Calculate filename length
	  tszFileName  = lpDirectory->tszFileName;
	  dwFileName  = lpDirectory->dwFileName - 1;
	  while (dwFileName && lpDirectory->tszFileName[dwFileName] != _TEXT('\\')) dwFileName--;
	  dwFileName  = lpDirectory->dwFileName - (dwFileName + 1);

	  lpFile  = (LPFILEINFO)Allocate("Directory:RootEntry", sizeof(FILEINFO) + dwFileName * sizeof(TCHAR));
	  if (! lpFile)
	  {
		  Free(lpDirectoryInfo);
		  Free(lpFileInfoArray);
		  return FALSE;
	  }
	  ZeroMemory(lpFile, sizeof(FILEINFO));
	  //  Update file structure
	  lpFile->dwSafety          = 0xDEADBEAF;
	  lpFile->lReferenceCount    = 1;
	  lpFile->dwFileAttributes  = FILE_ATTRIBUTE_DIRECTORY;
	  lpFile->dwFileName  = dwFileName;
	  lpFile->Uid  = DefaultUid[1];
	  lpFile->Gid  = DefaultGid[1];
	  lpFile->dwFileMode  = dwDefaultFileMode[1];
	  CopyMemory(lpFile->tszFileName,
		  &tszFileName[lpDirectory->dwFileName - dwFileName], (dwFileName + 1) * sizeof(TCHAR));
  }
  else
  {
	  lpParent = lpDirectory->lpDirectoryInfo->lpRootEntry;
	  lpParent->dwFileAttributes |= FILE_ATTRIBUTE_DIRTY;
	  lpFile  = (LPFILEINFO)Allocate("Directory:RootEntry", sizeof(FILEINFO) + lpParent->dwFileName * sizeof(TCHAR) + lpParent->Context.dwData);
	  if (! lpFile)
	  {
		  Free(lpDirectoryInfo);
		  Free(lpFileInfoArray);
		  return FALSE;
	  }
	  // make sure to copy context as well
	  CopyMemory(lpFile, lpParent, sizeof(FILEINFO)+lpParent->dwFileName+lpParent->Context.dwData);
	  lpFile->dwSafety          = 0xDEADBEAF;
	  lpFile->lReferenceCount    = 1;
	  lpParent  = (lpParent->dwFileAttributes & FILE_ATTRIBUTE_IOFTPD ? lpParent : NULL);
	  lpFile->dwFileAttributes  = FILE_ATTRIBUTE_DIRECTORY;
	  lpFile->dwFileMode       &= S_ACCESS;
	  // update context pointer
	  lpFile->Context.lpData  = (LPVOID)((ULONG)lpFile + sizeof(FILEINFO) + lpFile->dwFileName * sizeof(TCHAR));
  }
  lpDirectoryInfo->lpRootEntry  = lpFile;

  //  Create search item to stack
  tszFileName  = _alloca((lpDirectory->dwFileName + MAX_PATH + 2) * sizeof(TCHAR));
  CopyMemory(tszFileName, lpDirectory->tszFileName, lpDirectory->dwFileName * sizeof(TCHAR));
  CopyMemory(&tszFileName[lpDirectory->dwFileName], _TEXT("\\*"), 3 * sizeof(TCHAR));

  hFind  = FindFirstFile(tszFileName, &FindData);
  if (hFind != INVALID_HANDLE_VALUE)
  {
    DirectorySize    = 0;
    dwSubDirectories  = 2;

    do
    {
		// real hidden/system files should be skipped
	  if ((FindData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ||
		  (FindData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)) continue;

      if (FindData.cFileName[0] == _TEXT('.'))
      {
        if (FindData.cFileName[1] == _TEXT('\0'))
        {
          CopyMemory(&ftRootEntry[0], &FindData.ftLastWriteTime, sizeof(FILETIME));
          CopyMemory(&ftRootEntry[1], &FindData.ftCreationTime, sizeof(FILETIME));
          continue;
        }
        if (! _tcsnicmp(FindData.cFileName, _TEXT(".ioFTPD"), 7) ||
          (FindData.cFileName[1] == _TEXT('.') && FindData.cFileName[2] == _TEXT('\0'))) continue;
        FindData.dwFileAttributes  |= FILE_ATTRIBUTE_HIDDEN;
      }

      if (dwItems == dwSize)
      {
        lpMemory  = ReAllocate(lpFileInfoArray, NULL, (dwSize + 25) * sizeof(LPFILEINFO));
        if (! lpMemory)
        {
          dwError  = ERROR_NOT_ENOUGH_MEMORY;
          break;
        }
        lpFileInfoArray  = (LPFILEINFO *)lpMemory;
        dwSize  += 25;
      }

      dwFileName  = _tcslen(FindData.cFileName) * sizeof(TCHAR);
      bDirectory  = FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
      lpFile    = FindFileInfo(FindData.dwFileAttributes, FindData.cFileName, lpDirectory->lpDirectoryInfo);
      FileSize  = FindData.nFileSizeLow + (FindData.nFileSizeHigh * 0x100000000);

	  if (lpFile && _tcscmp(FindData.cFileName, lpFile->tszFileName))
	  {
		  // name must differ in case, overwrite with new one
		  CopyMemory(lpFile->tszFileName, FindData.cFileName, lpFile->dwFileName);
	  }
	  
	  if (lpFile &&
        ! (lpFile->dwFileAttributes & FILE_ATTRIBUTE_DIRTY) &&
		(!lpFile->lpLinkedRoot || !(lpFile->lpLinkedRoot->dwFileAttributes & FILE_ATTRIBUTE_DIRTY)) &&
		! CompareFileTime(&FindData.ftLastWriteTime, &lpFile->ftModificationTime) &&
		((bDirectory && (bFakeDirs || (!bFakeDirs && !(lpFile->dwFileAttributes & FILE_ATTRIBUTE_FAKE)))) ||
		 (!bDirectory && (FileSize == lpFile->FileSize))))
      {
        //  Fileinfo was found, and was up-to date
        if (! QuickInsert(lpFileInfoArray, dwItems, lpFile, (QUICKCOMPAREPROC) CompareFileName))
        {
          if (bDirectory) dwSubDirectories++;
		  if (bDirectory && lpFile->dwFileAttributes & FILE_ATTRIBUTE_FAKE) bFakeEntry = TRUE;
          InterlockedIncrement(&lpFile->lReferenceCount);
          dwItems++;
        }
      }
      else if (lpFile && ! bDirectory)
      {
        //  Update fileinfo structure
        lpFile->FileSize  = FileSize;
        CopyMemory(&lpFile->ftModificationTime, &FindData.ftLastWriteTime, sizeof(FILETIME));

        //  Fileinfo was found, and it was updated
        if (! QuickInsert(lpFileInfoArray, dwItems, lpFile, (QUICKCOMPAREPROC) CompareFileName))
        {
          if (bDirectory) dwSubDirectories++;
          InterlockedIncrement(&lpFile->lReferenceCount);
          dwItems++;
        }
      }
      else
      {
        //  Fileinfo was not found, or is shared
        if (bDirectory)
        {
          if (bRecursive)
          {
			  CopyMemory(
				  &tszFileName[lpDirectory->dwFileName + 1],
				  FindData.cFileName,
				  dwFileName + sizeof(TCHAR));

			  //  Get populated information for directory
			  lpPopulated  = OpenDirectory(tszFileName, FALSE, FALSE, FALSE, NULL, NULL);

			  if (! lpPopulated)
			  {
				  if ((dwError = GetLastError()) != ERROR_FILE_NOT_FOUND &&
					  dwError != ERROR_PATH_NOT_FOUND &&
					  dwError != ERROR_ACCESS_DENIED &&
					  dwError != ERROR_DIRECTORY_LOCKED) break;
				  dwError  = NO_ERROR;
				  continue;
			  }
			  //  Get root entry, should always exist!
			  lpFile  = lpPopulated->lpRootEntry;
			  InterlockedIncrement(&lpFile->lReferenceCount);
			  CloseDirectory(lpPopulated);
		  }
		  else if (!bFakeDirs)
		  {
			  dwSubDirectories++;
			  lpDirectory->lForceUpdate  = TRUE;
			  continue;
		  }
		  else
		  {
			  bFakeEntry = TRUE;
			  // we're gonna fake out the directory entry though :)
			  lpFile  = (LPFILEINFO)Allocate("Directory:Info:Dir:Fake", sizeof(FILEINFO) + dwFileName);
			  if (! lpFile)
			  {
				  dwError  = ERROR_NOT_ENOUGH_MEMORY;
				  break;
			  }
			  //  Udpate fileinfo structure
			  lpFile->dwSafety          = 0xDEADBEAF;
			  lpFile->lReferenceCount  = 1;
			  lpFile->dwFileName  = dwFileName / sizeof(TCHAR);
			  CopyMemory(lpFile->tszFileName, FindData.cFileName, dwFileName + sizeof(TCHAR));
			  CopyMemory(&lpFile->ftModificationTime, &FindData.ftLastWriteTime, sizeof(FILETIME));
			  ZeroMemory(&lpFile->ftAlternateTime, sizeof(FILETIME));
			  lpFile->Context.dwData = 0;
			  lpFile->Context.lpData = 0;
			  lpFile->FileSize  = FileSize;
			  lpFile->dwFileAttributes  = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_FAKE;
			  lpFile->dwSubDirectories = 0;
			  lpFile->lpLinkedRoot = NULL;
			  lpFile->dwUploadTimeInMs = 0;

			  // dwFileAttributes, dwFileMode, Uid, Gid, Context
			  lpFile->Uid  = DefaultUid[1];
			  lpFile->Gid  = DefaultGid[1];
			  lpFile->dwFileMode  = dwDefaultFileMode[1];

			  tszTemp = &tszFileName[lpDirectory->dwFileName + 1],
			  CopyMemory(tszTemp, FindData.cFileName, dwFileName + sizeof(TCHAR));
			  tszTemp += dwFileName;
			  CopyMemory(tszTemp, _T("\\.ioFTPD"), 9 * sizeof(TCHAR));

			  hFile  = CreateFile(tszFileName, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
				  NULL, OPEN_EXISTING, 0, NULL);
			  if (hFile != INVALID_HANDLE_VALUE)
			  {
				  if (ReadFile(hFile, pBuffer, sizeof(pBuffer), &dwBytesRead, NULL) &&
					  dwBytesRead >= (11 * sizeof(UINT)) &&
					  pBuffer[0] == 1 &&
					  pBuffer[1] &&
					  pBuffer[2] & 1)
				  {
					  // the first entry is the root entry, so we need to read it
					  if (pBuffer[5] && pBuffer[5] < 1024*128)
					  {
						  // file context exists
						  lpFile  = (LPFILEINFO)ReAllocate(lpFile, "Directory:Info:Dir:Fake",
							  sizeof(FILEINFO) + lpFile->dwFileName * sizeof(TCHAR) + pBuffer[5]);
						  lpFile->Context.dwData = pBuffer[5];
						  lpFile->Context.lpData = (LPVOID)((ULONG)lpFile + sizeof(FILEINFO)
							  + lpFile->dwFileName * sizeof(TCHAR));
						  if (lpFile->Context.dwData < (sizeof(pBuffer)-11*sizeof(UINT)))
						  {
							  // we read the whole thing already so just copy it from buffer
							  CopyMemory(lpFile->Context.lpData, &pBuffer[11], pBuffer[5]);
						  }
						  else
						  {
							  // it's a huge context so just read it from the file and ignore
							  // the part we already have
							  SetFilePointer(hFile, 11*sizeof(UINT), NULL, FILE_BEGIN);
							  if (!ReadFile(hFile, lpFile->Context.lpData, pBuffer[5], &dwBytesRead, NULL) ||
								  dwBytesRead != pBuffer[5])
							  {
								  // an error happened so just skip it
								  lpFile->Context.dwData = 0;
								  lpFile->Context.lpData = 0;
							  }
						  }
						  if (!ValidFileContext(&lpFile->Context))
						  {
							  lpFile->Context.dwData = 0;
							  lpFile->Context.lpData = NULL;
						  }
					  }
					  lpFile->Uid = pBuffer[6];
					  lpFile->Gid = pBuffer[7];
					  lpFile->dwFileMode = pBuffer[8];
					  lpFile->dwFileAttributes  |= FILE_ATTRIBUTE_IOFTPD;
					  lpFile->ftAlternateTime.dwLowDateTime   = pBuffer[9];
					  lpFile->ftAlternateTime.dwHighDateTime  = pBuffer[10];

				  }
				  CloseHandle(hFile);
			  }
		  }
		}
        else
        {
          lpFile  = (LPFILEINFO)Allocate("Directory:Info:File:Info", sizeof(FILEINFO) + dwFileName);
          if (! lpFile)
          {
            dwError  = ERROR_NOT_ENOUGH_MEMORY;
            break;
          }
          //  Update fileinfo structure
		  lpFile->dwSafety         = 0xDEADBEAF;
          lpFile->lReferenceCount  = 1;
          lpFile->FileSize  = FileSize;
          if (lpParent)
          {
            lpFile->Uid    = lpParent->Uid;
            lpFile->Gid    = lpParent->Gid;
          }
          else
          {
            lpFile->Uid    = DefaultUid[0];
            lpFile->Gid    = DefaultGid[0];
          }
		  lpFile->dwSubDirectories  = 0;
          lpFile->dwFileAttributes  = 0;
		  lpFile->dwUploadTimeInMs  = 0;
          lpFile->dwFileMode  = dwDefaultFileMode[0];
          lpFile->dwFileName  = dwFileName / sizeof(TCHAR);
		  lpFile->lpLinkedRoot = NULL;
          CopyMemory(lpFile->tszFileName, FindData.cFileName, dwFileName + sizeof(TCHAR));
          CopyMemory(&lpFile->ftModificationTime, &FindData.ftLastWriteTime, sizeof(FILETIME));
		  ZeroMemory(&lpFile->ftAlternateTime, sizeof(FILETIME));
          ZeroMemory(&lpFile->Context, sizeof(FILECONTEXT));
        }

        if (QuickInsert(lpFileInfoArray, dwItems, lpFile, (QUICKCOMPAREPROC) CompareFileName))
        {
			CloseFileInfo(lpFile);
        }
        else
        {
          if (bDirectory) dwSubDirectories++;
          dwItems++;
        }
      }
      DirectorySize  += FileSize;

    } while (FindNextFile(hFind, &FindData));
    FindClose(hFind);
  }
  else 
  {
    dwError  = GetLastError();
    DirectorySize = 0;
    dwSubDirectories = 0;
  }

  lpDirectoryInfo->lpRootEntry->FileSize  = DirectorySize;
  lpDirectoryInfo->lReferenceCount  = 1;
  lpDirectoryInfo->dwDirectorySize  = dwItems;
  lpDirectoryInfo->lpRootEntry->dwSubDirectories  = dwSubDirectories;
  lpDirectoryInfo->lpFileInfo  = lpFileInfoArray;

  if (dwError == NO_ERROR)
  {
	  lpFile = lpDirectoryInfo->lpRootEntry;
	  lpFile->FileSize  = DirectorySize;
	  lpFile->dwSubDirectories  = dwSubDirectories;

	  if (!CompareFileTime(&ftRootEntry[0], &ftRootEntry[2]) &&
		  GetFileAttributesEx(lpDirectory->tszFileName, GetFileExInfoStandard, &DirData))
	  {
		  // need to get the modification/creation times for drive letters
		  // because Find*File won't return a '.' entry.
		  CopyMemory(&lpFile->ftModificationTime, &DirData.ftLastWriteTime, sizeof(FILETIME));
	  }
	  else
	  {
		  CopyMemory(&lpFile->ftModificationTime, &ftRootEntry[0], sizeof(FILETIME));
	  }

	  //  Update directory structure
	  FreeDirectoryInfo(lpDirectory->lpDirectoryInfo, TRUE);
	  lpDirectory->lpDirectoryInfo  = lpDirectoryInfo;
	  if (! lpDirectory->bPopulated) lpDirectory->bPopulated  = bRecursive;
	  lpDirectory->bHasFakeSubDirs = bFakeEntry;
	  bReturn  = TRUE;
  }
  else
  {
	  FreeDirectoryInfo(lpDirectoryInfo, TRUE);
	  SetLastError(dwError);
  }

  return bReturn;
}



BOOL WriteDirectoryPermissions(LPDIRECTORY lpDirectory, LPTSTR tszName, DWORD dwName)
{
  UINT    pBuffer[1024], Crc;
  LPUINT    pOffset;
  LPFILEINFO  *lpFileInfo, lpFile;
  HANDLE    hFile;
  DWORD    n, dwBytesWritten, dwItems, dwFlags;
  LPTSTR    tszFileName;

  pOffset  = &pBuffer[2];
  //  Copy filename to stack
  tszFileName  = _alloca((dwName + 9) * sizeof(TCHAR));
  CopyMemory(tszFileName, tszName, dwName * sizeof(TCHAR));
  CopyMemory(&tszFileName[dwName], _TEXT("\\.ioFTPD"), 9 * sizeof(TCHAR));

  hFile  = CreateFile(tszFileName, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
    NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
  if (hFile == INVALID_HANDLE_VALUE) return FALSE;

  dwItems  = 0;
  dwFlags  = 0;
  lpFile  = lpDirectory->lpDirectoryInfo->lpRootEntry;

  SetFilePointer(hFile, sizeof(UINT) * 3, NULL, FILE_BEGIN);
  //  Write directory entry to disk
  if (lpFile->dwFileAttributes & FILE_ATTRIBUTE_IOFTPD)
  {
    dwFlags  |= 1;
    dwItems++;
    pOffset[0]  = lpFile->Context.dwData;
    pOffset[1]  = lpFile->Uid;
    pOffset[2]  = lpFile->Gid;
    pOffset[3]  = lpFile->dwFileMode & S_ACCESS;
	pOffset[4]  = lpFile->ftAlternateTime.dwLowDateTime;
	pOffset[5]  = lpFile->ftAlternateTime.dwHighDateTime;
    pOffset    = &pOffset[6];

    if (lpFile->Context.dwData)
    {
      if (((ULONG)pOffset - (ULONG)pBuffer + lpFile->Context.dwData) <= sizeof(pBuffer))
      {
        CopyMemory(pOffset, lpFile->Context.lpData, lpFile->Context.dwData);
        pOffset  = (LPUINT)((ULONG)pOffset + lpFile->Context.dwData);
      }
      else
      {
        //  Not enough buffer space, dump context to file
        SetFilePointer(hFile, (ULONG)pOffset - (ULONG)pBuffer, NULL, FILE_CURRENT);
        WriteFile(hFile, lpFile->Context.lpData, lpFile->Context.dwData, &dwBytesWritten, NULL);

        //  Create header for block
        Crc  = CalculateCrc32((PCHAR)&pBuffer[2], (ULONG)pOffset - (ULONG)&pBuffer[2], NULL);
        pBuffer[1]  = CalculateCrc32(lpFile->Context.lpData, lpFile->Context.dwData, &Crc);
        pBuffer[0]  = lpFile->Context.dwData + ((ULONG)pOffset - (ULONG)pBuffer);

        //  Write block to file
        SetFilePointer(hFile, -(LONG)pBuffer[0], NULL, FILE_CURRENT);
        WriteFile(hFile, pBuffer, (ULONG)pOffset - (ULONG)pBuffer, &dwBytesWritten, NULL);
        SetFilePointer(hFile, lpFile->Context.dwData, NULL, FILE_CURRENT);

        pOffset  = &pBuffer[2];
      }
    }
  }

  lpFileInfo  = lpDirectory->lpDirectoryInfo->lpFileInfo;
  //  Write file entries to disk
  for (n = lpDirectory->lpDirectoryInfo->dwDirectorySize;n--;)
  {
    lpFile  = lpFileInfo[n];
    if (lpFile->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ||
      ! (lpFile->dwFileAttributes & FILE_ATTRIBUTE_IOFTPD)) continue;

    if (sizeof(pBuffer) - ((ULONG)pOffset - (ULONG)pBuffer) < ((lpFile->dwFileName + 1) * sizeof(TCHAR) + 6 * sizeof(UINT)))
    {
      //  Create header for block
      pBuffer[0]  = (ULONG)pOffset - (ULONG)pBuffer;
      pBuffer[1]  = CalculateCrc32((PCHAR)&pBuffer[2], pBuffer[0] - 2 * sizeof(UINT), NULL);

      //  Write block to file
      WriteFile(hFile, pBuffer, pBuffer[0], &dwBytesWritten, NULL);
      pOffset  = &pBuffer[2];
    }

    dwItems++;
    //  Copy fileinfo to write buffer
    pOffset[0]  = lpFile->dwFileName + 1;
    pOffset[1]  = lpFile->Uid;
    pOffset[2]  = lpFile->Gid;
	// Shouldn't need to AND with S_ACCESS since these aren't directories and the new
	// attributes only apply to directories, but playing it safe.
    pOffset[3]  = lpFile->dwFileMode & S_ACCESS;
	pOffset[4]  = lpFile->dwUploadTimeInMs;
    pOffset[5]  = 0;
    CopyMemory(&pOffset[6], lpFile->tszFileName, pOffset[0] * sizeof(TCHAR));
    pOffset    = (LPUINT)(pOffset[0] * sizeof(TCHAR) + (ULONG)&pOffset[6]);
  }

  if (pOffset != &pBuffer[2])
  {
    //  Create header for block
    pBuffer[0]  = (ULONG)pOffset - (ULONG)pBuffer;
    pBuffer[1]  = CalculateCrc32((PCHAR)&pBuffer[2], pBuffer[0] - 2 * sizeof(UINT), NULL);

    //  Write block to file
    WriteFile(hFile, pBuffer, pBuffer[0], &dwBytesWritten, NULL);      
    pOffset  = &pBuffer[2];
  }
  SetEndOfFile(hFile);

  //  Write header
  pBuffer[0]  = 1;    //  Database version
  pBuffer[1]  = dwItems;  //  Database entries
  pBuffer[2]  = dwFlags;  //  Database flags/attributes

  SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
  WriteFile(hFile, pBuffer, sizeof(UINT) * 3, &dwBytesWritten, NULL);  
  CloseHandle(hFile);

  return TRUE;
}






// only called while the directory is locked and after a successful UpdateDirectory,
// it may reallocate the root entry if it has a context.
BOOL ReadDirectoryPermissions(LPDIRECTORY lpDirectory)
{
  LPFILEINFO  lpFile, lpParent;
  LPTSTR    tszFileName;
  LPUINT    pBuffer, pOffset;
  LPVOID    lpMemory;
  UINT    BlockSize, BlockCrc, ItemSize;
  DWORD    dwBufferSize, dwBytesRead, dwItems, dwFlags, n, z;
  BOOL    bContinue, bCleanUp, bError;
  HANDLE    hFile;

  //  Allocate read buffer
  dwBufferSize  = 1024;
  bError    = FALSE;
  bCleanUp  = FALSE;
  pBuffer  = (LPUINT)Allocate("ReadBuffer", dwBufferSize * sizeof(UINT));
  if (! pBuffer) return FALSE;

  //  Copy filename to stack
  tszFileName  = _alloca((lpDirectory->dwFileName + 9) * sizeof(TCHAR));
  CopyMemory(tszFileName, lpDirectory->tszFileName, lpDirectory->dwFileName * sizeof(TCHAR));
  CopyMemory(&tszFileName[lpDirectory->dwFileName], _TEXT("\\.ioFTPD"), 9 * sizeof(TCHAR));

  //  Open file
  hFile  = CreateFile(tszFileName, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
    NULL, OPEN_EXISTING, 0, NULL);
  if (hFile == INVALID_HANDLE_VALUE)
  {
    n  = GetLastError();
    Free(pBuffer);
    if (n != ERROR_FILE_NOT_FOUND) return FALSE;
    return TRUE;
  }

  if (ReadFile(hFile, pBuffer, 5 * sizeof(UINT), &dwBytesRead, NULL) &&
    pBuffer[0] == 1 && dwBytesRead >= (3 * sizeof(UINT)))
  {
    dwItems  = pBuffer[1];
    dwFlags  = pBuffer[2];
    pOffset  = &pBuffer[3];
    bContinue  = (dwItems ? TRUE : FALSE);

    for (n = 0;bContinue;)
    {
      BlockSize  = pOffset[0];
	  if (BlockSize <= 2 * sizeof(UINT) || BlockSize > 1024*128) break;
	  BlockCrc  = pOffset[1];

      //  Verify that block fits in buffer
      if (BlockSize > (dwBufferSize * sizeof(UINT)))
      {
        if (BlockSize > 1024 * 128) break;
        dwBufferSize  = (BlockSize / sizeof(UINT)) + 5;
        lpMemory  = ReAllocate(pBuffer, NULL, dwBufferSize * sizeof(UINT));
        if (! lpMemory)
        {
          bError  = TRUE;
          break;
        }
        pBuffer  = (LPUINT)lpMemory;
      }

      //  Read block to buffer
      if (! ReadFile(hFile, pBuffer, BlockSize, &dwBytesRead, NULL)) break;

      if (dwBytesRead < BlockSize)
      {
        if ((dwBytesRead + 2 * sizeof(UINT)) != BlockSize) break;
        bContinue  = FALSE;
      }
      BlockSize  -= (2 * sizeof(UINT));

      //  Perform crc check
      if (BlockCrc != CalculateCrc32((PCHAR)pBuffer, BlockSize, NULL))
      {
        pOffset  = &pBuffer[BlockSize / sizeof(UINT)];
        continue;
      }

      //  Process buffer
      for (pOffset = pBuffer;BlockSize;BlockSize -= ItemSize)
      {
        if (! n++ && (dwFlags & 1))
        {
          //  Root entry, safe to manipulate this since we hold the dircachelock whenever this
			// routine is called and we have always called UpdateDirectory before this while the
			// lock is held so we know we got a new un-shared copy at this time.
          ItemSize  = (sizeof(UINT) * 6) + pOffset[0];
          lpFile  = lpDirectory->lpDirectoryInfo->lpRootEntry;

          lpFile->Uid  = min(pOffset[1], MAX_UID);
          lpFile->Gid  = min(pOffset[2], MAX_GID);
          lpFile->dwFileMode  = pOffset[3];
          lpFile->dwFileAttributes  |= FILE_ATTRIBUTE_IOFTPD;
		  lpFile->ftAlternateTime.dwLowDateTime   = pOffset[4];
		  lpFile->ftAlternateTime.dwHighDateTime  = pOffset[5];

          //  Copy directory context to memory
          if ((lpFile->Context.dwData = pOffset[0]))
          {
			  lpFile  = (LPFILEINFO)ReAllocate(lpFile, NULL,
				  sizeof(FILEINFO) + lpFile->dwFileName * sizeof(TCHAR) + pOffset[0]);
            if (! lpFile)
            {
              bError  = TRUE;
              break;
            }
            lpDirectory->lpDirectoryInfo->lpRootEntry  = lpFile;
			lpFile->Context.lpData  = (LPVOID)((ULONG)lpFile + sizeof(FILEINFO) + lpFile->dwFileName * sizeof(TCHAR));
            CopyMemory(lpFile->Context.lpData, &pOffset[6], pOffset[0]);
			if (!ValidFileContext(&lpFile->Context))
			{
				lpFile->Context.dwData = 0;
				lpFile->Context.lpData = NULL;
			}
			lpFile->dwFileMode |= GetFileModeContextFlags(&lpFile->Context);
          }

          lpParent  = lpFile;
          //  Update file ownerships
          for (z = lpDirectory->lpDirectoryInfo->dwDirectorySize;z--;)
          {
            lpFile  = lpDirectory->lpDirectoryInfo->lpFileInfo[z];
            if (! (lpFile->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
              lpFile->Uid  = lpParent->Uid;
              lpFile->Gid  = lpParent->Gid;
			  bCleanUp = TRUE;
            }
          }
        }
        else
        {
          //  File entry
          ItemSize  = (sizeof(UINT) * 6) + (pOffset[0] * sizeof(TCHAR));
          lpFile  = FindFileInfo(0, (LPTSTR)&pOffset[6], lpDirectory->lpDirectoryInfo);

          //  Update fileinfo
          if (lpFile &&
            ! (lpFile->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
          {
            lpFile->Uid  = min(pOffset[1], MAX_UID);
            lpFile->Gid  = min(pOffset[2], MAX_GID);
            lpFile->dwFileMode  = pOffset[3];
			lpFile->dwUploadTimeInMs = pOffset[4];

            lpFile->dwFileAttributes  |= FILE_ATTRIBUTE_IOFTPD;
          }
          else bCleanUp  = TRUE;
        }
        pOffset  = (LPUINT)((ULONG)pOffset + ItemSize);
      }
    }
  }
  CloseHandle(hFile);
  Free(pBuffer);

  if (bError)
  {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return FALSE;
  }

  //  Re-write .ioFTPD file
  if (bCleanUp) WriteDirectoryPermissions(lpDirectory, lpDirectory->tszFileName, lpDirectory->dwFileName);

  return TRUE;
}




BOOL MarkDirectory2(LPDIRECTORY lpDirectory, LPTSTR tszFileName)
{
	LPDIRECTORYTABLE  lpDirectoryTable;
	LPDIRECTORY       *lpResult, lpDir;
	LPFILEINFO        lpFile;

	lpDirectoryTable  = &DirectoryTable[lpDirectory->Hash % dwCacheBuckets];
	EnterCriticalSection(&lpDirectoryTable->CriticalSection);

	//  Find directory from cache
	lpResult  = (LPDIRECTORY *)bsearch(&lpDirectory, lpDirectoryTable->lpDirectory, lpDirectoryTable->dwDirectories,
		sizeof(LPDIRECTORY), (QUICKCOMPAREPROC) CompareDirectoryName);

	if (lpResult)
	{
		lpDir = *lpResult;
		lpDir->lForceUpdate  = TRUE;

		if (tszFileName)
		{
			AcquireDirCacheLock(lpDirectoryTable, lpDir);
			lpDir->lForceUpdate  = TRUE;

			if (lpDir->lpDirectoryInfo)
			{
				lpDir->lpDirectoryInfo->lpRootEntry->dwFileAttributes |= FILE_ATTRIBUTE_DIRTY;
				if (lpDir->lpDirectoryInfo->lpLinkedInfo)
				{
					lpDir->lpDirectoryInfo->lpLinkedInfo->lpRootEntry->dwFileAttributes |= FILE_ATTRIBUTE_DIRTY;
				}

				//  Find child by name
				lpFile  = FindFileInfo(FILE_ATTRIBUTE_DIRECTORY, tszFileName, lpDir->lpDirectoryInfo);

				if (lpFile)
				{
					lpFile->dwFileAttributes |= FILE_ATTRIBUTE_DIRTY;
				}

			}
			// this does an implicit LeaveCriticalSection(&lpDirectoryTable->CriticalSection)...
			ReleaseDirCacheLock(lpDir, FALSE);
		}
		else
		{
			LeaveCriticalSection(&lpDirectoryTable->CriticalSection);
		}
	}
	else
	{
		LeaveCriticalSection(&lpDirectoryTable->CriticalSection);
	}

	return TRUE;
}




BOOL MarkDirectory(LPTSTR tszFileName)
{
	LPDIRECTORY       lpDirectory;
	DWORD             dwFileName;

	//  Trim filename
	dwFileName  = _tcslen(tszFileName) * sizeof(TCHAR);
	lpDirectory = _alloca(sizeof(DIRECTORY) + dwFileName + sizeof(TCHAR));
	if (!lpDirectory) return FALSE;

	//  Create search item
	dwFileName  = TrimFileName(tszFileName, lpDirectory->tszFileName);
	lpDirectory->dwFileName = dwFileName * sizeof(TCHAR);
	lpDirectory->Hash       = HashFileName(tszFileName, dwFileName);

	MarkDirectory2(lpDirectory, NULL);
	return TRUE;
}


BOOL MarkParent(LPTSTR tszFileName, BOOL bParent2)
{
	LPDIRECTORY       lpDirectory;
	DWORD             dwFileName;

	//  Trim filename
	dwFileName  = _tcslen(tszFileName) * sizeof(TCHAR);
	lpDirectory = _alloca(sizeof(DIRECTORY) + dwFileName + sizeof(TCHAR));
	if (!lpDirectory) return FALSE;

	//  Create search item
	dwFileName  = TrimFileName(tszFileName, lpDirectory->tszFileName);
	for ( ; dwFileName > 3; dwFileName--)
	{
		if (lpDirectory->tszFileName[dwFileName-1] == _T('\\'))
		{
			lpDirectory->tszFileName[--dwFileName] = 0;
			break;
		}
	}

	lpDirectory->dwFileName = dwFileName * sizeof(TCHAR);
	lpDirectory->Hash       = HashFileName(tszFileName, dwFileName);

	MarkDirectory2(lpDirectory, NULL);

	if (bParent2)
	{
		dwFileName--;
		for ( ; dwFileName > 3; dwFileName--)
		{
			if (lpDirectory->tszFileName[dwFileName-1] == _T('\\'))
			{
				lpDirectory->tszFileName[--dwFileName] = 0;
				break;
			}
		}

		lpDirectory->dwFileName = dwFileName * sizeof(TCHAR);
		lpDirectory->Hash       = HashFileName(tszFileName, dwFileName);

		MarkDirectory2(lpDirectory, NULL);
	}
	return TRUE;
}



BOOL UpdateFileInfo(LPTSTR tszFileName, LPVFSUPDATE lpData)
{
	LPDIRECTORY      lpDirectory, lpParent;
	LPDIRECTORYINFO  lpDirInfo, lpNewInfo;
	LPFILEINFO       lpFile, lpNewRoot, lpFileInfo;
	LPTSTR           tszPath;
	DWORD            dwFileName, dwFileAttributes, dwSize, dwSlash, dwPath, dwError, n;
	BOOL             bDirectory, bReturn, bWrite, bUpdateParent;
	TCHAR            tTemp;

	//  Determinate file type and verify existence (directory/file)
	dwFileAttributes  = GetFileAttributes(tszFileName);
	if (dwFileAttributes == INVALID_FILE_ATTRIBUTES) return FALSE;
	bDirectory  = (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? TRUE : FALSE);

	//  Trim filename
	dwFileName  = _tcslen(tszFileName) * sizeof(TCHAR);
	tszPath  = (LPTSTR)_alloca(dwFileName + sizeof(TCHAR));
	if (! tszPath)
	{
		return FALSE;
	}
	dwPath  = TrimFileName(tszFileName, tszPath);
	if (!dwPath)
	{
		return FALSE;
	}

	//  Find first slash/backslash
	for (dwSlash = dwPath - 1;dwSlash && tszPath[dwSlash] != _TEXT('\\');dwSlash--);
	if (! bDirectory)
	{
		if (! dwSlash)
		{
			return FALSE;
		}
		tszFileName  = &tszPath[dwSlash + 1];
		dwPath  = dwSlash + (dwSlash == 2 && tszPath[1] == _TEXT(':') ? 1 : 0);
	}
	else if (dwSlash == 2 && dwPath > 3 && tszPath[1] == _TEXT(':')) dwSlash  = 3;

	tTemp = tszPath[dwPath];
	tszPath[dwPath] = 0;
	if (!OpenDirectory(tszPath, FALSE, FALSE, FALSE, NULL, &lpDirectory) || ! lpDirectory )
	{
		return FALSE;
	}
	tszPath[dwPath] = tTemp;

	bReturn    = FALSE;
	bUpdateParent  = FALSE;
	bWrite     = TRUE;
	dwError    = NO_ERROR;

	// IMPORTANT NOTE:  Since we hold the directory lock we know we are the only writer to
	// the fileinfos.  We use this to cheat and update the permission info for files in the
	// directory directly.  This is probably wrong, BUT is unlikely to cause any issue that
	// I can see.  On the other hand, the original code replaced the root entry in the dirinfo
	// and then decremented it's share count.  That's WRONG because the directory could be
	// opened and thus updating the root entry is bad, but even worse it would free the
	// root entry when it had never been incremented because it was counting on the dirinfo
	// reference count to keep it valid!  If the dirinfo is shared then a new one is now
	// created and the root entry is always recreated and properly handled.

	if (lpDirectory->lpDirectoryInfo->lpLinkedInfo)
	{
		// doh, we are an NTFS junction/symlink... so call on real directory instead...
		if (UpdateFileInfo(lpDirectory->lpDirectoryInfo->lpLinkedInfo->tszRealPath, lpData))
		{
			bReturn = TRUE;
			if (dwSlash >= 3) bUpdateParent  = TRUE;
		}
		else
		{
			dwError = GetLastError();
		}
		bWrite = FALSE;
	}
	else if (bDirectory)
	{
		do
		{
			lpDirInfo  = lpDirectory->lpDirectoryInfo;
			lpFileInfo = lpDirInfo->lpRootEntry;
			dwSize     = lpFileInfo->dwFileName * sizeof(TCHAR) + sizeof(FILEINFO);

			lpNewRoot  = (LPFILEINFO)Allocate("Directory:Info", dwSize + lpData->Context.dwData);
			if ( ! lpNewRoot )
			{
				dwError  = ERROR_NOT_ENOUGH_MEMORY;
				break;
			}

			// Copy over root entry details now, we'll fix up the fields below
			CopyMemory(lpNewRoot, lpFileInfo, dwSize);
			//  Force update for previous directory
			lpFileInfo->dwFileAttributes  |= FILE_ATTRIBUTE_DIRTY;

			if (lpDirInfo->lReferenceCount == 2)
			{
				// we can re-purpose the existing lpDirInfo

				// we are going to loose the reference to the original root entry in our re-purposed DirInfo
				// so it can't be decremented when we close the dir below so decrement the reference now and
				// free it if necessary though unlikely since it's probably still referenced by parent dir
				// though it's been marked as dirty now and we'll mark parent at end.
				CloseFileInfo(lpFileInfo);
			}
			else
			{
				// directory is opened somewhere, we need to create our own
				lpNewInfo = Allocate("Directory:Info", sizeof(DIRECTORYINFO) + lpDirInfo->dwRealPath*sizeof(TCHAR));
				if ( ! lpNewInfo )
				{
					Free(lpNewRoot);
					dwError  = ERROR_NOT_ENOUGH_MEMORY;
					break;
				}

				CopyMemory(lpNewInfo, lpDirInfo, sizeof(DIRECTORYINFO) + lpDirInfo->dwRealPath*sizeof(TCHAR));
				lpNewInfo->lReferenceCount = 2;  // Careful!  1 for Dir cache entry, 1 for open dir we are holding lock for
				// linkedinfo tested above so we know it's zero...

				//  Allocate new file table array as well since it will need to be freed with dir
				lpNewInfo->lpFileInfo  = (LPFILEINFO *)Allocate("Directory:Info:File", sizeof(LPFILEINFO) * lpDirInfo->dwDirectorySize);
				if (! lpNewInfo->lpFileInfo )
				{
					Free(lpNewRoot);
					Free(lpNewInfo);
					dwError  = ERROR_NOT_ENOUGH_MEMORY;
					break;
				}
				CopyMemory(lpNewInfo->lpFileInfo, lpDirInfo->lpFileInfo, sizeof(LPFILEINFO) * lpDirInfo->dwDirectorySize);

				// we also need to update the reference count for the content fileinfo's since they are now shared by both
				// the old dirinfo and the new one
				for (n = lpDirInfo->dwDirectorySize ; n-- ; )
				{
					lpFile  = lpDirInfo->lpFileInfo[n];
					InterlockedIncrement(&lpFile->lReferenceCount);
				}

				// Time to "close" old entry we got from OpenDirectory()
				CloseDirectory(lpDirInfo);

				// Time to remove the old entry lpDirectory... since we couldn't re-use the DirInfo (lref != 2)
				// this probably won't delete it unless between the test above and now the dir was closed somewhere.
				CloseDirectory(lpDirInfo);

				// update dirinfo with new one
				lpDirectory->lpDirectoryInfo = lpNewInfo;
				lpDirInfo = lpNewInfo;
			}

			// Point to new root entry
			lpDirInfo->lpRootEntry = lpNewRoot;

			//  Update fileinfo structure
			lpNewRoot->Uid  = lpData->Uid;
			lpNewRoot->Gid  = lpData->Gid;
			lpNewRoot->dwFileAttributes  |= FILE_ATTRIBUTE_IOFTPD;
			lpNewRoot->dwFileMode  = (lpData->dwFileMode & S_ACCESS) | GetFileModeContextFlags(&lpData->Context);
			lpNewRoot->ftAlternateTime  = lpData->ftAlternateTime;
			lpNewRoot->dwUploadTimeInMs = lpData->dwUploadTimeInMs;
			lpNewRoot->lReferenceCount  = 1;
			lpNewRoot->Context.lpData   = (LPVOID)((ULONG)lpNewRoot + dwSize);
			lpNewRoot->Context.dwData   = lpData->Context.dwData;
			// append context
			CopyMemory(lpNewRoot->Context.lpData, lpData->Context.lpData, lpData->Context.dwData);

			// Update file ownerships if necessary
			for (n = lpDirInfo->dwDirectorySize ; n-- ; )
			{
				lpFile  = lpDirInfo->lpFileInfo[n];
				if (! (lpFile->dwFileAttributes & (FILE_ATTRIBUTE_IOFTPD|FILE_ATTRIBUTE_DIRECTORY)) )
				{
					lpFile->Uid  = lpData->Uid;
					lpFile->Gid  = lpData->Gid;
				}
			}

			//  Resolve parent directory (x:\)
			if (dwSlash >= 3) bUpdateParent  = TRUE;
			bReturn  = TRUE;
		} while(0);
	}
	else
	{
		//  Find file
		lpFile  = FindFileInfo(0, tszFileName, lpDirectory->lpDirectoryInfo);

		if (lpFile && !(lpFile->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			//  Update entry
			lpFile->Uid  = lpData->Uid;
			lpFile->Gid  = lpData->Gid;
			lpFile->ftAlternateTime   = lpData->ftAlternateTime;
			lpFile->dwUploadTimeInMs  = lpData->dwUploadTimeInMs;
			lpFile->dwFileAttributes |= FILE_ATTRIBUTE_IOFTPD;
			lpFile->dwFileMode        = lpData->dwFileMode;

			bReturn  = TRUE;
		}
		else
		{
			dwError = ERROR_FILE_MISSING;
		}
	}
	if (bReturn && bWrite) WriteDirectoryPermissions(lpDirectory, lpDirectory->tszFileName, lpDirectory->dwFileName);

	CloseDirectory(lpDirectory->lpDirectoryInfo);
	ReleaseDirCacheLock(lpDirectory, FALSE);

	//  Force update on parent directory
	if (bUpdateParent)
	{
		lpParent = _alloca(sizeof(DIRECTORY) + (dwSlash + 1)*sizeof(TCHAR));
		if (lpParent)
		{
			lpParent->dwFileName  = dwSlash;
			CopyMemory(lpParent->tszFileName, tszPath, dwSlash);
			lpParent->tszFileName[dwSlash] = 0;
			lpParent->Hash = HashFileName(lpParent->tszFileName, dwSlash);

			MarkDirectory2(lpParent, &tszFileName[dwSlash+1]);
		}
	}

	if (! bReturn) 
	{
		SetLastError(dwError);
	}
	return bReturn;
}




LPDIRECTORYINFO AllocateFakeDirInfo(DWORD dwDirRealPath, LPTSTR tszDirRealPath,
									DWORD dwRootName, LPTSTR tszRootName, 
									DWORD dwRootPath, LPTSTR tszRootPath,
									LPFILEINFO lpRootTemplate)
{
	LPDIRECTORYINFO   lpDirectoryInfo;
	LPFILEINFO        lpFile;

	lpDirectoryInfo  = (LPDIRECTORYINFO)Allocate("Directory:Info", sizeof(DIRECTORYINFO) + dwDirRealPath * sizeof(TCHAR));
	if (! lpDirectoryInfo)
	{
		return NULL;
	}
	lpFile = (LPFILEINFO)Allocate("Directory:RootEntry", sizeof(FILEINFO) + (dwRootName + dwRootPath + 1) * sizeof(TCHAR));
	if (! lpFile)
	{
		Free(lpDirectoryInfo);
		return NULL;
	}
	lpDirectoryInfo->lReferenceCount = 1;
	lpDirectoryInfo->lpFileInfo      = NULL;
	lpDirectoryInfo->lpRootEntry     = lpFile;
	lpDirectoryInfo->lpLinkedInfo    = NULL;
	lpDirectoryInfo->dwDirectorySize = 0;
	lpDirectoryInfo->dwRealPath      = dwDirRealPath;
	CopyMemory(lpDirectoryInfo->tszRealPath, tszDirRealPath, dwDirRealPath);
	lpDirectoryInfo->tszRealPath[dwDirRealPath] = 0;

	if (lpRootTemplate)
	{
		CopyMemory(lpFile, lpRootTemplate, sizeof(*lpFile));
		lpFile->dwFileAttributes |= FILE_ATTRIBUTE_LINK;
		// NOTE: sharing FileContext!
	}
	else
	{
		ZeroMemory(lpFile, sizeof(*lpFile));
		lpFile->Uid = DefaultUid[1];
		lpFile->Gid = DefaultGid[1];
		GetSystemTimeAsFileTime(&lpFile->ftModificationTime);
		lpFile->dwFileAttributes = FILE_ATTRIBUTE_LINK;
	}
	lpFile->dwSafety          = 0xDEADBEAF;
	lpFile->lReferenceCount = 1;
	lpFile->dwFileName = dwRootName;
	CopyMemory(lpFile->tszFileName, tszRootName, dwRootName);
	lpFile->tszFileName[dwRootName] = 0;
	CopyMemory(&lpFile->tszFileName[dwRootName+1], tszRootPath, dwRootPath);
	lpFile->tszFileName[dwRootName+dwRootPath+1] = 0;

	return lpDirectoryInfo;
}



LPDIRECTORYINFO OpenDirectory(LPTSTR tszFileName, BOOL bRecursive, BOOL bFakeDirs, BOOL bSetLockFlag, PBOOL pbLockFlagStatus, LPDIRECTORY *lppDirectory)
{
	WIN32_FILE_ATTRIBUTE_DATA  FileAttributes;
	LPDIRECTORY                lpDirectory;
	LPDIRECTORYINFO            lpDirectoryInfo, lpTargetInfo;
	LPTSTR                     tszLocalName, tszLinkName, tszTemp;
	DWORD                      dwFileName, dwError, dwSize, dwPos, dwLoop, dwType, dwLen;
	BOOL                       bDoUpdate, bNoCheck, bResult, bReParse, bValid, bCreateSymlink;
	HANDLE                     hDir;
	LPREPARSE_DATA_BUFFER      pReParseBuf;
	char                       buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
	TCHAR                      tszTargetName[_MAX_PATH*2+1], tszRelativeName[_MAX_PATH*2+1];
	TCHAR                      tszRootDir[MAX_PATH+1];
	FILETIME                   ftLastWriteTime;
	WCHAR                      *wPos;
	LONG                       lOldUpdateState;

	//  Trim filename
	dwFileName  = _tcslen(tszFileName);
	if (dwFileName > _MAX_PATH )
	{
		SetLastError(ERROR_PATH_NOT_FOUND);
		return NULL;
	}
	tszLocalName   = _alloca((dwFileName+1) + sizeof(TCHAR));
	if (! tszLocalName) return NULL;
	dwFileName  = TrimFileName(tszFileName, tszLocalName);
	tszFileName  = tszLocalName;

	lpDirectory = InsertAndAcquireDirCacheLock(tszFileName, dwFileName, &bDoUpdate);
	if (! lpDirectory) return NULL;

	// this guarantees that we don't miss it being set to true
	lOldUpdateState = InterlockedExchange(&lpDirectory->lForceUpdate, FALSE);

	if (lOldUpdateState ||
		(bRecursive && (!lpDirectory->bPopulated || lpDirectory->bHasFakeSubDirs)) ||
		(lpDirectory->lpDirectoryInfo && (lpDirectory->lpDirectoryInfo->lpRootEntry->dwFileAttributes & FILE_ATTRIBUTE_DIRTY)))
	{
		bNoCheck  = TRUE;
		bDoUpdate  = TRUE;
	}
	else
	{
		bNoCheck = FALSE;
	}

	lpDirectoryInfo  = NULL;
	bCreateSymlink   = FALSE;
	tszTargetName[0] = 0;

	if (!bDoUpdate)
	{
		// bDoUpdate is only false if we had to wait for another thread to update the directory and
		// we think everything up to date...
		lpDirectoryInfo  = lpDirectory->lpDirectoryInfo;
	}
	else
	{
		bResult  = GetFileAttributesEx(lpDirectory->tszFileName, GetFileExInfoStandard, &FileAttributes);
		//  Update contents of directory
		if (!bResult)
		{
			dwError = GetLastError();
			goto CONTINUE;
		}
		if (!(FileAttributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			dwError = ERROR_DIRECTORY;
			goto CONTINUE;
		}

		if (lpDirectory->lpDirectoryInfo && !bNoCheck &&
			!(FileAttributes.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
			!lpDirectory->lpDirectoryInfo->lpLinkedInfo &&
			!CompareFileTime(&lpDirectory->ftCacheTime, &FileAttributes.ftLastWriteTime))
		{
			// it's a simple directory and all looks good, we can use the cached entry
			lpDirectoryInfo  = lpDirectory->lpDirectoryInfo;
			goto CONTINUE;
		}

		CopyMemory(&ftLastWriteTime, &FileAttributes.ftLastWriteTime, sizeof(FILETIME));

		dwType = DRIVE_UNKNOWN;
		// try to find the mountpoint, network share, driveletter that is closest to path in hierarchy
		if (GetVolumePathName(tszFileName, tszRootDir, sizeof(tszRootDir)/sizeof(*tszRootDir)))
		{
			// Now make sure root path ends in a '\', i.e. "\\MyServer\MyShare\", or "C:\".
			dwLen = _tcslen(tszRootDir);
			if ((dwLen < sizeof(tszRootDir)/sizeof(*tszRootDir)-1) && dwLen && tszRootDir[dwLen-1] != _T('\\'))
			{
				tszRootDir[dwLen] = _T('\\');
				tszRootDir[dwLen+1] = 0;
			}

			// GetVolumeInformation(tszRootDir, NULL, 0, &dwSerialNum, &dwComponentLen, &dwFileSysFlags, NULL, 0)
			// can't use GetVolumeInformationByHandleW as its Vista+
			// GetFileInformationByHandle(hDir, &HandleFileInfo)

			dwType = GetDriveType(tszRootDir);
		}
		if ((FileAttributes.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && (dwType != DRIVE_REMOTE))
		{
			bReParse = TRUE;
		}
		else
		{
			bReParse = FALSE;
		}

		// if we are dealing with a ReParse point we need to figure out the final target directory and the
		// latest timestamp along the way so we can tell if we need to update anything...
		if (bReParse)
		{
			tszLinkName = tszFileName;
			bValid = FALSE;
			// make sure we don't chase ourselves in a loop resolving circular links...
			for (dwLoop = 10 ; dwLoop ; dwLoop--) 
			{
				if (dwLoop && !GetFileAttributesEx(tszLinkName, GetFileExInfoStandard, &FileAttributes))
				{
					// we failed to get info on the target...
					// note: we don't update on the first pass because we did that above so just use that
					break;
				}
				if (!(FileAttributes.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
				{
					// stop when we found the target, FileAttributes will be valid for target now
					bValid = TRUE;
					break;
				}

				// it's a ReParse point of some kind (NTFS junction, symlink, etc) so try to
				// determine the target of the link

				// TODO: can this share read/write and open it via GENERIC_READ instead?
				hDir = CreateFile(tszLinkName, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING,
					(FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS), 0);
				if (hDir == INVALID_HANDLE_VALUE)
				{
					break;
				}

				if (!DeviceIoControl(hDir, FSCTL_GET_REPARSE_POINT, NULL, 0, buf, sizeof(buf), &dwSize, NULL))
				{
					CloseHandle(hDir);
					break;
				}
				CloseHandle(hDir);

				pReParseBuf = (LPREPARSE_DATA_BUFFER) buf;
				if (pReParseBuf->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
				{
					// regular junction or mounted drive (starts with "\\??\\Volume")
					dwSize = pReParseBuf->MountPointReparseBuffer.SubstituteNameLength/2;
					wPos = &pReParseBuf->MountPointReparseBuffer.PathBuffer[pReParseBuf->MountPointReparseBuffer.SubstituteNameOffset/2];
					if (!wcsncmp(wPos, L"\\??\\Volume", 10))
					{
						// it's a drive mount point, handle this as a special case by just treating it as a normal dir (ignore reparse point).
						wPos[dwSize] = 0;
						if (!GetFileAttributesExW(wPos, GetFileExInfoStandard, &FileAttributes))
						{
							break;
						}
						// make sure string is null terminated
						tszTargetName[0] = 0;
						break;
					}
					if (wcsncmp(wPos, L"\\??\\", 4))
					{
						// don't understand the link format
						break;
					}
					dwSize -= 4;
					if (dwSize > _MAX_PATH)
					{
						// we don't handle long names... just bail
						break;
					}
					if (dwSize != sprintf_s(tszTargetName, sizeof(tszTargetName)/sizeof(TCHAR), "%.*S", dwSize, wPos+4))
					{
						break;
					}
					if (dwSize > 3 && tszTargetName[dwSize-1] == _T('\\'))
					{
						// strip off the trailing slash
						tszTargetName[dwSize-1] = 0;
					}
					tszLinkName = tszTargetName;
					continue;
				}
				else if (pReParseBuf->ReparseTag == IO_REPARSE_TAG_SYMLINK)
				{
					// NOTE: on Vista+ we have symlinks so consider GetFinalPathNameByHandle if symlink resolving turns out to be hard...
					dwSize = pReParseBuf->SymbolicLinkReparseBuffer.SubstituteNameLength/2;
					if (dwSize > _MAX_PATH)
					{
						// we don't handle long names... just bail
						break;
					}
					wPos   = &pReParseBuf->SymbolicLinkReparseBuffer.PathBuffer[pReParseBuf->SymbolicLinkReparseBuffer.SubstituteNameOffset / 2];
					if (pReParseBuf->SymbolicLinkReparseBuffer.Flags != SYMLINK_FLAG_RELATIVE)
					{
						if (dwSize != sprintf_s(tszTargetName, sizeof(tszTargetName)/sizeof(TCHAR), "%.*S", dwSize, wPos))
						{
							break;
						}
						if (dwSize > 3 && tszTargetName[dwSize-1] == _T('\\'))
						{
							// strip off the trailing slash
							tszTargetName[dwSize-1] = 0;
						}
						tszLinkName = tszTargetName;
						continue;
					}
					// it's a relative link from here, so resolve the path...
					if (tszLinkName == tszFileName)
					{
						// we need to make a copy of the string first...
						_tcscpy_s(tszTargetName, sizeof(tszTargetName)/sizeof(TCHAR), tszFileName);
					}

					dwPos = _tcslen(tszTargetName);
					if (dwPos > 3 && tszTargetName[dwPos-1] == _T('\\'))
					{
						// strip off the trailing slash
						tszTargetName[dwPos-1] = 0;
						dwPos--;
					}
					// now strip off the last path element
					tszTemp = _tcsrchr(tszTargetName, _T('\\'));
					if (dwPos < 3 || !tszTemp)
					{
						// it's not a valid path
						break;
					}
					if (dwPos > 3 && tszTemp)
					{
						*++tszTemp = 0;
					}
					dwPos = (tszTemp - tszTargetName)/sizeof(TCHAR);
					// copy over our just computed parent directory position
					strncpy_s(tszRelativeName, sizeof(tszRelativeName)/sizeof(TCHAR), tszTargetName, dwPos);
					// and append relative path to it
					if (dwSize != sprintf_s(&tszRelativeName[dwPos], sizeof(tszRelativeName)/sizeof(TCHAR) - dwPos, "%.*S", dwSize, wPos))
					{
						break;
					}
					// WARNING: c:\..\..\..\f => c:\f which is probably not what we wanted from a security standpoint, however
					// the lame PathCanonicalize function has the same behavior as the shell so we get the same answer even
					// if I think it's wrong and a risk.
					// TODO: is there really a limit of MAX_PATH on the input path which can include a long path and another long
					// relative path but the result is less than the total?
					if (!PathCanonicalize(tszTargetName, tszRelativeName))
					{
						break;
					}
					dwSize = _tcslen(tszTargetName);
					if (dwSize > _MAX_PATH)
					{
						// we don't handle long names... just bail
						break;
					}
					if (dwSize > 3 && tszTargetName[dwSize-1] == _T('\\'))
					{
						// strip off the trailing slash
						tszTargetName[dwSize-1] = 0;
					}
					tszLinkName = tszTargetName;
					continue;
				}
				// it's something we don't know how to handle...
				break;
			}
		}

		dwPos = GetTailOfPath(tszFileName, dwFileName);
		if (bReParse && (!bValid || !_tcsicmp(tszTargetName, lpDirectory->tszFileName)))
		{
			// doh, the target points back to us somehow!  ouch!  or it's an invalid target
			bCreateSymlink = TRUE;
			goto CONTINUE;
		}

		if (!bReParse && lpDirectory->lpDirectoryInfo && lpDirectory->lpDirectoryInfo->lpLinkedInfo)
		{
			// it was a linked directory but now it isn't so mark the old fake entry as obsolete and try to delete it
			FreeDirectoryInfo(lpDirectory->lpDirectoryInfo, TRUE);
			lpDirectory->lpDirectoryInfo = NULL;
		}

		if (!bReParse || (NtfsReparseMethod == NTFS_REPARSE_IGNORE))
		{
			// Either we aren't a ReParse point and we need to update a normal directory, or
			// we are and just plan on ignoring that fact.  Ignoring them is probably a bad idea
			// but just in case something breaks we're supporting it for legacy compatibility mode.
			// It's not exactly the same because we double check the timestamp of the target
			// dir and if the link to the target is broken we create a fake link in directory
			// listings so you can see the broken link...
			if (lpDirectory->lpDirectoryInfo && !bNoCheck &&
				!CompareFileTime(&lpDirectory->ftCacheTime, &ftLastWriteTime) &&
				(bReParse && bValid && !CompareFileTime(&lpDirectory->lpDirectoryInfo->lpRootEntry->ftModificationTime, &FileAttributes.ftLastWriteTime)))
			{
				// a simple directory that was up to date would have already been handled up top, so this
				// is a ReParse point which appears up to date so just return the cached answer
				lpDirectoryInfo  = lpDirectory->lpDirectoryInfo;
				goto CONTINUE;
			}

			//  Try to perform an update
			if (UpdateDirectory(lpDirectory, bRecursive, bFakeDirs))
			{
				if (ReadDirectoryPermissions(lpDirectory))
				{
					CopyMemory(&lpDirectory->ftCacheTime, &ftLastWriteTime, sizeof(FILETIME));
					lpDirectoryInfo  = lpDirectory->lpDirectoryInfo;
				}
				else dwError  = GetLastError();
				goto CONTINUE;
			}
			// error processing directory
			dwError  = GetLastError();
			if (!bReParse || (dwError != ERROR_PATH_NOT_FOUND))
			{
				if (lpDirectory->lpDirectoryInfo)
				{
					FreeDirectoryInfo(lpDirectory->lpDirectoryInfo, TRUE);
					lpDirectory->lpDirectoryInfo = NULL;
				}
				goto CONTINUE;
			}
			// new behavior says we return a faked out link here if we have a broken link in a ReParse point
			bCreateSymlink = TRUE;
			goto CONTINUE;
		}

		//
		// it's a linked directory at this point
		//

		// try to open the target directory here...
		lpTargetInfo = OpenDirectory(tszTargetName, bRecursive, bFakeDirs, FALSE, NULL, NULL);

		if (!lpTargetInfo)
		{
			if (lpDirectory->lpDirectoryInfo)
			{
				FreeDirectoryInfo(lpDirectory->lpDirectoryInfo, TRUE);
				lpDirectory->lpDirectoryInfo = NULL;
			}
			CloseDirectory(lpTargetInfo);
			bCreateSymlink = TRUE;
			goto CONTINUE;
		}

		if (lpDirectory->lpDirectoryInfo)
		{
			if ((lpDirectory->lpDirectoryInfo->lpLinkedInfo == lpTargetInfo) &&
				(lpDirectory->lpDirectoryInfo->lpRootEntry->lpLinkedRoot == lpTargetInfo->lpRootEntry) &&
				!(lpTargetInfo->lpRootEntry->dwFileAttributes & FILE_ATTRIBUTE_DIRTY) &&
				!CompareFileTime(&lpDirectory->ftCacheTime, &ftLastWriteTime) &&
				!CompareFileTime(&lpTargetInfo->lpRootEntry->ftModificationTime, &FileAttributes.ftLastWriteTime))
			{
				// hey, nothing changed and things look up to date, we can use our existing directory info
				lpDirectoryInfo = lpDirectory->lpDirectoryInfo;
				goto CONTINUE;
			}

			//  We are going to have to update the structure which means starting over since we can't modify it
			FreeDirectoryInfo(lpDirectory->lpDirectoryInfo, TRUE);
			lpDirectory->lpDirectoryInfo  = NULL;
		}

		lpDirectoryInfo = AllocateFakeDirInfo(dwFileName, tszFileName, dwFileName - dwPos, &tszFileName[dwPos],
			                                  lpTargetInfo->dwRealPath, lpTargetInfo->tszRealPath, lpTargetInfo->lpRootEntry);
		if (!lpDirectoryInfo)
		{
			CloseDirectory(lpTargetInfo);
			goto CONTINUE;
		}

		// We'll hold open the reference to the target directory since we are sharing the
		// lpFileInfo array.  It will get closed when we no longer need it.
		lpDirectoryInfo->lpLinkedInfo              = lpTargetInfo;
		lpDirectoryInfo->dwDirectorySize           = lpTargetInfo->dwDirectorySize;
		lpDirectoryInfo->lpFileInfo                = lpTargetInfo->lpFileInfo;
		lpDirectoryInfo->lpRootEntry->lpLinkedRoot = lpTargetInfo->lpRootEntry;
		// need to make sure the linked root entry stays around even if we close our reference
		// to the directory itself.
		InterlockedIncrement(&lpTargetInfo->lpRootEntry->lReferenceCount);

		// all looks good
		lpDirectory->lpDirectoryInfo = lpDirectoryInfo;
		CopyMemory(&lpDirectory->ftCacheTime, &FileAttributes.ftLastWriteTime, sizeof(FILETIME));
	}

CONTINUE:
	if (bCreateSymlink)
	{
		if (lpDirectory->lpDirectoryInfo)
		{
			// free any old info that might still be around
			FreeDirectoryInfo(lpDirectory->lpDirectoryInfo, TRUE);
			lpDirectory->lpDirectoryInfo = NULL;
		}
		dwError = NO_ERROR;
		lpDirectoryInfo = AllocateFakeDirInfo(dwFileName, tszFileName, dwFileName-dwPos, &tszFileName[dwPos],
			                                  _tcslen(tszTargetName), tszTargetName, NULL);
		if (lpDirectoryInfo)
		{
			CopyMemory(&lpDirectory->ftCacheTime, &ftLastWriteTime, sizeof(FILETIME));
			lpDirectory->lpDirectoryInfo = lpDirectoryInfo;
			lpDirectoryInfo->lpRootEntry->dwFileMode = 0; // make it obvious we have a problem...
		}
		else dwError = GetLastError();
	}

	// we want to return the previous lock status since we know it will be locked if we request it
	if (pbLockFlagStatus)
	{
		*pbLockFlagStatus = lpDirectory->bLocked;
	}

	// if we are trying to lock the directory then we don't want to abort if already locked
	if (!bSetLockFlag && lpDirectory->bLocked)
	{
		dwError = ERROR_DIRECTORY_LOCKED;
		lpDirectoryInfo = NULL;
	}

	if (bSetLockFlag)
	{
		lpDirectory->bLocked = TRUE;
	}

	if (lpDirectoryInfo)
	{
		InterlockedIncrement(&lpDirectoryInfo->lReferenceCount);
	}

	if (bDoUpdate && !lpDirectoryInfo && !lpDirectory->bLocked && !bResult && 
		((dwError == ERROR_FILE_NOT_FOUND) || (dwError == ERROR_PATH_NOT_FOUND) || (dwError == ERROR_DIRECTORY)))
	{
		// this means the directory has been deleted and thus no reason to cache it anymore...
		// TODO: trigger all subdirs of this dir that are in the cache to also be removed...
		ReleaseDirCacheLock(lpDirectory, TRUE);
		if (lppDirectory) *lppDirectory = NULL;
	}
	else if (!lpDirectoryInfo || !lppDirectory)
	{
		ReleaseDirCacheLock(lpDirectory, FALSE);
		if (lppDirectory) *lppDirectory = NULL;
	}
	else
	{
		// we want to keep the dircache lock
		*lppDirectory = lpDirectory;
	}

	//  Contents of returned variable are thread safe
	if (! lpDirectoryInfo) SetLastError(dwError);
	return lpDirectoryInfo;
}









BOOL CloseDirectory(LPDIRECTORYINFO lpDirectoryInfo)
{
  if (! lpDirectoryInfo) return FALSE;

  return FreeDirectoryInfo(lpDirectoryInfo, FALSE);
}



BOOL GetFileInfo2(LPTSTR tszFileName, LPFILEINFO *lpFileInfo, BOOL bNoCheck, LPDIRECTORYINFO *lppDirInfo)
{
  LPDIRECTORYINFO  lpDirectory;
  LPFILEINFO    lpFile;
  TCHAR      tFirstChar;
  DWORD      dwFileAttributes, dwPath, dwSlash;
  LPTSTR      tszPath;
  BOOL        bLocked;

  lpFile  = NULL;
  tszPath  = tszFileName;
  dwPath  = _tcslen(tszPath);
  //  Determinate filetype and verify existence
  dwFileAttributes  = GetFileAttributes(tszFileName);
  if (dwFileAttributes == INVALID_FILE_ATTRIBUTES) return FALSE;
  if (!bNoCheck && ((dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) || (dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)))
  {
	  if ((dwPath > 1 && tszPath[1] == _T(':')) &&
		  (tszPath[2] == 0 || ( tszPath[2] == _T('\\') && tszPath[3] == 0)))
	  {
		  // it a drive root so it's ok, easier to think of it as a positive than a negative
	  }
	  else
	  {
		  SetLastError(IO_NO_ACCESS);
		  return FALSE;
	  }
  }

  if (! (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
  {
    //  Get parent directory
    dwSlash  = dwPath;
    tszPath  = (LPTSTR)_alloca(sizeof(TCHAR)* (dwPath+1));
    if (! tszPath) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, FALSE);
    CopyMemory(tszPath, tszFileName, (dwPath + 1) * sizeof(TCHAR));
    while (--dwSlash && tszPath[dwSlash] != _TEXT('\\') && tszPath[dwSlash] != _TEXT('/'));

    tszFileName  = &tszPath[dwSlash + 1];
    tFirstChar  = tszFileName[0];
    if (dwSlash == 2 && tszPath[1] == _TEXT(':')) dwSlash++;
    tszPath[dwSlash]  = _TEXT('\0');
  }

  //  Open directory
  lpDirectory  = OpenDirectory(tszPath, !FtpSettings.bNoSubDirSizing, TRUE, FALSE, &bLocked, NULL);
  if (! lpDirectory) return FALSE;

  if (bLocked)
  {
	  CloseDirectory(lpDirectory);
	  SetLastError(ERROR_DIRECTORY_LOCKED);
	  return FALSE;
  }

  //  Get fileinformation
  if (! (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
  {
    tszFileName[0]  = tFirstChar;
    lpFile  = FindFileInfo(0, tszFileName, lpDirectory);
  }
  else if (lpDirectory->lpLinkedInfo)
  {
	  lpFile  = lpDirectory->lpLinkedInfo->lpRootEntry;
  }
  else
  {
	  lpFile  = lpDirectory->lpRootEntry;
  }

  if (!lpFile)
  {
	  CloseDirectory(lpDirectory);
	  SetLastError(ERROR_FILE_NOT_FOUND);
	  return FALSE;
  }

  //  Increase reference count
  if (lpFile) InterlockedIncrement(&lpFile->lReferenceCount);

  lpFileInfo[0]  = lpFile;

  if (lppDirInfo)
  {
	  *lppDirInfo = lpDirectory;
  }
  else
  {
	  CloseDirectory(lpDirectory);
  }

  return TRUE;
}



BOOL GetFileInfo(LPTSTR tszFileName, LPFILEINFO *lpFileInfo)
{
	return GetFileInfo2(tszFileName, lpFileInfo, FALSE, NULL);
}


BOOL GetFileInfoNoCheck(LPTSTR tszFileName, LPFILEINFO *lpFileInfo)
{
	return GetFileInfo2(tszFileName, lpFileInfo, TRUE, NULL);
}




VOID CloseFileInfo(LPFILEINFO lpFileInfo)
{
	//  Decrease reference count
	if (lpFileInfo->dwSafety != 0xDEADBEAF)
	{
		Putlog(LOG_ERROR, "CloseFileInfo: Discovered corrupted FileInfo - %d.\r\n", lpFileInfo->lReferenceCount);
		return;
	}
	if (! InterlockedDecrement(&lpFileInfo->lReferenceCount))
	{
		if (lpFileInfo->lpLinkedRoot)
		{
			CloseFileInfo(lpFileInfo->lpLinkedRoot);
		}
		lpFileInfo->dwSafety = 0;
		Free(lpFileInfo);
	}
}


VOID IoCloseFind(LPFIND hFind)
{
  CloseDirectory(hFind->lpDirectoryInfo);
  Free(hFind);
}


BOOL IoFindParent(LPFIND hFind, LPFILEINFO *lpResult)
{
  lpResult[0]  = hFind->lpDirectoryInfo->lpRootEntry;
  return TRUE;
}


BOOL IoFindNextFile(LPFIND hFind, LPFILEINFO *lpResult)
{
  LPDIRECTORYINFO    lpDirectoryInfo;
  LPFILEINFO      *lpFileInfo;
  register DWORD    n;

  n  = hFind->dwOffset;
  lpDirectoryInfo  = hFind->lpDirectoryInfo;
  lpFileInfo  = lpDirectoryInfo->lpFileInfo;
  //  Seek through all items
  for (;n--;)
  {
    if (! iCompare(hFind->tszFilter, lpFileInfo[n]->tszFileName))
    {
      lpResult[0]  = lpFileInfo[n];
      hFind->dwOffset  = n;
      return TRUE;
    }
  }
  return FALSE;
}


LPFIND IoFindFirstFile(LPTSTR tszPath, LPTSTR tszFilter, LPFILEINFO *lpFileInfo)
{
  LPDIRECTORYINFO  lpDirectoryInfo;
  DWORD      dwFilter;
  LPFIND      hFind;
  BOOL      bResult;

  if (! tszFilter) return NULL;
  //  Allocate memory for find
  dwFilter  = _tcslen(tszFilter) * sizeof(TCHAR);
  hFind  = (LPFIND)Allocate("Find", sizeof(FIND) + dwFilter);
  if (! hFind) return NULL;

  //  Open directory
  lpDirectoryInfo  = OpenDirectory(tszPath, TRUE, FALSE, FALSE, NULL, NULL);
  if (! lpDirectoryInfo)
  {
    Free(hFind);
    return NULL;
  }
  //  Initialize find structure
  hFind->dwOffset  = lpDirectoryInfo->dwDirectorySize;
  hFind->lpDirectoryInfo  = lpDirectoryInfo;
  CopyMemory(hFind->tszFilter, tszFilter, dwFilter + sizeof(TCHAR));

  //  Find first item
  bResult  = IoFindNextFile(hFind, lpFileInfo);
  if (! bResult)
  {
    IoCloseFind(hFind);
    SetLastError(ERROR_FILE_NOT_FOUND);
    return NULL;
  }
  return hFind;
}



// TODO: remove EXECUTE checks?
BOOL Access(LPUSERFILE lpUserFile, LPFILEINFO lpFileInfo, DWORD dwMode)
{
	LPTSTR tszPrivate;
	UINT32 Gid;
	DWORD n;

	//  Check privacy
	if ((lpFileInfo->dwFileMode & S_PRIVATE) && (tszPrivate = (LPTSTR)FindFileContext(PRIVATE, &lpFileInfo->Context)))
	{
		if (HavePermission(lpUserFile, tszPrivate))
		{
			//  Private, only masters or VFS Admins can see without proper access
			if ((dwMode & _I_PASS) && tszPrivate[0] == _T(':'))
			{
				// this is ok, we're allowing it to pass through
			}
			else if (HasFlag(lpUserFile, "MV"))
			{
				SetLastError(ERROR_FILE_NOT_FOUND);
				return FALSE;
			}
		}
	}

	if (! dwMode) return TRUE;

	if (lpFileInfo->Uid == (UINT)lpUserFile->Uid)
	{
		//  User is 'owner' of file
		if ((! (dwMode & _I_READ) || lpFileInfo->dwFileMode & S_IRUSR) &&
			(! (dwMode & _I_WRITE) || lpFileInfo->dwFileMode & S_IWUSR) &&
			(! (dwMode & _I_EXECUTE) || lpFileInfo->dwFileMode & S_IXUSR)) return TRUE;
	}
	else if (! (dwMode & _I_OWN))
	{
		//  User is not owner of file
		for (n = 0 ; n < MAX_GROUPS && ((Gid = lpUserFile->Groups[n]) != -1) ; n++)
		{
			if (lpFileInfo->Gid == Gid) break;
		}
		if (n < MAX_GROUPS && lpFileInfo->Gid == Gid)
		{
			// it's a match for a group we are in
			if ((! (dwMode & _I_READ) || lpFileInfo->dwFileMode & S_IRGRP) &&
				(! (dwMode & _I_WRITE) || lpFileInfo->dwFileMode & S_IWGRP) &&
				(! (dwMode & _I_EXECUTE) || lpFileInfo->dwFileMode & S_IXGRP)) return TRUE;
		}
		else
		{
			//  User is 'other' to this file
			if ((! (dwMode & _I_READ) || lpFileInfo->dwFileMode & S_IROTH) &&
				(! (dwMode & _I_WRITE) || lpFileInfo->dwFileMode & S_IWOTH) &&
				(! (dwMode & _I_EXECUTE) || lpFileInfo->dwFileMode & S_IXOTH)) return TRUE;
		}
	}

	//  Administrator flags: 'M' (master) - let do anything
	if (! HasFlag(lpUserFile, "M")) return TRUE;

	// allow VFS Admins to read/see anything but not to write without dir perms
	if ( !(dwMode & _I_WRITE) && !HasFlag(lpUserFile, "V") ) return TRUE;

	SetLastError(IO_NO_ACCESS_VFS);
	return FALSE;
}



BOOL CreateFileContext(LPFILECONTEXT lpContext, LPFILECONTEXT lpSourceContext)
{
  //  Copy context
  if (lpSourceContext &&
    lpSourceContext->dwData)
  {
    lpContext->dwData  = lpSourceContext->dwData;
    lpContext->lpData  = Allocate("FileContext", lpContext->dwData);
    if (! lpContext->lpData) return FALSE;
    CopyMemory(lpContext->lpData, lpSourceContext->lpData, lpContext->dwData);
  }
  else ZeroMemory(lpContext, sizeof(FILECONTEXT));

  return TRUE;
}




BOOL InsertFileContext(LPFILECONTEXT lpContext, BYTE Item, LPVOID lpData, DWORD dwData)
{
	LPVOID  lpMemory;
	DWORD  dwSlack, dwMemory;
	TCHAR  pBuffer[1];

	pBuffer[0]  = _TEXT('\0');
	//  Remove existing context
	dwSlack  = lpContext->dwData;
	DeleteFileContext(lpContext, Item);
	dwSlack  = (lpContext->dwData ? dwSlack - lpContext->dwData : 0);

	//  Allocate more memory
	if (dwSlack < (dwData + sizeof(BYTE) + sizeof(UINT32) + sizeof(TCHAR)))
	{
		lpMemory  = ReAllocate(lpContext->lpData, "FileContext",
			lpContext->dwData + dwData + sizeof(BYTE) + sizeof(UINT32) + sizeof(TCHAR));
		if (! lpMemory) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, FALSE);
		lpContext->lpData  = lpMemory;
	}

	dwMemory  = dwData + sizeof(BYTE) + sizeof(UINT32) + sizeof(TCHAR);
	//  Copy new data
	CopyMemory((LPVOID)((ULONG)lpContext->lpData + lpContext->dwData),
		&Item, sizeof(BYTE));
	lpContext->dwData  += sizeof(BYTE);
	CopyMemory((LPVOID)((ULONG)lpContext->lpData + lpContext->dwData),
		&dwMemory, sizeof(UINT32));
	lpContext->dwData  += sizeof(UINT32);
	CopyMemory((LPVOID)((ULONG)lpContext->lpData + lpContext->dwData),
		lpData, dwData);
	lpContext->dwData  += dwData;
	CopyMemory((LPVOID)((ULONG)lpContext->lpData + lpContext->dwData), pBuffer, sizeof(TCHAR));
	lpContext->dwData  += sizeof(TCHAR);

	return TRUE;
}


BOOL FreeFileContext(LPFILECONTEXT lpContext)
{
  Free(lpContext->lpData);
  lpContext->lpData  = NULL;
  lpContext->dwData  = 0;
  return FALSE;
}


BOOL DeleteFileContext(LPFILECONTEXT lpContext, BYTE Item)
{
  LPVOID  lpMemory;
  DWORD  dwMemory;

  //  Remove existing context
  lpMemory = FindFileContext(Item, lpContext);
  if (lpMemory)
  {
    dwMemory  = ((PUINT32)lpMemory)[-1];
    lpMemory  = (LPVOID)((ULONG)lpMemory - sizeof(UINT32) - sizeof(BYTE));
    MoveMemory(lpMemory,
      (LPVOID)((ULONG)lpMemory + dwMemory),
      ((ULONG)lpContext->lpData + lpContext->dwData) - ((ULONG)lpMemory + dwMemory));
    lpContext->dwData  -= dwMemory;
    return TRUE;
  }
  return FALSE;
}




LPVOID FindFileContext(BYTE Item, LPFILECONTEXT lpContext)
{
  LPBYTE  lpOffset, lpEnd;

  //  Seek through buffer
  lpOffset  = (LPBYTE)lpContext->lpData;
  lpEnd  = &lpOffset[lpContext->dwData];
  for (;lpOffset < lpEnd;lpOffset += ((PUINT32)&lpOffset[1])[0])
  {
    if (lpOffset[0] == Item) return &lpOffset[sizeof(BYTE) + sizeof(UINT32)];  
  }
  return NULL;
}


static BOOL ValidFileContext(LPFILECONTEXT lpContext)
{
	LPBYTE lpOffset, lpEnd;

	if (lpContext->dwData > 128 * 1024)
	{
		return FALSE;
	}
	lpOffset = (LPBYTE)lpContext->lpData;
	lpEnd    = &lpOffset[lpContext->dwData];
	for (;lpOffset < lpEnd;lpOffset += ((PUINT32)&lpOffset[1])[0])
	{
	}
	if (lpOffset == lpEnd)
	{
		return TRUE;
	}
	return FALSE;
}


static DWORD GetFileModeContextFlags(LPFILECONTEXT lpContext)
{
	LPBYTE  lpOffset, lpEnd;
	DWORD   dwMask = 0;

	lpOffset  = (LPBYTE)lpContext->lpData;
	lpEnd  = &lpOffset[lpContext->dwData];
	for (;lpOffset < lpEnd;lpOffset += ((PUINT32)&lpOffset[1])[0])
	{
		if (lpOffset[0] < 4)
		{
			// this will set S_PRIVATE, S_SYMBOLIC, S_REDIRECTED, UNUSED
			dwMask |= S_PRIVATE << lpOffset[0];
		}
	}
	return dwMask;
}


BOOL IoMoveFile(LPTSTR tszExistingFileName, LPTSTR tszNewFileName)
{
  LPFILEINFO  lpFileInfo;
  VFSUPDATE  UpdateData;
  DWORD    dwExistingFileName, dwNewFileName, dwSlash, dwFileName;
  BOOL    dwError;
  LPTSTR    tszFileName;
  
  dwError  = NO_ERROR;
  if (! GetFileInfo(tszExistingFileName, &lpFileInfo)) return FALSE;

  // this could take a while...
  SetBlockingThreadFlag();

  //  Move file on filesystem
  if (MoveFileEx(tszExistingFileName, tszNewFileName, MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH))
  {
    dwExistingFileName  = _tcslen(tszExistingFileName);
    dwNewFileName  = _tcslen(tszNewFileName);
    tszFileName  = (LPTSTR)_alloca((max(dwNewFileName, dwExistingFileName) + 1) * sizeof(TCHAR));
    if (tszFileName)
    {
      //  Update permissions for file (directory is auto updated)
      if (! (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_IOFTPD)
      {
		  ZeroMemory(&UpdateData, sizeof(VFSUPDATE));
		  UpdateData.Uid  = lpFileInfo->Uid;
		  UpdateData.Gid  = lpFileInfo->Gid;
		  UpdateData.dwFileMode  = lpFileInfo->dwFileMode;
		  UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
		  UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;

		  UpdateFileInfo(tszNewFileName, &UpdateData);
      }
      else
      {
        //  Force update on parent for new filename
        dwFileName  = TrimFileName(tszNewFileName, tszFileName);
        for (dwSlash = dwFileName - 1;dwSlash && tszFileName[dwSlash] != _TEXT('\\');dwSlash--);
        if (dwSlash == 2 && dwFileName > 3 && tszFileName[1] == _TEXT(':')) dwSlash  = 3;
        if (dwSlash >= 3)
        {
          tszFileName[dwSlash]  = _TEXT('\0');
          MarkDirectory(tszFileName);
        }
      }

      //  Force update on parent for old file
      dwFileName  = TrimFileName(tszExistingFileName, tszFileName);
      for (dwSlash = dwFileName - 1;dwSlash && tszFileName[dwSlash] != _TEXT('\\');dwSlash--);
      if (dwSlash == 2 && dwFileName > 3 && tszFileName[1] == _TEXT(':')) dwSlash  = 3;
      if (dwSlash >= 3)
      {
        tszFileName[dwSlash]  = _TEXT('\0');
        MarkDirectory(tszFileName);
      }
    }
  }
  else dwError  = GetLastError();
  CloseFileInfo(lpFileInfo);

  SetNonBlockingThreadFlag();

  if (dwError) ERROR_RETURN(dwError, FALSE);

  return TRUE;
}


BOOL
IoDeleteFile(LPTSTR tszFileName, DWORD dwFileName)
{
  LPTSTR  tszPath;
  DWORD  dwPath;

  //  Delete file from filesystem
  if (! DeleteFile(tszFileName)) return FALSE;

  //  Get parent filename
  dwPath  = _tcslen(tszFileName);
  tszPath  = (LPTSTR)_alloca(dwPath * sizeof(TCHAR));
  if (! tszPath || ! dwPath) return TRUE;

  //  Focrce update parent
  while(--dwPath &&
        tszFileName[dwPath]!=_TEXT('\\') &&
        tszFileName[dwPath] != _TEXT('/'))
    ;
  if (dwPath == 2 && tszFileName[1] == _TEXT(':')) dwPath++;
  if (dwPath >= 3)
  {
    CopyMemory(tszPath, tszFileName, dwPath * sizeof(TCHAR));
    tszPath[dwPath]  = _TEXT('\0');
    MarkDirectory(tszPath);
  }
  return TRUE;
}



BOOL IoRemoveReparsePoint(LPTSTR tszPath)
{
	LPREPARSE_DATA_BUFFER      pReParseBuf;
	char    buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
	HANDLE  hDir;
	DWORD   dwSize, dwError;
	WCHAR  *pwPos;

	hDir = CreateFile(tszPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING,
		(FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS), 0);
	if (hDir == INVALID_HANDLE_VALUE)
	{
		return TRUE;
	}

	if (!DeviceIoControl(hDir, FSCTL_GET_REPARSE_POINT, NULL, 0, buf, sizeof(buf), &dwSize, NULL))
	{
		CloseHandle(hDir);
		return TRUE;
	}

	dwError = NO_ERROR;
	pReParseBuf = (LPREPARSE_DATA_BUFFER) buf;
	if (pReParseBuf->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
	{
		// regular junction or mounted drive (starts with "\\??\\Volume")
		dwSize = pReParseBuf->MountPointReparseBuffer.SubstituteNameLength/2;
		pwPos = &pReParseBuf->MountPointReparseBuffer.PathBuffer[pReParseBuf->MountPointReparseBuffer.SubstituteNameOffset/2];
		if (!wcsncmp(pwPos, L"\\??\\Volume", 10))
		{
			// it's a drive mount point, don't allow these to be deleted
			dwError = ERROR_DIRECTORY;
		}
	}
	else if (pReParseBuf->ReparseTag != IO_REPARSE_TAG_SYMLINK)
	{
		dwError = ERROR_PATH_NOT_FOUND;
	}

	if (dwError == NO_ERROR)
	{
		// zero out length
		pReParseBuf->ReparseDataLength = 0;
		if (!DeviceIoControl(hDir, FSCTL_DELETE_REPARSE_POINT, buf, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, NULL, 0, &dwSize, NULL))
		{
			dwError = GetLastError();
		}
	}

	CloseHandle(hDir);
	if (dwError == NO_ERROR)
	{
		return FALSE;
	}
	return TRUE;
}


BOOL IoRemoveDirectory2(LPTSTR tszPath, LPFILEINFO lpFileInfo)
{
	VFSUPDATE      UpdateData;
	DWORD  dwError, dwAttributes;

	dwError = NO_ERROR;

	// customized folders end up read-only so undo that if set...
	// have to check the actual attributes since the read-only flag
	// gets lost on directories
	if ((dwAttributes = GetFileAttributes(tszPath)) != INVALID_FILE_ATTRIBUTES)
	{
		if (dwAttributes & FILE_ATTRIBUTE_READONLY)
		{
			// strip off our internal flags
			if (!SetFileAttributes(tszPath, dwAttributes & ~FILE_ATTRIBUTE_READONLY))
			{
				dwError = GetLastError();
			}
		}
	}
	else dwError = GetLastError();

	if (dwError == NO_ERROR && !RemoveDirectory(tszPath))
	{
		dwError = GetLastError();
		//  See if we should restore directory's fileinfo
		if (lpFileInfo && lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_IOFTPD)
		{
			//  Restore directory ownership
			UpdateData.Uid  = lpFileInfo->Uid;
			UpdateData.Gid  = lpFileInfo->Gid;
			UpdateData.dwFileMode  = lpFileInfo->dwFileMode;
			UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
			UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;
			UpdateData.Context.lpData  = lpFileInfo->Context.lpData;
			UpdateData.Context.dwData  = lpFileInfo->Context.dwData;

			UpdateFileInfo(tszPath, &UpdateData);
		}
	}

	//  Return error
	if (dwError != NO_ERROR) ERROR_RETURN(dwError, FALSE);
	return TRUE;
}




BOOL IoRemoveDirectory(LPTSTR tszPath)
{
  WIN32_FIND_DATA    FindData;
  HANDLE        hFind;
  LPFILEINFO      lpFileInfo;
  LPDELETELIST    lpListHead, lpItem;
  DWORD        dwError, dwFileName, dwMaxFileName, dwPath;
  LPVOID        lpMemory;
  LPTSTR        tszFileName, tszPathName, tszDeleteName;
  TCHAR        *tpOffset;
  BOOL        bDelete, bAdd;

  if (! tszPath) return FALSE;
  dwPath  = _tcslen(tszPath);
  dwError    = NO_ERROR;
  lpListHead  = NULL;
  dwMaxFileName  = 0;
  tszFileName = 0;
  tszDeleteName = 0;

  //  Copy directory name
  tszPathName  = (LPTSTR)_alloca((dwPath + 3) * sizeof(TCHAR));
  dwPath  = TrimFileName(tszPath, tszPathName);
  _tcscpy(&tszPathName[dwPath], _TEXT("\\*"));

  //  Begin directory search
  hFind  = FindFirstFile(tszPathName, &FindData);
  if (hFind == INVALID_HANDLE_VALUE) return FALSE;

  //  Get fileinfo
  if (! GetFileInfo(tszPath, &lpFileInfo))
  {
    dwError  = GetLastError();
    FindClose(hFind);
    ERROR_RETURN(dwError, FALSE);
  }

  do
  {
    bDelete  = FALSE;
	bAdd     = FALSE;
    tszFileName  = FindData.cFileName;

    //  Compare filename
    if (tszFileName[0] == _TEXT('.'))
    {
      if (! _tcsnicmp(&tszFileName[1], _TEXT("ioFTPD"), 6))
      {
		  bAdd    = TRUE;
	  }
	  else if (tszFileName[1] == _TEXT('\0') ||
		  (tszFileName[1] == _TEXT('.') && tszFileName[2] == _TEXT('\0')))
	  {
		  bDelete  = TRUE;
	  }
	}
	else if ((FindData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) &&
		     (!_tcsicmp(tszFileName, _T("thumbs.db"))  || !_tcsicmp(tszFileName, _T("desktop.ini")) ||
		      !_tcsicmp(tszFileName, _T("folder.jpg")) || !_tcsicmp(tszFileName, _T("AlbumArtSmall.jpg")) ))
	{
		bAdd    = TRUE;
	}

	if (bAdd)
	{
        dwFileName  = _tcslen(tszFileName);
        //  Allocate memory for item
        lpMemory  = Allocate("DeleteList", sizeof(struct _DELETELIST) + dwFileName * sizeof(TCHAR));
        if (! lpMemory)
		{
			dwError = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

        //  Push item to list
        if (! lpListHead)
        {
          lpListHead  = (LPDELETELIST)lpMemory;
          lpItem  = (LPDELETELIST)lpMemory;
        }
        else
        {
          lpItem->lpNext  = (LPDELETELIST)lpMemory;
          lpItem  = lpItem->lpNext;
        }
        //  Update structure
        CopyMemory(lpItem->tszFileName, tszFileName, (dwFileName + 1) * sizeof(TCHAR));
        lpItem->dwFileName  = dwFileName;
        dwMaxFileName  = max(dwMaxFileName, dwFileName);
        bDelete  = TRUE;
      }

  } while (bDelete && FindNextFile(hFind, &FindData));
  FindClose(hFind);

  if (lpListHead)
  {
    //  Go through list of .ioFTPD files
    lpItem->lpNext  = NULL;
    tszFileName  = NULL;

    if (! bDelete)
    {
      //  Directory was not empty
      dwError  = ERROR_NOT_EMPTY;
    }
    else
    {
      //  Allocate memory for delete item
      tszDeleteName  = (LPTSTR)Allocate("DirectoryDelete", (dwPath + 9 + dwMaxFileName) * sizeof(TCHAR));
      if (! tszDeleteName)
      {
		  dwError  = ERROR_NOT_ENOUGH_MEMORY;
		  bDelete  = FALSE;
	  }
      else
	  {
		  dwFileName  = wsprintf(tszDeleteName, _TEXT("%s\\"), tszPath);
		  tpOffset  = &tszDeleteName[dwFileName];
      }
    }

    //  Go through all items
    while ((lpItem = lpListHead))
    {
      lpListHead  = lpListHead->lpNext;
      //  Delete file
      if (dwError == NO_ERROR)
      {
        CopyMemory(tpOffset, lpItem->tszFileName, (lpItem->dwFileName + 1) * sizeof(TCHAR));
        bDelete = DeleteFile(tszDeleteName);
        if (! bDelete)
        {
          dwError  = GetLastError();
          if (dwError == ERROR_FILE_NOT_FOUND ||
            dwError == ERROR_PATH_NOT_FOUND) dwError  = NO_ERROR;
        }
      }
      Free(lpItem);
    }
  }

  //  Remove directory if all still OK
  if (dwError == NO_ERROR)
  {
	  if (!IoRemoveDirectory2(tszPath, lpFileInfo))
	  {
		  dwError = GetLastError();
	  }
  }

  if (tszDeleteName)
  {
	  Free(tszDeleteName);
  }

  CloseFileInfo(lpFileInfo);
  //  Return error
  if (dwError != NO_ERROR) ERROR_RETURN(dwError, FALSE);

  if (dwPath <= 3 ||
    (tszPathName[1] != _TEXT(':') && dwPath < 7)) return TRUE;
  //  Force update parent
  while (--dwPath && tszPathName[dwPath] != _TEXT('/') && tszPathName[dwPath] != _TEXT('\\'));
  if (dwPath == 2 && tszPathName[1] == _TEXT(':')) dwPath++;
  if (dwPath >= 3)
  {
    tszPathName[dwPath]  = '\0';
    MarkDirectory(tszPathName);
  }
  return TRUE;
}



DWORD CALLBACK 
IoCopyProgressCallback(LARGE_INTEGER TotalFileSize, LARGE_INTEGER TotalBytesTransferred,
					   LARGE_INTEGER StreamSize,    LARGE_INTEGER StreamBytesTransferred,
					   DWORD dwStreamNumber,        DWORD dwCallbackReason,
					   HANDLE hSourceFile,          HANDLE hDestinationFile,
					   LPVOID lpData)
{
	LPCMD_PROGRESS lpProgress = (LPCMD_PROGRESS) lpData;

	if (GetTickCount() > lpProgress->dwTicks)
	{
		Progress_Update(lpProgress);
	}
	return PROGRESS_CONTINUE;
}



BOOL IoMoveDirectory(LPTSTR tszSrcPath, LPTSTR tszDestPath, CMD_PROGRESS *lpProgress)
{
	HANDLE             hFind, hNew;
	WIN32_FIND_DATA    FindData, ThisDir;
	WIN32_FILE_ATTRIBUTE_DATA  FileAttributes;
	LPTSTR             tszName, tszSrcParent, tszDestParent;
	TCHAR              tszSrc[MAX_PATH+1], tszDest[MAX_PATH+1];
	DWORD              dwSrcLen, dwDestLen;
	DWORD              dwError = 0;
	BOOL               bFirst = TRUE, bResult, bLocked, bRelease;
	BOOL               bContinue, bSuccess, bDelete, bSecond;
	LPDIRECTORY        lpSrcDirectory, lpDestDirectory;
	LPDIRECTORYINFO    lpSrcInfo;
	LPDIRECTORYTABLE   lpDirectoryTable;

	// rather than use IoFindFile, IoMoveFile, etc, we're going to
	// cheat and copy everything.  We'll take special care with the
	// .ioFTPD file....

	if (!tszSrcPath || !tszDestPath)
	{
		SetLastError(IO_INVALID_ARGUMENTS);
		return FALSE;
	}
	dwSrcLen  = _tcslen(tszSrcPath);
	dwDestLen = _tcslen(tszDestPath);

	// verify the real directory paths aren't too short or too long to be
	// valid and that they  don't end in a '\'.  In theory it might be
	// possible to have a path MAX_PATH-2 length path but it couldn't have
	// a .ioFTPD file so we'll skip trying to move such a dir if it does
	// exist because we could copy smaller files but fail to copy the
	// permissions with it...
	// also abort if src == dest because that would cause lots of problems...
	if (dwSrcLen < 3 || dwDestLen < 3 || dwSrcLen > MAX_PATH-9 || dwDestLen > MAX_PATH-9 ||
		tszSrcPath[dwSrcLen-1] == _T('\\') || tszDestPath[dwDestLen-1] == _T('\\') ||
		!_tcsicmp(tszSrcPath, tszDestPath))
	{
		SetLastError(IO_INVALID_ARGUMENTS);
		return FALSE;
	}

	_tcscpy_s(tszSrc,  MAX_PATH, tszSrcPath);
	_tcscpy_s(tszDest, MAX_PATH, tszDestPath);

	tszSrcParent  = _tcsrchr(tszSrc,  _T('\\'));
	tszDestParent = _tcsrchr(tszDest, _T('\\'));

	if (!tszSrcParent || !tszDestParent)
	{
		SetLastError(IO_INVALID_ARGUMENTS);
		return FALSE;
	}

	if (GetFileAttributes(tszDest) != INVALID_FILE_ATTRIBUTES)
	{
		// rut ro, dir exists!
		SetLastError(ERROR_ALREADY_EXISTS);
		return FALSE;
	}

	bSuccess        = FALSE;
	bLocked         = FALSE;
	bRelease        = TRUE;
	lpDestDirectory = NULL;
	lpSrcInfo       = NULL;

	lpSrcDirectory = InsertAndAcquireDirCacheLock(tszSrc, dwSrcLen, NULL);
	if (!lpSrcDirectory)
	{
		return FALSE;
	}

	if (lpSrcDirectory->bLocked)
	{
		dwError = ERROR_DIRECTORY_LOCKED;
		goto cleanup;
	}

	bResult  = GetFileAttributesEx(lpSrcDirectory->tszFileName, GetFileExInfoStandard, &FileAttributes);
	if (!bResult)
	{
		dwError = GetLastError();
		goto cleanup;
	}

	lpDestDirectory = InsertAndAcquireDirCacheLock(tszDest, dwDestLen, NULL);
	if (!lpDestDirectory)
	{
		dwError = GetLastError();
		goto cleanup;
	}

	if (lpDestDirectory->bLocked)
	{
		dwError = ERROR_DIRECTORY_LOCKED;
		goto cleanup;
	}

	// we have both cache entries "locked", now try to create the destination dir
	if (!CreateDirectory(tszDest, NULL))
	{
		dwError = GetLastError();
		goto cleanup;
	}

	// just force an update on the source directory to make sure it's valid
	if (UpdateDirectory(lpSrcDirectory, FALSE, FALSE))
	{
		CopyMemory(&lpSrcDirectory->ftCacheTime, &FileAttributes.ftLastWriteTime, sizeof(FILETIME));
		if (!ReadDirectoryPermissions(lpSrcDirectory))
		{
			dwError = GetLastError();
			goto cleanup;
		}
	}
	else
	{
		dwError  = GetLastError();
		goto cleanup;
	}

	// toggle the lock flag so we can release the real locks so we don't hold things up
	// NOTE: because we don't add locked items back to the LRU these addresses are safe..
	bLocked = TRUE;
	lpSrcDirectory->bLocked  = TRUE;
	lpDestDirectory->bLocked = TRUE;

	// now make sure Src/Dest end in '\'
	if (tszSrc[dwSrcLen-1] != _T('\\'))
	{
		tszSrc[dwSrcLen] = _T('\\');
		tszSrc[++dwSrcLen] = 0;
	}
	if (tszDest[dwDestLen-1] != _T('\\'))
	{
		tszDest[dwDestLen] = _T('\\');
		tszDest[++dwDestLen] = 0;
	}

	// increment and hold onto the directory info/permissions
	if (lpSrcDirectory->lpDirectoryInfo)
	{
		lpSrcInfo = lpSrcDirectory->lpDirectoryInfo;
		InterlockedIncrement(&lpSrcInfo->lReferenceCount);

		// now we are going to cheat, and write directory permissions for the dest dir based on the src dir.
		// Since bLocked is set on the brand new dest dir nobody is going to come along and update it behind
		// our backs which is important since it would remove entries for items that haven't been moved yet.
		// However after copying all the files, we'll correct any differences later.  The primary reason we
		// do this first, even if it might not be correct, is to handle the case of the server/computer
		// crashing then coming back up and the permissions being incorrect.

		// In theory we could copy the .ioFTPD file, but there is no guarantee it's flushed to disk via
		// CopyFileEx and the transacted version isn't available pre-vista so rather than figure out what
		// we should be doing just punt and call WriteDirectoryPermissions with a fake target.
		WriteDirectoryPermissions(lpSrcDirectory, lpDestDirectory->tszFileName, lpDestDirectory->dwFileName);
	}
	// there can't be any destination file info yet since we just created the dir and have it locked.

	ReleaseDirCacheLock(lpSrcDirectory,  FALSE);
	ReleaseDirCacheLock(lpDestDirectory, FALSE);

	// with the release of the actual directory lock nobody should be able to grab new fileinfo's, but
	// that doesn't mean existing file handles aren't open to it...







	// we can't use the FileInfo's in the directory entry because it skips hidden/system files
	// and we want to deal with those.  We'll also do this in 2 passes, in the first we move
	// all regular files and some special hidden files, as well as make a copy of all .ioFTPD*
	// files (except .ioFTPD itself) and if that was successful in the 2nd we delete all
	// .ioFTPD* files and descend into directories.

	tszSrc[dwSrcLen] = _T('*');
	tszSrc[dwSrcLen+1] = 0;

	hFind = FindFirstFile(tszSrc, &FindData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		dwError = GetLastError();
		goto cleanup;
	}

	ThisDir.dwFileAttributes = INVALID_FILE_ATTRIBUTES;

	bContinue = FALSE;
	bSuccess  = TRUE;
	bSecond   = FALSE;
	dwError   = NO_ERROR;

	if (GetTickCount() > lpProgress->dwTicks)
	{
		Progress_Update(lpProgress);
	}

	while (bFirst || FindNextFile(hFind, &FindData))
	{
		bFirst  = FALSE;
		bDelete   = TRUE;
		tszName = FindData.cFileName;

		if (tszName[0] == _T('.'))
		{
			if (tszName[1] == 0)
			{
				// it's this dir, so save the info so we can later set the times/perms
				CopyMemory(&ThisDir, &FindData, sizeof(FindData));
				continue;
			}
			if (tszName[1] == '.' && tszName[2] == 0)
			{
				// it's a pointer to our parent, just ignore it
				continue;
			}
			if (! _tcsnicmp(&tszName[1], _T("ioFTPD"), 6))
			{
				// It's an ioFTPD hidden file (also maybe NTFS hidden)
				bSecond = TRUE;
				if (tszName[7] == 0)
				{
					// it's the .ioFTPD permission file which we special case
					continue;
				}
				// make a copy of these instead of moving
				bDelete = FALSE;
			}
		}
		else if ( (FindData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) &&
			      !(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
			      _tcsicmp(tszName, _T("thumbs.db"))  && _tcsicmp(tszName, _T("desktop.ini"))   &&
				  _tcsicmp(tszName, _T("folder.jpg")) && _tcsicmp(tszName, _T("AlbumArtSmall.jpg")) )
		{
			// refuse to process most NTFS hidden or system files, the .ioFTPD* files are
			// caught above
			bSuccess = FALSE;
			if (dwError == NO_ERROR)
			{
				dwError = ERROR_DIR_NOT_EMPTY;
			}
			continue;
		}
		_tcscpy_s(&tszSrc[dwSrcLen], MAX_PATH-dwSrcLen, tszName);
		_tcscpy_s(&tszDest[dwDestLen], MAX_PATH-dwDestLen, tszName);

		if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			// recursively handle directories
			if (!IoMoveDirectory(tszSrc, tszDest, lpProgress))
			{
				// wasn't successful
				bSuccess = FALSE;
				if (dwError == NO_ERROR)
				{
					dwError = GetLastError();
				}
			}
			continue;
		}

		if (!CopyFileEx(tszSrc, tszDest, IoCopyProgressCallback, lpProgress, &bContinue, COPY_FILE_FAIL_IF_EXISTS))
		{
			bSuccess = FALSE;
			if (dwError == NO_ERROR)
			{
				dwError = GetLastError();
			}
		}
		else if (bDelete)
		{
			// copy was successful, delete or at least mark file for deletion when all handles closed
			lpProgress->dwArg3++;
			if (!DeleteFile(tszSrc))
			{
				bSuccess = FALSE;
				if (dwError == NO_ERROR)
				{
					dwError = GetLastError();
				}
			}
		}
	}
	FindClose(hFind);

	// see if we need a 2nd pass
	if (bSuccess && bSecond)
	{
		tszSrc[dwSrcLen] = _T('*');
		tszSrc[dwSrcLen+1] = 0;

		hFind = FindFirstFile(tszSrc, &FindData);

		if (hFind == INVALID_HANDLE_VALUE)
		{
			dwError = GetLastError();
			goto cleanup;
		}

		while (bFirst || FindNextFile(hFind, &FindData))
		{
			bFirst  = FALSE;
			bDelete   = TRUE;
			tszName = FindData.cFileName;

			if (tszName[0] == _T('.') && (tszName[1] == 0 || (tszName[1] == _T('.') && tszName[2] == 0)))
			{
				continue;
			}

			if (!_tcsnicmp(tszName, _T(".ioFTPD"), 7) ||
				((FindData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) &&
				 (!_tcsicmp(tszName, _T("thumbs.db"))  || !_tcsicmp(tszName, _T("desktop.ini"))   ||
				  !_tcsicmp(tszName, _T("folder.jpg")) || !_tcsicmp(tszName, _T("AlbumArtSmall.jpg"))) ) )
			{
				// we want to delete these
				_tcscpy_s(&tszSrc[dwSrcLen], MAX_PATH-dwSrcLen, tszName);
				if (!DeleteFile(tszSrc))
				{
					bSuccess = FALSE;
					if (dwError == NO_ERROR)
					{
						dwError  = GetLastError();
					}
				}
				continue;
			}

			// file/dir present was considered an error, but it might be marked for deletion
			// and still in use... so let's try to delete the directory anyway below and let
			// it spit out an error...
		}
		FindClose(hFind);
	}

	if (bSuccess && (ThisDir.dwFileAttributes != INVALID_FILE_ATTRIBUTES))
	{
		// everything appears to have gone OK... update directory timestamp
		hNew = CreateFile(tszDestPath, FILE_WRITE_ATTRIBUTES,
			(FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
			NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (hNew != INVALID_HANDLE_VALUE)
		{
			if (!SetFileTime(hNew, &ThisDir.ftCreationTime, &ThisDir.ftLastAccessTime,
				&ThisDir.ftLastWriteTime))
			{
				bSuccess = FALSE;
				dwError = GetLastError();
			}
			CloseHandle(hNew);
			// to preserve the read-only directory flag for customized folders
			SetFileAttributes(tszDestPath, ThisDir.dwFileAttributes);
		}
		else
		{
			bSuccess = FALSE;
			dwError = GetLastError();
		}
	}

	// now try to delete the directory if we think everything OK at this point
	if (bSuccess && !RemoveDirectory(tszSrcPath))
	{
		bSuccess = FALSE;
		dwError = GetLastError();

		//  Restore directory's permission information
		lpDirectoryTable  = &DirectoryTable[lpSrcDirectory->Hash % dwCacheBuckets];
		EnterCriticalSection(&lpDirectoryTable->CriticalSection);
		// we have it blocked so we know it hasn't moved or been deleted
		AcquireDirCacheLock(lpDirectoryTable, lpSrcDirectory);
		lpSrcDirectory->lForceUpdate  = TRUE;
		WriteDirectoryPermissions(lpSrcDirectory, lpSrcDirectory->tszFileName, lpSrcDirectory->dwFileName);
		ReleaseDirCacheLock(lpSrcDirectory, FALSE);
	}

	lpProgress->dwArg1++;

cleanup:
	if (lpSrcDirectory)
	{
		lpDirectoryTable  = &DirectoryTable[lpSrcDirectory->Hash % dwCacheBuckets];
		EnterCriticalSection(&lpDirectoryTable->CriticalSection);
		lpSrcDirectory->lForceUpdate = TRUE;
		if (bLocked)
		{
			lpSrcDirectory->bLocked = FALSE;
		}
		else
		{
			// we broke before bLocking it, so holding regular style lock
			lpSrcDirectory->lReferenceCount--;
		}
		ReleaseDirCacheLock2(lpDirectoryTable, lpSrcDirectory, bSuccess);
		LeaveCriticalSection(&lpDirectoryTable->CriticalSection);
	}

	if (lpDestDirectory)
	{
		lpDirectoryTable  = &DirectoryTable[lpDestDirectory->Hash % dwCacheBuckets];
		EnterCriticalSection(&lpDirectoryTable->CriticalSection);
		lpDestDirectory->lForceUpdate = TRUE;
		if (bLocked)
		{
			lpDestDirectory->bLocked = FALSE;
		}
		else
		{
			// we broke before bLocking it, so holding regular style lock
			lpDestDirectory->lReferenceCount--;
		}
		ReleaseDirCacheLock2(lpDirectoryTable, lpDestDirectory, FALSE);
		LeaveCriticalSection(&lpDirectoryTable->CriticalSection);
	}

	if (lpSrcInfo)
	{
		FreeDirectoryInfo(lpSrcInfo, TRUE);
	}
	tszSrcParent[1] = 0;
	tszDestParent[1] = 0;
	MarkDirectory(tszSrc);
	MarkDirectory(tszDest);
	if (bSuccess)
	{
		return TRUE;
	}
	SetLastError(dwError);
	return FALSE;
}


BOOL DirectoryCache_Init(BOOL bFirstInitialization)
{
  LPTSTR      tszFileAttributes, tszReparse;
  TCHAR      *tpHead, *tpTail;
  DWORD       n, dwFileMode;
  UINT32      Uid, Gid;
  BOOL        bDelay;

  if (! bFirstInitialization) return TRUE;

  //  Set max size for cache bucket
  if (Config_Get_Int(&IniConfigFile, _TEXT("File"), _TEXT("DirectoryCache_Buckets"), (LPINT)&dwCacheBuckets)
    || dwCacheBuckets < 5) dwCacheBuckets  = 5;
  if (Config_Get_Int(&IniConfigFile, _TEXT("File"), _TEXT("DirectoryCache_Size"), (LPINT)&dwCacheBucketSize)
    || dwCacheBucketSize < 10) dwCacheBucketSize  = 10;

  DirectoryTable = (DIRECTORYTABLE *) Allocate("DIRECTORYTABLE", sizeof(DIRECTORYTABLE)*dwCacheBuckets);
  ZeroMemory(DirectoryTable, sizeof(DIRECTORYTABLE)*dwCacheBuckets);

  for (n = 0;n < dwCacheBuckets;n++)
  {
	  InitializeCriticalSectionAndSpinCount(&DirectoryTable[n].CriticalSection, 500);
  }

  tszFileAttributes  = Config_Get(&IniConfigFile, _TEXT("VFS"), _TEXT("Default_File_Attributes"), NULL, NULL);
  //  Get default file data
  if (tszFileAttributes &&
    (dwFileMode = _tcstoul(tpHead = tszFileAttributes, &tpTail, 8)) <= 0777L && tpTail < &tpHead[4] && tpTail[0] == _TEXT(' ') &&
    (Uid = _tcstoul(tpHead = &tpTail[1], &tpTail, 10)) <= MAX_UID && tpTail != tpHead && tpTail[0] == _TEXT(':') &&
    (Gid = _tcstoul(tpHead = &tpTail[1], &tpTail, 10)) <= MAX_GID && tpTail != tpHead)
  {
    DefaultUid[0]  = Uid;
    DefaultGid[0]  = Gid;
    dwDefaultFileMode[0]  = dwFileMode;
  }
  else
  {
    DefaultUid[0]  = 0;
    DefaultGid[0]  = 0;
    dwDefaultFileMode[0]  = 0644L;
  }
  Free(tszFileAttributes);

  tszFileAttributes  = Config_Get(&IniConfigFile, _TEXT("VFS"), _TEXT("Default_Directory_Attributes"), NULL, NULL);
  //  Get default directory data
  if (tszFileAttributes &&
    (dwFileMode = _tcstoul(tpHead = tszFileAttributes, &tpTail, 8)) <= 0777L && tpTail < &tpHead[4] && tpTail[0] == _TEXT(' ') &&
    (Uid = _tcstoul(tpHead = &tpTail[1], &tpTail, 10)) <= MAX_UID && tpTail != tpHead && tpTail[0] == _TEXT(':') &&
    (Gid = _tcstoul(tpHead = &tpTail[1], &tpTail, 10)) <= MAX_GID && tpTail != tpHead)
  {
    DefaultUid[1]  = Uid;
    DefaultGid[1]  = Gid;
    dwDefaultFileMode[1]  = dwFileMode;
  }
  else
  {
    DefaultUid[1]  = 0;
    DefaultGid[1]  = 0;
    dwDefaultFileMode[1]  = 0755L;
  }
  Free(tszFileAttributes);

  tszReparse  = Config_Get(&IniConfigFile, _TEXT("VFS"), _TEXT("NTFS_Reparse_Method"), NULL, NULL);
  if (!tszReparse || !_tcsicmp(tszReparse, _T("IGNORE")))
  {
	  NtfsReparseMethod = NTFS_REPARSE_IGNORE;
  }
  else if (!_tcsicmp(tszReparse, _T("SHARE")))
  {
	  NtfsReparseMethod = NTFS_REPARSE_SHARE;
  }
  else if (!_tcsicmp(tszReparse, _T("SYMLINK")))
  {
	  NtfsReparseMethod = NTFS_REPARSE_SYMLINK;
  }
  else
  {
	  NtfsReparseMethod = NTFS_REPARSE_SYMLINK;
  }
  Free(tszReparse);

  if (Config_Get_Bool(&IniConfigFile, _TEXT("VFS"), _TEXT("VFS_Exported_Paths_Only"), &bVfsExportedPathsOnly))
  {
	  bVfsExportedPathsOnly = FALSE;
  }

  if (!Config_Get_Bool(&IniConfigFile, _TEXT("VFS_PreLoad"), _TEXT("DELAY"), &bDelay) && bDelay)
  {
	  PreLoad_VFS((LPVOID) TRUE);
  }

  return TRUE;
}

VOID DirectoryCache_DeInit(VOID)
{
  DWORD  n, z;

  //  Free resources
  for (n = 0;n < dwCacheBuckets;n++)
  {
    for (z = 0;z < DirectoryTable[n].dwDirectories;z++)
    {
      CloseDirectory(DirectoryTable[n].lpDirectory[z]->lpDirectoryInfo);
      Free(DirectoryTable[n].lpDirectory[z]);
    }
    Free(DirectoryTable[n].lpDirectory);
    DeleteCriticalSection(&DirectoryTable[n].CriticalSection);
  }
  Free(DirectoryTable);
}





// Site refresh cmd?  Just call site mark on current directory or argument if present
LPTSTR Admin_Refresh(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR tszDirName, tszPath;
	VIRTUALPATH Path;
	MOUNT_DATA MountData;

	PWD_Reset(&Path);
	PWD_Copy(&lpUser->CommandChannel.Path, &Path, FALSE);

	if (GetStringItems(Args) < 2) 
	{
		tszDirName = _T(".");
	}
	else
	{
		tszDirName	= GetStringRange(Args, 1, STR_END);
	}

	ZeroMemory(&MountData, sizeof(MountData));
	// use PWD_CWD2 so we resolve the virtual path and leave the resume point to get the other dirs
	// in MountData.
	if (!(tszPath = PWD_CWD2(lpUser->UserFile, &Path, tszDirName, lpUser->hMountFile, &MountData, EXISTS, NULL, NULL, NULL)))
	{
		return GetStringIndexStatic(Args, 0);
	}
	MarkDirectory(tszPath);
	MarkVirtualDir(&Path, lpUser->hMountFile);
	while (tszPath = PWD_Resolve(Path.pwd, lpUser->hMountFile, &MountData, TRUE, 0))
	{
		MarkDirectory(tszPath);
		MarkVirtualDir(&Path, lpUser->hMountFile);
		FreeShared(tszPath);
	}
	PWD_Free(&Path);
	return NULL;
}



LPTSTR Admin_DirCache(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPDIRECTORYTABLE lpDirTable;
	LPDIRECTORY      lpDir;
	LPBUFFER         lpBuffer;
	DWORD            n, m, z, dwTotal, dwShared, dwInUse, dwLocked, dwInfoInUse, dwNoInfo, dwFiles;

	dwTotal = dwShared = dwInUse = dwInUse = dwLocked = dwInfoInUse = dwNoInfo = dwFiles = 0;
	lpBuffer = &lpUser->CommandChannel.Out;

	z = 0;
	FormatString(lpBuffer, "%s Buckets (num=%d, size=%d):\r\n", tszMultilinePrefix, dwCacheBuckets, dwCacheBucketSize);
	for (n = 0;n < dwCacheBuckets;n++)
	{
		lpDirTable = &DirectoryTable[n];
		EnterCriticalSection(&lpDirTable->CriticalSection);
		for (m = 0 ; m < lpDirTable->dwDirectories ; m++ , dwTotal++)
		{
			lpDir = lpDirTable->lpDirectory[m];
			if (lpDir->lReferenceCount > 1)
			{
				dwInUse++;
			}
			if (lpDir->bLocked)
			{
				dwLocked++;
			}
			if (lpDir->lpDirectoryInfo)
			{
				if (lpDir->lpDirectoryInfo->lpLinkedInfo)
				{
					dwShared++;
				}
				if (lpDir->lpDirectoryInfo->lReferenceCount > 1)
				{
					dwInfoInUse++;
				}
				dwFiles += lpDir->lpDirectoryInfo->dwDirectorySize;
			}
			else
			{
				dwNoInfo++;
			}
		}
		LeaveCriticalSection(&lpDirTable->CriticalSection);

		if (z == 0)
		{
			FormatString(lpBuffer, "%s", tszMultilinePrefix);
		}
		FormatString(lpBuffer, " %6d", lpDirTable->dwDirectories);
		if (++z > 5)
		{
			FormatString(lpBuffer, "%s", _T("\r\n"));
			z = 0;
		}
	}
	if (z != 0)
	{
		FormatString(lpBuffer, "%s", _T("\r\n"));
	}
	FormatString(lpBuffer, "%s\r\n%s Total: Dirs = %d, InUse = %d, Locked = %d, Shared = %d\r\n", tszMultilinePrefix, tszMultilinePrefix,
		dwTotal, dwInUse, dwLocked, dwShared);
	FormatString(lpBuffer, "%s Total: FileInfos = %d, InfosUsed = %d, NoInfo = %d\r\n", tszMultilinePrefix, dwFiles, dwInfoInUse, dwNoInfo);
	return NULL;
}

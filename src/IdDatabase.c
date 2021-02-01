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



__inline static
INT __cdecl IdDataBase_NameCompare(LPIDITEM *a, LPIDITEM *b)
{
  register INT  i;

  i  = _tcsicmp(a[0]->tszName, b[0]->tszName);
  return (i != 0 ? i : _tcscmp(a[0]->tszName, b[0]->tszName));
}





BOOL IdDataBase_Init(LPTSTR tszTableLocation, LPIDDATABASE lpDataBase,
           LPVOID FindModuleProc, ID_DB_CB OpenIdDataProc, ID_DB_ERROR ErrorProc)
{
  LPIDITEM  lpId;
  HANDLE    hTableFile;
  LPVOID    hModule, lpMemory;
  DWORD    dwFileSize, dwBytesRead;
  LPTSTR    tszModuleName, tszName;
  TCHAR    *tpBuffer, *tpBufferEnd, *tpNewline, *tpLine, *tpDoubleColon[2], *tpCheck;
  DWORD    dwName, dwModuleName, dwSubTable;
  BOOL    bReturn;
  INT      Id, iResult;

  tpBuffer = NULL;
  lpDataBase->tszFileName  = tszTableLocation;
  bReturn  = TRUE;
  //  Initialize lock object unless just validating
  if (!ErrorProc && !InitializeLockObject(&lpDataBase->loDataBase)) return TRUE;
  //  Open file
  hTableFile  = CreateFile(tszTableLocation, GENERIC_READ, FILE_SHARE_READ,
    NULL, OPEN_EXISTING, 0, NULL);
  if (hTableFile == INVALID_HANDLE_VALUE) return TRUE;
  //  Get filesize
  if ((dwFileSize = GetFileSize(hTableFile, NULL)) == INVALID_FILE_SIZE) goto DONE;
  //  Allocate read buffer
  if (! (tpBuffer = (PCHAR)Allocate(NULL, dwFileSize + 2*sizeof(TCHAR)))) goto DONE;
  //  Read from file
  if (! ReadFile(hTableFile, tpBuffer, dwFileSize, &dwBytesRead, NULL) ||
    ! dwBytesRead) goto DONE;

  //  Pad buffer with newline just in case it doesn't have one after last line
  tpBuffer[dwBytesRead++]  = _T('\n');
  tpBuffer[dwBytesRead]  = 0;
  dwBytesRead  = dwBytesRead / sizeof(TCHAR) + 1;
  //  Store important offsets
  tpBufferEnd  = &tpBuffer[dwBytesRead];
  tpLine    = tpBuffer;
  //  Seek through buffer
  for (;tpNewline = (TCHAR *)_tmemchr(tpLine, _TEXT('\n'), tpBufferEnd - tpLine);tpLine = &tpNewline[1])
  {
    tpNewline[0]  = '\0';
	if (*tpLine == _T('\r'))
	{
		// skip over return char
		tpLine++;
	}

	if (tpLine == tpNewline) continue;
	//  Find two double colons from line
    if (! (tpDoubleColon[0] = _tcschr(tpLine, _TEXT(':'))) ||
      ! (tpDoubleColon[1] = _tcschr(&tpDoubleColon[0][1], _TEXT(':')))) 
	{
		if ((tpNewline != tpBuffer) && tpNewline[-1] == _T('\r'))
		{
			tpNewline[-1] = 0;
		}
		Putlog(LOG_ERROR, _T("Unable to parse tuple on line '%s' in '%s'.\r\n"), tpLine, tszTableLocation);
		if (ErrorProc) (ErrorProc)(4, tszTableLocation, -1, tpLine, tszModuleName);
		continue;
	}
    //  Get offsets
    tszName      = tpLine;
    tszModuleName  = &tpDoubleColon[1][1];
    //  Calculate string lengths
    dwName      = tpDoubleColon[0] - tszName;
    dwModuleName  = &tpNewline[(tpNewline[-1] == _TEXT('\r') ? -1 : 0)] - tszModuleName;
    //  Add zero paddings
    tszModuleName[dwModuleName]  = _TEXT('\0');
    tszName[dwName]  = _TEXT('\0');
    //  Convert id string to integer
    Id  = _tcstol(&tpDoubleColon[0][1], &tpCheck, 10);
    //  Verify id conversion and string lengths
    if (! dwName || dwName > _MAX_NAME ||
      ! dwModuleName || dwModuleName > _MAX_NAME ||
      tpCheck[0] != _TEXT(':') || tpCheck == &tpDoubleColon[0][1]) 
	{
		Putlog(LOG_ERROR, _T("Something wrong on line starting with '%s' in '%s'.\r\n"), tpLine, tszTableLocation);
		if (ErrorProc) (ErrorProc)(5, tszTableLocation, -1, tpLine, tszModuleName);
		continue;
	}

    //  Get subtable
    dwSubTable  = Id >> 10;
    //  Check if table has been allocated
    if (! lpDataBase->lpNameTable[dwSubTable])
    {
      //  Allocate memory for subtable
      if (! (lpMemory = Allocate(NULL, sizeof(LPTSTR) * 1024))) break;
      //  Zero table
      ZeroMemory(lpMemory, sizeof(LPTSTR) * 1024);
      lpDataBase->lpNameTable[dwSubTable]  = (LPTSTR *)lpMemory;
    }

	if (lpDataBase->lpNameTable[dwSubTable][Id & 01777] != 0)
	{
		// duplicate id!
		Putlog(LOG_ERROR, _TEXT("Warning: Duplicate ID (#%d) defined in '%s': entry '%s' ignored!\r\n"),
			Id, tszTableLocation, tszName);
		if (ErrorProc) (ErrorProc)(1, tszTableLocation, Id, tszName, tszModuleName);
		continue;
	}

	//  Allocate memory for id item
    if (! (lpId = (LPIDITEM)Allocate(NULL, sizeof(IDITEM))))
    {
      Free(lpId);
      break;
    }
    //  Copy data
    lpId->Id  = Id;
    lpId->tszName[_MAX_NAME]  = _TEXT('\0');
    CopyMemory(lpId->tszName, tszName, (dwName + 1) * sizeof(TCHAR));
    CopyMemory(lpId->tszModuleName, tszModuleName, (dwModuleName + 1) * sizeof(TCHAR));
    //  Find module by name
    hModule  = ((LPVOID (__cdecl *)(LPSTR))FindModuleProc)(tszModuleName);
    if (hModule)
    {
      //  Call read procedure
      iResult  = (OpenIdDataProc)(hModule, tszName, Id);

      if (iResult == DB_DELETED)
      {
		  //  Item has been deleted
		  Putlog(LOG_SYSOP, _T("Module '%s' reports item ID=%d NAME='%s' deleted from database '%s'.\r\n"),
			  tszModuleName, lpId->Id, lpId->tszName, tszTableLocation);
		  Free(lpId);
		  continue;
      }
      if (iResult == DB_FATAL) break;
    }
    else
    {
      //  Log error
      Putlog(LOG_ERROR, _TEXT("Warning: Unknown module '%s' defined in '%s': entry '%s' ignored!\r\n"),
        tszModuleName, tszTableLocation, tszName);
	  if (ErrorProc) (ErrorProc)(2, tszTableLocation, Id, tszName, tszModuleName);
    }

    //  Check available buffer space
    if (lpDataBase->dwIdArraySize == lpDataBase->dwIdArrayItems)
    {
      //  Reallocate memory
      lpMemory  = ReAllocate(lpDataBase->lpIdArray, NULL, (lpDataBase->dwIdArraySize + 512) * sizeof(LPIDITEM));
      if (! lpMemory) break;
      //  Update array
      lpDataBase->lpIdArray    = (LPIDITEM *)lpMemory;
      lpDataBase->dwIdArraySize  += 512;
    }

    //  Insert item to array
    iResult  = QuickInsert(lpDataBase->lpIdArray, lpDataBase->dwIdArrayItems,
		                   lpId, (QUICKCOMPAREPROC) IdDataBase_NameCompare);
    //  Sanity check
    if (iResult)
    {
		// duplicate name!
		Putlog(LOG_ERROR, _TEXT("Warning: Duplicate name defined in '%s': entry '%s' (id=%d) ignored!\r\n"),
			tszTableLocation, tszName, Id);
		if (ErrorProc) (ErrorProc)(3, tszTableLocation, Id, tszName, tszModuleName);

		Free(lpId);
		continue;
    }

    lpDataBase->dwIdArrayItems++;
    lpDataBase->iNextFreeId  = max(lpDataBase->iNextFreeId, Id + 1);
	lpDataBase->lpNameTable[dwSubTable][Id & 01777]  = lpId->tszName;
  }
  if (! tpNewline) bReturn  = FALSE;
DONE:
  //  Close handle
  CloseHandle(hTableFile);
  if (tpBuffer) Free(tpBuffer);

  return bReturn;
}



BOOL IdDataBase_Write(LPIDDATABASE lpDataBase)
{
  HANDLE    hTableFile;
  LPTSTR    tszFileName;
  DWORD    dwFileName, n, dwBytesWritten, dwBytesToWrite;
  TCHAR    tpBuffer[256];
  BOOL    bReturn;

  bReturn    = TRUE;
  dwFileName  = strlen(lpDataBase->tszFileName);
  //  Allocate buffer for temporary filename
  tszFileName  = (LPSTR)Allocate(NULL, dwFileName + 5);
  if (! tszFileName) return TRUE;

  //  Open file
  wsprintf(tszFileName, "%s.new", lpDataBase->tszFileName);
  hTableFile  = CreateFile(tszFileName, GENERIC_WRITE, FILE_SHARE_READ,
    NULL, CREATE_ALWAYS, 0, NULL);
  if (hTableFile == INVALID_HANDLE_VALUE) goto DONE;

  bReturn  = FALSE;
  //  Loop through database
  for (n = 0;n < lpDataBase->dwIdArrayItems;n++)
  {
    //  Prepare buffer
    dwBytesToWrite  = wsprintf(tpBuffer, "%s:%i:%s\r\n",
      lpDataBase->lpIdArray[n]->tszName,
      lpDataBase->lpIdArray[n]->Id, lpDataBase->lpIdArray[n]->tszModuleName) * sizeof(TCHAR);
    //  Write to file
    if (! WriteFile(hTableFile, tpBuffer, dwBytesToWrite, &dwBytesWritten, NULL) ||
      dwBytesWritten != dwBytesToWrite)
    {
      bReturn  = TRUE;
      break;
    }
  }
DONE:
  //  Close handle
  if (hTableFile != INVALID_HANDLE_VALUE)
  {
	  FlushFileBuffers(hTableFile);
	  CloseHandle(hTableFile);
  }
  //  Replace file
  if (! bReturn) bReturn  = (MoveFileEx(tszFileName, lpDataBase->tszFileName, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? FALSE : TRUE);
  Free(tszFileName);

  return bReturn;
}



INT32 IdDataBase_Add(LPTSTR tszName, LPTSTR tszModuleName, LPIDDATABASE lpDataBase)
{
  LPFREED_IDITEM  lpFreedId;
  LPIDITEM    lpId;
  LPVOID      lpMemory;
  DWORD      dwSubTable, dwError;
  INT        iResult, iReturn, Id;

  iReturn  = -1;
  
  AcquireExclusiveLock(&lpDataBase->loDataBase);
  //  Check memory pool
  if (lpDataBase->lpFreedId)
  {
    lpId  = (LPIDITEM)lpDataBase->lpFreedId;
    lpDataBase->lpFreedId  = lpDataBase->lpFreedId->lpNext;
  }
  else
  {
    //  Allocate memory for new item
    if (! (lpId = (LPIDITEM)Allocate(NULL, sizeof(IDITEM))))
    {
      ReleaseExclusiveLock(&lpDataBase->loDataBase);
      SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      return -1;
    }
    //  Reset memory
    lpId->tszName[_MAX_NAME]  = _TEXT('\0');
    lpId->tszModuleName[_MAX_NAME]  = _TEXT('\0');
  }
  //  Copy data
  _tcsncpy(lpId->tszName, tszName, _MAX_NAME);
  _tcsncpy(lpId->tszModuleName, tszModuleName, _MAX_NAME);
  //  Get next id
  Id  = lpDataBase->iNextFreeId;
  lpId->Id  = Id;
  dwSubTable  = Id >> 10;

  //  Check available buffer space
  if (lpDataBase->dwIdArraySize == lpDataBase->dwIdArrayItems)
  {
    //  Reallocate memory
    lpMemory  = ReAllocate(lpDataBase->lpIdArray, NULL, (lpDataBase->dwIdArraySize + 512) * sizeof(LPIDITEM));
    //  Verify allocation
    if (lpMemory)
    {
      //  Update array
      lpDataBase->lpIdArray    = (LPIDITEM *)lpMemory;
      lpDataBase->dwIdArraySize  += 512;
    }
    else
    {
      //  Out of memory
      dwError  = ERROR_NOT_ENOUGH_MEMORY;
      Id    = -1;
    }
  }
  
  if (Id != -1 &&
    ! lpDataBase->lpNameTable[dwSubTable])
  {
    //  Allocate memory for subtable
    if (! (lpMemory = Allocate(NULL, sizeof(LPTSTR) * 1024)))
    {
      //  Zero table
      ZeroMemory(lpMemory, sizeof(LPTSTR) * 1024);
      lpDataBase->lpNameTable[dwSubTable]  = (LPTSTR *)lpMemory;
    }
    else
    {
      //  Out of memory
      dwError  = ERROR_NOT_ENOUGH_MEMORY;
      Id    = -1;
    }
  }

  if (Id != -1)
  {
    //  Insert item to array
    iResult  = QuickInsert(lpDataBase->lpIdArray, lpDataBase->dwIdArrayItems,
		                   lpId, (QUICKCOMPAREPROC) IdDataBase_NameCompare);
    //  Check result
    if (! iResult)
    {
      dwError  = NO_ERROR;
      iReturn  = Id;
      lpDataBase->iNextFreeId++;
      lpDataBase->dwIdArrayItems++;
      lpDataBase->lpNameTable[dwSubTable][Id & 01777]  = lpId->tszName;
      //  Update id database
      IdDataBase_Write(lpDataBase);
    }
    else dwError  = ERROR_ID_EXISTS;
  }

  //  Add unused item to pool
  if (iReturn == -1)
  {
    lpFreedId  = (LPFREED_IDITEM)lpId;
    lpFreedId->lpNext  = lpDataBase->lpFreedId;
    lpDataBase->lpFreedId  = lpFreedId;
  }
  ReleaseExclusiveLock(&lpDataBase->loDataBase);
  //  Set error
  SetLastError(dwError);

  return iReturn;
}





BOOL IdDataBase_Rename(LPTSTR tszName, LPTSTR tszModuleName, LPTSTR tszNewName, LPIDDATABASE lpDataBase)
{
  LPIDITEM  lpId, *lpResult;
  DWORD    dwBytesToMove, dwError;
  INT      iResult;


  //  Prepare search item
  lpId  = (LPIDITEM)((ULONG)tszName - offsetof(IDITEM, tszName));

  AcquireExclusiveLock(&lpDataBase->loDataBase);
  //  Binary search
  lpResult  = (LPIDITEM *)bsearch(&lpId, lpDataBase->lpIdArray, lpDataBase->dwIdArrayItems,
	                              sizeof(LPIDITEM), (QUICKCOMPAREPROC) IdDataBase_NameCompare);

  if (lpResult)
  {
    lpId  = lpResult[0];
    //  Verify module name
    if (! _tcsicmp(lpId->tszModuleName, tszModuleName))
    {
      //  Reduce size
      lpDataBase->dwIdArrayItems--;
      //  Calculate number of bytes to move
      dwBytesToMove  = (ULONG)&lpDataBase->lpIdArray[lpDataBase->dwIdArrayItems] - (ULONG)lpResult;
      //  Remove item from array
      MoveMemory(lpResult, &lpResult[1], dwBytesToMove);
      //  Update name
      _tcsncpy(lpId->tszName, tszNewName, _MAX_NAME);
      //  Insert item to database
      iResult  = QuickInsert(lpDataBase->lpIdArray, lpDataBase->dwIdArrayItems,
		                     lpId, (QUICKCOMPAREPROC) IdDataBase_NameCompare);

      if (! iResult)
      {
        dwError  = NO_ERROR;
      }
      else
      {
        //  Already exists
        dwError  = ERROR_ID_EXISTS;
        //  Update name
        _tcsncpy(lpId->tszName, tszName, _MAX_NAME);
        //  Insert item to database
        QuickInsert(lpDataBase->lpIdArray, lpDataBase->dwIdArrayItems,
			        lpId, (QUICKCOMPAREPROC) IdDataBase_NameCompare);
      }
      //  Increase size
      lpDataBase->dwIdArrayItems++;
      //  Update id database
      if (! iResult) IdDataBase_Write(lpDataBase);
    }
  }
  else dwError  = ERROR_ID_NOT_FOUND;
  ReleaseExclusiveLock(&lpDataBase->loDataBase);
  //  Set error
  SetLastError(dwError);

  return (dwError ? TRUE : FALSE);
}




BOOL IdDataBase_Remove(LPTSTR tszName, LPTSTR tszModuleName, LPIDDATABASE lpDataBase)
{
  LPFREED_IDITEM  lpFreedId;
  LPIDITEM    lpId, *lpResult;
  DWORD      dwBytesToMove;
  DWORD      dwError;

  //  Prepare search item
  lpId  = (LPIDITEM)((ULONG)tszName - offsetof(IDITEM, tszName));

  AcquireExclusiveLock(&lpDataBase->loDataBase);
  //  Binary search
  lpResult  = (LPIDITEM *)bsearch(&lpId, lpDataBase->lpIdArray, lpDataBase->dwIdArrayItems,
	                              sizeof(LPIDITEM), (QUICKCOMPAREPROC) IdDataBase_NameCompare);

  if (lpResult)
  {
    lpId  = lpResult[0];
    //  Verify module name
    if (! _tcsicmp(lpId->tszModuleName, tszModuleName))
    {
      dwError  = NO_ERROR;
      //  Reduce size
      lpDataBase->dwIdArrayItems--;
      //  Calculate number of bytes to move
      dwBytesToMove  = (ULONG)&lpDataBase->lpIdArray[lpDataBase->dwIdArrayItems] - (ULONG)lpResult;
      //  Remove item from array
      MoveMemory(lpResult, &lpResult[1], dwBytesToMove);
      //  Update id database
      IdDataBase_Write(lpDataBase);
      //  Remove pointer from tables
      lpDataBase->lpNameTable[lpId->Id >> 10][lpId->Id & 01777]  = NULL;
      //  Add unuseditem to pool
      lpFreedId  = (LPFREED_IDITEM)lpId;
      lpFreedId->lpNext  = lpDataBase->lpFreedId;
      lpDataBase->lpFreedId  = lpFreedId;
    }
    else dwError  = ERROR_MODULE_CONFLICT;
  }
  else dwError  = ERROR_ID_NOT_FOUND;
  ReleaseExclusiveLock(&lpDataBase->loDataBase);
  //  Set error
  SetLastError(dwError);

  return (dwError ? TRUE : FALSE);
}


INT32 IdDataBase_GetNextId(LPINT lpOffset, LPIDDATABASE lpDataBase)
{
  INT  iOffset, iReturn;

  //  Get next offset
  iOffset  = lpOffset[0] + 1;
  //  Set current offset
  lpOffset[0]  = iOffset;

  AcquireSharedLock(&lpDataBase->loDataBase);
  //  Get id
  iReturn  = (iOffset < (int) lpDataBase->dwIdArrayItems ? lpDataBase->lpIdArray[iOffset]->Id : -1);
  ReleaseSharedLock(&lpDataBase->loDataBase);

  return iReturn;
}


PINT32 IdDataBase_GetIdList(LPIDDATABASE lpDataBase, LPDWORD lpIdCount)
{
  LPIDITEM  *lpIdArray;
  PINT32    lpReturn, lpIdCopy;
  DWORD    n;

  AcquireSharedLock(&lpDataBase->loDataBase);
  //  Allocate memory
  lpReturn  = (LPINT)Allocate("IdDataBase:Copy", lpDataBase->dwIdArrayItems * sizeof(INT32));
  if (lpReturn)
  {
    lpIdArray  = lpDataBase->lpIdArray;
    lpIdCopy  = lpReturn;
    //  Copy ids
    for (n = lpDataBase->dwIdArrayItems;n--;)
    {
      (lpIdCopy++)[0]  = (lpIdArray++)[0]->Id;
    }
    lpIdCount[0]  = lpDataBase->dwIdArrayItems;
  }
  ReleaseSharedLock(&lpDataBase->loDataBase);
  return lpReturn;
}


INT32
IdDataBase_SearchByName(LPTSTR tszName,
                        LPIDDATABASE lpDataBase)
{
  LPIDITEM lpSearch;
  LPVOID   p;
  INT      r;

  if(!tszName) {
    SetLastError(ERROR_INVALID_IDNAME);
    return -1;
  }

  //  Prepare search item
  lpSearch  = (LPIDITEM)((ULONG)tszName - offsetof(IDITEM, tszName));

  AcquireSharedLock(&lpDataBase->loDataBase);
  //  Binary search
  p=bsearch(&lpSearch,
            lpDataBase->lpIdArray,
            lpDataBase->dwIdArrayItems,
            sizeof(LPIDITEM),
            (QUICKCOMPAREPROC) IdDataBase_NameCompare);
  //  Check result
  r=(p ? ((LPIDITEM *)p)[0]->Id : -1);
  ReleaseSharedLock(&lpDataBase->loDataBase);

  if(-1==r) SetLastError(ERROR_ID_NOT_FOUND);
  return r;
}


LPSTR IdDataBase_SearchById(INT32 Id, LPIDDATABASE lpDataBase)
{
	DWORD Index = Id >> 10;

	if (Index > 1024) return NULL;
	//  Get name
	return (lpDataBase->lpNameTable[Index] ?
		lpDataBase->lpNameTable[Index][Id & 01777] : NULL);
}



// Used by Admin_Verify and during shutdown
VOID
IdDataBase_Free(LPIDDATABASE lpDataBase)
{
	LPIDITEM  *lpIdArray;
	DWORD      n;
	LPTSTR	  **lpNameTable;

	//  Allocate memory
	lpIdArray  = lpDataBase->lpIdArray;

	if (lpIdArray)
	{
		for (n = lpDataBase->dwIdArrayItems; n-- ; )
		{
			Free(*lpIdArray++);
		}
		Free(lpDataBase->lpIdArray);
	}

	if (lpDataBase->lpNameTable)
	{
		lpNameTable = lpDataBase->lpNameTable;
		for (n=1024 ; n ; n--, lpNameTable++)
		{
			if (*lpNameTable)
			{
				Free(*lpNameTable);
			}
		}
	}

	//  Delete lock object
	DeleteLockObject(&lpDataBase->loDataBase);
	//  Free memory
	if (lpDataBase->tszFileName) Free(lpDataBase->tszFileName);
}

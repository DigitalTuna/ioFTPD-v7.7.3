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

// Used in Admin_Verify
IDDATABASE           dbGroupId;
LPPARENT_GROUPFILE  *lpGroupFileArray;
LOCKOBJECT           loGroupFileArray;
DWORD                dwGroupFileArrayItems, dwGroupFileArraySize;


//  Local declarations
static INT GroupFile_GidCompare(LPPARENT_GROUPFILE *a, LPPARENT_GROUPFILE *b);
static BOOL Group_Unregister(LPGROUP_MODULE lpModule, LPTSTR tszGroupName);
static BOOL Group_RegisterAs(LPGROUP_MODULE lpModule, LPTSTR tszGroupName, LPTSTR tszNewGroupName);
static INT Group_Open(LPGROUP_MODULE lpModule, LPTSTR tszGroupName, INT32 Gid);
static INT32 Group_Register(LPGROUP_MODULE lpModule, LPTSTR tszGroupName, LPGROUPFILE lpGroupFile);
static BOOL Group_Update(LPGROUPFILE lpGroupFile);


//  Local variables
static LPGROUP_MODULE    lpGroupModuleList, lpStandardGroupModule;


// unused?
static INT32 Group_Module_Create(LPGROUP_MODULE lpModule, LPTSTR lpGroupName)
{
	INT32 iResult;

	if (lpModule != lpStandardGroupModule) SetBlockingThreadFlag();
	iResult = (lpModule->Create)(lpGroupName);
	if (lpModule != lpStandardGroupModule) SetNonBlockingThreadFlag();
	return iResult;
}

static BOOL Group_Module_Delete(LPGROUP_MODULE lpModule, LPTSTR lpGroupName, INT32 Uid)
{
	BOOL bResult;

	if (lpModule != lpStandardGroupModule) SetBlockingThreadFlag();
	bResult = (lpModule->Delete)(lpGroupName, Uid);
	if (lpModule != lpStandardGroupModule) SetNonBlockingThreadFlag();
	return bResult;
}

static BOOL Group_Module_Rename(LPGROUP_MODULE lpModule, LPTSTR lpOldName, INT32 Uid, LPTSTR lpNewName)
{
	BOOL bResult;

	if (lpModule != lpStandardGroupModule) SetBlockingThreadFlag();
	bResult = (lpModule->Rename)(lpOldName, Uid, lpNewName);
	if (lpModule != lpStandardGroupModule) SetNonBlockingThreadFlag();
	return bResult;
}

static BOOL Group_Module_Lock(LPGROUP_MODULE lpModule, LPGROUPFILE lpGroupFile)
{
	BOOL bResult;

	if (lpModule != lpStandardGroupModule) SetBlockingThreadFlag();
	bResult = (lpModule->Lock)(lpGroupFile);
	if (lpModule != lpStandardGroupModule) SetNonBlockingThreadFlag();
	return bResult;
}

static BOOL Group_Module_Write(LPGROUP_MODULE lpModule, LPGROUPFILE lpGroupFile)
{
	BOOL bResult;

	if (lpModule != lpStandardGroupModule) SetBlockingThreadFlag();
	bResult = (lpModule->Write)(lpGroupFile);
	if (lpModule != lpStandardGroupModule) SetNonBlockingThreadFlag();
	return bResult;
}

static INT Group_Module_Open(LPGROUP_MODULE lpModule, LPTSTR lpGroupName, LPGROUPFILE lpGroupFile)
{
	INT  iResult;

	if (lpModule != lpStandardGroupModule) SetBlockingThreadFlag();
	iResult = (lpModule->Open)(lpGroupName, lpGroupFile);
	if (lpModule != lpStandardGroupModule) SetNonBlockingThreadFlag();
	return iResult;
}

static BOOL Group_Module_Close(LPGROUP_MODULE lpModule, LPGROUPFILE lpGroupFile)
{
	BOOL bResult;

	if (lpModule != lpStandardGroupModule) SetBlockingThreadFlag();
	bResult = (lpModule->Close)(lpGroupFile);
	if (lpModule != lpStandardGroupModule) SetNonBlockingThreadFlag();
	return bResult;
}

static BOOL Group_Module_Unlock(LPGROUP_MODULE lpModule, LPGROUPFILE lpGroupFile)
{
	BOOL bResult;

	if (lpModule != lpStandardGroupModule) SetBlockingThreadFlag();
	bResult = (lpModule->Unlock)(lpGroupFile);
	if (lpModule != lpStandardGroupModule) SetNonBlockingThreadFlag();
	return bResult;
}



static INT __cdecl GroupFile_GidCompare(LPPARENT_GROUPFILE *a, LPPARENT_GROUPFILE *b)
{
  if (a[0]->Gid < b[0]->Gid) return -1;
  return (a[0]->Gid > b[0]->Gid);
}



LPPARENT_GROUPFILE GetParentGroupFileSafely(INT32 Gid)
{
	LPPARENT_GROUPFILE  lpParentGroupFile, *lppResult;
	PARENT_GROUPFILE  ParentGroupFile;

	ParentGroupFile.Gid  = Gid;
	lpParentGroupFile  = &ParentGroupFile;
	//  Locate groupfile
	AcquireSharedLock(&loGroupFileArray);
	lppResult  = bsearch(&lpParentGroupFile,	lpGroupFileArray, dwGroupFileArrayItems,
	 	                 sizeof(LPPARENT_GROUPFILE), (QUICKCOMPAREPROC) GroupFile_GidCompare);
	ReleaseSharedLock(&loGroupFileArray);

	return (lppResult ? *lppResult : NULL);
}


LPPARENT_GROUPFILE GetParentGroupFile(INT32 Gid)
{
  LPPARENT_GROUPFILE  lpParentGroupFile, *lppResult;
  PARENT_GROUPFILE  ParentGroupFile;

  ParentGroupFile.Gid  = Gid;
  lpParentGroupFile  = &ParentGroupFile;
  //  Locate groupfile
  lppResult  = bsearch(&lpParentGroupFile, lpGroupFileArray, dwGroupFileArrayItems,
	                   sizeof(LPPARENT_GROUPFILE), (QUICKCOMPAREPROC) GroupFile_GidCompare);

  return (lppResult ? *lppResult : NULL);
}



INT32 Group2Gid(LPTSTR tszGroupName)
{
  //  Seek by name
  return IdDataBase_SearchByName(tszGroupName, &dbGroupId);
}

LPTSTR Gid2Group(INT32 Gid)
{
  //  Seek by id
  return IdDataBase_SearchById(Gid, &dbGroupId);
}




LPGROUP_MODULE Group_FindModule(LPTSTR tszModuleName)
{
  LPGROUP_MODULE  lpModule;

  for (lpModule = lpGroupModuleList;lpModule;lpModule = lpModule->lpNext)
  {
    if (! strcmp(lpModule->tszModuleName, tszModuleName)) break;
  }
  return lpModule;
}






BOOL Group_Init(BOOL bFirstInitialization)
{
  LPGROUP_MODULE  lpModule;
  LPTSTR      tszFileName;
  BOOL      (* InitProc)(LPGROUP_MODULE);
  INT        iOffset;

  if (! bFirstInitialization) return TRUE;

  //  Reset local variables
  tszFileName  = NULL;
  iOffset    = 0;
  lpGroupFileArray  = NULL;
  lpGroupModuleList  = NULL;
  dwGroupFileArrayItems  = 0;
  dwGroupFileArraySize  = 0;
  ZeroMemory(&dbGroupId, sizeof(IDDATABASE));
  ZeroMemory(&loGroupFileArray, sizeof(LOCKOBJECT));
  //  Initialize modules
  do
  {
    //  Allocate memory for module
    lpModule  = (LPGROUP_MODULE)Allocate(NULL, sizeof(USER_MODULE));
    if (! lpModule)
    {
      Free(tszFileName);
      return FALSE;
    }
    //  Set pointers to local functions
    ZeroMemory(lpModule, sizeof(GROUP_MODULE));
    lpModule->Register    = Group_Register;
    lpModule->RegisterAs  = Group_RegisterAs;
    lpModule->Unregister  = Group_Unregister;
    lpModule->Update    = Group_Update;
    lpModule->GetProc    = GetProc;
    lpModule->tszModuleFileName  = tszFileName;

    if (lpGroupModuleList)
    {
      //  Load module
      if (lpModule->hModule = LoadLibrary(tszFileName))
      {
        InitProc  = (BOOL (__cdecl *)(LPGROUP_MODULE))GetProcAddress(lpModule->hModule, _TEXT("GroupModuleInit"));
      }
      else InitProc  = NULL;
    }
    else
    {
      //  Internal module
      InitProc  = Group_StandardInit;
    }

    //  Initialize module
    if (InitProc && ! InitProc(lpModule))
    {
      lpModule->lpNext  = lpGroupModuleList;
      lpGroupModuleList  = lpModule;
    }
    else
    {
      if (lpModule->hModule) FreeLibrary(lpModule->hModule);
      Free(tszFileName);
      Free(lpModule);
    }

  } while ((tszFileName = Config_Get(&IniConfigFile, _TEXT("Modules"), _TEXT("GroupModule"), NULL, &iOffset)));

  if (! lpGroupModuleList) return FALSE;
  lpStandardGroupModule = Group_FindModule(_T("STANDARD"));
  if (! lpStandardGroupModule) return FALSE;

  //  Get path to local group id table
  tszFileName  = Config_Get(&IniConfigFile, _TEXT("Locations"), _TEXT("Group_Id_Table"), NULL, NULL);
  if (! tszFileName) return FALSE;
  //  Initialize lock object
  if (! InitializeLockObject(&loGroupFileArray)) return FALSE;
  //  Read id table
  if (IdDataBase_Init(tszFileName, &dbGroupId, Group_FindModule, Group_Open, 0)) return FALSE;

  return TRUE;
}




VOID Group_DeInit(VOID)
{
  LPGROUP_MODULE  lpModule;
  LPPARENT_GROUPFILE lpParent;

  //  Remove cache
  while (dwGroupFileArrayItems-- > 0)
  {
    //  Free resources
	  lpParent = lpGroupFileArray[dwGroupFileArrayItems];
    StopIoTimer(lpParent->lpTimer, FALSE);
	lpParent->lpModule->Close(lpParent->lpGroupFile);
	CloseHandle(lpParent->hPrimaryLock);
    FreeShared(lpParent->lpGroupFile);
	Free(lpParent->tszDefaultName);
	Free(lpParent);    
  }
  //  Close modules
  for (;lpModule = lpGroupModuleList;)
  {
    lpGroupModuleList  = lpGroupModuleList->lpNext;
    if (lpModule->DeInitialize) lpModule->DeInitialize();
    if (lpModule->hModule) FreeLibrary(lpModule->hModule);
    Free(lpModule->tszModuleFileName);
    Free(lpModule);
  }
  //  Free memory
  Free(lpGroupFileArray);
  //  Delete lock object
  DeleteLockObject(&loGroupFileArray);
  IdDataBase_Free(&dbGroupId);
}




static BOOL Group_Unregister(LPGROUP_MODULE lpModule, LPTSTR tszGroupName)
{
  LPPARENT_GROUPFILE  lpParentGroupFile;
  BOOL        bReturn, bFree;
  DWORD        dwError;
  INT32        Gid;

  bReturn        = TRUE;
  lpParentGroupFile  = NULL;
  dwError        = NO_ERROR;
  bFree        = FALSE;

  AcquireSharedLock(&loGroupFileArray);
  //  Resolve groupname to gid
  Gid  = Group2Gid(tszGroupName);
  if (Gid != -1)
  {
    //  Get parent groupfile
    lpParentGroupFile  = GetParentGroupFile(Gid);
    //  Check if module is owner
    if (lpParentGroupFile &&
      lpParentGroupFile->lpModule != lpModule)
    {
      //  Reset pointer
      lpParentGroupFile  = NULL;
      dwError        = ERROR_MODULE_CONFLICT;
    }
    else
    {
      //  Increase usage counter
      InterlockedIncrement(&lpParentGroupFile->lOpenCount);
    }
  }
  else dwError  = ERROR_GROUP_NOT_FOUND;
  ReleaseSharedLock(&loGroupFileArray);

  //  Sanity check
  if (! lpParentGroupFile) ERROR_RETURN(dwError, TRUE);
  
  AcquireExclusiveLock(&loGroupFileArray);
  //  Sanity check
  if (! (lpParentGroupFile->dwErrorFlags & WAIT_UNREGISTER))
  {
    //  Delete item from array
    QuickDelete(lpGroupFileArray, dwGroupFileArrayItems--,
		        lpParentGroupFile, (QUICKCOMPAREPROC) GroupFile_GidCompare, NULL);
    //  Set return value
    bReturn  = FALSE;
  }
  else dwError  = ERROR_GROUP_NOT_FOUND;
  //  Check usage
  if (InterlockedDecrement(&lpParentGroupFile->lOpenCount))
  {
    //  Memory in use, cannot free yet
    lpParentGroupFile->dwErrorFlags  |= WAIT_UNREGISTER;
  }
  else bFree  = TRUE;
  ReleaseExclusiveLock(&loGroupFileArray);

  //  Free resources
  if (bFree)
  {
    IdDataBase_Remove(tszGroupName, lpParentGroupFile->lpModule->tszModuleName, &dbGroupId);
    StopIoTimer(lpParentGroupFile->lpTimer, FALSE);
	Group_Module_Close(lpParentGroupFile->lpModule, lpParentGroupFile->lpGroupFile);
    CloseHandle(lpParentGroupFile->hPrimaryLock);
    FreeShared(lpParentGroupFile->lpGroupFile);
    Free(lpParentGroupFile);
  }
  SetLastError(dwError);
  return bReturn;
}





static INT Group_Open(LPGROUP_MODULE lpModule, LPTSTR tszGroupName, INT32 Gid)
{
  LPPARENT_GROUPFILE  lpParentGroupFile;
  LPGROUPFILE      lpGroupFile;
  LPVOID        lpMemory;
  HANDLE        hEvent;
  INT          iResult, iReturn;
  DWORD        dwLen;

  iReturn    = DB_FATAL;
  //  Allocate memory for new item
  lpParentGroupFile  = (LPPARENT_GROUPFILE)Allocate(NULL, sizeof(PARENT_GROUPFILE));
  //  Allocate shared memory for groupfile
  lpGroupFile  = (LPGROUPFILE)AllocateShared(NULL, "GroupFile", sizeof(GROUPFILE));
  //  Create event
  hEvent  = CreateEvent(NULL, FALSE, TRUE, NULL);

  if (! lpParentGroupFile ||
    ! lpGroupFile ||
    ! hEvent)
  {
    //  Free resources
    Free(lpParentGroupFile);
    FreeShared(lpGroupFile);
    if (hEvent) CloseHandle(hEvent);
    ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, DB_FATAL);
  }
  //  Reset contents of groupfile
  ZeroMemory(lpGroupFile, sizeof(GROUPFILE));
  ZeroMemory(lpParentGroupFile, sizeof(PARENT_GROUPFILE));
  //  Move groupfile inside parent
  lpParentGroupFile->Gid        = Gid;
  lpParentGroupFile->lpModule      = lpModule;
  lpParentGroupFile->hPrimaryLock    = hEvent;
  lpGroupFile->Gid          = Gid;
  //  Read groupfile
  iResult  = Group_Module_Open(lpModule, tszGroupName, lpGroupFile);
  //  Handle result
  switch (iResult)
  {
  case GM_SUCCESS:
	  //  Succesfully read
	  lpParentGroupFile->lpGroupFile      = lpGroupFile;
	  lpParentGroupFile->lpGroupFile->Gid    = Gid;
	  lpParentGroupFile->lpGroupFile->lpParent  = (LPVOID)lpParentGroupFile;
	  break;
  case GM_FATAL:
	  FreeShared(lpGroupFile);
	  Free(lpParentGroupFile);
	  CloseHandle(hEvent);
	  return DB_FATAL;
  case GM_DELETED:
	  //  Free resources
	  FreeShared(lpGroupFile);
	  Free(lpParentGroupFile);
	  CloseHandle(hEvent);
	  ERROR_RETURN(ERROR_GROUP_NOT_FOUND, DB_DELETED);
  }
  dwLen = 9 + _tcslen(tszGroupName);
  if (lpParentGroupFile->tszDefaultName = Allocate("DefaultGroupName", dwLen * sizeof(TCHAR)))
  {
	  sprintf_s(lpParentGroupFile->tszDefaultName, dwLen, "Default=%s", tszGroupName);
  }

  AcquireExclusiveLock(&loGroupFileArray);
  //  Check available memory
  if (dwGroupFileArrayItems == dwGroupFileArraySize)
  {
    lpMemory  = ReAllocate(lpGroupFileArray, NULL,
      (dwGroupFileArraySize + 512) * sizeof(LPPARENT_GROUPFILE));
    //  Verify allocation
    if (lpMemory)
    {
      lpGroupFileArray    = (LPPARENT_GROUPFILE *)lpMemory;
      dwGroupFileArraySize  += 512;
    }
  }
  //  Verify available memory
  if (dwGroupFileArrayItems < dwGroupFileArraySize)
  {
    //  Add parent groupfile to group array.. always succeeds
    QuickInsert(lpGroupFileArray, dwGroupFileArrayItems++,
      lpParentGroupFile, (QUICKCOMPAREPROC) GroupFile_GidCompare);

    iReturn  = DB_SUCCESS;
  }
  //  Release exclusive lock
  ReleaseExclusiveLock(&loGroupFileArray);
  //  Free resources
  if (iReturn == DB_FATAL)
  {
    FreeShared(lpGroupFile);
    Free(lpParentGroupFile);
    CloseHandle(hEvent);
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
  }

  return iReturn;
}


static INT Group_Close(LPGROUP_MODULE lpModule, LPTSTR tszGroupName, INT32 Gid)
{
}


DWORD TranslateGroupError(DWORD dwError)
{
  switch (dwError)
  {
  case ERROR_ID_EXISTS:
    return ERROR_GROUP_EXISTS;
  case ERROR_ID_NOT_FOUND:
    return ERROR_GROUP_NOT_FOUND;
  default:
    return dwError;
  }
}


static BOOL Group_RegisterAs(LPGROUP_MODULE lpModule, LPTSTR tszGroupName, LPTSTR tszNewGroupName)
{
  BOOL  bReturn;

  if (! tszGroupName) return TRUE;
  //  Call id rename
  bReturn  = IdDataBase_Rename(tszGroupName, lpModule->tszModuleName, tszNewGroupName, &dbGroupId);
  //  Error handler
  if (bReturn) SetLastError(TranslateGroupError(GetLastError()));
  return bReturn;
}



static INT32
Group_Register(LPGROUP_MODULE lpModule,
			   LPTSTR tszGroupName,
			   LPGROUPFILE lpGroupFile)
{
  LPPARENT_GROUPFILE  lpParentGroupFile;
  LPVOID        lpMemory;
  HANDLE        hEvent;
  DWORD        dwError;
  BOOL        bFreeParent, bFreeShared, bCloseEvent;
  INT32        Gid, iReturn;

  dwError=0;
  bFreeParent=TRUE;
  bFreeShared=TRUE;
  bCloseEvent=TRUE;
  iReturn=-1;

  if (! tszGroupName) return -1;
  //  Allocate memory for new item
  lpParentGroupFile  = (LPPARENT_GROUPFILE)Allocate(NULL, sizeof(PARENT_GROUPFILE));
  //  Allocate shared memory for groupfile
  if (lpGroupFile) lpMemory  = AllocateShared(NULL, "GroupFile", sizeof(GROUPFILE));
  //  Create event
  hEvent  = CreateEvent(NULL, FALSE, TRUE, NULL);

  if (! lpParentGroupFile ||
    (! lpMemory && lpGroupFile) ||
    ! hEvent)
  {
    //  Free resources
    Free(lpParentGroupFile);
    FreeShared(lpMemory);
    if (hEvent) CloseHandle(hEvent);
    ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, -1);
  }
  //  Reset contents of parent
  ZeroMemory(lpParentGroupFile, sizeof(PARENT_GROUPFILE));
  //  Copy groupfile contents
  if (lpGroupFile) CopyMemory(lpMemory, lpGroupFile, sizeof(GROUPFILE));
  //  Move groupfile inside parent
  lpParentGroupFile->lpGroupFile  = (LPGROUPFILE)lpMemory;
  lpParentGroupFile->hPrimaryLock  = hEvent;
  lpParentGroupFile->lpModule    = lpModule;

  AcquireExclusiveLock(&loGroupFileArray);
  //  Seek by groupname
  Gid  = Group2Gid(tszGroupName);

  if (Gid == -1)
  {
    //  Check available memory
    if (dwGroupFileArrayItems == dwGroupFileArraySize)
    {
      lpMemory  = ReAllocate(lpGroupFileArray, NULL,
        (dwGroupFileArraySize + 512) * sizeof(LPPARENT_GROUPFILE));
      //  Verify allocation
      if (lpMemory) {
        lpGroupFileArray    = (LPPARENT_GROUPFILE *)lpMemory;
        dwGroupFileArraySize  += 512;
      } else {
        dwError  = ERROR_NOT_ENOUGH_MEMORY;
      }
    }
    //  Verify available memory
    if (dwGroupFileArrayItems < dwGroupFileArraySize)
    {
      //  Add group to local database
      Gid  = IdDataBase_Add(tszGroupName,
        lpParentGroupFile->lpModule->tszModuleName, &dbGroupId);

      if (Gid != -1) {
        //  Store gid
        if (lpGroupFile)
        {
          lpParentGroupFile->lpGroupFile->Gid    = Gid;
          lpParentGroupFile->lpGroupFile->lpParent  = (LPVOID)lpParentGroupFile;
        }
        lpParentGroupFile->Gid  = Gid;
        //  Add parent groupfile to group array.. always succeeds
        QuickInsert(lpGroupFileArray, dwGroupFileArrayItems++,
          lpParentGroupFile, (QUICKCOMPAREPROC) GroupFile_GidCompare);
    
        bFreeParent  = FALSE;
        bFreeShared  = FALSE;
        bCloseEvent  = FALSE;
        iReturn    = Gid;
      } else {
        dwError  = TranslateGroupError(GetLastError());
      }
    }
  }
  else if (lpGroupFile)
  {
    lpMemory  = (LPVOID)lpParentGroupFile;
    //  Get parent groupfile
    lpParentGroupFile  = GetParentGroupFile(Gid);
    //  Make sure that groupfile is not loaded
    if (! lpParentGroupFile->lpGroupFile) {
      lpParentGroupFile->lpGroupFile        = ((LPPARENT_GROUPFILE)lpMemory)->lpGroupFile;
      lpParentGroupFile->lpGroupFile->lpParent  = (LPVOID)lpParentGroupFile;
      lpParentGroupFile->lpGroupFile->Gid      = Gid;
      lpParentGroupFile->lpModule          = lpModule;
      bFreeShared  = FALSE;
      iReturn    = Gid;
    } else {
      dwError  = ERROR_MODULE_CONFLICT;
    }
    //  Restore pointer
    lpParentGroupFile  = (LPPARENT_GROUPFILE)lpMemory;
  }
  ReleaseExclusiveLock(&loGroupFileArray);
  //  Free unused resources memory
  if (bFreeShared) FreeShared(lpParentGroupFile->lpGroupFile);
  if (bFreeParent) Free(lpParentGroupFile);
  if (bCloseEvent) CloseHandle(hEvent);
  //  Set error
  SetLastError(dwError);
  return iReturn;
}






static BOOL Group_Update(LPGROUPFILE lpGroupFile)
{
  LPGROUPFILE      lpNewGroupFile;
  DWORD        dwError;
  BOOL        bReturn;

  bReturn  = TRUE;
  //  Open groupfile
  if (! GroupFile_OpenPrimitive(lpGroupFile->Gid, &lpNewGroupFile, 0))
  {
    //  Lock groupfile
    if (! GroupFile_Lock(&lpNewGroupFile, NO_SYNC))
    {
      dwError  = NO_ERROR;
      bReturn  = FALSE;
      //  Get impotrant variables
      lpGroupFile->lpInternal  = lpNewGroupFile->lpInternal;
      lpGroupFile->lpParent  = lpNewGroupFile->lpParent;
      //  Update new groupfile
      CopyMemory(lpNewGroupFile, lpGroupFile, sizeof(GROUPFILE));
      //  Unlock groupfile
      GroupFile_Unlock(&lpNewGroupFile, NO_SYNC);
    }
    else dwError  = GetLastError();
    //  Close groupfile
    GroupFile_Close(&lpNewGroupFile, 0);
    //  Set last error
    SetLastError(dwError);
  }
  return bReturn;
}







BOOL GroupFile_Lock(LPGROUPFILE *lpGroupFile, DWORD dwFlags)
{
  LPPARENT_GROUPFILE  lpParentGroupFile;
  LPGROUPFILE      lpNewGroupFile;

  //  Get parent groupfile
  lpParentGroupFile  = (LPPARENT_GROUPFILE)lpGroupFile[0]->lpParent;
  //  Allocate shared memory
  lpNewGroupFile  = (LPGROUPFILE)AllocateShared(NULL, "GroupFile", sizeof(GROUPFILE));
  if (! lpNewGroupFile) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, TRUE);
  //  Free shared memory
  if (! (dwFlags & STATIC_SOURCE)) FreeShared(lpGroupFile[0]);
  //  Acquire primary lock
  WaitForSingleObject(lpParentGroupFile->hPrimaryLock, INFINITE);
  //  Copy data
  if (dwFlags & NO_SYNC)
  {
    lpNewGroupFile->lpInternal  = lpParentGroupFile->lpGroupFile->lpInternal;
  }
  else
  {
    CopyMemory(lpNewGroupFile, lpParentGroupFile->lpGroupFile, sizeof(GROUPFILE));
  }
  //  Tell module to lock now
  if (lpParentGroupFile->dwErrorFlags ||
    (! (dwFlags & NO_SYNC) && Group_Module_Lock(lpParentGroupFile->lpModule, lpNewGroupFile)))
  {
    //  Restore groupfile
    if (! (dwFlags & STATIC_SOURCE)) lpGroupFile[0]  = (LPGROUPFILE)AllocateShared(lpParentGroupFile->lpGroupFile, NULL, 0);
    //  Lock failed, handle error
    SetEvent(lpParentGroupFile->hPrimaryLock);
    //  Free memory
    FreeShared(lpNewGroupFile);
    ERROR_RETURN(ERROR_GROUP_LOCK_FAILED, TRUE);
  }
  //  Restore parent pointer
  lpNewGroupFile->lpParent  = lpParentGroupFile;
  //  Store groupfile location
  if (dwFlags & STATIC_SOURCE)
  {
    //  Copy memory
    CopyMemory(lpGroupFile[0], lpNewGroupFile, sizeof(GROUPFILE));
    //  Replace internal offset
    lpGroupFile[0]->lpInternal  = (LPVOID)lpNewGroupFile;
  }
  else lpGroupFile[0]  = lpNewGroupFile;

  return FALSE;
}






BOOL GroupFile_WriteTimer(LPPARENT_GROUPFILE lpParentGroupFile, LPTIMER lpTimer)
{
  WaitForSingleObject(lpParentGroupFile->hPrimaryLock, INFINITE);
  //  Try to write again
  if (Group_Module_Write(lpParentGroupFile->lpModule, lpParentGroupFile->lpGroupFile))
  {
    SetEvent(lpParentGroupFile->hPrimaryLock);
    //  Retry in 60seconds
    return (lpParentGroupFile->dwErrorFlags & WAIT_UNREGISTER ? 0 : 60000);
  }
  //  Remove error
  lpParentGroupFile->dwErrorFlags  = lpParentGroupFile->dwErrorFlags & (0xFFFF - WRITE_ERROR);
  //  Release module lock
  Group_Module_Unlock(lpParentGroupFile->lpModule, lpParentGroupFile->lpGroupFile);
  SetEvent(lpParentGroupFile->hPrimaryLock);
  return 0;
}






BOOL GroupFile_Unlock(LPGROUPFILE *lpGroupFile, DWORD dwFlags)
{
  LPPARENT_GROUPFILE  lpParentGroupFile;
  LPVOID        lpMemory, lpTemp;
  BOOL        bReturn;

  bReturn    = FALSE;

  if (dwFlags & STATIC_SOURCE)
  {
    lpTemp  = (LPVOID)lpGroupFile[0]->lpInternal;
    //  Restore local pointers
    lpGroupFile[0]->lpParent  = ((LPGROUPFILE)lpTemp)->lpParent;
    lpGroupFile[0]->lpInternal  = ((LPGROUPFILE)lpTemp)->lpInternal;
    //  Copy memory
    CopyMemory(lpTemp, lpGroupFile[0], sizeof(GROUPFILE));
    //  Swap pointer
    lpGroupFile  = (LPGROUPFILE *)&lpTemp;
  }
  //  Get parent groupfile
  lpParentGroupFile  = (LPPARENT_GROUPFILE)lpGroupFile[0]->lpParent;
  //  Compare data
  if (dwFlags & NO_SYNC ||
    memcmp(lpGroupFile[0], lpParentGroupFile->lpGroupFile, sizeof(GROUPFILE)))
  {
    //  Get old shared memory offset
    lpMemory  = (LPVOID)lpParentGroupFile->lpGroupFile;
    //  Acquire secondary lock
    while (InterlockedExchange(&lpParentGroupFile->lSecondaryLock, TRUE)) SwitchToThread();
    //  Store new shared memory offset
    lpParentGroupFile->lpGroupFile  = 
      (! (dwFlags & STATIC_SOURCE) ? (LPGROUPFILE)AllocateShared(lpGroupFile[0], NULL, 0) : lpGroupFile[0]);
    //  Release secondary lock
    InterlockedExchange(&lpParentGroupFile->lSecondaryLock, FALSE);
    //  Call write routine
    if (! lpParentGroupFile->dwErrorFlags &&
      ! (dwFlags & NO_SYNC) &&
	  Group_Module_Write(lpParentGroupFile->lpModule, lpGroupFile[0]))
    {
      //  Error while writing
      lpParentGroupFile->dwErrorFlags  |= WRITE_ERROR;
      //  Release resources from previous timer
      StopIoTimer(lpParentGroupFile->lpTimer, FALSE);
      //  Try to retry in 5seconds
      lpParentGroupFile->lpTimer  = StartIoTimer(NULL, GroupFile_WriteTimer, lpParentGroupFile, 5000);
      bReturn  = TRUE;
    }
  }
  else
  {
    //  Release new shared memory 
    lpMemory  = lpGroupFile[0];
    //  Get old share
    if (! (dwFlags & STATIC_SOURCE)) lpGroupFile[0]  = (LPGROUPFILE)AllocateShared(lpParentGroupFile->lpGroupFile, NULL, 0);
  }
  //  Release module lock
  if (! bReturn &&
    ! lpParentGroupFile->dwErrorFlags &&
    ! (dwFlags & NO_SYNC) &&
	Group_Module_Unlock(lpParentGroupFile->lpModule, lpGroupFile[0]))
  {
	  bReturn  = TRUE;
  }
  //  Release primary lock
  SetEvent(lpParentGroupFile->hPrimaryLock);
  //  Free old shared memory
  FreeShared(lpMemory);
  return bReturn;
}



BOOL RenameGroup(LPTSTR tszGroupName, LPTSTR tszNewGroupName)
{
  LPGROUP_MODULE    lpModule;
  LPPARENT_GROUPFILE  lpParentGroupFile;
  INT32        Gid;

  lpModule  = NULL;
  //  Find id by groupname
  if ((Gid = Group2Gid(tszGroupName)) == -1) ERROR_RETURN(ERROR_GROUP_NOT_FOUND, TRUE);

  AcquireSharedLock(&loGroupFileArray);
  //  Find module by id
  if ((lpParentGroupFile = GetParentGroupFile(Gid))) lpModule  = lpParentGroupFile->lpModule;
  ReleaseSharedLock(&loGroupFileArray);
  //  Sanity check
  if (! lpParentGroupFile) ERROR_RETURN(ERROR_GROUP_NOT_FOUND, TRUE);
  if (! lpModule) ERROR_RETURN(ERROR_MODULE_NOT_FOUND, TRUE);
  //  Call delete routine for group
  return Group_Module_Rename(lpModule, tszGroupName, Gid, tszNewGroupName);
}


INT32 CreateGroup(LPTSTR tszGroupName)
{
	INT32 Gid;
	DWORD dwLen;
	LPPARENT_GROUPFILE lpParentGroupFile;

	//  Call create routine of last module
	Gid = Group_Module_Create(lpGroupModuleList, tszGroupName);

	if (Gid == -1) return Gid;

	dwLen = 9 + _tcslen(tszGroupName);
	lpParentGroupFile = GetParentGroupFileSafely(Gid);
	if (!lpParentGroupFile)
	{
		// probably not a good sign...
		return Gid;
	}

	if (lpParentGroupFile->tszDefaultName = Allocate("DefaultGroupName", dwLen * sizeof(TCHAR)))
	{
		sprintf_s(lpParentGroupFile->tszDefaultName, dwLen, "Default=%s", tszGroupName);
	}

	return Gid;
}


BOOL DeleteGroup(LPTSTR tszGroupName)
{
  LPGROUP_MODULE    lpModule;
  LPPARENT_GROUPFILE  lpParentGroupFile;
  INT          Gid;
  LPTSTR       tszFileName;

  lpModule  = NULL;
  //  Find id by groupname
  if ((Gid = Group2Gid(tszGroupName)) == -1) ERROR_RETURN(ERROR_GROUP_NOT_FOUND, TRUE);

  AcquireSharedLock(&loGroupFileArray);
  //  Find module by id
  if ((lpParentGroupFile = GetParentGroupFile(Gid))) lpModule  = lpParentGroupFile->lpModule;
  ReleaseSharedLock(&loGroupFileArray);

  //  Sanity check
  if (! lpParentGroupFile) ERROR_RETURN(ERROR_GROUP_NOT_FOUND, TRUE);
  if (! lpModule) ERROR_RETURN(ERROR_MODULE_NOT_FOUND, TRUE);

  if (lpParentGroupFile->lpGroupFile && lpParentGroupFile->lpGroupFile->Users)
  {
	  ERROR_RETURN(ERROR_GROUP_NOT_EMPTY, TRUE);
  }

  // Delete Default=Group file if present
  if (lpParentGroupFile->tszDefaultName && lpParentGroupFile->tszDefaultName[0])
  {
	  if (tszFileName = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("User_Files"), lpParentGroupFile->tszDefaultName, NULL))
	  {
		  // don't care if we can't delete it for some reason, or it isn't even there...
		  DeleteFile(tszFileName);
		  Free(tszFileName);
	  }
	  Free(lpParentGroupFile->tszDefaultName);
  }

  //  Call delete routine for group
  return Group_Module_Delete(lpModule, tszGroupName, Gid);
}





BOOL GroupFile_Close(LPGROUPFILE *lpGroupFile, DWORD dwCloseFlags)
{
  LPPARENT_GROUPFILE  lpParentGroupFile;
  LPSTR        tszGroupName;
  BOOL        bFree;

  bFree  = FALSE;
  //  Sanity check
  if (! lpGroupFile ||
    ! lpGroupFile[0]) return TRUE;
  //  Get parent groupfile
  lpParentGroupFile  = (LPPARENT_GROUPFILE)lpGroupFile[0]->lpParent;

  AcquireSharedLock(&loGroupFileArray);
  //  Decrease share count
  if (! InterlockedDecrement(&lpParentGroupFile->lOpenCount) &&
    lpParentGroupFile->dwErrorFlags & WAIT_UNREGISTER)
  {
    bFree  = TRUE;
  }
  ReleaseSharedLock(&loGroupFileArray);

  //  Free shared memory
  if (! (dwCloseFlags & STATIC_SOURCE))
  {
    FreeShared(lpGroupFile[0]);
    lpGroupFile[0]  = NULL;
  }
  //  Free resources
  if (bFree)
  {
    tszGroupName  = Gid2Group(lpParentGroupFile->Gid);
    IdDataBase_Remove(tszGroupName, lpParentGroupFile->lpModule->tszModuleName, &dbGroupId);
	Group_Module_Close(lpParentGroupFile->lpModule, lpParentGroupFile->lpGroupFile);
    StopIoTimer(lpParentGroupFile->lpTimer, FALSE);
    CloseHandle(lpParentGroupFile->hPrimaryLock);
    FreeShared(lpParentGroupFile->lpGroupFile);
    Free(lpParentGroupFile);
  }  
  return FALSE;
}






BOOL GroupFile_OpenPrimitive(INT32 Gid, LPGROUPFILE *lpGroupFile, DWORD dwOpenFlags)
{
  LPPARENT_GROUPFILE  lpParentGroupFile;

  //  Acquire shared lock
  AcquireSharedLock(&loGroupFileArray);
  //  Get parent groupfile
  lpParentGroupFile  = GetParentGroupFile(Gid);
  //  Check search result
  if (lpParentGroupFile &&
    lpParentGroupFile->lpGroupFile)
  {
    //  Check errors
    if (! (lpParentGroupFile->dwErrorFlags & WAIT_UNREGISTER))
    {
      //  Increase share count
      InterlockedIncrement(&lpParentGroupFile->lOpenCount);
    }
    else lpParentGroupFile  = NULL;
  }
  //  Release lock
  ReleaseSharedLock(&loGroupFileArray);
  //  Sanity check
  if (! lpParentGroupFile) ERROR_RETURN(ERROR_GROUP_NOT_FOUND, TRUE);
  //  Acquire secondary lock
  while (InterlockedExchange(&lpParentGroupFile->lSecondaryLock, TRUE)) SwitchToThread();
  //  Select method
  if (dwOpenFlags & STATIC_SOURCE)
  {
    //  Copy to static buffer
    CopyMemory(lpGroupFile[0], lpParentGroupFile->lpGroupFile, sizeof(GROUPFILE));
  }
  else
  {
    //  Allocate shared memory
    lpGroupFile[0]  = (LPGROUPFILE)AllocateShared(lpParentGroupFile->lpGroupFile, NULL, 0);
  }
  //  Release secondary lock
  InterlockedExchange(&lpParentGroupFile->lSecondaryLock, FALSE);

  return FALSE;
}



PINT32 GetGroups(LPDWORD lpGroupIdCount)
{
  return IdDataBase_GetIdList(&dbGroupId, lpGroupIdCount);
}



BOOL GroupFile_OpenNext(LPGROUPFILE *lpGroupFile, LPINT lpOffset)
{
  INT32  Gid;

  for (;(Gid = IdDataBase_GetNextId(lpOffset, &dbGroupId)) != -1;)
  {
    //  Open groupfile
    if (! GroupFile_OpenPrimitive(Gid, lpGroupFile, 0)) return FALSE;
  }
  return TRUE;
}


BOOL GroupFile_Open(LPTSTR tszGroupName, LPGROUPFILE *lpGroupFile, DWORD dwOpenFlags)
{
  INT32  Gid;

  //  Get group id
  if ((Gid = Group2Gid(tszGroupName)) == -1) ERROR_RETURN(ERROR_GROUP_NOT_FOUND, TRUE);
  //  Call primitive open
  return GroupFile_OpenPrimitive(Gid, lpGroupFile, dwOpenFlags);
}

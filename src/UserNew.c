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

// Used in Admin_Verify which needs access to module names
IDDATABASE          dbUserId;
LPPARENT_USERFILE  *lpUserFileArray;
LOCKOBJECT          loUserFileArray;
DWORD               dwUserFileArrayItems, dwUserFileArraySize;


//  Local declarations
static INT UserFile_UidCompare(LPPARENT_USERFILE *a, LPPARENT_USERFILE *b);
static BOOL User_Unregister(LPUSER_MODULE lpModule, LPTSTR tszUserName);
static BOOL User_RegisterAs(LPUSER_MODULE lpModule, LPTSTR tszUserName, LPTSTR tszNewUserName);
static INT _cdecl User_Open(LPUSER_MODULE lpModule, LPTSTR tszUserName, INT32 Uid);
static INT32 User_Register(LPUSER_MODULE lpModule, LPTSTR tszUserName, LPUSERFILE lpUserFile);
static BOOL User_Update(LPUSERFILE lpUserFile);


//  Local variables
static LPUSER_MODULE  lpUserModuleList, lpStandardUserModule;
static USERFILE       fakeUserFile;


static INT32 User_Module_Create(LPUSER_MODULE lpModule, LPTSTR lpUserName, INT32 Uid)
{
	INT32 iResult;

	if (lpModule != lpStandardUserModule) SetBlockingThreadFlag();
	iResult = (lpModule->Create)(lpUserName, Uid);
	if (lpModule != lpStandardUserModule) SetNonBlockingThreadFlag();
	return iResult;
}

static BOOL User_Module_Delete(LPUSER_MODULE lpModule, LPTSTR lpUserName, INT32 Uid)
{
	BOOL bResult;

	if (lpModule != lpStandardUserModule) SetBlockingThreadFlag();
	bResult = (lpModule->Delete)(lpUserName, Uid);
	if (lpModule != lpStandardUserModule) SetNonBlockingThreadFlag();
	return bResult;
}

static BOOL User_Module_Rename(LPUSER_MODULE lpModule, LPTSTR lpOldName, INT32 Uid, LPTSTR lpNewName)
{
	BOOL bResult;

	if (lpModule != lpStandardUserModule) SetBlockingThreadFlag();
	bResult = (lpModule->Rename)(lpOldName, Uid, lpNewName);
	if (lpModule != lpStandardUserModule) SetNonBlockingThreadFlag();
	return bResult;
}

static BOOL User_Module_Lock(LPUSER_MODULE lpModule, LPUSERFILE lpUserFile)
{
	BOOL bResult;

	if (lpModule != lpStandardUserModule) SetBlockingThreadFlag();
	bResult = (lpModule->Lock)(lpUserFile);
	if (lpModule != lpStandardUserModule) SetNonBlockingThreadFlag();
	return bResult;
}

static BOOL User_Module_Write(LPUSER_MODULE lpModule, LPUSERFILE lpUserFile)
{
	BOOL bResult;

	if (lpModule != lpStandardUserModule) SetBlockingThreadFlag();
	bResult = (lpModule->Write)(lpUserFile);
	if (lpModule != lpStandardUserModule) SetNonBlockingThreadFlag();
	return bResult;
}

static INT User_Module_Open(LPUSER_MODULE lpModule, LPTSTR lpUserName, LPUSERFILE lpUserFile)
{
	INT  iResult;

	if (lpModule != lpStandardUserModule) SetBlockingThreadFlag();
	iResult = (lpModule->Open)(lpUserName, lpUserFile);
	if (lpModule != lpStandardUserModule) SetNonBlockingThreadFlag();
	return iResult;
}

static BOOL User_Module_Close(LPUSER_MODULE lpModule, LPUSERFILE lpUserFile)
{
	BOOL bResult;

	if (lpModule != lpStandardUserModule) SetBlockingThreadFlag();
	bResult = (lpModule->Close)(lpUserFile);
	if (lpModule != lpStandardUserModule) SetNonBlockingThreadFlag();
	return bResult;
}

static BOOL User_Module_Unlock(LPUSER_MODULE lpModule, LPUSERFILE lpUserFile)
{
	BOOL bResult;

	if (lpModule != lpStandardUserModule) SetBlockingThreadFlag();
	bResult = (lpModule->Unlock)(lpUserFile);
	if (lpModule != lpStandardUserModule) SetNonBlockingThreadFlag();
	return bResult;
}





static INT __cdecl UserFile_UidCompare(LPPARENT_USERFILE *a, LPPARENT_USERFILE *b)
{
  if (a[0]->Uid < b[0]->Uid) return -1;
  return (a[0]->Uid > b[0]->Uid);
}



LPPARENT_USERFILE GetParentUserFile(INT32 Uid)
{
  LPPARENT_USERFILE  lpParentUserFile, *lppResult;
  PARENT_USERFILE    ParentUserFile;

  ParentUserFile.Uid  = Uid;
  lpParentUserFile  = &ParentUserFile;
  //  Locate userfile
  lppResult  = bsearch(&lpParentUserFile, lpUserFileArray, dwUserFileArrayItems,
	                   sizeof(LPPARENT_USERFILE), (QUICKCOMPAREPROC) UserFile_UidCompare);

  return (lppResult ? *lppResult : NULL);
}


INT32
User2Uid(LPTSTR tszUserName)
{
  //  Seek by name
  return IdDataBase_SearchByName(tszUserName, &dbUserId);
}

LPSTR
Uid2User(INT32 Uid)
{
	LPPARENT_GROUPFILE lpParentGroupFile;

	if (Uid == -1)
	{
		return NULL;
	}
	if (Uid == -2)
	{
		return "Default.User";
	}
	if (Uid < -2)
	{
		lpParentGroupFile = GetParentGroupFileSafely(-3 - Uid);
		if (lpParentGroupFile)
		{
			return lpParentGroupFile->tszDefaultName;
		}
		else
		{
			return NULL;
		}
	}

	//  Seek by id
	return IdDataBase_SearchById(Uid, &dbUserId);
}




LPUSER_MODULE User_FindModule(LPTSTR tszModuleName)
{
  LPUSER_MODULE  lpModule;

  for (lpModule = lpUserModuleList;lpModule;lpModule = lpModule->lpNext)
  {
    if (! _tcscmp(lpModule->tszModuleName, tszModuleName)) break;
  }
  return lpModule;
}






BOOL User_Init(BOOL bFirstInitialization)
{
  LPUSER_MODULE  lpModule;
  LPTSTR      tszFileName;
  BOOL      (* InitProc)(LPUSER_MODULE);
  INT        iOffset;

  if (! bFirstInitialization) return TRUE;

  //  Reset local variables
  tszFileName  = NULL;
  iOffset    = 0;
  lpUserFileArray    = NULL;
  lpUserModuleList  = NULL;
  dwUserFileArrayItems  = 0;
  dwUserFileArraySize    = 0;
  ZeroMemory(&dbUserId, sizeof(IDDATABASE));
  ZeroMemory(&loUserFileArray, sizeof(LOCKOBJECT));
  //  Initialize modules
  do
  {
    //  Allocate memory for module
    lpModule  = (LPUSER_MODULE)Allocate(NULL, sizeof(USER_MODULE));
    if (! lpModule)
    {
      Free(tszFileName);
      return FALSE;
    }
    //  Set pointers to local functions
    ZeroMemory(lpModule, sizeof(USER_MODULE));
    lpModule->Register    = User_Register;
    lpModule->RegisterAs  = User_RegisterAs;
    lpModule->Unregister  = User_Unregister;
    lpModule->Update    = User_Update;
    lpModule->GetProc    = GetProc;
    lpModule->tszModuleFileName  = tszFileName;

    if (lpUserModuleList)
    {
      //  Load module
      if (lpModule->hModule = LoadLibrary(tszFileName))
      {
        InitProc  = (BOOL (__cdecl *)(LPUSER_MODULE))GetProcAddress(lpModule->hModule, _TEXT("UserModuleInit"));
      }
      else InitProc  = NULL;
    }
    else
    {
      //  Internal module
      InitProc  = User_StandardInit;
    }

    //  Initialize module
    if (InitProc && ! InitProc(lpModule))
    {
      lpModule->lpNext  = lpUserModuleList;
      lpUserModuleList  = lpModule;
    }
    else
    {
      if (lpModule->hModule) FreeLibrary(lpModule->hModule);
      Free(tszFileName);
      Free(lpModule);
    }

  } while ((tszFileName = Config_Get(&IniConfigFile, _TEXT("Modules"), _TEXT("UserModule"), NULL, &iOffset)));

  if (! lpUserModuleList) return FALSE;
  lpStandardUserModule = User_FindModule(_T("STANDARD"));
  if (!lpStandardUserModule) return FALSE;

  //  Get path to local user id table
  tszFileName  = Config_Get(&IniConfigFile, _TEXT("Locations"), _TEXT("User_Id_Table"), NULL, NULL);
  if (! tszFileName) return FALSE;
  //  Initialize lock object
  if (! InitializeLockObject(&loUserFileArray)) return FALSE;
  //  Read id table
  if (IdDataBase_Init(tszFileName, &dbUserId, User_FindModule, User_Open, 0)) return FALSE;

  ZeroMemory(&fakeUserFile, sizeof(USERFILE));
  fakeUserFile.Uid = -1;
  fakeUserFile.Gid = -1;
  fakeUserFile.AdminGroups[0] = -1;
  fakeUserFile.Groups[0] = -1;

  return TRUE;
}




VOID User_DeInit(VOID)
{
  LPUSER_MODULE  lpModule;
  LPPARENT_USERFILE lpParent;

  //  Remove cache
  while (dwUserFileArrayItems-- > 0)
  {
	  lpParent = lpUserFileArray[dwUserFileArrayItems];
	  //  Free resources
	  StopIoTimer(lpParent->lpTimer, FALSE);
	  lpParent->lpModule->Close(lpParent->lpUserFile);
	  CloseHandle(lpParent->hPrimaryLock);
	  FreeShared(lpParent->lpUserFile);
	  Free(lpParent);    
  }
  //  Close modules
  for (;lpModule = lpUserModuleList;)
  {
    lpUserModuleList  = lpUserModuleList->lpNext;
    if (lpModule->DeInitialize) lpModule->DeInitialize();
    if (lpModule->hModule) FreeLibrary(lpModule->hModule);
    Free(lpModule->tszModuleFileName);
    Free(lpModule);
  }
  //  Free memory
  Free(lpUserFileArray);
  //  Delete lock object
  DeleteLockObject(&loUserFileArray);
  IdDataBase_Free(&dbUserId);
}




static BOOL User_Unregister(LPUSER_MODULE lpModule, LPTSTR tszUserName)
{
  LPPARENT_USERFILE  lpParentUserFile;
  BOOL        bReturn, bFree;
  DWORD        dwError;
  INT32        Uid;

  bReturn        = TRUE;
  dwError        = NO_ERROR;
  lpParentUserFile  = NULL;
  bFree        = FALSE;

  AcquireSharedLock(&loUserFileArray);
  //  Resolve username to uid
  Uid  = User2Uid(tszUserName);

  if (Uid != -1)
  {
    //  Get parent userfile
    lpParentUserFile  = GetParentUserFile(Uid);
    //  Check if module is owner
    if (lpParentUserFile &&
      lpParentUserFile->lpModule != lpModule)
    {
      //  Reset pointer
      lpParentUserFile  = NULL;
      dwError        = ERROR_MODULE_CONFLICT;
    }
    else
    {
      //  Increase usage counter
      InterlockedIncrement(&lpParentUserFile->lOpenCount);
    }
  }
  else dwError  = ERROR_USER_NOT_FOUND;
  ReleaseSharedLock(&loUserFileArray);

  //  Sanity check
  if (! lpParentUserFile) ERROR_RETURN(dwError, TRUE);
  
  AcquireExclusiveLock(&loUserFileArray);
  //  Sanity check
  if (! (lpParentUserFile->dwErrorFlags & WAIT_UNREGISTER))
  {
    //  Delete item from array
    QuickDelete(lpUserFileArray, dwUserFileArrayItems--, lpParentUserFile, (QUICKCOMPAREPROC) UserFile_UidCompare, NULL);
    bReturn  = FALSE;
  }
  else dwError  = ERROR_USER_NOT_FOUND;
  //  Decrement reference counter
  if (InterlockedDecrement(&lpParentUserFile->lOpenCount))
  {
    //  Memory in use, cannot free yet
    lpParentUserFile->dwErrorFlags  |= WAIT_UNREGISTER;
  }
  else bFree  = TRUE;
  ReleaseExclusiveLock(&loUserFileArray);

  UserIpHostMaskRemove(Uid);

  //  Free resources
  if (bFree)
  {
    IdDataBase_Remove(tszUserName, lpParentUserFile->lpModule->tszModuleName, &dbUserId);
	User_Module_Close(lpParentUserFile->lpModule, lpParentUserFile->lpUserFile);
    StopIoTimer(lpParentUserFile->lpTimer, FALSE);
    CloseHandle(lpParentUserFile->hPrimaryLock);
    FreeShared(lpParentUserFile->lpUserFile);
    Free(lpParentUserFile);
  }
  //  Set error
  SetLastError(dwError);
  return bReturn;
}





static INT __cdecl User_Open(LPUSER_MODULE lpModule, LPTSTR tszUserName, INT32 Uid)
{
  LPPARENT_USERFILE  lpParentUserFile, *lpMemory;
  LPUSERFILE      lpUserFile;
  HANDLE        hEvent;
  INT          iResult, iReturn;

  iReturn    = DB_FATAL;
  //  Allocate memory for new item
  lpParentUserFile  = (LPPARENT_USERFILE)Allocate("ParentUserFile", sizeof(PARENT_USERFILE));
  //  Allocate shared memory for userfile
  lpUserFile  = (LPUSERFILE)AllocateShared(NULL, "UserFile", sizeof(USERFILE));
  //  Create event
  hEvent  = CreateEvent(NULL, FALSE, TRUE, NULL);

  if (! lpParentUserFile ||
    ! lpUserFile ||
    ! hEvent)
  {
    //  Free resources
    Free(lpParentUserFile);
    FreeShared(lpUserFile);
    if (hEvent) CloseHandle(hEvent);
    ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, DB_FATAL);
  }
  //  Reset contents of userfile
  ZeroMemory(lpUserFile, sizeof(USERFILE));
  ZeroMemory(lpParentUserFile, sizeof(PARENT_USERFILE));
  //  Move userfile inside parent
  lpParentUserFile->Uid      = Uid;
  lpParentUserFile->lpModule    = lpModule;
  lpParentUserFile->hPrimaryLock  = hEvent;
  lpUserFile->Uid          = Uid;
  lpUserFile->Groups[0]      = NOGROUP_ID;
  lpUserFile->Groups[1]      = -1;
  lpUserFile->AdminGroups[0]    = -1;
  //  Read userfile
  iResult  = User_Module_Open(lpModule, tszUserName, lpUserFile);

  switch (iResult)
  {
  case UM_SUCCESS:
	  //  Succesfully read
	  lpParentUserFile->lpUserFile      = lpUserFile;
	  lpParentUserFile->lpUserFile->Uid    = Uid;
	  lpParentUserFile->lpUserFile->lpParent  = (LPVOID)lpParentUserFile;
	  break;
  case UM_FATAL:
	  //  Free resources
	  FreeShared(lpUserFile);
	  Free(lpParentUserFile);
	  CloseHandle(hEvent);
	  return DB_FATAL;
  case UM_DELETED:
	  //  Free resources
	  FreeShared(lpUserFile);
	  Free(lpParentUserFile);
	  CloseHandle(hEvent);
	  ERROR_RETURN(ERROR_USER_NOT_FOUND, DB_DELETED);
  }

  AcquireExclusiveLock(&loUserFileArray);
  //  Check available memory
  if (dwUserFileArrayItems == dwUserFileArraySize)
  {
    lpMemory  = ReAllocate(lpUserFileArray, NULL, (dwUserFileArraySize + 512) * sizeof(LPPARENT_USERFILE));
    if (lpMemory)
    {
      lpUserFileArray     = lpMemory;
      dwUserFileArraySize += 512;
    }
  }
  //  Verify available memory
  if (dwUserFileArrayItems < dwUserFileArraySize)
  {
    //  Add parent userfile to user array.. always succeeds
    QuickInsert(lpUserFileArray, dwUserFileArrayItems++, lpParentUserFile, (QUICKCOMPAREPROC) UserFile_UidCompare);
    iReturn  = DB_SUCCESS;
  }
  ReleaseExclusiveLock(&loUserFileArray);

  //  Free resources
  if (iReturn == DB_FATAL)
  {
    FreeShared(lpUserFile);
    Free(lpParentUserFile);
    CloseHandle(hEvent);
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
  }

  return iReturn;
}


DWORD TranslateUserError(DWORD dwError)
{
  switch (dwError)
  {
  case ERROR_ID_EXISTS:
    return ERROR_USER_EXISTS;
  case ERROR_ID_NOT_FOUND:
    return ERROR_USER_NOT_FOUND;
  default:
    return dwError;
  }
}




static BOOL User_RegisterAs(LPUSER_MODULE lpModule, LPTSTR tszUserName, LPSTR tszNewUserName)
{
  BOOL  bReturn;

  if (! tszUserName || ! tszNewUserName) return TRUE;
  //  Call id rename
  bReturn  = IdDataBase_Rename(tszUserName, lpModule->tszModuleName, tszNewUserName, &dbUserId);
  //  Error handler
  if (bReturn) SetLastError(TranslateUserError(GetLastError()));
  return bReturn;
}




static INT32 User_Register(LPUSER_MODULE lpModule, LPTSTR tszUserName, LPUSERFILE lpUserFile)
{
  LPPARENT_USERFILE  lpParentUserFile;
  LPVOID        lpMemory;
  HANDLE        hEvent;
  DWORD        dwError;
  BOOL        bFreeParent, bFreeShared, bCloseEvent;
  INT32        Uid, iReturn;

  bFreeParent  = TRUE;
  bFreeShared  = TRUE;
  bCloseEvent  = TRUE;
  iReturn    = -1;

  if (! tszUserName) return -1;
  //  Allocate memory for new item
  lpParentUserFile  = (LPPARENT_USERFILE)Allocate("ParentUserFile", sizeof(PARENT_USERFILE));
  //  Allocate shared memory for userfile
  lpMemory  = (lpUserFile ? AllocateShared(NULL, "UserFile", sizeof(USERFILE)) : NULL);
  //  Create event
  hEvent  = CreateEvent(NULL, FALSE, TRUE, NULL);

  if (! lpParentUserFile ||
    (! lpMemory && lpUserFile) ||
    ! hEvent)
  {
    //  Free resources
    Free(lpParentUserFile);
    FreeShared(lpMemory);
    if (hEvent) CloseHandle(hEvent);
    ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, -1);
  }
  //  Reset contents of parent
  ZeroMemory(lpParentUserFile, sizeof(PARENT_USERFILE));
  //  Copy userfile contents
  if (lpUserFile) CopyMemory(lpMemory, lpUserFile, sizeof(USERFILE));
  //  Move userfile inside parent
  lpParentUserFile->lpUserFile   = (LPUSERFILE)lpMemory;
  lpParentUserFile->hPrimaryLock = hEvent;
  lpParentUserFile->lpModule     = lpModule;

  AcquireExclusiveLock(&loUserFileArray);
  //  Seek by username
  Uid  = User2Uid(tszUserName);
  //  Check result
  if (Uid == -1)
  {
    //  Check available memory
    if (dwUserFileArrayItems == dwUserFileArraySize)
    {
      lpMemory  = ReAllocate(lpUserFileArray, NULL,
        (dwUserFileArraySize + 512) * sizeof(LPPARENT_USERFILE));
      //  Verify allocation
      if (lpMemory)
      {
        lpUserFileArray    = (LPPARENT_USERFILE *)lpMemory;
        dwUserFileArraySize  += 512;
      }
      else dwError  = ERROR_NOT_ENOUGH_MEMORY;
    }
    //  Verify available memory
    if (dwUserFileArrayItems < dwUserFileArraySize)
    {
      //  Add user to local database
      Uid  = IdDataBase_Add(tszUserName,
        lpParentUserFile->lpModule->tszModuleName, &dbUserId);

      if (Uid != -1)
      {
        //  Store uid
        if (lpUserFile)
        {
          lpParentUserFile->lpUserFile->Uid    = Uid;
          lpParentUserFile->lpUserFile->lpParent  = (LPVOID)lpParentUserFile;
        }
        lpParentUserFile->Uid  = Uid;
        //  Add parent userfile to user array.. always succeeds
        QuickInsert(lpUserFileArray, dwUserFileArrayItems++, lpParentUserFile, (QUICKCOMPAREPROC) UserFile_UidCompare);
    
        bFreeParent  = FALSE;
        bFreeShared  = FALSE;
        bCloseEvent  = FALSE;
        iReturn    = Uid;
        dwError    = NO_ERROR;
      }
      else dwError  = TranslateUserError(GetLastError());
    }
  }
  else if (lpUserFile)
  {
    lpMemory  = (LPVOID)lpParentUserFile;
    //  Get parent userfile
    lpParentUserFile  = GetParentUserFile(Uid);
    //  Make sure that userfile is not loaded
    if (! lpParentUserFile->lpUserFile)
    {
      lpParentUserFile->lpUserFile      = ((LPPARENT_USERFILE)lpMemory)->lpUserFile;
      lpParentUserFile->lpUserFile->lpParent  = (LPVOID)lpParentUserFile;
      lpParentUserFile->lpUserFile->Uid    = Uid;
      lpParentUserFile->lpModule        = lpModule;
      bFreeShared  = FALSE;
      iReturn    = Uid;
      dwError    = NO_ERROR;
    }
    else dwError  = ERROR_MODULE_CONFLICT;
    //  Restore pointer
    lpParentUserFile  = (LPPARENT_USERFILE)lpMemory;
  }

  if (iReturn != -1)
  {
	  UserIpHostMaskUpdate(lpParentUserFile->lpUserFile);
  }
  ReleaseExclusiveLock(&loUserFileArray);

  //  Free unused resources memory
  if (bFreeShared) FreeShared(lpParentUserFile->lpUserFile);
  if (bFreeParent) Free(lpParentUserFile);
  if (bCloseEvent) CloseHandle(hEvent);
  //  Set error
  SetLastError(dwError);
  return iReturn;
}






static BOOL User_Update(LPUSERFILE lpUserFile)
{
  LPUSERFILE    lpNewUserFile;
  DWORD      dwError;
  BOOL      bReturn;

  bReturn  = TRUE;
  //  Open userfile
  if (! UserFile_OpenPrimitive(lpUserFile->Uid, &lpNewUserFile, 0))
  {
    //  Lock userfile
    if (! UserFile_Lock(&lpNewUserFile, NO_SYNC))
    {
      dwError  = NO_ERROR;
      bReturn  = FALSE;
      //  Get impotrant variables
      lpUserFile->lpInternal  = lpNewUserFile->lpInternal;
      lpUserFile->lpParent  = lpNewUserFile->lpParent;
      //  Update new userfile
      CopyMemory(lpNewUserFile, lpUserFile, sizeof(USERFILE));
      //  Unlock userfile
      UserFile_Unlock(&lpNewUserFile, NO_SYNC);
    }
    else dwError  = GetLastError();
    //  Close userfile
    UserFile_Close(&lpNewUserFile, 0);
    //  Set last error
    SetLastError(dwError);
  }
  return bReturn;
}







BOOL UserFile_Lock(LPUSERFILE *lpUserFile, DWORD dwFlags)
{
  LPPARENT_USERFILE  lpParentUserFile;
  LPUSERFILE      lpNewUserFile;

  //  Get parent userfile
  lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile[0]->lpParent;
  //  Allocate shared memory
  lpNewUserFile  = (LPUSERFILE)AllocateShared(NULL, "UserFile", sizeof(USERFILE));
  if (! lpNewUserFile) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, TRUE);
  //  Free shared memory
  if (! (dwFlags & STATIC_SOURCE)) FreeShared(lpUserFile[0]);
  //  Acquire primary lock
  WaitForSingleObject(lpParentUserFile->hPrimaryLock, INFINITE);
  //  Copy data
  if (! (dwFlags & NO_SYNC))
  {
    CopyMemory(lpNewUserFile, lpParentUserFile->lpUserFile, sizeof(USERFILE));
  }
  else lpNewUserFile->lpInternal  = lpParentUserFile->lpUserFile->lpInternal;

  //  Tell module to lock now
  if (lpParentUserFile->dwErrorFlags ||
    (! (dwFlags & NO_SYNC) && User_Module_Lock(lpParentUserFile->lpModule, lpNewUserFile)))
  {
    //  Restore userfile
    if (! (dwFlags & STATIC_SOURCE)) lpUserFile[0]  = (LPUSERFILE)AllocateShared(lpParentUserFile->lpUserFile, NULL, 0);
    //  Lock failed, handle error
    SetEvent(lpParentUserFile->hPrimaryLock);
    //  Free memorym
    FreeShared(lpNewUserFile);
	Putlog(LOG_ERROR, _T("User lock failed for id %d.\r\n"), lpParentUserFile->Uid);
    ERROR_RETURN(ERROR_USER_LOCK_FAILED, TRUE);
  }
  //  Restore parent pointer
  lpNewUserFile->lpParent  = lpParentUserFile;
  //  Store userfile location
  if (dwFlags & STATIC_SOURCE)
  {
    //  Copy memory
    CopyMemory(lpUserFile[0], lpNewUserFile, sizeof(USERFILE));
    //  Replace internal offset
    lpUserFile[0]->lpInternal  = (LPVOID)lpNewUserFile;
  }
  else lpUserFile[0]  = lpNewUserFile;

  return FALSE;
}






BOOL UserFile_WriteTimer(LPPARENT_USERFILE lpParentUserFile, LPTIMER lpTimer)
{
  //  Acquire primary lock
  WaitForSingleObject(lpParentUserFile->hPrimaryLock, INFINITE);
  //  Try to write again
  if (User_Module_Write(lpParentUserFile->lpModule, lpParentUserFile->lpUserFile))
  {
    SetEvent(lpParentUserFile->hPrimaryLock);
    //  Retry in 60seconds
    return (lpParentUserFile->dwErrorFlags & WAIT_UNREGISTER ? 0 : 60000);
  }
  //  Remove error
  lpParentUserFile->dwErrorFlags  = lpParentUserFile->dwErrorFlags & (0xFFFF - WRITE_ERROR);
  //  Release module lock
  User_Module_Unlock(lpParentUserFile->lpModule, lpParentUserFile->lpUserFile);
  //  Release primary lock
  SetEvent(lpParentUserFile->hPrimaryLock);
  return 0;
}






BOOL UserFile_Unlock(LPUSERFILE *lpUserFile, DWORD dwFlags)
{
  LPPARENT_USERFILE  lpParentUserFile;
  LPVOID        lpMemory, lpTemp;
  BOOL        bReturn;

  bReturn    = FALSE;

  if (dwFlags & STATIC_SOURCE)
  {
    lpTemp  = (LPVOID)lpUserFile[0]->lpInternal;
    //  Restore local pointers
    lpUserFile[0]->lpParent    = ((LPUSERFILE)lpTemp)->lpParent;
    lpUserFile[0]->lpInternal  = ((LPUSERFILE)lpTemp)->lpInternal;
    //  Copy memory
    CopyMemory(lpTemp, lpUserFile[0], sizeof(USERFILE));
    //  Swap pointer
    lpUserFile  = (LPUSERFILE *)&lpTemp;
  }
  //  Get parent userfile
  lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile[0]->lpParent;
  //  Compare data
  if (dwFlags & NO_SYNC ||
    memcmp(lpUserFile[0], lpParentUserFile->lpUserFile, sizeof(USERFILE)))
  {
    //  Set primary group
    lpUserFile[0]->Gid  = lpUserFile[0]->Groups[0];
    //  Get old shared memory offset
    lpMemory  = (LPVOID)lpParentUserFile->lpUserFile;
    //  Acquire secondary lock
    while (InterlockedExchange(&lpParentUserFile->lSecondaryLock, TRUE)) SwitchToThread();
    //  Store new shared memory offset
    lpParentUserFile->lpUserFile  =
      (! (dwFlags & STATIC_SOURCE) ? (LPUSERFILE)AllocateShared(lpUserFile[0], NULL, 0) : lpUserFile[0]);
    //  Release secondary lock
    InterlockedExchange(&lpParentUserFile->lSecondaryLock, FALSE);

	// update IP mask DB if IP data changed
	if (memcmp(lpUserFile[0]->Ip, lpParentUserFile->lpUserFile->Ip, sizeof(lpUserFile[0]->Ip)))
	{
		UserIpHostMaskUpdate(*lpUserFile);
	}

    //  Call write routine
    if (! lpParentUserFile->dwErrorFlags &&
      ! (dwFlags & NO_SYNC) &&
	  User_Module_Write(lpParentUserFile->lpModule, lpUserFile[0]))
    {
      //  Error while writing
      lpParentUserFile->dwErrorFlags  |= WRITE_ERROR;
      //  Release resources from previous timer
      StopIoTimer(lpParentUserFile->lpTimer, FALSE);
      //  Try to retry in 5seconds
      lpParentUserFile->lpTimer  = StartIoTimer(NULL, UserFile_WriteTimer, lpParentUserFile, 5000);
      bReturn  = TRUE;
    }
  }
  else
  {
    //  Release new shared memory 
    lpMemory  = lpUserFile[0];
    //  Get old share
    if (! (dwFlags & STATIC_SOURCE)) lpUserFile[0]  = (LPUSERFILE)AllocateShared(lpParentUserFile->lpUserFile, NULL, 0);
  }

  //  Release module lock
  if (! bReturn &&
    ! lpParentUserFile->dwErrorFlags &&
    ! (dwFlags & NO_SYNC) &&
	User_Module_Unlock(lpParentUserFile->lpModule, lpUserFile[0]))
  {
	  bReturn  = TRUE;
  }
  //  Release primary lock
  SetEvent(lpParentUserFile->hPrimaryLock);
  //  Free old shared memory
  FreeShared(lpMemory);

  return bReturn;
}




BOOL RenameUser(LPTSTR tszUserName, LPSTR tszNewUserName)
{
  LPUSER_MODULE    lpModule;
  LPPARENT_USERFILE  lpParentUserFile;
  INT32        Uid;

  lpModule  = NULL;
  //  Find id by username
  if ((Uid = User2Uid(tszUserName)) == -1) ERROR_RETURN(ERROR_USER_NOT_FOUND, TRUE);
  AcquireSharedLock(&loUserFileArray);
  //  Find module by id
  if ((lpParentUserFile = GetParentUserFile(Uid))) lpModule  = lpParentUserFile->lpModule;
  //  Release shared lock
  ReleaseSharedLock(&loUserFileArray);
  //  Sanity check
  if (! lpParentUserFile) ERROR_RETURN(ERROR_USER_NOT_FOUND, TRUE);
  if (! lpModule) ERROR_RETURN(ERROR_MODULE_NOT_FOUND, TRUE);
  //  Call rename routine for user
  return User_Module_Rename(lpModule, tszUserName, Uid, tszNewUserName);
}




INT32 CreateUser(LPTSTR tszUserName, INT32 Gid)
{
	if (User2Uid(tszUserName) != -1)
	{
		SetLastError(ERROR_USER_EXISTS);
		return -1;
	}

	//  Call create routine of last module
	return User_Module_Create(lpUserModuleList, tszUserName, Gid);
}


BOOL DeleteUser(LPTSTR tszUserName)
{
  LPUSER_MODULE    lpModule;
  LPPARENT_USERFILE  lpParentUserFile;
  INT          Uid;

  lpModule  = NULL;
  //  Find id by username
  if ((Uid = User2Uid(tszUserName)) == -1) ERROR_RETURN(ERROR_USER_NOT_FOUND, TRUE);

  AcquireSharedLock(&loUserFileArray);
  //  Find module by id
  if ((lpParentUserFile = GetParentUserFile(Uid))) lpModule  = lpParentUserFile->lpModule;
  ReleaseSharedLock(&loUserFileArray);

  //  Sanity check
  if (! lpParentUserFile) ERROR_RETURN(ERROR_USER_NOT_FOUND, TRUE);
  if (! lpModule) ERROR_RETURN(ERROR_MODULE_NOT_FOUND, TRUE);
  //  Call delete routine for user
  return User_Module_Delete(lpModule, tszUserName, Uid);
}





BOOL UserFile_Close(LPUSERFILE *lpUserFile, DWORD dwCloseFlags)
{
  LPPARENT_USERFILE  lpParentUserFile;
  LPTSTR        tszUserName;
  BOOL        bFree;

  bFree  = FALSE;

  if (! lpUserFile ||
    ! lpUserFile[0]) return TRUE;
  //  Get parent userfile
  lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile[0]->lpParent;
  AcquireSharedLock(&loUserFileArray);
  //  Decrease share count
  if (! InterlockedDecrement(&lpParentUserFile->lOpenCount) && (lpParentUserFile->dwErrorFlags & WAIT_UNREGISTER))
  {
	  bFree  = TRUE;
	  tszUserName  = Uid2User(lpParentUserFile->Uid);
  }
  ReleaseSharedLock(&loUserFileArray);

  //  Free shared memory
  if (! (dwCloseFlags & STATIC_SOURCE))
  {
    FreeShared(lpUserFile[0]);
    lpUserFile[0]  = NULL;
  }
  //  Free parent userfile
  if (bFree)
  {
	  if (tszUserName)
	  {
		  IdDataBase_Remove(tszUserName, lpParentUserFile->lpModule->tszModuleName, &dbUserId);
	  }
	  User_Module_Close(lpParentUserFile->lpModule, lpParentUserFile->lpUserFile);
	  StopIoTimer(lpParentUserFile->lpTimer, FALSE);
	  CloseHandle(lpParentUserFile->hPrimaryLock);
	  FreeShared(lpParentUserFile->lpUserFile);
	  Free(lpParentUserFile);
  }
  return FALSE;
}



BOOL UserFile_Sync(LPUSERFILE *lpUserFile)
{
  LPPARENT_USERFILE  lpParentUserFile;

  //  Sanity check
  if (! lpUserFile ||
    ! lpUserFile[0]) return FALSE;
  //  Get parent userfile
  lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile[0]->lpParent;

  if (lpParentUserFile->dwErrorFlags & WAIT_UNREGISTER) return TRUE;
  if (lpParentUserFile->lpUserFile == lpUserFile[0]) return FALSE;
  //  Release shared memory
  FreeShared(lpUserFile[0]);
  //  Acquire secondary lock
  while (InterlockedExchange(&lpParentUserFile->lSecondaryLock, TRUE)) SwitchToThread();
  //  Allocate shared memory
  lpUserFile[0]  = (LPUSERFILE)AllocateShared(lpParentUserFile->lpUserFile, NULL, 0);
  //  Release secondary lock
  InterlockedExchange(&lpParentUserFile->lSecondaryLock, FALSE);

  return FALSE;
}



BOOL UserFile_OpenPrimitive(INT32 Uid, LPUSERFILE *lpUserFile, DWORD dwOpenFlags)
{
  LPPARENT_USERFILE  lpParentUserFile;

  AcquireSharedLock(&loUserFileArray);
  //  Get parent userfile
  lpParentUserFile  = GetParentUserFile(Uid);
  //  Check search result
  if (lpParentUserFile &&
    lpParentUserFile->lpUserFile)
  {
    //  Check errors
    if (! (lpParentUserFile->dwErrorFlags & WAIT_UNREGISTER))
    {
      //  Increase share count
      InterlockedIncrement(&lpParentUserFile->lOpenCount);
    }
    else lpParentUserFile  = NULL;
  }
  ReleaseSharedLock(&loUserFileArray);
  //  Sanity check
  if (! lpParentUserFile) ERROR_RETURN(ERROR_USER_NOT_FOUND, TRUE);
  //  Acquire secondary lock
  while (InterlockedExchange(&lpParentUserFile->lSecondaryLock, TRUE)) SwitchToThread();
  //  Select method
  if (dwOpenFlags & STATIC_SOURCE)
  {
    //  Copy to static buffer
    CopyMemory(lpUserFile[0], lpParentUserFile->lpUserFile, sizeof(USERFILE));
  }
  else
  {
    //  Allocate shared memory
    lpUserFile[0]  = (LPUSERFILE)AllocateShared(lpParentUserFile->lpUserFile, NULL, 0);
  }
  //  Release secondary lock
  InterlockedExchange(&lpParentUserFile->lSecondaryLock, FALSE);

  return FALSE;
}



LONG UserFile_GetLoginCount(LPUSERFILE lpUserFile, DWORD dwType)
{
  LPPARENT_USERFILE  lpParentUserFile;

  //  Get parent userfile
  lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile->lpParent;
  //  Get login count
  return lpParentUserFile->lLoginCount[dwType];
}


VOID UserFile_IncrementLoginCount(LPUSERFILE lpUserFile, DWORD dwType)
{
  LPPARENT_USERFILE  lpParentUserFile;

  //  Get parent userfile
  lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile->lpParent;
  //  Get login count
  InterlockedIncrement(&lpParentUserFile->lLoginCount[dwType]);
}


VOID UserFile_DecrementLoginCount(LPUSERFILE lpUserFile, DWORD dwType)
{
  LPPARENT_USERFILE  lpParentUserFile;

  //  Get parent userfile
  lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile->lpParent;
  //  Decrease login count
  InterlockedDecrement(&lpParentUserFile->lLoginCount[dwType]);
}


BOOL UserFile_DownloadBegin(LPUSERFILE lpUserFile)
{
	LPPARENT_USERFILE  lpParentUserFile;

	//  Get parent userfile
	lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile->lpParent;

	if (lpUserFile->MaxDownloads < 0)
	{
		InterlockedIncrement(&lpParentUserFile->lDownloads);
		return FALSE;
	}

	if (lpParentUserFile->lDownloads >= lpUserFile->MaxDownloads)
	{
		SetLastError(IO_MAX_DOWNLOADS);
		return TRUE;
	}

	if (InterlockedIncrement(&lpParentUserFile->lDownloads) > lpUserFile->MaxDownloads)
	{
		// guess 2 threads started to download and the other got the last opening
		InterlockedDecrement(&lpParentUserFile->lDownloads);
		SetLastError(IO_MAX_DOWNLOADS);
		return TRUE;
	}

	return FALSE;
}


VOID UserFile_DownloadEnd(LPUSERFILE lpUserFile)
{
	LPPARENT_USERFILE  lpParentUserFile;

	//  Get parent userfile
	lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile->lpParent;

	InterlockedDecrement(&lpParentUserFile->lDownloads);
}


BOOL UserFile_UploadBegin(LPUSERFILE lpUserFile)
{
	LPPARENT_USERFILE  lpParentUserFile;

	//  Get parent userfile
	lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile->lpParent;

	if (lpUserFile->MaxUploads < 0)
	{
		InterlockedIncrement(&lpParentUserFile->lUploads);
		return FALSE;
	}

	if (lpParentUserFile->lUploads >= lpUserFile->MaxUploads)
	{
		SetLastError(IO_MAX_UPLOADS);
		return TRUE;
	}

	if (InterlockedIncrement(&lpParentUserFile->lUploads) > lpUserFile->MaxUploads)
	{
		// guess 2 threads started to upload and the other got the last opening
		InterlockedDecrement(&lpParentUserFile->lUploads);
		SetLastError(IO_MAX_UPLOADS);
		return TRUE;
	}

	return FALSE;
}


VOID UserFile_UploadEnd(LPUSERFILE lpUserFile)
{
	LPPARENT_USERFILE  lpParentUserFile;

	//  Get parent userfile
	lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile->lpParent;

	InterlockedDecrement(&lpParentUserFile->lUploads);
}


BOOL
UserFile_Open(LPTSTR tszUserName,
        LPUSERFILE *lpUserFile,
        DWORD dwOpenFlags)
{
  INT32 uid;

  //  Get user id
  if(-1==(uid=User2Uid(tszUserName)))
    ERROR_RETURN(ERROR_USER_NOT_FOUND, TRUE);

  //  Call primitive open
  return UserFile_OpenPrimitive(uid, lpUserFile, dwOpenFlags);
}



// FALSE on include, TRUE on exclude
BOOL FindIsMatch(LPUSERSEARCH lpSearch, LPUSERFILE lpUserFile, BOOL bTestUid)
{
	LPUSERSEARCH_ID    lpId;
	LPUSERSEARCH_NAME  lpName;
	LPUSERSEARCH_FLAG  lpFlag;
	LPUSERSEARCH_RATIO lpRatio;
	LPTSTR        tszUserName;
	PINT32        lpGroup, lpAdminGroup;
	INT32         i;
	DWORD         n;

	//  Get username
	if ( ! (tszUserName = Uid2User(lpUserFile->Uid)) )
	{
		return TRUE;
	}
	lpGroup      = lpUserFile->Groups;
	lpAdminGroup = lpUserFile->AdminGroups;

	// if "matching" and not adding users make sure we test the UID to see if it's in the current set
	if (bTestUid && lpSearch->bUseMatchList && lpSearch->lpMatchList)
	{
		for ( n = 0 ; n < lpSearch->dwUidList ; n++ )
		{
			//  Compare uid
			if ( lpUserFile->Uid == lpSearch->lpMatchList[n] ) break;
		}
		if (n == lpSearch->dwUidList)
		{
			if (!lpSearch->bAdding)
			{
				// we didn't find ourselves in the list and aren't adding users to it's rejected
				return TRUE;
			}
		}
		else if (lpSearch->bAdding)
		{
			// we're adding to the list, but since we already there just return
			return FALSE;
		}
	}

	//  Check id exclusion list
	for ( lpId = lpSearch->lpExcludedUserId ; lpId ; lpId = lpId->lpNext )
	{
		//  Compare id
		if (lpId->Id != lpUserFile->Uid) continue;
		return TRUE;
	}
	//  Check flag exclusion list
	for ( lpFlag = lpSearch->lpExcludedFlag ; lpFlag ; lpFlag = lpFlag->lpNext )
	{
		if (! HasFlag(lpUserFile, lpFlag->tszFlag)) return TRUE;
	}
	//  Check name exclusion list
	for ( lpName = lpSearch->lpExcludedUserName ; lpName ; lpName = lpName->lpNext )
	{
		if (! iCompare(lpName->tszName, tszUserName)) return TRUE;
	}
	//  Check group exclusion list
	for ( lpId = lpSearch->lpExcludedGroupId ; lpId ; lpId = lpId->lpNext )
	{
		//  Go through all groups from user
		for  (i = 0 ; (i < MAX_GROUPS) && (lpGroup[i] != -1) ; i++)
		{
			if (lpGroup[i] == lpId->Id) return TRUE;
			// if we only care about primary group we can stop now...
			if (lpId->bPrimary) break;
		}
	}
	//  Check admingroup exclusion list
	for ( lpId = lpSearch->lpExcludedAdminGroupId ; lpId ; lpId = lpId->lpNext )
	{
		//  Go through all groups from user
		for ( i = 0 ; (i < MAX_GROUPS) && (lpAdminGroup[i] != -1) ; i++ )
		{
			if (lpAdminGroup[i] == lpId->Id) return TRUE;
			// if we only care about primary group we can stop now...
			if (lpId->bPrimary) break;
		}
	}
	//  Check ratio exclusion list
	for ( lpRatio = lpSearch->lpExcludedRatio ; lpRatio ; lpRatio = lpRatio->lpNext )
	{
		i = lpUserFile->Ratio[lpRatio->Section] - lpRatio->Ratio;

		if ( (lpRatio->CompareType <  0) && (i <  0) ) return TRUE;
		if ( (lpRatio->CompareType >  0) && (i >  0) ) return TRUE;
		if ( (lpRatio->CompareType == 0) && (i == 0) ) return TRUE;
	}

	if (bTestUid && lpSearch->bDefaultMatch)
	{
		// handle the special case of '--' since plan to always return a match...
		return FALSE;
	}

	//  Check id inclusion list
	for ( lpId = lpSearch->lpIncludedUserId ; lpId ; lpId = lpId->lpNext )
	{
		if (lpId->Id == lpUserFile->Uid) return FALSE;
	}
	//  Check flag inclusion list
	for ( lpFlag = lpSearch->lpIncludedFlag ; lpFlag ; lpFlag = lpFlag->lpNext )
	{
		if (! HasFlag(lpUserFile, lpFlag->tszFlag)) return FALSE;
	}
	//  Check name inclusion list
	for ( lpName = lpSearch->lpIncludedUserName ; lpName ; lpName = lpName->lpNext )
	{
		//  Compare name
		if (! iCompare(lpName->tszName, tszUserName)) return FALSE;
	}
	//  Check group inclusion list
	for ( lpId = lpSearch->lpIncludedGroupId ; lpId ; lpId = lpId->lpNext )
	{
		//  Go through all groups from user
		for ( i = 0 ; (i < MAX_GROUPS) && (lpGroup[i] != -1) ; i++ )
		{
			if (lpGroup[i] == lpId->Id) return FALSE;
			// if we only care about primary group we can stop now...
			if (lpId->bPrimary) break;
		}
	}
	for ( lpId = lpSearch->lpIncludedAdminGroupId ; lpId ; lpId = lpId->lpNext )
	{
		//  Go through all groups from user
		for (i = 0 ; (i < MAX_GROUPS) && (lpAdminGroup[i] != -1) ; i++ )
		{
			if (lpAdminGroup[i] == lpId->Id) return FALSE;
			// if we only care about primary group we can stop now...
			if (lpId->bPrimary) break;
		}
	}
	//  Check ratio inclusion list
	for ( lpRatio = lpSearch->lpIncludedRatio ; lpRatio ; lpRatio = lpRatio->lpNext )
	{
		i = lpUserFile->Ratio[lpRatio->Section] - lpRatio->Ratio;

		if ( (lpRatio->CompareType <  0) && (i <  0) ) return FALSE;
		if ( (lpRatio->CompareType >  0) && (i >  0) ) return FALSE;
		if ( (lpRatio->CompareType == 0) && (i == 0) ) return FALSE;
	}

	return TRUE;
}



VOID FindFinished(LPUSERSEARCH lpSearch)
{
	LPUSERSEARCH_ID    lpId;
	LPUSERSEARCH_NAME  lpName;
	LPUSERSEARCH_FLAG  lpFlag;
	LPUSERSEARCH_RATIO lpRatio;

	//  Free database
	for (lpId = lpSearch->lpIncludedUserId       ; lpSearch->lpIncludedUserId = lpId       ; ) { lpId = lpId->lpNext; Free(lpSearch->lpIncludedUserId);       }
	for (lpId = lpSearch->lpExcludedUserId       ; lpSearch->lpExcludedUserId = lpId       ; ) { lpId = lpId->lpNext; Free(lpSearch->lpExcludedUserId);       }
	for (lpId = lpSearch->lpIncludedGroupId      ; lpSearch->lpIncludedGroupId = lpId      ; ) { lpId = lpId->lpNext; Free(lpSearch->lpIncludedGroupId);      }
	for (lpId = lpSearch->lpExcludedGroupId      ; lpSearch->lpExcludedGroupId = lpId      ; ) { lpId = lpId->lpNext; Free(lpSearch->lpExcludedGroupId);      }
	for (lpId = lpSearch->lpIncludedAdminGroupId ; lpSearch->lpIncludedAdminGroupId = lpId ; ) { lpId = lpId->lpNext; Free(lpSearch->lpIncludedAdminGroupId); }
	for (lpId = lpSearch->lpExcludedAdminGroupId ; lpSearch->lpExcludedAdminGroupId = lpId ; ) { lpId = lpId->lpNext; Free(lpSearch->lpExcludedAdminGroupId); }

	for (lpFlag = lpSearch->lpIncludedFlag ; lpSearch->lpIncludedFlag = lpFlag ; ) { lpFlag = lpFlag->lpNext; Free(lpSearch->lpIncludedFlag); }
	for (lpFlag = lpSearch->lpExcludedFlag ; lpSearch->lpExcludedFlag = lpFlag ; ) { lpFlag = lpFlag->lpNext; Free(lpSearch->lpExcludedFlag); }

	for (lpName = lpSearch->lpIncludedUserName ; lpSearch->lpIncludedUserName = lpName ; ) { lpName = lpName->lpNext; Free(lpSearch->lpIncludedUserName); }
	for (lpName = lpSearch->lpExcludedUserName ; lpSearch->lpExcludedUserName = lpName ; ) { lpName = lpName->lpNext; Free(lpSearch->lpExcludedUserName); }

	for (lpRatio = lpSearch->lpIncludedRatio ; lpSearch->lpIncludedRatio = lpRatio ; ) { lpRatio = lpRatio->lpNext; Free(lpSearch->lpIncludedRatio); }
	for (lpRatio = lpSearch->lpExcludedRatio ; lpSearch->lpExcludedRatio = lpRatio ; ) { lpRatio = lpRatio->lpNext; Free(lpSearch->lpExcludedRatio); }

	if ( ! lpSearch->bMatching && ! lpSearch->bUseMatchList )
	{
		if (lpSearch->lpUidList) Free(lpSearch->lpUidList);
	}
	Free(lpSearch);
}



BOOL FindNextUser(LPUSERSEARCH lpSearch, LPUSERFILE *lppUserFile)
{
	INT32  Uid;

	if (! lpSearch->bAdding )
	{
		// default is to prune users...
		for ( ; lpSearch->dwOffset < lpSearch->dwUidList ; lpSearch->dwOffset++)
		{
			if (lpSearch->bUseMatchList)
			{
				Uid  = lpSearch->lpMatchList[lpSearch->dwOffset];
			}
			else
			{
				Uid  = lpSearch->lpUidList[lpSearch->dwOffset];
			}
			if (Uid < 0)
			{
				// this will skip non-matching users when bUseMatchList is true
				continue;
			}

			//  Open userfile
			if (UserFile_OpenPrimitive(Uid, lppUserFile, 0))
			{
				// guess user just got deleted or never loaded from external user module
				if (lpSearch->bMatching)
				{
					lpSearch->lpMatchList[lpSearch->dwOffset] = -2;
				}
				continue;
			}
			if (lpSearch->bDefaultMatch || !FindIsMatch(lpSearch, *lppUserFile, FALSE))
			{
				// we found a match and userfile loaded, just return...
				lpSearch->dwOffset++;
				return FALSE;
			}
			// close non-match
			UserFile_Close(lppUserFile, 0);
			// if matching, then mark this user as not matching :)
			if (lpSearch->bMatching)
			{
				lpSearch->lpMatchList[lpSearch->dwOffset] = -1;
			}
		}

		// no more matches, cleanup search params...
		FindFinished(lpSearch);
		return TRUE;
	}

	// try to add users to those already matching the original list of users...
	for ( ; lpSearch->dwOffset < lpSearch->dwUidList ; lpSearch->dwOffset++ )
	{
		Uid  = lpSearch->lpUidList[lpSearch->dwOffset];

		//  Open userfile
		if (Uid == -2)
		{
			// this will skip unavailable users
			continue;
		}

		if (UserFile_OpenPrimitive(Uid, lppUserFile, 0))
		{
			// guess user just got deleted or never loaded from external user module
			if (lpSearch->bMatching)
			{
				lpSearch->lpMatchList[lpSearch->dwOffset] = -2;
			}
			continue;
		}

		if (lpSearch->bUseMatchList && (lpSearch->lpMatchList[lpSearch->dwOffset] >= 0))
		{
			// this user already matches so just return
			lpSearch->dwOffset++;
			return FALSE;
		}
		if (!FindIsMatch(lpSearch, *lppUserFile, FALSE))
		{
			// we found a new match and userfile loaded, just return...
			if (lpSearch->bMatching)
			{
				lpSearch->lpMatchList[lpSearch->dwOffset] = Uid;
			}
			lpSearch->dwOffset++;
			return FALSE;
		}
		// close non-match
		UserFile_Close(lppUserFile, 0);
	}

	// no more matches, cleanup search params...
	FindFinished(lpSearch);
	return TRUE;
}



LPUSERSEARCH FindParse(LPTSTR tszWildCard, LPUSERFILE lpCaller, struct _FTP_USER *lpUser, BOOL bMatching)
{
	LPUSERSEARCH_ID    lpId;
	LPUSERSEARCH_NAME  lpName;
	LPUSERSEARCH_FLAG  lpFlag;
	LPUSERSEARCH_RATIO lpRatio;
	LPUSERSEARCH    lpSearch;
	TCHAR        *tpQuote, *tpSpace, *tpTemp, *tpTemp2;
	DWORD        dwWildCard, dwName;
	INT32        Id, Gid;
	INT          iSection, iRatio, iType, i;
	BOOL         bExclude, bPrimary, bLoop, bLimited;

	if (! tszWildCard) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, NULL);
	//  Allocate memory for search item
	lpSearch  = (LPUSERSEARCH)Allocate("User:Search", sizeof(USERSEARCH));
	if (! lpSearch) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, NULL);
	//  Reset memory
	ZeroMemory(lpSearch, sizeof(USERSEARCH));
	if ( ! lpCaller || ! HasFlag(lpCaller, _T("1M")) )
	{
		bLimited = FALSE;
	}
	else
	{
		bLimited = TRUE;
	}

	dwWildCard  = _tcslen(tszWildCard);
	//  Remove quotes
	if (tszWildCard[0] == '"' &&
		(tpQuote = (PCHAR)_tmemchr(&tszWildCard[1], _TEXT('"'), dwWildCard)))
	{
		tpQuote[0]   = '\0';
		dwWildCard  -= 2;
		tszWildCard++;
	}

	for ( bLoop = TRUE ; bLoop ; tszWildCard = &tpSpace[1] )
	{
		//  Remove leading spaces
		for ( ;_istspace(tszWildCard[0]) ; tszWildCard++ ) dwWildCard--;
		//  Find next space
		tpSpace  = (TCHAR *)_tmemchr(tszWildCard, _TEXT(' '), dwWildCard);
		//  Check space existance
		if (! tpSpace)
		{
			tpSpace  = &tszWildCard[dwWildCard];
			bLoop  = FALSE;
		}
		else
		{
			tpSpace[0]  = '\0';
		}
		//  Calculate new length
		dwWildCard  -= (&tpSpace[1] - tszWildCard);

		//  Exclude term
		if (tszWildCard[0] == '!')
		{
			bExclude  = TRUE;
			tszWildCard++;
		}
		else
		{
			bExclude  = FALSE;
		}

		switch (tszWildCard[0])
		{
		case _T('='):
			//  Group
			bPrimary = FALSE;

			tszWildCard++;
			if (*tszWildCard == _T('.'))
			{
				bPrimary = TRUE;
				tszWildCard++;
			}

			Id  = Group2Gid(tszWildCard);

			if (Id != -1)
			{
				if (bLimited)
				{
					// see if the user is a member of the group they want to limit results to
					for (i = 0 ; i < MAX_GROUPS && ((Gid = lpCaller->Groups[i]) != -1) ; i++)
					{
						if (Gid == Id) break;
					}
					if ((i >= MAX_GROUPS) || (Gid == -1))
					{
						// no match found
						goto cleanup;
					}
				}

				//  Allocate memory for search item
				lpId  = (LPUSERSEARCH_ID)Allocate("User:Search:Group", sizeof(USERSEARCH_ID));
				if (! lpId) goto DONE;
				//  Store id
				lpId->Id  = Id;
				lpId->bPrimary = bPrimary;

				if (bExclude)
				{
					lpId->lpNext  = lpSearch->lpExcludedGroupId;
					lpSearch->lpExcludedGroupId  = lpId;
				}
				else
				{
					lpId->lpNext  = lpSearch->lpIncludedGroupId;
					lpSearch->lpIncludedGroupId  = lpId;
				}
			}
			break;

		case _T('+'):
			//  Group
			if (bLimited) goto cleanup;
			bPrimary = FALSE;

			tszWildCard++;
			if (*tszWildCard == _T('.'))
			{
				bPrimary = TRUE;
				tszWildCard++;
			}

			Id  = Group2Gid(tszWildCard);

			if (Id != -1)
			{
				//  Allocate memory for search item
				lpId  = (LPUSERSEARCH_ID)Allocate("User:Search:Group", sizeof(USERSEARCH_ID));
				if (! lpId) goto DONE;
				//  Store id
				lpId->Id  = Id;
				lpId->bPrimary = bPrimary;

				if (bExclude)
				{
					lpId->lpNext  = lpSearch->lpExcludedAdminGroupId;
					lpSearch->lpExcludedAdminGroupId  = lpId;
				}
				else
				{
					lpId->lpNext  = lpSearch->lpIncludedAdminGroupId;
					lpSearch->lpIncludedAdminGroupId  = lpId;
				}
			}
			break;

		case _T('.'):
			if (bLimited) goto cleanup;
			if (!tszWildCard[1])
			{
				break;
			}

			//  Allocate memory for search item
			dwName  = tpSpace - tszWildCard;
			lpFlag  = (LPUSERSEARCH_FLAG)Allocate("User:Search:Flag", sizeof(USERSEARCH_FLAG) + dwName * sizeof(TCHAR));
			if (! lpFlag) goto DONE;
			//  Store flag
			CopyMemory(lpFlag->tszFlag, tszWildCard, (dwName + 1) * sizeof(TCHAR));

			if (bExclude)
			{
				lpFlag->lpNext  = lpSearch->lpExcludedFlag;
				lpSearch->lpExcludedFlag  = lpFlag;
			}
			else
			{
				lpFlag->lpNext  = lpSearch->lpIncludedFlag;
				lpSearch->lpIncludedFlag  = lpFlag;
			}

			break;

		case _T(':'):
			if (bLimited) goto cleanup;

			tpTemp = &tszWildCard[1];
			iSection = _tcstol(tpTemp, &tpTemp2, 10);
			if (tpTemp == tpTemp2)
			{
				iSection = 0;
			}

			if (!*tpTemp2)
			{
				break;
			}
			else if (*tpTemp2 == _T('>'))
			{
				iType = 1;
			}
			else if (*tpTemp2 == _T('<'))
			{
				iType = -1;
			}
			else if (*tpTemp2 == _T('='))
			{
				iType = 0;
			}
			else
			{
				break;
			}

			iRatio = _tcstol(++tpTemp2, &tpTemp, 10);
			if (tpTemp == tpTemp2)
			{
				// no ratio
				break;
			}

			//  Allocate memory for search item
			lpRatio  = (LPUSERSEARCH_RATIO)Allocate("User:Search:Ratio", sizeof(USERSEARCH_RATIO));
			if (! lpRatio) goto DONE;
			//  Store ratio
			lpRatio->Ratio       = iRatio;
			lpRatio->Section     = iSection;
			lpRatio->CompareType = iType;

			if (bExclude)
			{
				lpRatio->lpNext  = lpSearch->lpExcludedRatio;
				lpSearch->lpExcludedRatio  = lpRatio;
			}
			else
			{
				lpRatio->lpNext  = lpSearch->lpIncludedRatio;
				lpSearch->lpIncludedRatio  = lpRatio;
			}

			break;

		default:
			//  User
			dwName  = tpSpace - tszWildCard;
			//  Select type
			if (_tmemchr(tszWildCard, _TEXT('*'), dwName) || _tmemchr(tszWildCard, _TEXT('?'), dwName) || _tmemchr(tszWildCard, _TEXT('['), dwName))
			{
				//  Wildcard
				lpName  = (LPUSERSEARCH_NAME)Allocate("User:Search:Name", sizeof(USERSEARCH_NAME) + (dwName + 1) * sizeof(TCHAR));
				if (! lpName) goto DONE;

				//  Copy name
				lpName->tszName  = (LPSTR)&lpName[1];
				CopyMemory(lpName->tszName, tszWildCard, (dwName + 1) * sizeof(TCHAR));

				if (bExclude)
				{
					lpName->lpNext  = lpSearch->lpExcludedUserName;
					lpSearch->lpExcludedUserName  = lpName;
				}
				else
				{
					lpName->lpNext  = lpSearch->lpIncludedUserName;
					lpSearch->lpIncludedUserName  = lpName;
				}
			}
			else if (!_tcsicmp(tszWildCard, _T("--")))
			{
				lpSearch->bUseMatchList = TRUE;
			}
			else if (!_tcsicmp(tszWildCard, _T("-+")))
			{
				lpSearch->bUseMatchList = TRUE;
				lpSearch->bAdding       = TRUE;
			}
			else
			{
				//  Username
				Id  = User2Uid(tszWildCard);

				if (Id != -1)
				{
					//  Allocate memory for search item
					lpId  = (LPUSERSEARCH_ID)Allocate("User:Search:Id", sizeof(USERSEARCH_ID));
					if (! lpId) goto DONE;
					//  Store id
					lpId->Id  = Id;

					if (bExclude)
					{
						lpId->lpNext  = lpSearch->lpExcludedUserId;
						lpSearch->lpExcludedUserId  = lpId;
					}
					else
					{
						lpId->lpNext  = lpSearch->lpIncludedUserId;
						lpSearch->lpIncludedUserId  = lpId;
					}
				}
			}
			break;
		}
	}
DONE:

	// treat an expression of just '--' as a special case because it would otherwise match nothing
	if (lpSearch->bUseMatchList && !lpSearch->bAdding && 
		!lpSearch->lpIncludedUserName     && !lpSearch->lpExcludedUserName     &&
		!lpSearch->lpIncludedUserId       && !lpSearch->lpExcludedUserId       &&
		!lpSearch->lpIncludedFlag         && !lpSearch->lpExcludedFlag         &&
		!lpSearch->lpIncludedRatio        && !lpSearch->lpExcludedRatio        &&
		!lpSearch->lpIncludedGroupId      && !lpSearch->lpExcludedGroupId      &&
		!lpSearch->lpIncludedAdminGroupId && !lpSearch->lpExcludedAdminGroupId)
	{
		lpSearch->bDefaultMatch = TRUE;
	}

	// silently disable bMatching if no FtpUser pointer available
	lpSearch->bMatching = (lpUser && bMatching ? TRUE : FALSE);

	if (lpSearch->bUseMatchList && ( !lpUser || ! lpUser->FtpVariables.lpUidList || ! lpUser->FtpVariables.lpUidMatches ) )
	{
		// we referenced the match list, return an error
		FindFinished(lpSearch);
		SetLastError(ERROR_MATCH_LIST);
		return NULL;
	}

	if ( lpSearch->bUseMatchList )
	{
		// we're using the old results, so we're done.
		lpSearch->dwUidList   = lpUser->FtpVariables.dwUidList;
		lpSearch->lpUidList   = lpUser->FtpVariables.lpUidList;
		lpSearch->lpMatchList = lpUser->FtpVariables.lpUidMatches;

		return lpSearch;
	}

	if ( lpSearch->bMatching )
	{
		// clear out old matching info
		if (lpUser->FtpVariables.lpUidList)
		{
			Free(lpUser->FtpVariables.lpUidList);
			lpUser->FtpVariables.lpUidList = NULL;
		}
		if (lpUser->FtpVariables.lpUidMatches)
		{
			Free(lpUser->FtpVariables.lpUidMatches);
			lpUser->FtpVariables.lpUidMatches = NULL;
		}
		lpUser->FtpVariables.dwUidList = 0;
	}

	lpSearch->lpUidList  = IdDataBase_GetIdList(&dbUserId, &lpSearch->dwUidList);
	if (lpSearch->bMatching)
	{
		lpSearch->lpMatchList             = Allocate("MatchList", sizeof(*lpSearch->lpMatchList) * lpSearch->dwUidList);
	}

	if ( ! lpSearch->lpUidList || ( lpSearch->bMatching && ! lpSearch->lpMatchList ) )
	{
		FindFinished(lpSearch);
		ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, NULL);
	}

	if (lpSearch->bMatching)
	{
		lpUser->FtpVariables.dwUidList    = lpSearch->dwUidList;
		lpUser->FtpVariables.lpUidList    = lpSearch->lpUidList;
		lpUser->FtpVariables.lpUidMatches = lpSearch->lpMatchList;
		CopyMemory(lpSearch->lpMatchList, lpSearch->lpUidList, sizeof(*lpSearch->lpUidList) * lpSearch->dwUidList);
	}

	return lpSearch;

cleanup:
	FindFinished(lpSearch);
	SetLastError(IO_NO_ACCESS);
	return NULL;
}



LPUSERSEARCH FindFirstUser(LPTSTR tszWildCard, LPUSERFILE *lppUserFile, LPUSERFILE lpCaller, struct _FTP_USER *lpUser, LPDWORD lpdwUsers)
{
	LPUSERSEARCH lpSearch;


	lpSearch = FindParse(tszWildCard, lpCaller, lpUser, FALSE);
	if (!lpSearch)
	{
		if (lpdwUsers) *lpdwUsers = 0;
		return NULL;
	}

	if (lpdwUsers) *lpdwUsers = lpSearch->dwMaxIds;

	if (FindNextUser(lpSearch , lppUserFile))
	{
		return NULL;
	}
	return lpSearch;
}




PINT32 GetUsers(LPDWORD lpUserIdCount)
{
  return IdDataBase_GetIdList(&dbUserId, lpUserIdCount);
}





VOID HashString(LPTSTR tszString, PUCHAR pHash)
{
  //  Hash String
  sha1(pHash, (const PUCHAR)tszString, _tcslen(tszString));
}



/*

  User_IsGadmin() - Is user a gadmin

  */
BOOL User_IsAdmin(LPUSERFILE lpAdmin, LPUSERFILE lpUser, PINT32 pGid)
{
  PINT32  lpAdminGroups, lpUserGroups;
  INT    i, j;

  //  Get pointers for quicker access
  lpAdminGroups  = lpAdmin->AdminGroups;
  lpUserGroups  = lpUser->Groups;
  //  Global admin check
  if (!HasFlag(lpAdmin, _T("M1")))
  {
	  // user can modify any group
	  return FALSE;
  }

  if (!HasFlag(lpUser, _T("1M")))
  {
	  // target is 1M flagged user and I'm just a group admin or regular user
	  SetLastError(ERROR_SITEOP);
	  return TRUE;
  }

  if (HasFlag(lpAdmin, _T("G")))
  {
	  // user doesn't have the G flag and without 1M flags can't admin the target
	  SetLastError(IO_NOT_GADMIN);
	  return TRUE;
  }

  //  Loop through admin's admingroups
  for (i = 0;i < MAX_GROUPS && lpAdminGroups[i] != -1;i++)
  {
    //  Loop through target's groups
    for (j = 0;j < MAX_GROUPS && lpUserGroups[j] != -1;j++)
    {
      if (lpAdminGroups[i] == lpUserGroups[j])
      {
        //  Store matching group id
        if (pGid) pGid[0]  = lpAdminGroups[i];
        return FALSE;
      }
    }
  }
  SetLastError(IO_NOT_GADMIN);
  return TRUE;
}


LPUSERFILE User_GetFake()
{
	return &fakeUserFile;
}

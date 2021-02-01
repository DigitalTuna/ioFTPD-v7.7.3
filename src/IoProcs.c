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


static LPIOPROC	*lpIoProcArray;
static DWORD		dwIoProcArray, dwIoProcArraySize;

INT __cdecl ProcCompare(LPIOPROC *lpProc1, LPIOPROC *lpProc2)
{
	return stricmp(lpProc1[0]->szName, lpProc2[0]->szName);
}

BOOL RegisterProc(LPSTR szName, LPVOID lpProc)
{
	LPIOPROC	lpIoProc;
	LPVOID		lpMemory;

	if (dwIoProcArray == dwIoProcArraySize)
	{
		//	Allocate more memory
		lpMemory	= ReAllocate(lpIoProcArray, "Proc:Array", sizeof(LPIOPROC) * (dwIoProcArray + 128));
		//	Verify allocation
		if (! lpMemory) return TRUE;
		//	Update proc array
		dwIoProcArraySize	+= 128;
		lpIoProcArray		= (LPIOPROC *)lpMemory;
	}
	//	Allocate memory
	if (! (lpIoProc = (LPIOPROC)Allocate("Proc:Item", sizeof(IOPROC)))) return TRUE;
	//	Update structure
	lpIoProc->szName	= szName;
	lpIoProc->lpProc	= lpProc;
	//	Insert proc
	if (QuickInsert(lpIoProcArray, dwIoProcArray, lpIoProc, (QUICKCOMPAREPROC) ProcCompare)) return TRUE;

	dwIoProcArray++;
	return FALSE;
}


LPVOID GetProc(LPSTR szName)
{
	LPIOPROC	lpSearch;
	IOPROC		Search;
	LPVOID		lpResult;

	//	Generate search item
	Search.szName	= szName;
	lpSearch		= &Search;
	//	Binary search
	lpResult	= bsearch(&lpSearch, lpIoProcArray,	dwIoProcArray, sizeof(LPIOPROC), (QUICKCOMPAREPROC) ProcCompare);

	return (lpResult ? ((LPIOPROC *)lpResult)[0]->lpProc : NULL);
}


LPSTR GetProcName(LPVOID addr)
{
	DWORD i;
	LPIOPROC *lpIoProc = lpIoProcArray;

	for(i=0;i<dwIoProcArray;i++,lpIoProc++)
	{
		if (lpIoProc[0]->lpProc == addr)
		{
			return lpIoProc[0]->szName;
		}
	}
	return NULL;
}


BOOL IoProc_Init(BOOL bFirstInitialization)
{
	DWORD	dwResult;

	if (! bFirstInitialization) return TRUE;

	lpIoProcArray		= NULL;
	dwIoProcArray		= 0;
	dwIoProcArraySize	= 0;
	dwResult			= 0;
	//	Memory procs
	dwResult	+= RegisterProc("Allocate", _Allocate);
	dwResult	+= RegisterProc("ReAllocate", _ReAllocate);
	dwResult	+= RegisterProc("Free", _Free);

	//	Config procs
	// IMPORTANT: The *_Old versions are to support nxMyDB which is really the only app that
	// uses this interface right now.  When that can be updated the shim *_Old functions should
	// be removed...
	dwResult	+= RegisterProc("Config_GetIniFile", Config_GetIniFile);
	dwResult	+= RegisterProc("Config_Read", Config_Read);
	dwResult	+= RegisterProc("Config_Write", Config_Write);
	dwResult	+= RegisterProc("Config_Get", Config_Get);
	dwResult	+= RegisterProc("Config_GetInt", Config_Get_Int);
	dwResult	+= RegisterProc("Config_GetBool", Config_Get_Bool);
	dwResult	+= RegisterProc("Config_GetPath", Config_Get_Path);
	dwResult	+= RegisterProc("Config_GetSection", Config_Get);
	dwResult	+= RegisterProc("Config_GetLinear", Config_Get_Linear);
	dwResult	+= RegisterProc("Config_GetPermission", Config_Get_Permission);
	//	Synchronization procs
	dwResult	+= RegisterProc("InitalizeLockObject", InitializeLockObject);
	dwResult	+= RegisterProc("DeleteLockObject", DeleteLockObject);
	dwResult	+= RegisterProc("AcquireSharedLock", AcquireSharedLock);
	dwResult	+= RegisterProc("ReleaseSharedLock", ReleaseSharedLock);
	dwResult	+= RegisterProc("AcquireExclusiveLock", AcquireExclusiveLock);
	dwResult	+= RegisterProc("AcquireHandleLock", AcquireHandleLock);
	dwResult	+= RegisterProc("ReleaseHandleLock", ReleaseHandleLock);
	//	User procs
	dwResult	+= RegisterProc("CreateUser", CreateUser);
	dwResult	+= RegisterProc("RenameUser", RenameUser);
	dwResult	+= RegisterProc("DeleteUser", DeleteUser);
	dwResult	+= RegisterProc("Uid2User", Uid2User);
	dwResult	+= RegisterProc("User2Uid", User2Uid);
	dwResult	+= RegisterProc("GetUsers", GetUsers);
	//	UserFile procs
	dwResult	+= RegisterProc("UserFile_Open", UserFile_Open);
	dwResult	+= RegisterProc("UserFile_OpenPrimitive", UserFile_OpenPrimitive);
	dwResult	+= RegisterProc("UserFile_Lock", UserFile_Lock);
	dwResult	+= RegisterProc("UserFile_Unlock", UserFile_Unlock);
	dwResult	+= RegisterProc("UserFile_Close", UserFile_Close);
	dwResult	+= RegisterProc("Ascii2UserFile", Ascii2UserFile);
	dwResult	+= RegisterProc("UserFile2Ascii", UserFile2Ascii);
	dwResult	+= RegisterProc("FindFirstUser", FindFirstUser);
	dwResult	+= RegisterProc("FindNextUser", FindNextUser);
	//	Group procs
	dwResult	+= RegisterProc("CreateGroup", CreateGroup);
	dwResult	+= RegisterProc("RenameGroup", RenameGroup);
	dwResult	+= RegisterProc("DeleteGroup", DeleteGroup);
	dwResult	+= RegisterProc("Gid2Group", Gid2Group);
	dwResult	+= RegisterProc("Group2Gid", Group2Gid);
	dwResult	+= RegisterProc("GetGroups", GetGroups);
	//	GroupFile procs
	dwResult	+= RegisterProc("GroupFile_Open", GroupFile_Open);
	dwResult	+= RegisterProc("GroupFile_OpenPrimitive", GroupFile_OpenPrimitive);
	dwResult	+= RegisterProc("GroupFile_Lock", GroupFile_Lock);
	dwResult	+= RegisterProc("GroupFile_Unlock", GroupFile_Unlock);
	dwResult	+= RegisterProc("GroupFile_Close", GroupFile_Close);
	dwResult	+= RegisterProc("GroupFile2Ascii", GroupFile2Ascii);
	dwResult	+= RegisterProc("Ascii2GroupFile", Ascii2GroupFile);
	//	Timer procs
	dwResult	+= RegisterProc("StartIoTimer", StartIoTimer);
	dwResult	+= RegisterProc("StopIoTimer", StopIoTimer);
	//	String procs
	dwResult	+= RegisterProc("SplitString", SplitString);
	dwResult	+= RegisterProc("ConcatString", ConcatString);
	dwResult	+= RegisterProc("GetStringIndex", GetStringIndex);
	dwResult	+= RegisterProc("GetStringIndexStatic", GetStringIndexStatic);
	dwResult	+= RegisterProc("GetStringRange", GetStringRange);
	dwResult	+= RegisterProc("FreeString", FreeString);
	//	Vfs procs
	dwResult	+= RegisterProc("Access", Access);
	dwResult	+= RegisterProc("GetFileInfo", GetFileInfo);
	dwResult	+= RegisterProc("UpdateFileInfo", UpdateFileInfo);
	dwResult	+= RegisterProc("CloseFileInfo", CloseFileInfo);
	dwResult	+= RegisterProc("FindFileContext", FindFileContext);
	dwResult	+= RegisterProc("InsertFileContext", InsertFileContext);
	dwResult	+= RegisterProc("DeleteFileContext", DeleteFileContext);
	dwResult	+= RegisterProc("CreateFileContext", CreateFileContext);
	dwResult	+= RegisterProc("FreeFileContext", FreeFileContext);
	dwResult	+= RegisterProc("OpenDirectory", OpenDirectory);
	dwResult	+= RegisterProc("MarkDirectory", MarkDirectory);
	dwResult	+= RegisterProc("CloseDirectory", CloseDirectory);
	dwResult	+= RegisterProc("IoRemoveDirectory", IoRemoveDirectory);
	//	File procs
	dwResult	+= RegisterProc("ioOpenFile", ioOpenFile);
	dwResult	+= RegisterProc("ioCloseFile", ioCloseFile);
	dwResult	+= RegisterProc("ioReadFile", IoReadFile);
	dwResult	+= RegisterProc("ioWriteFile", IoWriteFile);
	dwResult	+= RegisterProc("ioSeekFile", ioSeekFile);
	//	Mountfile handling
	dwResult	+= RegisterProc("MountFile_Open", MountFile_Open);
	dwResult	+= RegisterProc("MountFile_Close", MountFile_Close);
	//	Virtual path handling
	dwResult	+= RegisterProc("PWD_CWD", PWD_CWD);
	dwResult	+= RegisterProc("PWD_GetTable", PWD_GetTable);
	dwResult	+= RegisterProc("PWD_Copy", PWD_Copy);
	dwResult	+= RegisterProc("PWD_Free", PWD_Free);
	dwResult	+= RegisterProc("PWD_Resolve", PWD_Resolve);
	//	Buffer formatting
	dwResult	+= RegisterProc("FormatBuffer", FormatString);
	dwResult	+= RegisterProc("InsertBuffer", Insert_Buffer);
	dwResult	+= RegisterProc("AppendBuffer", Put_Buffer);
	//	Message handling
	dwResult	+= RegisterProc("Message_Load", Message_Load);
	dwResult	+= RegisterProc("Message_PreCompile", Message_PreCompile);
	dwResult	+= RegisterProc("MessageFile_Show", MessageFile_Show);
	dwResult	+= RegisterProc("Message_Compile", Message_Compile);
	dwResult	+= RegisterProc("Message_Object_GetInt", Object_Get_Int);
	dwResult	+= RegisterProc("Message_Object_GetString", Object_Get_String);
	//	Service managment
	dwResult	+= RegisterProc("Service_Start", Service_Start);
	dwResult	+= RegisterProc("Service_Stop", Service_Stop);
	//	Job managment
	dwResult	+= RegisterProc("QueueJob", QueueJob);
	dwResult	+= RegisterProc("AddClientJob", AddClientJob);
	dwResult	+= RegisterProc("EndClientJob", EndClientJob);
	dwResult	+= RegisterProc("AddClientJobTimer", AddClientJobTimer);
	dwResult	+= RegisterProc("SetJobFilter", SetJobFilter);
	//	Misc procs
	dwResult	+= RegisterProc("InstallMessageHandler", InstallMessageHandler);
	dwResult	+= RegisterProc("Putlog", Putlog);
	dwResult	+= RegisterProc("InitDataOffsets", InitDataOffsets);
	dwResult	+= RegisterProc("GetOnlineData", GetOnlineData);
	dwResult	+= RegisterProc("SeekOnlineData", SeekOnlineData);
	dwResult	+= RegisterProc("BindCompletionPort", BindCompletionPort);

	return (dwResult ? FALSE : TRUE);
}

VOID IoProc_DeInit(VOID)
{
	//	Free memory
	while (dwIoProcArray--) Free(lpIoProcArray[dwIoProcArray]);
	Free(lpIoProcArray);
}

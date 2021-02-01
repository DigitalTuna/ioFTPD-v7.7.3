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

typedef struct _GROUP_MODULE
{
	//	These items are filled by ioftpd
	LPSTR	tszModuleFileName;
	HANDLE	hModule;

	INT32	(* Register)(LPVOID, LPSTR, LPGROUPFILE);	//	Register group to database
	BOOL	(* RegisterAs)(LPVOID, LPSTR, LPSTR);		//	Register group x as y (rename)
	BOOL	(* Unregister)(LPVOID, LPSTR);				//	Unregister group from database
	BOOL	(* Update)(LPGROUPFILE);					//	Update group in memory database
	LPVOID	(* GetProc)(LPTSTR);

	struct _GROUP_MODULE	*lpNext;

	//	These items are filled by module
	LPSTR	tszModuleName;

	BOOL	(* DeInitialize)(VOID);
	INT32	(* Create)(LPTSTR);				//	Create group
	BOOL	(* Delete)(LPTSTR, INT32);			//	Delete group
	BOOL	(* Rename)(LPTSTR, INT32, LPSTR);		//	Rename group
	BOOL	(* Lock)(LPGROUPFILE);			//	Lock group, exclusive
	BOOL	(* Write)(LPGROUPFILE);			//	Write group
	INT		(* Open)(LPTSTR, LPGROUPFILE);	//	Open & read group
	BOOL	(* Close)(LPGROUPFILE);
	BOOL	(* Unlock)(LPGROUPFILE);			//	Unlock group

} GROUP_MODULE, * LPGROUP_MODULE;


typedef struct _PARENT_GROUPFILE
{
	INT					Gid;
	DWORD				dwErrorFlags;
	LPTIMER				lpTimer;
	LPGROUP_MODULE		lpModule;
	LPGROUPFILE			lpGroupFile;
	HANDLE				hPrimaryLock;
	LONG volatile		lOpenCount;
	LONG volatile		lSecondaryLock;
	LPTSTR              tszDefaultName;

} PARENT_GROUPFILE, * LPPARENT_GROUPFILE;


typedef struct _GROUPFILE_CONTEXT
{
	HANDLE		hFileHandle;

} GROUPFILE_CONTEXT, * LPGROUPFILE_CONTEXT;

#define	WRITE_ERROR	0001
#define	WAIT_UNREGISTER	0002
#define STATIC_SOURCE	0001
#define	NO_SYNC		0002
#define INVALID_GROUP	(UINT32)-1

#define	GM_SUCCESS	0
#define	GM_ERROR	1
#define	GM_DELETED	2
#define GM_FATAL	3

BOOL Group_Init(BOOL bFirstInitialization);
VOID Group_DeInit(VOID);
INT32 Group2Gid(LPSTR szGroupName);
LPSTR Gid2Group(INT32 Gid);
BOOL GroupFile2Ascii(LPBUFFER lpBuffer, LPGROUPFILE lpGroupFile);
BOOL Ascii2GroupFile(PCHAR pBuffer, DWORD dwBuffer, LPGROUPFILE lpGroupFile);
INT32 CreateGroup(LPSTR szGroupName);
PINT32 GetGroups(LPDWORD lpGroupIdCount);
BOOL DeleteGroup(LPSTR szGroupName);
BOOL RenameGroup(LPSTR szGroupName, LPSTR szNewGroupName);
BOOL GroupFile_Lock(LPGROUPFILE *lpGroupFile, DWORD dwFlags);
BOOL GroupFile_WriteTimer(LPPARENT_GROUPFILE lpParentGroupFile, LPTIMER lpTimer);
BOOL GroupFile_Unlock(LPGROUPFILE *lpGroupFile, DWORD dwFlags);
BOOL GroupFile_Close(LPGROUPFILE *lpGroupFile, DWORD dwOpenFlags);
BOOL GroupFile_OpenPrimitive(INT32 Gid, LPGROUPFILE *lpGroupFile, DWORD dwOpenFlags);
BOOL GroupFile_OpenNext(LPGROUPFILE *lpGroupFile, LPINT lpOffset);
BOOL GroupFile_Open(LPSTR szGroupName, LPGROUPFILE *lpGroupFile, DWORD dwOpenFlags);
LPPARENT_GROUPFILE GetParentGroupFile(INT32 Gid);
LPPARENT_GROUPFILE GetParentGroupFileSafely(INT32 Gid);
LPGROUP_MODULE Group_FindModule(LPSTR tszModuleName);

//	Default group module initalization routine
BOOL Group_StandardInit(LPGROUP_MODULE lpModule);

BOOL Group_Default_Open(LPGROUPFILE lpGroupFile);
BOOL Group_Default_Write(LPGROUPFILE lpGroupFile);
BOOL Group_Default_Close(LPGROUPFILE lpGroupFile);

extern IDDATABASE           dbGroupId;
extern LPPARENT_GROUPFILE  *lpGroupFileArray;
extern LOCKOBJECT           loGroupFileArray;
extern DWORD                dwGroupFileArrayItems, dwGroupFileArraySize;

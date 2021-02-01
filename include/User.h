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

typedef struct _USER_MODULE
{
	//	These items are filled by ioftpd
	LPSTR	tszModuleFileName;
	HANDLE	hModule;

	INT32	(* Register)(LPVOID, LPTSTR, LPUSERFILE);	//	Register user to database
	BOOL	(* RegisterAs)(LPVOID, LPTSTR, LPTSTR);		//	Register user x as user y (rename :p)
	BOOL		(* Unregister)(LPVOID, LPSTR);				//	Unregister user from database
	BOOL	(* Update)(LPUSERFILE);						//	Update user in memory database
	LPVOID	(* GetProc)(LPTSTR);

	struct _USER_MODULE	*lpNext;

	//	These items are filled by module
	LPSTR	tszModuleName;

	BOOL	(* DeInitialize)(VOID);
	INT32	(* Create)(LPTSTR, INT32);			//	Create user
	BOOL	(* Delete)(LPTSTR, INT32);			//	Delete user
	BOOL	(* Rename)(LPTSTR, INT32, LPSTR);		//	Rename user
	BOOL	(* Lock)(LPUSERFILE);			//	Lock user, exclusive
	BOOL	(* Write)(LPUSERFILE);			//	Write user
	INT		(* Open)(LPTSTR, LPUSERFILE);	//	Open & read user
	BOOL	(* Close)(LPUSERFILE);
	BOOL	(* Unlock)(LPUSERFILE);			//	Unlock user

} USER_MODULE, * LPUSER_MODULE;


typedef struct _PARENT_USERFILE
{
	INT					Uid;	//	User id
	DWORD				dwErrorFlags;	//	Pending error flags
	LPTIMER				lpTimer;	//	Pointer to timer
	LPUSER_MODULE		lpModule;		//	Pointer to module
	LPUSERFILE			lpUserFile;	//	Current shared userfile
	HANDLE				hPrimaryLock;	//	Primary lock, used for exclusive locking
	LONG volatile		lLoginCount[3];	//	Login counters (by service type)
	LONG volatile       lDownloads;        // Active downloads
	LONG volatile       lUploads;          // Active uploads
	LONG volatile		lOpenCount;		//	Usage counter
	LONG volatile		lSecondaryLock;		//	Secodary lock, used for shared locking

} PARENT_USERFILE, * LPPARENT_USERFILE;


typedef struct _USERFILE_CONTEXT
{
	HANDLE		hFileHandle;

} USERFILE_CONTEXT, * LPUSERFILE_CONTEXT;


typedef struct _USERSEARCH_NAME
{
	LPTSTR					tszName;
	struct _USERSEARCH_NAME	*lpNext;
	
} USERSEARCH_NAME, * LPUSERSEARCH_NAME;


typedef struct _USERSEARCH_ID
{
	INT32					Id;
	BOOL                    bPrimary;
	struct _USERSEARCH_ID	*lpNext;

} USERSEARCH_ID, * LPUSERSEARCH_ID;

typedef struct _USERSEARCH_FLAG
{
	struct _USERSEARCH_FLAG	*lpNext;
	TCHAR                    tszFlag[1];

} USERSEARCH_FLAG, * LPUSERSEARCH_FLAG;


typedef struct _USERSEARCH_RATIO
{
	INT                        Section;
	INT                        Ratio;
	// -1 less than, 0 = equal, 1 greater than
	INT                        CompareType;
	struct _USERSEARCH_RATIO  *lpNext;

} USERSEARCH_RATIO, * LPUSERSEARCH_RATIO;


typedef struct _USERSEARCH
{
	LPUSERSEARCH_FLAG	lpIncludedFlag;
	LPUSERSEARCH_FLAG	lpExcludedFlag;
	LPUSERSEARCH_NAME	lpIncludedUserName;
	LPUSERSEARCH_NAME	lpExcludedUserName;
	LPUSERSEARCH_ID		lpIncludedUserId;
	LPUSERSEARCH_ID		lpExcludedUserId;
	LPUSERSEARCH_ID		lpIncludedGroupId;
	LPUSERSEARCH_ID		lpExcludedGroupId;
	LPUSERSEARCH_ID		lpIncludedAdminGroupId;
	LPUSERSEARCH_ID		lpExcludedAdminGroupId;
	LPUSERSEARCH_RATIO  lpIncludedRatio;
	LPUSERSEARCH_RATIO  lpExcludedRatio;
	PINT32				lpUidList;
	DWORD				dwUidList;
	DWORD				dwOffset;
	DWORD               dwMaxIds;
	BOOL                bMatching;
	BOOL                bUseMatchList;
	BOOL                bAdding;
	BOOL                bDefaultMatch;
	PINT32				lpMatchList;

} USERSEARCH, * LPUSERSEARCH;


#define	UM_SUCCESS	0
#define	UM_ERROR	1
#define	UM_DELETED	2
#define UM_FATAL	3
#define INVALID_USER	(UINT32)-1

#ifndef WRITE_ERROR
#define STATIC_SOURCE	0001
#define	NO_SYNC		0002
#define	WRITE_ERROR	0001
#define	WAIT_UNREGISTER	0002
#endif


BOOL User_Init(BOOL bFirstInitialization);
VOID User_DeInit(VOID);
PINT32 GetUsers(LPDWORD lpUserIdCount);
INT32 User2Uid(LPSTR szUserName);
LPSTR Uid2User(INT32 Uid);
BOOL UserFile2Ascii(LPBUFFER lpBuffer, LPUSERFILE lpUserFile);
BOOL Ascii2UserFile(PCHAR pBuffer, DWORD dwBuffer, LPUSERFILE lpUserFile);
BOOL CreateUser(LPSTR szUserName, INT32 Gid);
BOOL DeleteUser(LPSTR szUserName);
BOOL RenameUser(LPSTR szUserName, LPSTR szNewUserName);
BOOL UserFile_Sync(LPUSERFILE *lpUserFile);
BOOL UserFile_Lock(LPUSERFILE *lpUserFile, DWORD dwFlags);
BOOL UserFile_WriteTimer(LPPARENT_USERFILE lpParentUserFile, LPTIMER lpTimer);
BOOL UserFile_Unlock(LPUSERFILE *lpUserFile, DWORD dwFlags);
BOOL UserFile_Close(LPUSERFILE *lpUserFile, DWORD dwCloseFlags);
BOOL UserFile_OpenPrimitive(INT32 Uid, LPUSERFILE *lpUserFile, DWORD dwOpenFlags);
BOOL UserFile_Open(LPSTR szUserName, LPUSERFILE *lpUserFile, DWORD dwOpenFlags);
LPUSERSEARCH FindFirstUser(LPTSTR tszWildCard, LPUSERFILE *lppUserFile, LPUSERFILE lpCaller, struct _FTP_USER *lpUser, LPDWORD lpdwUsers);
BOOL FindNextUser(LPUSERSEARCH hUserSearch, LPUSERFILE *lppUserFile);
LPUSERSEARCH FindParse(LPTSTR tszWildCard, LPUSERFILE lpCaller, struct _FTP_USER *lpUser, BOOL bMatching);
BOOL FindIsMatch(LPUSERSEARCH lpSearch, LPUSERFILE lpUserFile, BOOL bTestUid);
VOID FindFinished(LPUSERSEARCH lpSearch);
VOID HashString(LPSTR szString, PUCHAR pHash);
BOOL User_IsAdmin(LPUSERFILE lpAdmin, LPUSERFILE lpUserFile, PINT32 pGid);
LONG UserFile_GetLoginCount(LPUSERFILE lpUserFile, DWORD dwType);
VOID UserFile_IncrementLoginCount(LPUSERFILE lpUserFile, DWORD dwType);
VOID UserFile_DecrementLoginCount(LPUSERFILE lpUserFile, DWORD dwType);
BOOL UserFile_DownloadBegin(LPUSERFILE lpUserFile);
VOID UserFile_DownloadEnd(LPUSERFILE lpUserFile);
BOOL UserFile_UploadBegin(LPUSERFILE lpUserFile);
VOID UserFile_UploadEnd(LPUSERFILE lpUserFile);
LPUSERFILE User_GetFake(void);
LPPARENT_USERFILE GetParentUserFile(INT32 Uid);
LPUSER_MODULE User_FindModule(LPTSTR tszModuleName);

//	Default group module initialization routine
BOOL User_StandardInit(LPUSER_MODULE lpModule);

BOOL User_Default_Open(LPUSERFILE lpUserFile, INT32 id);
BOOL User_Default_Write(LPUSERFILE lpUserFile);
BOOL User_Default_Close(LPUSERFILE lpUserFile);

extern IDDATABASE          dbUserId;
extern LPPARENT_USERFILE  *lpUserFileArray;
extern LOCKOBJECT          loUserFileArray;
extern DWORD               dwUserFileArrayItems, dwUserFileArraySize;

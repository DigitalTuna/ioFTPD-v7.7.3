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

#define	TYPE_DIRECTORY	 0001L
#define TYPE_FILE		 0002L
#define	TYPE_LINK		 0004L
#define	EXISTS		 	 0010L
#define KEEP_CASE        0100L
#define KEEP_LINKS       0200L
#define IGNORE_PERMS     0400L
#define VIRTUAL_PWD     01000L
#define VIRTUAL_UPDATE  02000L
#define VIRTUAL_NOCACHE	04000L
#define VIRTUAL_ONCE   010000L
#define VIRTUAL_DONE   020000L

typedef struct _MOUNT_ITEM
{

	LPSTR	szFileName;
	DWORD	dwFileName;

} MOUNT_ITEM, * LPMOUNT_ITEM;



typedef struct _MOUNT_POINT
{

	LPSTR		szName;
	DWORD		dwName;

	MOUNT_ITEM	lpSubMount[MAX_SUBMOUNTS];
	DWORD		dwSubMounts;

	struct _MOUNT_POINT *lpParent;
	DWORD                dwPathLen;
	LPTSTR               tszFullPath;

	LPVOID		lpNextTable;

} MOUNT_POINT, * LPMOUNT_POINT;


// Simple parsed input with real path in file order and the associated virtual path.
// This is used to reverse NTFS junction/symlinks to a virtual path...
typedef struct _MOUNT_ENTRIES
{
	DWORD           dwAllocatedSize;
	DWORD           dwEntries;
	LPMOUNT_ITEM   *lpRealItemArray;
	LPMOUNT_ITEM    VirtualItemArray;
} MOUNT_ENTRIES, * LPMOUNT_ENTRIES;


// lShareCount moved here from _MOUNTCACHEDATA when it could no longer be
// shared because of virtual directories.  Only valid in the first entry.
// Likewise lpMountEntries only valid in first entry...
typedef struct _MOUNT_TABLE
{
	LONG volatile	 lShareCount;
	LPMOUNT_ENTRIES  lpEntries;
	LPMOUNT_POINT   *lpMountPoints;
	DWORD		 	 dwMountPoints;
	DWORD			 dwMountPointsAllocated;

} MOUNT_TABLE, * LPMOUNT_TABLE;


// NOTE: this structure will be allocated two ways.  struct+name+link+pointer
//       in the case of a real FileInfo, or struct+fileinfo(with name)+link if
//       lpFileInfo->lReferenceCount == 0 which implies it's fake.
// NOTE: FileInfo entries are in DirectoryCache.h
typedef struct _VIRTUALINFO
{
	LPTSTR             tszName;
	LPTSTR             tszLink;
	LPTSTR             tszUser;
	LPTSTR             tszGroup;
	BOOL               bHideLink;
	struct _FILEINFO  *lpFileInfo;
} VIRTUALINFO, *LPVIRTUALINFO;



typedef struct _VIRTUAL_DIR
{
	CHAR           pwd[_MAX_PWD+1];
	DWORD          dwLen;
	FILETIME       ftLastUpdate;
	CHAR           szTarget[_MAX_PWD+1];
	CHAR           szLastGlob[_MAX_PWD+1];
	BOOL           bListed;
	DWORD          dwMaxVirtualInfos;
	DWORD          dwVirtualInfos;
	LPVIRTUALINFO *lpVirtualInfoArray;

} VIRTUALDIR, *LPVIRTUALDIR;


typedef struct _VIRTUAL_DIR_EVENT
{
	DWORD   dwId;
	DWORD   dwName;
	LPTSTR  tszName;
	DWORD   dwEvent;
	LPTSTR  tszEvent;
	LPTSTR  tszPrivate;

} VIRTUALDIREVENT, *LPVIRTUALDIREVENT;


typedef struct _MOUNT_DATA
{
	LPMOUNT_POINT	  Resume;
	INT				  Last;
	DWORD			  dwLastPath;
	LPSTR			  Pwd;
	INT				  Length;
	BOOL			  Initialized;
	LPVIRTUALDIREVENT lpVirtualDirEvent;

} MOUNT_DATA, *LPMOUNT_DATA;


typedef struct _MOUNTCACHEDATA
{
	LPMOUNT_TABLE		lpMountTable;
	struct _FTP_USER   *lpFtpUser;
	LPVIRTUALDIR        lpVirtualDirArray[MAX_VIRTUAL_DIR_MOUNTPOINTS];
	DWORD               dwFileName;
	CHAR 			    szFileName[1];

} MOUNTCACHEDATA, * MOUNTFILE;


typedef struct _MOUNTCACHE
{
	MOUNTFILE  lpCacheData;
	LPSTR	   szFileName;
	FILETIME   ftCacheTime;

} MOUNTCACHE, * LPMOUNTCACHE;


typedef struct _PWD
{
	CHAR	pwd[_MAX_PWD+1];
	DWORD	len;
	CHAR	Symbolic[_MAX_PWD+1];
	DWORD	Symlen;
	LPSTR	RealPath;
	DWORD	l_RealPath;

	// TODO: use these
	LPMOUNT_POINT     lpMountPoint;
	LPVIRTUALDIREVENT lpVirtualDirEvent;
	INT     iMountIndex;

	CHAR	SectionName[64 + 1];	// MAX_NAME + 1
	INT		StatsSection;
	INT		CreditSection;
	INT     ShareSection;

} PWD, * PPWD, * PVIRTUALPATH, VIRTUALPATH;


typedef struct _VFS_PRELOAD
{
	MOUNTFILE            hMountFile;
	TCHAR                pBuffer[_INI_LINE_LENGTH + 1];
	volatile LONG *      lplCount;
	int                  depth;

	volatile LONG        lThreads;
	HANDLE               hSema;

} VFSPRELOAD, *LPVFSPRELOAD;


typedef struct _VFS_PRELOAD_DIR
{
	LPVFSPRELOAD  lpPre;
	LPTSTR        szDirName;               
	HANDLE        hSema;

} VFSPRELOADDIR, *LPVFSPRELOADDIR;


typedef struct _VFS_PRELOAD_POINT
{
	LPVFSPRELOAD    lpPre;
	LPMOUNT_POINT   lpMountPoint;
	BOOL            bThreaded;

} VFSPRELOADPOINT, *LPVFSPRELOADPOINT;


typedef struct _VFS_PRELOAD_TABLE
{
	LPVFSPRELOAD    lpPre;
	LPMOUNT_TABLE   lpMountTable;

} VFSPRELOADTABLE, *LPVFSPRELOADTABLE;


extern VIRTUALDIREVENT    KnownVirtualDirEvents[MAX_VIRTUAL_DIR_MOUNTPOINTS];
extern DWORD              dwKnownVirtualDirEvents;


//	Function prototypes
VOID MountFile_DeInit(VOID);
BOOL MountFile_Init(BOOL bFirstInitialization);
VOID MountFile_Close(MOUNTFILE hMountFile);
MOUNTFILE MountFile_Open(LPSTR szFileName, struct _FTP_USER *lpFtpUser);

BOOL PWD_Copy(PVIRTUALPATH Source, PVIRTUALPATH Target, BOOL AllocatePath);
BOOL PWD_CopyAddSym(PVIRTUALPATH Source, PVIRTUALPATH Target, BOOL AllocatePath);
VOID PWD_Free(PVIRTUALPATH VirtualPath);
VOID PWD_Reset(PVIRTUALPATH VirtualPath);
VOID PWD_Zero(PVIRTUALPATH VirtualPath);
VOID PWD_Set(PVIRTUALPATH VirtualPath, LPTSTR tszPath);
LPSTR PWD_CWD(LPUSERFILE lpUserFile, PVIRTUALPATH Pwd, LPSTR ChangeTo, MOUNTFILE hMountFile, DWORD dwFlags);
LPTSTR PWD_CWD2(LPUSERFILE lpUserFile, PVIRTUALPATH Pwd, LPTSTR tszChangeTo, MOUNTFILE hMountFile, LPMOUNT_DATA lpMountData, DWORD dwFlags, 
                struct _FTP_USER *lpUser, LPTSTR tszCommand, IO_STRING *Args);
BOOL PWD_Normalize(LPTSTR tszPath, LPTSTR tszNormalized, LPTSTR tszCWD);
LPSTR PWD_Resolve(LPSTR szVirtualPath, MOUNTFILE hMountFile, LPMOUNT_DATA Data, BOOL Exists, INT ExtraMem);
LPMOUNT_TABLE PWD_GetTable(LPSTR szVirtualPath, MOUNTFILE hMountFile);
BOOL PWD_IsMountPoint(LPSTR szVirtualPath, MOUNTFILE hMountFile);
VOID PreLoad_VFS(LPVOID bLogCount);
LPVIRTUALDIR FindVirtualDir(LPTSTR tszPath);
BOOL InsertVirtualInfo(LPVIRTUALDIR lpVirtualDir, LPVIRTUALINFO lpVirtualInfo);
VOID MarkVirtualDir(PVIRTUALPATH Path, MOUNTFILE hMountFile);
LPVIRTUALDIREVENT FindVirtualDirEvent(LPTSTR tszPath);
LPVIRTUALDIR VirtualDirLoad(MOUNTFILE hMountFile, LPVIRTUALDIREVENT lpVirtualDirEvent, LPSTR szPath, LPSTR szGlob, BOOL bUpdate, BOOL bListing, BOOL bExists, LPTSTR tszCommand);
DWORD ReverseResolve(MOUNTFILE hMountFile, LPTSTR tszRealPath);

BOOL GetVfsParentFileInfo(LPUSERFILE lpUserFile, MOUNTFILE hMountFile, PVIRTUALPATH pvpPath, struct _FILEINFO **lppFileInfo, BOOL bRootOk);

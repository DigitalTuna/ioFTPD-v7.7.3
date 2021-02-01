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


#define NTFS_REPARSE_IGNORE		0
#define NTFS_REPARSE_SHARE		1
#define NTFS_REPARSE_SYMLINK	2

#define	FILE_ATTRIBUTE_IOFTPD	0x10000000
#define FILE_ATTRIBUTE_DIRTY	0x20000000
#define FILE_ATTRIBUTE_FAKE		0x40000000
#define FILE_ATTRIBUTE_LINK     0x80000000
#define FILE_ATTRIBUTE_MASK     0x0FFFFFFF


#define _I_READ		0002L
#define _I_WRITE	0004L
#define _I_EXECUTE	0001L
#define _I_OWN	   01000L
#define _I_PASS    02000L


#define S_REDIRECTED	040000
#define S_SYMBOLIC		020000
#define S_PRIVATE		010000
#define S_IRWXU			  0700
#define S_IRUSR		      0400
#define S_IWUSR		      0200
#define S_IXUSR		      0100
#define S_IRWXG		      0070
#define S_IRGRP		      0040
#define S_IWGRP		      0020
#define S_IXGRP		      0010
#define S_IRWXO		      0007
#define S_IROTH		      0004
#define S_IWOTH		      0002
#define S_IXOTH		      0001
#define S_ACCESS          0777

#define	PRIVATE			0
#define SYMBOLICLINK	1
#define REDIRECTED      2




typedef struct _FILECONTEXT
{
	LPVOID			lpData;
	DWORD			dwData;

} FILECONTEXT, * LPFILECONTEXT;

typedef struct _FSEARCH
{
	DWORD	dwFileAttributes;
	LPTSTR	tszFileName;

} FSEARCH, * LPFSEARCH;

// NOTE: lReferenceCount==0 implies it's a fake entry for Virtual Events
// NOTE: The memory for Context is also allocated/re-allocated with this
//       structure so the layout is struct+filename+context
// NOTE: if dwFileAttributes has the FILE_ATTRIBUTE_LINK flag set then
//       this is a linked entry and lpLinkedRoot might be valid and point to
//       the target directory's root entry.  It also means that no context
//       will be used but instead the full path to the file will follow
//       so the layout is struct+filename+fullpath
typedef struct _FILEINFO
{
	DWORD             dwSafety;
	LONG volatile	  lReferenceCount;
	UINT64			  FileSize;
	DWORD			  dwSubDirectories;
	FILETIME		  ftModificationTime;
	FILETIME		  ftAlternateTime;
	UINT32			  Uid;
	UINT32			  Gid;
	DWORD             dwUploadTimeInMs;
	DWORD			  dwFileMode;
	DWORD volatile	  dwFileAttributes;
	FILECONTEXT		  Context;
	struct _FILEINFO *lpLinkedRoot;
	DWORD			  dwFileName;
	TCHAR			  tszFileName[1];

} FILEINFO, * LPFILEINFO;


typedef struct _DIRECTORYINFO
{
	LONG volatile		   lReferenceCount;
	LPFILEINFO			  *lpFileInfo;
	LPFILEINFO			   lpRootEntry;
	struct _DIRECTORYINFO *lpLinkedInfo;
	DWORD				   dwDirectorySize;
	DWORD			       dwRealPath;
	TCHAR			       tszRealPath[1];

} DIRECTORYINFO, * LPDIRECTORYINFO;


typedef struct _DIRECTORYCACHEINFO
{
	LONG	lHits;
	LONG	lMisses;
	LONG	lFlushes;
	LONG	lCollisions;

} DIRECTORYCACHEINFO, *LPDIRECTORYCACHEINFO;


typedef struct _DIRECTORY
{
	struct _DIRECTORY	*lpPrev;
	struct _DIRECTORY	*lpNext;

	HANDLE				hEvent;
	LONG volatile		lForceUpdate;
	BOOL				bPopulated;
	BOOL                bHasFakeSubDirs;
	BOOL                bLocked;
	LONG 				lReferenceCount;
	FILETIME			ftCacheTime;
	LPDIRECTORYINFO		lpDirectoryInfo;
	UINT32				Hash;
	DWORD				dwFileName;
	TCHAR				tszFileName[1];	

} DIRECTORY, * LPDIRECTORY;

typedef struct _DIRECTORYTABLE
{
	LPDIRECTORY			*lpDirectory;
	DWORD				dwDirectories;
	DWORD				dwAllocated;
	LPDIRECTORY			lpDirectoryList[2];
	CRITICAL_SECTION	CriticalSection;

} DIRECTORYTABLE, * LPDIRECTORYTABLE;

typedef struct _VFSUPDATE
{
	UINT32		Uid;
	UINT32		Gid;
	DWORD		dwFileMode;
	FILETIME	ftAlternateTime;  // only used for dirs
	DWORD       dwUploadTimeInMs; // only used for files
	FILECONTEXT	Context;

} VFSUPDATE, * LPVFSUPDATE;

typedef struct _FIND
{
	LPDIRECTORYINFO	lpDirectoryInfo;
	DWORD			dwOffset;
	TCHAR			tszFilter[1];

} FIND, *LPFIND;

// DELETELIST already used by List.h
typedef struct _DELETELIST
{
	struct _DELETELIST	*lpNext;
	DWORD			dwFileName;
	TCHAR			tszFileName[1];
} *LPDELETELIST;


#if 1
// The is from DDK/ntifs.h which really should be available to the SDK...
# define SYMLINK_FLAG_RELATIVE   1
typedef struct _REPARSE_DATA_BUFFER {
	ULONG  ReparseTag;
	USHORT ReparseDataLength;
	USHORT Reserved;
	union {
		struct {
			USHORT SubstituteNameOffset;
			USHORT SubstituteNameLength;
			USHORT PrintNameOffset;
			USHORT PrintNameLength;
			ULONG Flags;
			WCHAR PathBuffer[1];
		} SymbolicLinkReparseBuffer;
		struct {
			USHORT SubstituteNameOffset;
			USHORT SubstituteNameLength;
			USHORT PrintNameOffset;
			USHORT PrintNameLength;
			WCHAR PathBuffer[1];
		} MountPointReparseBuffer;
		struct {
			UCHAR  DataBuffer[1];
		} GenericReparseBuffer;
	} DUMMYUNIONNAME;
} REPARSE_DATA_BUFFER, *LPREPARSE_DATA_BUFFER;

// This is defined here when we are using _WIN32_WINNT of 0x403 since it isn't defined until 0x500
# ifndef FSCTL_GET_REPARSE_POINT
#  define FSCTL_GET_REPARSE_POINT         CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 42, METHOD_BUFFERED, FILE_ANY_ACCESS) // REPARSE_DATA_BUFFER
# endif

#endif




BOOL DeleteFileContext(LPFILECONTEXT lpContext, BYTE Item);
BOOL FreeFileContext(LPFILECONTEXT lpContext);
LPVOID FindFileContext(BYTE Item, LPFILECONTEXT lpContext);
BOOL InsertFileContext(LPFILECONTEXT lpContext, BYTE Item, LPVOID lpData, DWORD dwData);
BOOL CreateFileContext(LPFILECONTEXT lpContext, LPFILECONTEXT lpSourceContext);

BOOL DirectoryCache_Init(BOOL bFirstInitialization);
VOID DirectoryCache_DeInit(VOID);


BOOL IoRemoveReparsePoint(LPTSTR tszPath);
BOOL IoMoveDirectory(LPTSTR tszSrcPath, LPTSTR tszDestPath, struct _CMD_PROGRESS *lpProgress);
BOOL IoRemoveDirectory(LPTSTR tszPath);
BOOL MarkDirectory(LPTSTR tszFileName);
BOOL MarkParent(LPTSTR tszFileName, BOOL bParent2);
BOOL UpdateFileInfo(LPTSTR tszFileName, LPVFSUPDATE lpData);
BOOL GetFileInfo(LPTSTR tszFileName, LPFILEINFO *lpFileInfo);
BOOL GetFileInfoNoCheck(LPTSTR tszFileName, LPFILEINFO *lpFileInfo);
VOID CloseFileInfo(LPFILEINFO lpFileInfo);
LPFIND IoFindFirstFile(LPTSTR tszPath, LPTSTR tszFilter, LPFILEINFO *lpFileInfo);
BOOL IoFindNextFile(LPFIND hFind, LPFILEINFO *lpResult);
VOID IoCloseFind(LPFIND hFind);
LPDIRECTORYINFO OpenDirectory(LPTSTR tszFileName, BOOL bRecursive, BOOL bFakeDirs, BOOL bSetLockFlag, PBOOL pbLockFlagStatus, LPDIRECTORY *lppDirectory);
BOOL CloseDirectory(LPDIRECTORYINFO lpDirectoryInfo);
BOOL Access(LPUSERFILE lpUserFile, LPFILEINFO lpFileInfo, DWORD dwMode);

BOOL IoDeleteFile(LPTSTR tszFileName, DWORD dwFileName);
BOOL IoMoveFile(LPTSTR tszExistingFileName,
                       LPTSTR tszNewFileName);
BOOL GetFileInfo2(LPTSTR tszFileName, LPFILEINFO *lpFileInfo, BOOL bNoCheck, LPDIRECTORYINFO *lppDirInfo);

INT __cdecl CompareFileName(LPCVOID *lpItem1, LPCVOID *lpItem2);

// Admin_DirCache and Admin_Refresh prototyped in AdminCommands.h at which time LPFTPUSER is defined

extern UINT32 volatile DefaultUid[], DefaultGid[];
extern DWORD volatile dwDefaultFileMode[];
extern DWORD NtfsReparseMethod;
extern BOOL  bVfsExportedPathsOnly;

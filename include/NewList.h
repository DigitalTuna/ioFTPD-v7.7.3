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

#define	LIST_DIRECTORIES_ONLY	      01
#define LIST_HIDDEN				      02
#define LIST_ALL				      04
#define LIST_LONG                    010
#define LIST_DATE_FULL               020
#define LIST_RECURSIVE			     040
#define LIST_SYMLINK_SIZE           0100
#define LIST_SUBDIR_SIZE            0200
#define LIST_ALTERNATE_TIME         0400
#define LIST_PRIVATE_GROUP         01000
#define LIST_MERGED_GROUP		   02000
#define LIST_UPSPEED_GROUP          04000
#define	LIST_FILES_ONLY			  010000

#define LIST_ADMIN                040000

#define LIST_VIRTUAL_DIR         0100000

#define LIST_FIRST_DIR			01000000
#define LIST_DID_ONE			02000000
#define LIST_MAYBE_GLOB			04000000

#define LIST_GROUP_OPTIONS      (LIST_PRIVATE_GROUP | LIST_MERGED_GROUP | LIST_UPSPEED_GROUP)

// This is used for combining FileInfo structures.
// It's a bit of a hack, but we can allocate these via alloca so it's fast
// and we don't have to malloc/free them.  In order to keep a pointer
// to the context parameter so we can lookup symlinks, private, etc we will
// increment the reference count in the FileContext structure but this means
// we need a pointer so we can undo that later.  Thus this structure is born
// to hold the fake and the pointer.
// If Combined.lReferenceCount == -1 this is a combined element and we don't
// need to free anything so lpReal is unset/invalid/ignored.
// If Combined.lReferenceCount == -2 then lpReal is valid, has had it's reference
// count incremented, and will need to have CloseFileInfo(lpReal) done at some
// point...
// Careful: pointer math used on fields in this structure if changed update newlist.c
typedef struct _FAKEFILEINFO {
	LPFILEINFO lpReal;
	FILEINFO   Combined;
} FAKEFILEINFO, *LPFAKEFILEINFO;


typedef struct _DIRLISTING
{
	VIRTUALPATH	        vpVirtPath;
	TCHAR	            szRelativeVPathName[_MAX_PWD+1];
	FAKEFILEINFO        FakeParentInfo;

	struct _DIRLISTING  *lpNext;

} DIRLISTING, * LPDIRLISTING;


typedef struct _LISTING
{
    struct _FTP_USER    *lpFtpUser;
	LPUSERFILE		     lpUserFile;
	MOUNTFILE            hMountFile;
	PVIRTUALPATH	     lpInitialVPath;
	LPBUFFER	  	     lpBuffer;
	BOOL			     (* lpPrint)(struct _LISTING *, LPTSTR, BOOL, PVIRTUALPATH, LPFILEINFO, BOOL, DWORD, LPVIRTUALINFO);
	DWORD			     dwFlags;
	LPSTR			     szCmdLine;
	LPSTR			     szGlobber;
	FILETIME             ftSixMonthsAgo;
	FILETIME             ftCurrent;
	LPVIRTUALDIR         lpVirtualDir;
	LPDIRLISTING         lpDirNext;

	struct _TCL_DIRLIST *lpTclDirList; // only used when listing a directory from TCL

	// information filled in when processing a directory (for use by printing routines)
	LPMOUNT_POINT        lpMountPoint; 
	LPDIRECTORYINFO      lpDirInfoArray[MAX_SUBMOUNTS+1];

} LISTING, * LPLISTING;


LPLISTING List_ParseCmdLine(IO_STRING *Args, PVIRTUALPATH lpVPath, MOUNTFILE hMountFile);
BOOL InitListing(LPLISTING lpListing, BOOL bNoVirtual);
BOOL ListNextDir(LPLISTING lpListing);

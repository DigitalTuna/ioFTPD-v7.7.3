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

#define DC_EXECUTE		0	//	Context = LPSTR
#define DC_USER_TO_UID		1	//	Context = DC_NAMEID
#define DC_UID_TO_USER		2	//	Context = DC_NAMEID
#define DC_GROUP_TO_GID		3	//	Context = DC_NAMEID
#define DC_GID_TO_GROUP		4	//	Context = DC_NAMEID
#define DC_USERFILE_OPEN	5	//	Context = USERFILE_OLD
#define DC_USERFILE_LOCK	6	//	Context = USERFILE_OLD
#define DC_USERFILE_UNLOCK	7	//	Context = USERFILE_OLD
#define DC_USERFILE_CLOSE	8	//	Context = USERFILE_OLD
#define DC_GROUPFILE_OPEN	9	//	Context = GROUPFILE
#define DC_GROUPFILE_CLOSE	10	//	Context = GROUPFILE
#define DC_FILEINFO_READ	11	//	Context = DC_VFS
#define DC_FILEINFO_WRITE	12	//	Context = DC_VFS
#define	DC_GET_ONLINEDATA	13	//	Context = DC_ONLINEDATA
#define DC_CREATE_USER		14	//	Context = DC_NAMEID
#define DC_RENAME_USER		15	//	Context = DC_RENAME
#define DC_DELETE_USER		16	//	Context = DC_NAMEID
#define DC_CREATE_GROUP		17	//	Context	= DC_NAMEID
#define DC_RENAME_GROUP		18	//	Context = DC_RENAME
#define DC_DELETE_GROUP		19	//	Context = DC_NAMEID
#define DC_GROUPFILE_LOCK	20	//	Context = GROUPFILE
#define DC_GROUPFILE_UNLOCK	21	//	Context = GROUPFILE
#define DC_DIRECTORY_MARKDIRTY	 22 //  Context = LPTSTR
#define DC_NEW_USERFILE_OPEN     23 //  Context = USERFILE
#define DC_NEW_USERFILE_LOCK     24 //  Context = USERFILE
#define DC_NEW_USERFILE_UNLOCK   25 //  Context = USERFILE
#define DC_NEW_USERFILE_CLOSE    26 //  Context = USERFILE



typedef struct _DC_MESSAGE
{
	HANDLE		hEvent;
	HANDLE		hObject;
	DWORD		dwIdentifier;
	DWORD		dwReturn;
	LPVOID		lpMemoryBase;
	LPVOID		lpContext;


} DC_MESSAGE, * LPDC_MESSAGE;




#define ER_AVAILABLE	0x0000
#define ER_IN_USE		0x0001
#define ER_REMOVED		0x0002

typedef struct _DC_USERFILE_REQUEST
{
	USERFILE           UserFile;

	struct _DC_USERFILE_REQUEST *lpNext;
	struct _DC_USERFILE_REQUEST *lpPrev;
} DC_USERFILE_REQUEST, *LPDC_USERFILE_REQUEST;


typedef struct _EXCHANGE_REQUEST
{
	WORD				  wType;		// Type of allocation
	WORD				  wStatus;		// Request status
	HANDLE				  hEvent;		// Event handle
	HANDLE				  hMemory;		// Memory object handle
	DWORD				  dwTickCount;	// When request was issued
	LPDC_USERFILE_REQUEST lpUserFileReqList[2]; // linked list of open userfile requests
	LPDC_MESSAGE		  lpMessage;

	struct _EXCHANGE_REQUEST	*lpNext;
	struct _EXCHANGE_REQUEST	*lpPrev;
	
} EXCHANGE_REQUEST, * LPEXCHANGE_REQUEST;



typedef struct _DC_RENAME
{
	TCHAR	tszName[_MAX_NAME + 1];
	TCHAR	tszNewName[_MAX_NAME + 1];
} DC_RENAME, * LPDC_RENAME;



typedef struct _DC_NAMEID
{
	TCHAR	tszName[_MAX_NAME + 1];
	INT32		Id;

} DC_NAMEID, * LPDC_NAMEID;



typedef struct _DC_ONLINEDATA
{
	ONLINEDATA	OnlineData;
	INT			iOffset;
	DWORD		dwSharedMemorySize;

} DC_ONLINEDATA, * LPDC_ONLINEDATA;



typedef struct _DC_VFS
{
	UINT32			Uid;
	UINT32			Gid;
	DWORD			dwFileMode;
	DWORD			dwBuffer;
	PBYTE			pBuffer[1];

} DC_VFS, * LPDC_VFS;


#define SHELL		0
#define FILEMAP		1

VOID DataCopy_DeInit(VOID);
BOOL DataCopy_Init(BOOL bFirstInitialization);

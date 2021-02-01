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

#define IO_TCL_MAJOR_VER	8
#define IO_TCL_MINOR_VER	5
#define IO_TCL_PATCH_VER	19

#define TCL_CMD		INT (__cdecl *)(LPVOID, Tcl_Interp *, INT, Tcl_Obj *const *)

#define INITIAL_VIRTUAL_DIR_ENTRIES  110

#define TCL_USERFILE_OPEN	 0001
#define TCL_USERFILE_LOCK	 0002
#define TCL_GROUPFILE_OPEN	 0010
#define TCL_GROUPFILE_LOCK	 0020
#define TCL_MOUNTFILE_OPEN	 0100
#define TCL_THEME_CHANGED    0200
#define TCL_VIRTUAL_EVENT    0400
#define TCL_HANDLE_LOCK     01000

#define TCL_REALPATH		1
#define TCL_REALDATAPATH	2
#define TCL_VIRTUALPATH		3
#define TCL_VIRTUALDATAPATH	4
#define TCL_TRANSFEROFFSET	5
#define TCL_SPEED		6
#define TCL_STATUS		7
#define TCL_LOGINTIME		8
#define TCL_TIMEIDLE		9
#define TCL_HOSTNAME		10
#define TCL_IDENT		11
#define TCL_IP			12
#define TCL_UID			13
#define TCL_CLIENTID		14
#define TCL_ACTION			15
#define TCL_DATAIP          16
#define TCL_DEVICE          17
#define TCL_SERVICE         18

#define SPINLOCK			0
#define SEMAPHORE			1
#define EVENT				2
#define TCL_MAX_WAITOBJECTS		100

typedef struct _TCL_WAITOBJECT
{
	struct _TCL_WAITOBJECT	*lpNext;
	struct _TCL_WAITOBJECT	*lpPrev;
	LONG volatile			lReferenceCount;
	DWORD					dwType;
	LPVOID volatile			lpData;
	TCHAR					tszName[1];

} TCL_WAITOBJECT, *LPTCL_WAITOBJECT;

typedef struct _TCL_WHO
{
	DWORD	dwType[32];
	DWORD	dwCount;
	INT	iOffset;

} TCL_WHO, * LPTCL_WHO;


typedef struct _TCL_DATA
{
	DWORD			  dwFlags;
	LPUSERFILE		  lpUserFile;
	LPGROUPFILE		  lpGroupFile;
	MOUNTFILE		  hMountFile;
	LPTCL_WAITOBJECT  lpWaitObject[TCL_MAX_WAITOBJECTS];
	DWORD			  dwWaitObject;
	TCL_WHO			  WhoData;
	LPOUTPUT_THEME    lpTheme;
	LPVIRTUALDIR      lpVirtualDir;
	LPFTPUSER         lpFtpUser;

} TCL_DATA, * LPTCL_DATA;


typedef struct _TCL_VARIABLE
{
	LPSTR       szTclStr;
	CHAR		szName[1];

} TCL_VARIABLE, * LPTCL_VARIABLE;



typedef struct _TCL_INTERPRETER
{
	LPVOID					lpInterp;
	LPEVENT_DATA			lpEventData;
	LPTCL_DATA				lpTclData;
	DWORD                   dwConfigCounter;
	DWORD                   dwUniqueId;
	struct _TCL_INTERPRETER	*lpNext;

} TCL_INTERPRETER, * LPTCL_INTERPRETER;


typedef struct _TCL_COMMAND
{
	LPSTR		szCommand;
	LPVOID		lpProc;

} TCL_COMMAND;


typedef struct _TCL_DIRLIST
{
	LPVOID   Interp;  // Tcl_Interp *
	INT      iResult;
	LPVOID   lpList;  // Tcl_Obj *
} TCL_DIRLIST, *LPTCL_DIRLIST;


#ifdef _UNICODE
#define Tcl_GetTString Tcl_GetUnicode
#define Tcl_SetTStringObj Tcl_SetUnicodeObj
#else
#define Tcl_GetTString Tcl_GetString
#define Tcl_SetTStringObj Tcl_SetStringObj
#endif


BOOL TclExecute(LPEVENT_DATA lpEventData, IO_STRING *Arguments);
BOOL TclExecute2(LPEVENT_DATA lpEventData, IO_STRING *Arguments, LPINT lpiResult, LPVIRTUALDIR lpVirtualDir);
BOOL Tcl_ModuleInit(VOID);
VOID Tcl_ModuleDeInit(VOID);
LPTCL_INTERPRETER Tcl_GetInterpreter(BOOL bCreate);

extern DWORD dwTclInterpreterTlsIndex;

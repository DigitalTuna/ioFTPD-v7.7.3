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


// an interval is 5 seconds
#define EXE_CHECK_INTERVAL		5000	// # of ms between checks
#define EXE_DIRECT_INTERVALS     6      // After 30 seconds, flush buffer of finished lines
#define EXE_INPUT_INTERVALS		18		// 90 seconds, but if no output and keep alive is off this is adjusted to 2 min



//	Structure passed to event module on initialization
typedef struct _EVENT_MODULE
{
	LPSTR					szName;
	LPVOID					(* lpGetProc)(LPSTR);
	BOOL					(* lpInstallEvent)(LPSTR, LPVOID);
	HMODULE					hModule;
	struct _EVENT_MODULE	*lpNext;

} EVENT_MODULE, * LPEVENT_MODULE;


//	Structure passed to event module proc
typedef struct _EVENT_DATA
{
	LPTSTR			tszPrefix;
	DWORD			dwPrefix;
	LPIOSOCKET		lpSocket;
	LPBUFFER		lpBuffer;
	LPVOID			lpData;
	DWORD			dwData;
	DWORD			dwFlags;
	VOID			(* lpCallbackProc)(LPVOID, BOOL);
	LPVOID			lpCallbackContext;

} EVENT_DATA, * LPEVENT_DATA;


typedef BOOL (__cdecl *EVENT_PROC_FUNC)(LPEVENT_DATA, IO_STRING *);



//	Internal structure
typedef struct _EVENT_PROC
{
	LPSTR	        szName;
	EVENT_PROC_FUNC	lpProc;

} EVENT_PROC, * LPEVENT_PROC;


//	Internal structure
typedef struct _Redirect_Command
{
	CHAR	    *Trigger;
	UCHAR	    lTrigger;
	INT		(* Command)(LPEVENT_DATA, IO_STRING *);

} Redirect_Command;


//	Suggested flags for event
#define EVENT_BUFFER		0001	//	Output to buffer
#define EVENT_PREFIX		0002	//	Append prefix to output
#define EVENT_NEWLINE		0004	//	Use newlines
#define	EVENT_DETACH		0100
#define EVENT_DIRECT		0xF0000000
#define	EVENT_ERROR			0200
#define	EVENT_SILENT		0400	//	No output


#define EVENT_BUFFER_SIZE   16*1024


typedef struct _REDIRECT
{
	// Actual overlapped structure must be first
	IOOVERLAPPED		Overlapped;
	LPIOOVERLAPPED      lpOverlapped; // pointer to overlapped struct to use
	BOOL volatile		bComplete;
	HANDLE				hHandle, hEvent;
	LPEVENT_DATA		lpEventData;
	TCHAR				pBuffer[16384];
	BUFFER				Output;
	DWORD volatile		dwBuffer, dwOffset, dwLastError, dwCounter;

} REDIRECT, *LPREDIRECT;


typedef struct _EVENT_COMMAND
{
	LPTSTR		tszCommand;
	LPTSTR		tszParameters;
	LPBUFFER	lpOutputBuffer;
	LPIOSOCKET	lpOutputSocket;
	LPTSTR		tszOutputPrefix;
	LPVOID		lpDataSource;
	DWORD		dwDataSource;

} EVENT_COMMAND, * LPEVENT_COMMAND;

BOOL RunEvent(LPEVENT_COMMAND lpCommandData);
BOOL RunTclEventWithResult(LPEVENT_DATA lpEventData, LPIO_STRING Arguments, LPINT lpiResult, LPVIRTUALDIR lpVirtualDir);
BOOL InstallEvent(LPSTR szName, EVENT_PROC_FUNC lpProc);
BOOL CreateAsyncPipe(LPHANDLE hRead, LPHANDLE hWrite, BOOL bNotAsync);
BOOL Event_Init(BOOL bFirstInitialization);
VOID Event_DeInit(VOID);

extern DWORD volatile dwConfigCounter;
extern LPTSTR  tszKeepAliveText;
extern DWORD   dwKeepAliveText;

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

//	Window messages
#define WM_KICK				(WM_USER + 6)
#define WM_KILL				(WM_USER + 15)
#define WM_PHANDLE			(WM_USER + 17)
#define	WM_SHMEM			(WM_USER + 101)
#define WM_PID                          (WM_USER + 18)
#define	WM_DATACOPY_SHELLALLOC		(WM_USER + 19)
#define	WM_DATACOPY_FREE		(WM_USER + 20)
#define WM_DATACOPY_FILEMAP		(WM_USER + 21)
#define WM_ACCEPTEX			(WM_USER + 22)
#define WM_ACCEPT			(WM_USER + 23)
#define WM_KNOCK            (WM_USER + 24)

#define WM_ASYNC_TEST       (WM_USER + 1022)
#define WM_ASYNC_CALLBACK   (WM_USER + 1023)

#define WM_MAX_NUM			(WM_USER + 1024)


//	Status flags
#define	S_DEAD		 0010	// client has been "killed" (but isn't actually dead yet, name a legacy issue)
#define S_SCRIPT     0020	// client has a script running
#define S_TERMINATE  0040	// client should terminate script, abandon child exe, or finish action right now

typedef struct _ONLINEDATA
{
	INT32		Uid;

	// declare this volatile so don't have to lock structure to test for S_TERMINATE
	volatile DWORD		dwFlags;
	TCHAR		tszServiceName[_MAX_NAME + 1];	// Name of service
	TCHAR		tszAction[64];					// User's last action

	ULONG		ulClientIp;
	USHORT		usClientPort;

	CHAR		szHostName[MAX_HOSTNAME];	// Hostname
	CHAR		szIdent[MAX_IDENT];		// Ident

	TCHAR		tszVirtualPath[_MAX_PWD + 1];	// Virtual path
	LPTSTR		tszRealPath;					// Real path
	DWORD		dwRealPath;

    // ioGUI2 / sitewho / etc need DWORD and updates instead of time_t...
	DWORD		dwOnlineTime;			// Session Time
	DWORD		dwIdleTickCount;		// Idle Time

	BYTE		bTransferStatus;		// (0 Inactive, 1 Upload, 2 Download, 3 List)
	USHORT      usDeviceNum;            // Index # of data device being used for data connection, 0 means non defined,
	                                    // it's only valid if 'OnlineData_Extra_Fields' is true in the .ini file
	ULONG		ulDataClientIp;
	USHORT		usDataClientPort;

	TCHAR		tszVirtualDataPath[_MAX_PWD + 1];
	LPTSTR		tszRealDataPath;
	DWORD		dwRealDataPath;

	DWORD		dwBytesTransfered;		// Bytes transferred during interval
	DWORD		dwIntervalLength;		// Milliseconds
	INT64		i64TotalBytesTransfered;	// Total bytes transferred during transfer

} ONLINEDATA, * PONLINEDATA;


HWND GetMainWindow(VOID);
BOOL InstallMessageHandler(DWORD dwMessage, LPVOID lpProc, BOOL bInstantOperation, BOOL bIgnoreShutdown);

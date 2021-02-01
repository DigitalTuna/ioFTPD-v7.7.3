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

#define	REMOVABLE			0001
#define	REMOVEALL			0004
#define	DENYALL				0010
#define	REMOVECURRENT			0020


#define	PRIMARY		0001
#define	SECONDARY	0002
#define	TERTIARY	0004
#define	QUATERNARY	0010
#define	QUINARY		0020
#define	SENARY		0040
#define	EXCLUSIVE	0100

#define CJOB_PRIMARY	00000001
#define CJOB_SECONDARY	00000002
#define CJOB_TERTIARY	00000004
#define CJOB_EXCLUSIVE	02000000
#define CJOB_INSTANT	01000000



typedef struct _CLIENTJOB
{
	DWORD				dwJobId, dwJobFlags, dwTimeOut;
	BOOL				(*lpJobProc)(LPVOID);
	VOID				(*lpCancelProc)(LPVOID);
	DWORD				(*lpTimerProc)(LPVOID, LPTIMER);
	LPVOID				lpContext;
	LPTIMER             lpTimer;
	HANDLE              hEvent;
	struct _CLIENTJOB  *lpNext, *lpExtended;

} CLIENTJOB, * PCLIENTJOB, * LPCLIENTJOB;




VOID SetJobFilter(DWORD hClient, DWORD dwFlags);
BOOL AddClientJob(DWORD hClient, DWORD dwJobId, DWORD dwJobFlags, DWORD dwJobTimeOut, BOOL (* lpJobProc)(LPVOID), VOID (*lpCancelProc)(LPVOID), DWORD (*lpTimerProc)(LPVOID, LPTIMER), LPVOID lpContext);
BOOL AddExclusiveClientJob(DWORD hClient, DWORD dwJobId, DWORD dwJobFlags, DWORD dwJobTimeOut, BOOL (* lpJobProc)(LPVOID), VOID (*lpCancelProc)(LPVOID), DWORD (*lpTimerProc)(LPVOID, LPTIMER), LPVOID lpContext);
BOOL AddClientJobTimer(DWORD hClient, DWORD dwJobId, DWORD dwJobTimeOut, DWORD (*lpTimerProc)(LPVOID, LPTIMER));
BOOL EndClientJob(DWORD hClient, DWORD dwJobId);


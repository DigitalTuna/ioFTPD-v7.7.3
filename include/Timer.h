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

typedef struct _TIMER
{
	UINT64			ui64DueTime;
	LONG volatile		lStatus;
	LPVOID			lpTimerProc;
	LPVOID			lpTimerContext;
	struct _TIMER	*lpNext;

} TIMER, * LPTIMER;


#define	TIMER_ACTIVE	0
#define	TIMER_INACTIVE	1
#define	TIMER_CANCEL	2
#define	TIMER_QUEUED	3
#define	TIMER_WAIT	4

BOOL Timer_Init(BOOL bFirstInitialization);
VOID Timer_DeInit(VOID);
LPTIMER StartIoTimer(LPTIMER lpTimer, LPVOID lpTimerProc, LPVOID lpTimerContext, DWORD dwTimeOut);
BOOL StopIoTimer(LPTIMER lpTimer, BOOL bInTimerProc);
BOOL DeleteIoTimer(LPTIMER lpTimer);
UINT WINAPI TimerThread(LPVOID lpVoid);

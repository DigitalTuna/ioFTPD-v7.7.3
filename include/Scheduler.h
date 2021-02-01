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

typedef struct _SCHEDULER_SET
{
	INT			Min;
	INT			Max;
	BOOLEAN		All;

} SCHEDULER_SET;




typedef struct _SCHEDULED_TASK
{
	LPTSTR					tszName;

	BYTE					Minute[8];
	SCHEDULER_SET			MinuteSet;

	BYTE					Hour[4];
	SCHEDULER_SET			HourSet;

	BYTE					DayOfWeek[1];
	SCHEDULER_SET			DayOfWeekSet;

	BYTE					DayOfMonth[5];
	SCHEDULER_SET			DayOfMonthSet;

	LPTSTR					tszCommandLine;
	BOOL					bFlagged;
	LONG volatile			lShareCount;
	FILETIME				Time;
	struct _SCHEDULED_TASK	*lpNext;

} SCHEDULED_TASK, * LPSCHEDULED_TASK;


typedef struct _DYNAMIC_TASK
{
	LPTSTR         tszName;
	LPTSTR         tszCommandLine;
	IO_STRING      Arguments;
	LPTIMER        lpTimer;

} DYNAMIC_TASK, *LPDYNAMIC_TASK;


BOOL Scheduler_Init(BOOL bFirstInitialization);
VOID Scheduler_DeInit(VOID);
VOID Scheduler_Shutdown(VOID);

extern LPSTR WeekDays[];
extern LPSTR WeekDay3[];
extern LPSTR Months[];

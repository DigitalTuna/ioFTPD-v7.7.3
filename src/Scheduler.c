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


// TODO BUG: The leap year calculation is off but ignoring it...

#include <ioFTPD.h>

LPSTR				WeekDays[]		= { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
LPSTR               WeekDay3[]      = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
LPSTR				Months[]		= { NULL, "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
BYTE				MonthDays[]		= { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static LPTIMER			 lpSchedulerTimer;
static LPSCHEDULED_TASK	*lpScheduledTasks;
static DWORD			 dwScheduledTasks, dwAllocatedScheduledTasks;


static DWORD            dwMaxDynamicTasks;
static DWORD            dwDynamicTasks;
static LPDYNAMIC_TASK  *lpDynamicTasks;


VOID SetBit(PBYTE pByte, DWORD dwBit)
{
	pByte[dwBit >> 3]	|= (BYTE)(1 << (dwBit & 0007));
}

BOOL IsBitSet(PBYTE pByte, register DWORD dwBit)
{
	return (pByte[dwBit >> 3] & (BYTE)(1 << (dwBit & 0007)));
}




BOOL IsLeapYear(DWORD dwYear)
{
	if (! (dwYear & 0003) && (dwYear % 1000)) return FALSE;
	return TRUE;
}


INT __cdecl CompareFileTime_Quick(LPSCHEDULED_TASK *lpItem1, LPSCHEDULED_TASK *lpItem2)
{
	register INT	iResult;

	if ((iResult = CompareFileTime(&lpItem1[0]->Time, &lpItem2[0]->Time))) return iResult;
	//	Compare item names
	return _tcscmp(lpItem1[0]->tszName, lpItem2[0]->tszName);
}





DWORD Get_MonthDays(LPSYSTEMTIME lpTime)
{
	if (lpTime->wMonth == 2 && ! IsLeapYear(lpTime->wYear)) return 29;

	return MonthDays[lpTime->wMonth - 1];
}



VOID SystemTime_IncrementMonth(LPSYSTEMTIME lpTime)
{
	if (++(lpTime->wMonth) > 12)
	{
		lpTime->wMonth	= 1;
		lpTime->wYear++;
	}
}


VOID SystemTime_IncrementDay(LPSYSTEMTIME lpTime)
{
	if (++(lpTime->wDayOfWeek) > 6) lpTime->wDayOfWeek	= 0;

	if (++(lpTime->wDay) > Get_MonthDays(lpTime))
	{
		lpTime->wDay	= 1;
		SystemTime_IncrementMonth(lpTime);
	}
}


VOID SystemTime_IncrementHour(LPSYSTEMTIME lpTime)
{
	if (++(lpTime->wHour) == 24)
	{
		lpTime->wHour	= 0;
		SystemTime_IncrementDay(lpTime);
	}
}


VOID SystemTime_IncrementMinute(LPSYSTEMTIME lpTime)
{
	if (++(lpTime->wMinute) == 60)
	{
		lpTime->wMinute	= 0;
		SystemTime_IncrementHour(lpTime);
	}
}









LPTSTR ReadSchedule(LPTSTR tszEventData, PBYTE pByte, SCHEDULER_SET *ArraySet, INT Min, INT Max)
{
	TCHAR	*tpCheck;
	INT		Value[2], Values, Set;

	if (! tszEventData) return NULL;

	ArraySet->Min	= Max;
	ArraySet->Max	= Min;

	for (Set = 0, Values = 0;;Values++)
	{
		//	Remove heading whitespaces
		while (_istspace(tszEventData[0])) tszEventData++;

		if (tszEventData[0] == _TEXT('-'))
		{
			if (Values == 2) return NULL;
			tszEventData++;
		}
		else
		{
			switch (Values)
			{
			case 1:
				Value[1]	= Value[0];
			case 2:
				for (;Value[0] <= Value[1];Value[0]++)
				{
					if (! IsBitSet(pByte, Value[0]))
					{
						if (Value[0] < ArraySet->Min) ArraySet->Min	= Value[0];
						if (Value[0] > ArraySet->Max) ArraySet->Max	= Value[0];

						SetBit(pByte, Value[0]);
						Set++;
					}
				}

				if (tszEventData[0] != _TEXT(','))
				{
					if (Set == Max - Min + 1) ArraySet->All	= TRUE;
					return tszEventData;
				}
				tszEventData++;
				Values	= 0;
			}
		}

		Value[Values]	= _tcstoul(tszEventData, &tpCheck, 10);

		if (tpCheck == tszEventData)
		{
			if (tpCheck[0] != _TEXT('*') || Values > 0) return NULL;
			Value[0]	= Min;
			Value[1]	= Max;
			Values		= 1;
			tpCheck++;
		}
		else if (Value[Values] < Min || Value[Values] > Max) return NULL;

		tszEventData	= tpCheck;
	}
}







BOOL Reset_Stats(LPSTR CommandLine)
{
	LPUSERFILE	 lpUserFile;
	SYSTEMTIME	 stTime;
	LPUSERSEARCH hUserSearch;
	LPSTR		 szResetDow, szResetDom;
	CHAR		 szSearch[2];
	BOOL		 bWeeklyReset, bMonthlyReset;
	INT			 iResetDow, iResetDom, i;

	GetSystemTime(&stTime);

	iResetDom	= 1;
	iResetDow	= 0;

	if ((szResetDow = Config_Get(&IniConfigFile, "Reset", "WeeklyReset", NULL, NULL)))
	{
		//	Get reset day of week
		for (i = 0 ; i < 7 ; i++)
		{
			if (! strnicmp(WeekDays[i], szResetDow, 3))
			{
				iResetDow	= i;
				break;
			}
		}
		//	Free memory
		Free(szResetDow);
	}

	if ((szResetDom = Config_Get(&IniConfigFile, "Reset", "MonthlyReset", NULL, NULL)))
	{
		//	Get reset day of month
		if ((iResetDom = strtol(szResetDom, NULL, 10)) < 1 ||
			iResetDom > 31) iResetDom	= 1;
		//	Free memory
		Free(szResetDom);
	}

	bWeeklyReset	= (stTime.wDayOfWeek == iResetDow ? TRUE : FALSE);
	bMonthlyReset	= (stTime.wDay == iResetDom ? TRUE : FALSE);
	CopyString(szSearch, "*");

	if ((hUserSearch = FindFirstUser(szSearch, &lpUserFile, NULL, NULL, NULL)))
	{
		do
		{
			//	Lock userfile
			if (! UserFile_Lock(&lpUserFile, 0))
			{
				//	Zero daily stats
				ZeroMemory(lpUserFile->DayUp, sizeof(INT64) * MAX_SECTIONS * 3);
				ZeroMemory(lpUserFile->DayDn, sizeof(INT64) * MAX_SECTIONS * 3);
				//	Zero weekly stats
				if (bWeeklyReset)
				{
					ZeroMemory(lpUserFile->WkUp, sizeof(INT64) * MAX_SECTIONS * 3);
					ZeroMemory(lpUserFile->WkDn, sizeof(INT64) * MAX_SECTIONS * 3);
				}
				//	Zero monthly stats
				if (bMonthlyReset)
				{
					ZeroMemory(lpUserFile->MonthDn, sizeof(INT64) * MAX_SECTIONS * 3);
					ZeroMemory(lpUserFile->MonthUp, sizeof(INT64) * MAX_SECTIONS * 3);
				}
				//	Unlock userfile
				UserFile_Unlock(&lpUserFile, 0);
			}
			//	Close userfile
			UserFile_Close(&lpUserFile, 0);

		} while (! FindNextUser(hUserSearch, &lpUserFile));
	}

	return FALSE;
}



DWORD DynamicProc(LPDYNAMIC_TASK lpTask)
{
	EVENT_DATA EventData;
	INT iResult;

	ZeroMemory(&EventData, sizeof(EventData));

	if (RunTclEventWithResult(&EventData, &lpTask->Arguments, &iResult, NULL))
	{
		Putlog(LOG_ERROR, _TEXT("Dynamic scheduler '%s' returned error.\r\n"), lpTask->tszName);
		iResult = 3600; // reschedule for 1 hour
	}
	if (iResult < 0) {
		Putlog(LOG_ERROR, _TEXT("Dynamic scheduler '%s' returned invalid delay.\r\n"), lpTask->tszName);
		iResult = 3600;
	}
	return iResult;
}



BOOL Dynamic_Add(LPTSTR tszName, LPTSTR tszCommandLine)
{
	LPDYNAMIC_TASK	    lpDynamic;
	LPIO_STRING         lpArgs;
	DWORD				dwName, dwCommandLine, dwDelay;

	if (! tszName ||
		! tszCommandLine) ERROR_RETURN(ERROR_MISSING_ARGUMENT, FALSE);

	dwName        = _tcslen(tszName);
	dwCommandLine = _tcslen(tszCommandLine);

	// struct + 2 strings all under 1 allocate so easy to free
	lpDynamic = (LPDYNAMIC_TASK) Allocate("DynamicTask", sizeof(DYNAMIC_TASK) + (dwName + dwCommandLine + 2)*sizeof(TCHAR));
	if (!lpDynamic) return FALSE;

	lpDynamic->tszName = (LPTSTR) &lpDynamic[1];
	_tcscpy_s(lpDynamic->tszName, dwName+1, tszName);

	lpDynamic->tszCommandLine = &lpDynamic->tszName[dwName+1];
	_tcscpy_s(lpDynamic->tszCommandLine, dwCommandLine+1, tszCommandLine);

	lpDynamic->lpTimer = 0;

	if (dwDynamicTasks >= dwMaxDynamicTasks)
	{
		dwMaxDynamicTasks += 10;

		lpDynamicTasks = (LPDYNAMIC_TASK *) ReAllocate(lpDynamicTasks, "DynamicTasks", sizeof(LPDYNAMIC_TASK) * dwMaxDynamicTasks);
		if (!lpDynamicTasks)
		{
			Free(lpDynamic);
			return FALSE;
		}
	}

	lpArgs = &lpDynamic->Arguments;
	ZeroMemory(lpArgs, sizeof(*lpArgs));

	if (SplitString(lpDynamic->tszCommandLine, lpArgs))
	{
		Free(lpDynamic);
		return FALSE;
	}
	if (AppendArgToString(lpArgs, lpDynamic->tszName))
	{
		FreeString(lpArgs);
		Free(lpDynamic);
		return FALSE;
	}

	lpDynamicTasks[dwDynamicTasks++] = lpDynamic;

	dwDelay = DynamicProc(lpDynamic);

	lpDynamic->lpTimer = StartIoTimer(NULL, DynamicProc, lpDynamic, dwDelay);

	return TRUE;
}






BOOL Scheduler_Queue_Add(LPSYSTEMTIME lpSystemTime, LPSCHEDULED_TASK lpTask)
{
	SYSTEMTIME	SystemTime;
	LPVOID		lpMemory;
	INT			Loop;

	if (lpSystemTime)
	{
		CopyMemory(&SystemTime, lpSystemTime, sizeof(SYSTEMTIME));
	}
	else GetSystemTime(&SystemTime);
	//	Increase time by one minute
	SystemTime_IncrementMinute(&SystemTime);

	for (Loop = 400;Loop;Loop--)
	{
		if (IsBitSet(lpTask->DayOfMonth, SystemTime.wDay) &&
			IsBitSet(lpTask->DayOfWeek, SystemTime.wDayOfWeek))
		{
			for (;SystemTime.wHour <= lpTask->HourSet.Max;SystemTime.wHour++)
			{
				if (IsBitSet(lpTask->Hour, SystemTime.wHour))
				{
					for (;SystemTime.wMinute <= lpTask->MinuteSet.Max;SystemTime.wMinute++)
					{
						if (IsBitSet(lpTask->Minute, SystemTime.wMinute))
						{
							if (dwAllocatedScheduledTasks == dwScheduledTasks)
							{
								lpMemory	= ReAllocate(lpScheduledTasks, "Scheduler:Queue", sizeof(LPSCHEDULED_TASK) * (dwAllocatedScheduledTasks + 50));
								if (! lpMemory) return TRUE;

								lpScheduledTasks	= (LPSCHEDULED_TASK *)lpMemory;
								dwAllocatedScheduledTasks	+= 50;
							}
							//	Convert system time to filetime
							SystemTimeToFileTime(&SystemTime, &lpTask->Time);
							//	Insert item to array
							if (QuickInsert(lpScheduledTasks, dwScheduledTasks, lpTask, (QUICKCOMPAREPROC) CompareFileTime_Quick))
							{
								return TRUE;
							}
							dwScheduledTasks++;
							return FALSE;
						}
					}
				}
				SystemTime.wMinute	= lpTask->MinuteSet.Min;
			}
		}

		SystemTime.wHour	= lpTask->HourSet.Min;
		SystemTime.wMinute	= lpTask->MinuteSet.Min;

		SystemTime_IncrementDay(&SystemTime);
	}

	return TRUE;
}







/*

  Minutes		[0 - 59]	
  Hours			[0 - 23]	
  Day Of Week	[0 - 6]		0,1-3
  Day Of Month	[1 - 31]	

  */
BOOL Scheduler_Add(LPTSTR tszName, LPTSTR tszCommandLine)
{
	LPSCHEDULED_TASK	lpTask;
	SCHEDULED_TASK		Task;
	DWORD				dwName, dwCommandLine, n;

	if (! tszName ||
		! tszCommandLine) ERROR_RETURN(ERROR_MISSING_ARGUMENT, FALSE);

	//	Find task by name
	lpTask	= NULL;
	for (n = 0;n < dwScheduledTasks;n++)
	{
		if (! _tcscmp(lpScheduledTasks[n]->tszName, tszName))
		{
			lpTask	= lpScheduledTasks[n];
			break;
		}
	}

	ZeroMemory(&Task, sizeof(SCHEDULED_TASK));
	//	Read schedule
	tszCommandLine	= ReadSchedule(tszCommandLine, Task.Minute, &Task.MinuteSet, 0, 59);
	tszCommandLine	= ReadSchedule(tszCommandLine, Task.Hour, &Task.HourSet, 0, 23);
	tszCommandLine	= ReadSchedule(tszCommandLine, Task.DayOfMonth, &Task.DayOfMonthSet, 1, 31);
	tszCommandLine	= ReadSchedule(tszCommandLine, Task.DayOfWeek, &Task.DayOfWeekSet, 0, 6);

	if (! tszCommandLine) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, FALSE);

	//	Compare new task with existing task
	if (lpTask)
	{
		if (! memcmp(Task.Minute, lpTask->Minute, sizeof(Task.Minute)) &&
			! memcmp(Task.Hour, lpTask->Hour, sizeof(Task.Hour)) &&
			! memcmp(Task.DayOfMonth, lpTask->DayOfMonth, sizeof(Task.DayOfMonth)) &&
			! memcmp(Task.DayOfWeek, lpTask->DayOfWeek, sizeof(Task.DayOfWeek)) &&
			! _tcsicmp(tszCommandLine, lpTask->tszCommandLine))
		{
			lpTask->bFlagged	= FALSE;
			return TRUE;
		}
	}

	dwName	= _tcslen(tszName);
	dwCommandLine	= _tcslen(tszCommandLine);
	//	Allocate continous memory for scheduler item
	lpTask	= (LPSCHEDULED_TASK)Allocate("Scheduler:Item", sizeof(SCHEDULED_TASK) + (dwName + dwCommandLine + 2) * sizeof(TCHAR));
	if (! lpTask) return FALSE;

	//	Initialize structure
	CopyMemory(lpTask, &Task, sizeof(SCHEDULED_TASK));
	lpTask->lShareCount	= 1;
	lpTask->tszName	= (LPTSTR)&lpTask[1];
	lpTask->tszCommandLine	= &lpTask->tszName[dwName + 1];
	_tcscpy(lpTask->tszName, tszName);
	_tcscpy(lpTask->tszCommandLine, tszCommandLine);

	if (! Scheduler_Queue_Add(NULL, lpTask)) return TRUE;
	Free(lpTask);

	return FALSE;
/*

	if (! szArgs || ! szName || ! (nLength = strlen(szName)) ||
		! (lpTask = (LPSCHEDULED_TASK)Allocate("Scheduler:Item", sizeof(SCHEDULED_TASK)))) return FALSE;

	ZeroMemory(lpTask, sizeof(SCHEDULED_TASK));
	//	Get Time Ranges

	szArgs	= Get_Time_Ranges(szArgs, lpTask->Minute, &lpTask->MinuteSet, 0, 59);
	szArgs	= Get_Time_Ranges(szArgs, lpTask->Hour, &lpTask->HourSet, 0, 23);
	szArgs	= Get_Time_Ranges(szArgs, lpTask->DayOfMonth, &lpTask->DayOfMonthSet, 1, 31);
	szArgs	= Get_Time_Ranges(szArgs, lpTask->DayOfWeek, &lpTask->DayOfWeekSet, 0, 6);

	if (! szArgs ||
		lpTask->MinuteSet.Min > lpTask->MinuteSet.Max ||
		lpTask->HourSet.Min > lpTask->HourSet.Max ||
		lpTask->DayOfWeekSet.Min > lpTask->DayOfWeekSet.Max ||
		lpTask->DayOfMonthSet.Min > lpTask->DayOfMonthSet.Max ||
		! (cLength = strlen(szArgs)) ||
		! (lpMemory = Allocate("Scheduler:Item:Data", cLength + nLength + 2)) )
	{
		Free(lpTask);
		return TRUE;
	}

	lpTask->szName		= (PCHAR)lpMemory;
	lpTask->szCommand	= &((PCHAR)lpMemory)[nLength + 1];

	CopyMemory(lpTask->szName, szName, nLength + 1);
	CopyMemory(lpTask->szCommand, szArgs, cLength + 1);

	bReturn	= Scheduler_Queue_Add(NULL, lpTask);

	if (bReturn)
	{
		Free(lpTask);
		Free(lpMemory);
	}
	return bReturn; */
}



BOOL SchedulerJobProc(LPSCHEDULED_TASK lpTask)
{
	EVENT_COMMAND	Event;

	if (lpTask->tszCommandLine[0] == _TEXT('&'))
	{
		//	Internal command
		if (! _tcsicmp(&lpTask->tszCommandLine[1], _TEXT("Reset")))
		{
			Reset_Stats(NULL);
		}
		else if (! _tcsicmp(&lpTask->tszCommandLine[1], _TEXT("ConfigUpdate")))
		{
			Services_Init(FALSE);
		}
		else if (! _tcsicmp(&lpTask->tszCommandLine[1], _TEXT("PreLoad")))
		{
			PreLoad_VFS(FALSE);
		}
	}
	else
	{
		//	External event
		ZeroMemory(&Event, sizeof(EVENT_COMMAND));
		Event.tszCommand	= lpTask->tszCommandLine;
		if (RunEvent(&Event))
		{
			Putlog(LOG_ERROR, _TEXT("Scheduler event '%s' returned error.\r\n"), lpTask->tszName);
		}
	}
	if (! InterlockedDecrement(&lpTask->lShareCount)) Free(lpTask);
	return FALSE;
}





DWORD SchedulerProc(LPVOID lpNull, LPTIMER lpTimer)
{
	SYSTEMTIME			SystemTime;
	FILETIME			FileTime;
	LPSCHEDULED_TASK	lpFirstTask, lpTask;
	UINT64				CurrentTime, NextEvent;
	DWORD				dwReturn, dwShift;

	dwShift		= 0;
	dwReturn	= 0;
	lpFirstTask	= NULL;

	if (! dwScheduledTasks) return 0;

	//	Get system time
	GetSystemTimeAsFileTime(&FileTime);
	FileTimeToSystemTime(&FileTime, &SystemTime);
	//	Copy time to int64
	CopyMemory(&CurrentTime, &FileTime, sizeof(UINT64));
	CurrentTime	>>= 12;

	do
	{
		CopyMemory(&NextEvent, &lpScheduledTasks[dwShift]->Time, sizeof(UINT64));
		//	Compare filetimes
		if (CurrentTime < (NextEvent >> 12)) break;

		//	Execute scheduled event
		InterlockedIncrement(&lpScheduledTasks[dwShift]->lShareCount);
		if (QueueJob(SchedulerJobProc, lpScheduledTasks[dwShift], JOB_PRIORITY_LOW))
		{
			// item never ran so we know we can decrement safely without freeing here
			InterlockedDecrement(&lpScheduledTasks[dwShift]->lShareCount);
			break;
		}

		if (! lpFirstTask)
		{
			lpTask	= lpScheduledTasks[0];
			lpFirstTask	= lpScheduledTasks[0];
		}
		else
		{
			lpTask->lpNext	= lpScheduledTasks[dwShift];
			lpTask	= lpTask->lpNext;
		}

	} while (++dwShift < dwScheduledTasks);

	//	Shift items
	if (dwShift)
	{
		dwScheduledTasks	-= dwShift;
		MoveMemory(lpScheduledTasks,
			&lpScheduledTasks[dwShift], dwScheduledTasks * sizeof(LPSCHEDULED_TASK));

		for (lpTask = lpFirstTask;dwShift--;lpTask = lpTask->lpNext)
		{
			//	Add item back to queue
			Scheduler_Queue_Add(&SystemTime, lpTask);
		}
	}

	//	Get system time
	GetSystemTimeAsFileTime(&FileTime);
	//	Copy time to int64
	CopyMemory(&CurrentTime, &FileTime, sizeof(UINT64));
	CopyMemory(&NextEvent, &lpScheduledTasks[0]->Time, sizeof(UINT64));

	if (NextEvent > CurrentTime)
	{
		//	Calculate time to next event
		dwReturn	= (DWORD)((NextEvent - CurrentTime) / 10000);
	}

	return (dwReturn ? dwReturn : 1);
}






BOOL Scheduler_Init(BOOL bFirstInitialization)
{
	LPDYNAMIC_TASK lpDynamicTask;
	LPVOID	lpOffset;
	TCHAR	pBuffer[_INI_LINE_LENGTH + 1], tszName[_MAX_NAME + 1];
	DWORD	n, i;

	if (bFirstInitialization)
	{
		lpSchedulerTimer				= NULL;
		lpScheduledTasks			= NULL;
		dwScheduledTasks			= 0;
		dwAllocatedScheduledTasks	= 0;

		lpDynamicTasks    = NULL;
		dwDynamicTasks    = 0;
		dwMaxDynamicTasks = 0;
	}
	else
	{
		//	Stop timer and flag pending events
		StopIoTimer(lpSchedulerTimer, FALSE);
		lpSchedulerTimer	= NULL;
		for (n = 0;n < dwScheduledTasks;n++) lpScheduledTasks[n]->bFlagged	= TRUE;

		// just cancel dynamic events and start over - probably only 1 anyway.
		for (n = 0; n < dwDynamicTasks ; n++)
		{
			lpDynamicTask = lpDynamicTasks[n];
			if (lpDynamicTask->lpTimer)
			{
				StopIoTimer(lpDynamicTask->lpTimer, FALSE);
			}
			FreeString(&lpDynamicTask->Arguments);
			Free(lpDynamicTask);
		}
		dwDynamicTasks = 0;
	}

	//	Read Scheduler entries from config
	for (lpOffset = NULL;Config_Get_Linear(&IniConfigFile, _TEXT("Scheduler"), tszName, pBuffer, &lpOffset);)
	{
		//	And new scheduler events
		if (Scheduler_Add(tszName, pBuffer))
		{
			//	Output to error log
		}
	}

	//	Delete flagged events
	for (n = 0;n < dwScheduledTasks;n++)
	{
		if (lpScheduledTasks[n]->bFlagged)
		{
			for (i = n;i < dwScheduledTasks && lpScheduledTasks[i]->bFlagged;i++)
			{
				if (! InterlockedDecrement(&lpScheduledTasks[i]->lShareCount))
				{
					Free(lpScheduledTasks[i]);
				}
			}
			MoveMemory(&lpScheduledTasks[n], &lpScheduledTasks[i], (dwScheduledTasks - i) * sizeof(LPSCHEDULED_TASK));
			dwScheduledTasks	-= i - n;
		}
	}

	if (dwScheduledTasks)
	{
		//	Launch timer
		lpSchedulerTimer	= StartIoTimer(NULL, SchedulerProc, NULL, 1);
		if (! lpSchedulerTimer) return FALSE;
	}

	for (lpOffset = NULL;Config_Get_Linear(&IniConfigFile, _TEXT("Dynamic_Scheduler"), tszName, pBuffer, &lpOffset);)
	{
		Dynamic_Add(tszName, pBuffer);
	}

	return TRUE;
}



VOID Scheduler_DeInit(VOID)
{
	//	Free resources
	if (lpSchedulerTimer) StopIoTimer(lpSchedulerTimer, FALSE);
	while (dwScheduledTasks--)
	{
		Free(lpScheduledTasks[dwScheduledTasks]);
	}
	Free(lpScheduledTasks);
}


// similar to _DeInit but called early during shutdown to take advantage
// of the grace periods during service shutdown and prevent new tasks from
// being started.
VOID Scheduler_Shutdown(VOID)
{
	if (lpSchedulerTimer)
	{
		StopIoTimer(lpSchedulerTimer, FALSE);
		lpSchedulerTimer = NULL;
	}
}

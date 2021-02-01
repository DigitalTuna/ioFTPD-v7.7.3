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

#include <ioFTPD.h>


// LOG <timestamp> <whata> <who> <arg1> <arg2>
//
//		WHAT	WHO							ARG1	ARG2
//
//		LOGIN	 "<user>:<group>:<tagline>" "<host>" "<type>"			<done ftp/telnet, host not ok yet>
//		LOGOUT	 "<user>:<group>:<tagline>" "<host>" "<type>:<reason>"	<done ftp/telnet, host/reason not ok yet>
//		NEWDIR	 "<user>:<group>" "<path>"								<done>
//		DELDIR	 "<user>:<group>" "<path>"								<done>
//		ADDUSER	 "<user>" "<user>[:<group>]"
//		RENUSER  "<user>" "<user>"
//		DELUSER  "<user>" "<user>"
//		GRPADD	 "<user>" "<group>:<group long name>"
//		GRPREN	 "<user>" "<group>:<group long name>" "<group>"
//		GRPDEL	 "<user>" "<group>:<group long name>"
//		ADDIP	 "<user>" "<user>" "<ip>"
//		DELIP	 "<user>" "<user>" "<ip>"
//		CHANGE	 "<user>" "<user>" "<change what>:<old value>:<new value>"
//		KICK	 "<user>" "<user>" "<reason>"



static SYSTEMTIME			LocalTime;
static UINT64				FileTime[2];
static volatile LPLOG_VECTOR		lpFirstLogItem, lpLastLogItem, lpLogWriteQueue;
static DWORD				dwLogVectorLength, dwLogVectorHistory;
static CRITICAL_SECTION	csLogLock;
static volatile LONG        lQueueLogEntries;
static LPTSTR               tszLogFileNameArray[LOG_TOTAL_TYPES];



BOOL
LogSystem_Init(BOOL bFirstInitialization)
{
	if (! bFirstInitialization) return TRUE;

	tszLogFileNameArray[LOG_ERROR]    = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Log_Files"), _TEXT("Error.log"), NULL);
	tszLogFileNameArray[LOG_TRANSFER] = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Log_Files"), _TEXT("xferlog"), NULL);
	tszLogFileNameArray[LOG_SYSOP]    = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Log_Files"), _TEXT("SysOp.log"), NULL);
	tszLogFileNameArray[LOG_GENERAL]  = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Log_Files"), _TEXT("ioFTPD.log"), NULL);
	tszLogFileNameArray[LOG_SYSTEM]   = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Log_Files"), _TEXT("SystemError.log"), NULL);
	tszLogFileNameArray[LOG_DEBUG]    = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Log_Files"), _TEXT("Debug.log"), NULL);

	//	Reset vector
	lpFirstLogItem		= NULL;
	lpLastLogItem		= NULL;
	lpLogWriteQueue		= NULL;
	dwLogVectorLength	= 0;
	dwLogVectorHistory	= 0;
	ZeroMemory(&FileTime, sizeof(UINT64) * 2);

	//	Initialize vector lock
	if (! InitializeCriticalSectionAndSpinCount(&csLogLock, 50)) return FALSE;

	return TRUE;
}


VOID LogSystem_Flush()
{
	DWORD dwTickCount;

	// make sure existing log writes are flushed
	EnterCriticalSection(&csLogLock);
	dwTickCount = GetTickCount() + 1000;
	while (lpLogWriteQueue && GetTickCount() < dwTickCount)
	{
		LeaveCriticalSection(&csLogLock);
		SleepEx(100, TRUE);
		EnterCriticalSection(&csLogLock);
	}
	LeaveCriticalSection(&csLogLock);

}



VOID LogSystem_DeInit()
{
	LPLOG_VECTOR	lpLogItem;
	DWORD n;

	Putlog(LOG_GENERAL, _TEXT("STOP: \"PID=%u\"\r\n"), GetCurrentProcessId());

	// make sure log writes are flushed
	LogSystem_Flush();

	//	Delete vector lock
	DeleteCriticalSection(&csLogLock);
	//	Free memory
	for (;lpLogItem = lpFirstLogItem;)
	{
		lpFirstLogItem	= lpLogItem->lpNext;
		Free(lpLogItem);
	}

	for(n=0;n<LOG_TOTAL_TYPES;n++)
	{
		Free(tszLogFileNameArray[n]);
	}

}


VOID LogSystem_NoQueue(VOID)
{
	// make new log entries print directly to file
	lQueueLogEntries = 0;

	// make sure log writes are flushed
	LogSystem_Flush();
}


BOOL
LogSystem_Queue(BOOL bFirstInitialization)
{
	if (! bFirstInitialization) return TRUE;

	lQueueLogEntries = 1;
	return TRUE;
}


VOID FormatLogEntry(LPLOG_VECTOR lpLogItem)
{
	SYSTEMTIME   SystemTime;
	HANDLE       FileHandle;
	DWORD		 dwBytesWritten, dwTimeBufferLength;
	TCHAR		 tpTimeBuffer[128];

	//	Get filehandle
	FileHandle	= (HANDLE)lpLogItem->lpPrevious;

	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		//	Convert filetime to systemtime
		CopyMemory(&FileTime[0], &lpLogItem->FileTime, sizeof(UINT64));
		if (FileTime[0] - FileTime[1] > 10000000)
		{
			FileTime[1]	= FileTime[0];
			FileTimeToSystemTime(&lpLogItem->FileTime, &SystemTime);
			SystemTimeToLocalTime(&SystemTime, &LocalTime);
		}

		//	Seek to end of file
		SetFilePointer(FileHandle, 0, 0, FILE_END);
		//	Format timestamp
		if (lpLogItem->dwLogCode != LOG_TRANSFER)
		{
			dwTimeBufferLength	= wsprintf(tpTimeBuffer, _TEXT("%02d-%02d-%04d %02d:%02d:%02d "),
				LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
				LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond);
		}
		else
		{
			dwTimeBufferLength	= wsprintf(tpTimeBuffer, _TEXT("%.3s %.3s %2d %02d:%02d:%02d %4d "),
				WeekDays[LocalTime.wDayOfWeek], Months[LocalTime.wMonth], LocalTime.wDay,
				LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wYear);
		}
		//	Write buffers to file
		if (! WriteFile(FileHandle, tpTimeBuffer, dwTimeBufferLength * sizeof(TCHAR), &dwBytesWritten, NULL) ||
			! WriteFile(FileHandle, lpLogItem->lpMessage, lpLogItem->dwMessageLength, &dwBytesWritten, NULL))
		{
			//	Write failed
		}
		CloseHandle(FileHandle);
	}
}




BOOL WriteLog(LPLOG_VECTOR lpQueueOffset)
{
	LPLOG_VECTOR	lpLogItem;

	for (;(lpLogItem = lpQueueOffset);)
	{
		FormatLogEntry(lpLogItem);

		EnterCriticalSection(&csLogLock);
		//	Pop list
		lpQueueOffset	= lpQueueOffset->lpNext;
		if (! lpQueueOffset) lpLogWriteQueue	= NULL;

		//	Add item to log vector
		dwLogVectorLength++;
		dwLogVectorHistory++;

		if (lpLastLogItem)
		{
			//	Append to vector
			lpLogItem->lpNext		= NULL;
			lpLogItem->lpPrevious	= lpLastLogItem;
			lpLastLogItem->lpNext	= lpLogItem;
			lpLastLogItem			= lpLogItem;
			//	Check if log limit has been reached
			if (dwLogVectorLength > MAX_LOGS)
			{
				//	Remove first item from memory
				lpFirstLogItem	= lpFirstLogItem->lpNext;
				Free(lpFirstLogItem->lpPrevious);
				lpFirstLogItem->lpPrevious	= NULL;
				dwLogVectorLength--;
			}
		}
		else
		{
			//	Vector is empty
			lpLogItem->lpNext		= NULL;
			lpLogItem->lpPrevious	= NULL;
			lpFirstLogItem			= lpLogItem;
			lpLastLogItem			= lpLogItem;
		}
		LeaveCriticalSection(&csLogLock);
	}
	return FALSE;
}



BOOL PutlogVA(DWORD dwLogCode, LPCTSTR tszFormatString, va_list Arguments)
{
	LPLOG_VECTOR	lpLogItem;
	LPTSTR			tszFileName;

	//	Allocate memory
	lpLogItem	= (LPLOG_VECTOR)Allocate("Log", sizeof(LOG_VECTOR));
	if (! lpLogItem) return TRUE;

	//	Initialize item contents
	GetSystemTimeAsFileTime(&lpLogItem->FileTime);
	lpLogItem->lpNext		= NULL;
	lpLogItem->dwLogCode	= dwLogCode;
	//	Format string to buffer, don't care if NULL terminated since we printing buffer based on length...
	lpLogItem->dwMessageLength	= _vsntprintf(lpLogItem->lpMessage, LOG_LENGTH, tszFormatString, Arguments);
	//	Check length for overflow
	if (lpLogItem->dwMessageLength == (DWORD)-1) 
	{
		// make sure it ends in \r\n so next message doesn't appear on same line
		lpLogItem->dwMessageLength	= LOG_LENGTH;
		lpLogItem->lpMessage[LOG_LENGTH-2] = _T('\r');
		lpLogItem->lpMessage[LOG_LENGTH-1] = _T('\n');
	}

	if (dwLogCode < LOG_TOTAL_TYPES)
	{
		tszFileName = tszLogFileNameArray[dwLogCode];
	}
	else
	{
		tszFileName = NULL;
	}

	//	Open logfile
	if (tszFileName)
	{
		lpLogItem->lpPrevious	= (LPLOG_VECTOR)CreateFile(tszFileName, GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
	}
	else lpLogItem->lpPrevious	= INVALID_HANDLE_VALUE;

	// hold lock to order events during shutdown
	EnterCriticalSection(&csLogLock);
	if (lQueueLogEntries)
	{
		//	Add item to write queue
		if (! lpLogWriteQueue)
		{
			//	New write queue
			lpLogWriteQueue	= lpLogItem;
			QueueJob(WriteLog, lpLogItem, JOB_PRIORITY_LOW);
		}
		else
		{
			//	Append to write queue
			lpLogWriteQueue->lpNext	= lpLogItem;
			lpLogWriteQueue	= lpLogItem;
		}
	}
	else
	{
		// startup/shutdown... don't queue stuff
		if (lpLogItem->lpPrevious)
		{
			FormatLogEntry(lpLogItem);
		}
		Free(lpLogItem);
	}
	LeaveCriticalSection(&csLogLock);

	return FALSE;
}


BOOL Putlog(DWORD dwLogCode, LPCTSTR tszFormatString, ...)
{
	BOOL     bReturn;
	va_list	 Arguments;

	va_start(Arguments, tszFormatString);
	bReturn = PutlogVA(dwLogCode, tszFormatString, Arguments);
	va_end(Arguments);
	return bReturn;
}



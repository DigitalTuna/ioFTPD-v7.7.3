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

#define WAIT_EVENT_INDEX  0
#define WAIT_THREAD_INDEX 1

typedef struct _MESSAGEPROC
{
	LPVOID	lpProc;
	BOOL	bInstantOperation;
	BOOL    bIgnoreShutdown;

} MESSAGEPROC, *LPMESSAGEPROC;


static LPCTSTR	tszDefaultClassName	= _TEXT("ioFTPD::MessageWindow");
static LPTSTR	tszClassName;
static HWND		hMessageWindow;
static HANDLE   hSingleMutex;
static HANDLE   hProcessArray[2]; // event, thread
static LPMESSAGEPROC volatile	lpMessageProc[1024];

static HANDLE          hLockupEvent;
static LONG volatile   lLastChecked;

HANDLE                 hRestartHeartbeat; // needed during shutdown

static HANDLE          hRestartEvent;
static HANDLE          hRestartProcess;

static BOOL volatile   bServiceTesting;

static DWORD           dwExitCode;


HWND GetMainWindow(VOID)
{
	return hMessageWindow;
}


BOOL InstallMessageHandler(DWORD dwMessageId, LPVOID lpProc, BOOL bInstantOperation, BOOL bIgnoreShutdown)
{
	if (dwMessageId < WM_USER) return FALSE;
	if (dwMessageId > WM_MAX_NUM) return FALSE;
	dwMessageId	-= WM_USER;
	if (lpMessageProc[dwMessageId]) return FALSE;

	//	Install message procedure
	lpMessageProc[dwMessageId]	= (LPMESSAGEPROC)Allocate("MessageProc", sizeof(MESSAGEPROC));
	if (! lpMessageProc[dwMessageId]) return FALSE;

	lpMessageProc[dwMessageId]->lpProc	= lpProc;
	lpMessageProc[dwMessageId]->bInstantOperation = bInstantOperation;
	lpMessageProc[dwMessageId]->bIgnoreShutdown	  = bIgnoreShutdown;

	return TRUE;
}



static
LRESULT CALLBACK MessageWindowProc(HWND hWindow, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	//	Custom procedure
	if (uMessage >= WM_USER &&
		uMessage < WM_MAX_NUM &&
		lpMessageProc[uMessage - WM_USER])
	{
		if ((dwDaemonStatus == DAEMON_ACTIVE) || (lpMessageProc[uMessage - WM_USER]->bIgnoreShutdown))
		{
			return ((LRESULT (__cdecl *)(WPARAM, LPARAM))lpMessageProc[uMessage - WM_USER]->lpProc)(wParam, lParam);
		}
		return 0;
	}
	//	Default window procedure
	return DefWindowProc(hWindow, uMessage, wParam, lParam);

}



static
BOOL MessageProcessor(LPMSG lpMessage)
{
	//	Process message
	DispatchMessage(lpMessage);
	Free(lpMessage);
	return FALSE;
}



UINT WINAPI LockupDetector(HINSTANCE *hInstance)
{
	TCHAR  szName[MAX_PATH];
	LONG   lTime;
	DWORD  dwCount;
	BOOL   bOnceValid, bReport, bChanged;
	INT    iNum, iActive, iOnline, iFailed;
	INT    iOldNum, iOldActive, iOldOnline, iOldFailed;

	// set this right away in case we get stuck the first time through...
	lTime = (LONG) time(NULL);
	InterlockedExchange(&lLastChecked, lTime);

	dwCount     = 0;
	bOnceValid  = FALSE;
	bReport     = TRUE;
	bChanged    = FALSE;
	while (1)
	{
		switch (WaitForSingleObjectEx(hLockupEvent, 10000, TRUE))
		{
		case WAIT_OBJECT_0:
			// we're exiting...
			CloseHandle(hLockupEvent);
			return 0;
		case WAIT_IO_COMPLETION:
			// we got an APC
			Putlog(LOG_DEBUG, _T("LockupDetector had an APC.\r\n"));
			continue;
		case WAIT_FAILED:
			Putlog(LOG_DEBUG, _T("LockupDetector failed wait.\r\n"));
			continue;
		}
		// must be timeout since we can't abandon events...

		// don't care if this works, just that it returns...
		GetModuleFileName(NULL, szName, sizeof(szName)/sizeof(*szName));

		// we only want to test the connect every minute or so...
		if (++dwCount > 5)
		{
			dwCount = 0;
			// don't really care about this either, just that it didn't lock up, but if we
			// can't connect to anything record that fact.  Services could be offline for a
			// short time while DNS address being updating, etc...
			bServiceTesting = TRUE;
			if ((dwDaemonStatus == DAEMON_ACTIVE) && !Services_Test(&iNum, &iActive, &iOnline, &iFailed))
			{
				// services are initialized if iActive set...
				if (!iOnline && bOnceValid && bReport)
				{
					// rut ro, this doesn't look good... try suiciding?
					Putlog(LOG_ERROR, _T("No services appear to be online! Defined=%d, Active=%d, Online=%d, Failed=%d!\r\n"),
						iNum, iActive, iOnline, iFailed);
					// reset this so we don't spam the error log...
					bReport = FALSE;
				}
				if (bOnceValid && ((iNum != iOldNum) || (iActive != iOldActive) || (iOnline != iOldOnline) || (iFailed != iOldFailed)))
				{
					Putlog(LOG_GENERAL, _T("SERVICES: Defined=%d Active=%d Online=%d Failed=%d\r\n"), iNum, iActive, iOnline, iFailed);
					iOldNum    = iNum;
					iOldActive = iActive;
					iOldOnline = iOnline;
					iOldFailed = iFailed;
					bReport = TRUE;
				}
				if (iOnline && !bOnceValid)
				{
					bOnceValid = TRUE;
					iOldNum    = iNum;
					iOldActive = iActive;
					iOldOnline = iOnline;
					iOldFailed = iFailed;
				}
				if (iActive && (iActive == iFailed))
				{
					// rut ro, this doesn't look good... try suiciding?
					Putlog(LOG_ERROR, _T("All services report failed status! Defined=%d, Active=%d, Online=%d, Failed=%d.  Exiting!\r\n"),
						iNum, iActive, iOnline, iFailed);
					// signal graceful shutdown, if it fails the watcher process will kill us...
					dwExitCode = ERROR_POSSIBLE_DEADLOCK;
					SetDaemonStatus(DAEMON_GRACE);
					return 0;
				}
			}
			bServiceTesting = FALSE;
		}

		lTime = (LONG) time(NULL);
		InterlockedExchange(&lLastChecked, lTime);

		// just loop...
	}
}




DWORD ProcessMessages(VOID)
{
	HANDLE hWaitArray[2];
	DWORD  dwCount, dwResult;
	LONG   lTest, lNow;
	BOOL   bReported, bDebugger;

	// event to wait on to know when to return from this function
	dwCount = 1;
	hWaitArray[0] = hProcessArray[WAIT_EVENT_INDEX];
	if (hRestartEvent != INVALID_HANDLE_VALUE)
	{
		dwCount = 2;
		hWaitArray[1] = hRestartProcess;
	}

	if (hRestartHeartbeat != INVALID_HANDLE_VALUE)
	{
		// send this immediately to indicate we should be watched
		SetEvent(hRestartHeartbeat);
	}

	bReported = FALSE;
	// this now just sits around until the server is shutdown
	while (1)
	{
		dwResult = WaitForMultipleObjectsEx(dwCount, hWaitArray, FALSE, 30000, TRUE);

		if (dwResult == WAIT_TIMEOUT)
		{
			lTest = lLastChecked;
			if (lTest)
			{
				// make sure other thread has run at least once...
				lNow = (LONG) time(NULL);
				if ((lTest < lNow) && (lNow - lTest > 60))
				{
					// stop checking, this isn't good...
					if (!IsDebuggerPresent() || (!CheckRemoteDebuggerPresent(GetCurrentProcess(), &bDebugger) || !bDebugger))
					{
						break;
					}
					if (!bReported)
					{
						bReported = TRUE;
						Putlog(LOG_DEBUG, _T("System detected loader lock / winsock lockup bug - debugger attached - ignoring!\r\n"));
					}
				}
				if (hRestartHeartbeat != INVALID_HANDLE_VALUE)
				{
					// report that we are still alive...
					SetEvent(hRestartHeartbeat);
				}
			}
			continue;
		}
		if (dwResult == WAIT_OBJECT_0)
		{
			// we're supposed to return as the server is shutting down, or we detected a lockup
			return dwExitCode;
		}
		if (dwResult == (WAIT_OBJECT_0 + 1))
		{
			// the watcher process died!
			Putlog(LOG_ERROR, _T("ioFTPD-Watcher process exited.\r\n"));
			// close process handle so windows can stop tracking it...
			CloseHandle(hRestartProcess);
			hRestartProcess = INVALID_HANDLE_VALUE;

			if (hRestartEvent != INVALID_HANDLE_VALUE)
			{
				// should always be true
				CloseHandle(hRestartEvent);
				hRestartEvent = INVALID_HANDLE_VALUE;
			}
			if (hRestartHeartbeat != INVALID_HANDLE_VALUE)
			{
				// should always be true
				CloseHandle(hRestartHeartbeat);
				hRestartHeartbeat = INVALID_HANDLE_VALUE;
			}
			// stop watching it now that we closed the handle...
			dwCount--;
			continue;
		}
		if (dwResult == WAIT_IO_COMPLETION)
		{
			// something weird going on if this happens... remote thread injection?
			Putlog(LOG_DEBUG, _T("Received APC IO completion in ProcessMessages...\r\n"));
			continue;
		}
		// there are no other valid cases...
	}

	if (!bServiceTesting)
	{
		Putlog(LOG_ERROR, _T("System detected loader lock compromised!  Terminating!\r\n"));
	}
	else
	{
		Putlog(LOG_ERROR, _T("System detected winsock compromised!  Terminating!\r\n"));
	}

	LogSystem_Flush();
	// stop new stuff from happening!
	dwExitCode     = ERROR_POSSIBLE_DEADLOCK;
	dwDaemonStatus = DAEMON_GRACE;

	if (hRestartEvent != INVALID_HANDLE_VALUE)
	{
		SetEvent(hRestartEvent);
	}

	// NotifyServiceOfShutdown(); // useless!
	return FALSE;
}



VOID SetDaemonStatus(DWORD dwStatus)
{
	dwDaemonStatus  = dwStatus;
	if ((dwStatus == DAEMON_GRACE) && (hProcessArray[WAIT_EVENT_INDEX] != INVALID_HANDLE_VALUE))
	{
		SetEvent(hProcessArray[WAIT_EVENT_INDEX]);
	}
}


UINT WINAPI CreateWindowAndProcessMessages(HINSTANCE *hInstance)
{
	LPMESSAGEPROC	lpProc;
	LPMSG			lpMessage;
	MSG				StackMessage;
	register UINT	lMessage;
	DWORD           dwError;

	//	Create window
	hMessageWindow	= CreateWindowEx(0, tszClassName,
		_TEXT("ioFTPD"), WS_OVERLAPPEDWINDOW, 1, 1, 1, 1, 0, 0, *hInstance, 0);

	if (!hMessageWindow)
	{
		return FALSE;
	}

	SetEvent(hProcessArray[WAIT_EVENT_INDEX]);

	lpMessage	= (LPMSG)Allocate("WindowMessage", sizeof(MSG));
	if (! lpMessage) lpMessage	= &StackMessage;

	while (1)
	{
		//	Get message from queue
		switch (GetMessage(lpMessage, NULL, 0, 0))
		{
		case 0:
			// WM_QUIT was received
			if (lpMessage != &StackMessage) Free(lpMessage);
			SetDaemonStatus(DAEMON_GRACE);
			return TRUE;

		case -1:
			dwError = GetLastError();
			Putlog(LOG_ERROR, "ProcessMessage() failure: %d\r\n", dwError);
			if (lpMessage != &StackMessage) Free(lpMessage);
			SetDaemonStatus(DAEMON_GRACE);
			return FALSE;
		default:
			lMessage	= lpMessage->message;
			if (lMessage == WM_CLOSE)
			{
				DestroyWindow(hMessageWindow);
				if (lpMessage != &StackMessage) Free(lpMessage);
				return TRUE;
			}
			if (lMessage >= WM_USER &&
				lMessage < WM_MAX_NUM &&
				(lpProc = lpMessageProc[lMessage - WM_USER]) &&
				! lpProc->bInstantOperation &&
				((dwDaemonStatus == DAEMON_ACTIVE) || (lpMessageProc[lMessage - WM_USER]->bIgnoreShutdown)))
			{
				//	Queue to message processing threads to be dispatched
				if (lpMessage != &StackMessage)
				{
					QueueJob(MessageProcessor,
						lpMessage, JOB_PRIORITY_NORMAL|JOB_FLAG_NOFIBER);
				}
				lpMessage	= (LPMSG)Allocate("WindowMessage", sizeof(MSG));
				if (! lpMessage) lpMessage	= &StackMessage;
			}
			else DispatchMessage(lpMessage);
		}
	}
	if (lpMessage != &StackMessage) Free(lpMessage);
	return TRUE;
}


BOOL
Start_Deadlock_Process()
{
	SECURITY_ATTRIBUTES  SecurityAttributes;
	PROCESS_INFORMATION  ProcessInformation;
	STARTUPINFO          StartUpInfo;
	HANDLE               hMyProcess, hFake;
	TCHAR                pBuffer[MAX_PATH*2];
	LPTSTR               tszLogDir;

	//  Reset memory
	ZeroMemory(&ProcessInformation, sizeof(PROCESS_INFORMATION));

	tszLogDir = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Log_Files"), _T(""), NULL);
	if (tszLogDir)
	{
		if (!SetEnvironmentVariable(_T("ioFTPD_LogDir"), tszLogDir))
		{
			Free(tszLogDir);
			return TRUE;
		}
		Free(tszLogDir);
	}

	SecurityAttributes.lpSecurityDescriptor  = NULL;
	SecurityAttributes.bInheritHandle        = TRUE;
	SecurityAttributes.nLength               = sizeof(SECURITY_ATTRIBUTES);
	if (!(hRestartEvent = CreateEvent(&SecurityAttributes, FALSE, FALSE, NULL)))
	{
		hRestartEvent = INVALID_HANDLE_VALUE;
		return FALSE;
	}
	if (!(hRestartHeartbeat = CreateEvent(&SecurityAttributes, FALSE, FALSE, NULL)))
	{
		CloseHandle(hRestartEvent);
		hRestartEvent = INVALID_HANDLE_VALUE;
		hRestartHeartbeat = INVALID_HANDLE_VALUE;
		return FALSE;
	}

	hFake = GetCurrentProcess();

	if (!DuplicateHandle(hFake, hFake, hFake, &hMyProcess, 0, TRUE, DUPLICATE_SAME_ACCESS))
	{
		CloseHandle(hRestartEvent);
		CloseHandle(hRestartHeartbeat);
		hRestartEvent = INVALID_HANDLE_VALUE;
		hRestartHeartbeat = INVALID_HANDLE_VALUE;
		return FALSE;
	}

	//  Set startup information
	GetStartupInfo(&StartUpInfo);
	StartUpInfo.dwFlags      = STARTF_USESHOWWINDOW;
	StartUpInfo.wShowWindow  = SW_HIDE;

	_stprintf(pBuffer, _T("\"%s\\ioFTPD-Watch.exe\" %d %d %d 60"), tszExePath, hMyProcess, hRestartEvent, hRestartHeartbeat);

	//  Create process
	if (CreateProcess(NULL, pBuffer, 0, 0, TRUE, 0, NULL, 0, &StartUpInfo, &ProcessInformation))
	{
		// don't need these anymore
		CloseHandle(hMyProcess);
		CloseHandle(ProcessInformation.hThread);
		hRestartProcess = ProcessInformation.hProcess;
		SetHandleInformation(hRestartEvent, HANDLE_FLAG_INHERIT, 0);
		SetHandleInformation(hRestartHeartbeat, HANDLE_FLAG_INHERIT, 0);
		return TRUE;
	}

	Putlog(LOG_ERROR, _T("Unable to start '%s': %d\r\n"), pBuffer, GetLastError());
	CloseHandle(hMyProcess);
	CloseHandle(hRestartEvent);
	CloseHandle(hRestartHeartbeat);
	hRestartEvent = INVALID_HANDLE_VALUE;
	hRestartHeartbeat = INVALID_HANDLE_VALUE;

	// don't make this a fatal error.
	return TRUE;
}




BOOL Windows_Init(BOOL bFirstInitialization, HINSTANCE *hInstance)
{
	WNDCLASSEX	WindowClass;
	DWORD       dwThreadId;
	HANDLE      hThread;
	BOOL        bRestartOnDeadlock;
	TCHAR       tszMutexName[_INI_LINE_LENGTH+7];

	if (! bFirstInitialization) return TRUE;

	//	Initialize variables
	hRestartProcess   = INVALID_HANDLE_VALUE;
	hRestartEvent     = INVALID_HANDLE_VALUE;
	hRestartHeartbeat = INVALID_HANDLE_VALUE;
	dwExitCode        = NO_ERROR;

	ZeroMemory(&lpMessageProc, sizeof(lpMessageProc));
	hMessageWindow	= NULL;
	tszClassName	= Config_Get(&IniConfigFile, _TEXT("Threads"), _TEXT("WindowName"), NULL, NULL);
	if (! tszClassName) tszClassName	= (LPTSTR)tszDefaultClassName;

	//	Setup Main window parameters
	ZeroMemory(&WindowClass, sizeof(WNDCLASSEX));
	WindowClass.cbSize			= sizeof(WNDCLASSEX);
	WindowClass.lpfnWndProc		= MessageWindowProc;
	WindowClass.hInstance		= *hInstance;
	WindowClass.lpszClassName	= tszClassName;

	_sntprintf_s(tszMutexName, sizeof(tszMutexName)/sizeof(*tszMutexName), _TRUNCATE, _T("Global\\%s"), tszClassName);

	if (!(hSingleMutex = CreateMutex(NULL, FALSE, tszMutexName)))
	{
		return FALSE;
	}

	switch (WaitForSingleObject(hSingleMutex, 0))
	{
	case WAIT_TIMEOUT:
		Putlog(LOG_ERROR, _T("PID=%u - Another ioFTPD with the same window name (%s) is running.  Exiting!\r\n"),
			GetCurrentProcessId(), tszClassName);
		SetLastError(ERROR_SINGLE_INSTANCE_APP);
	default:
		return FALSE;

	case WAIT_ABANDONED: 
	case WAIT_OBJECT_0: 
		// we're good
		break;
	}

	//	Register main window class
	if (! RegisterClassEx(&WindowClass)) return FALSE;

	hProcessArray[WAIT_EVENT_INDEX] = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hProcessArray[WAIT_EVENT_INDEX] == INVALID_HANDLE_VALUE) return FALSE;
	hProcessArray[WAIT_THREAD_INDEX] = CreateThread(NULL, 0, CreateWindowAndProcessMessages, hInstance, 0, &dwThreadId); 
	if (hProcessArray[WAIT_THREAD_INDEX] == INVALID_HANDLE_VALUE) return FALSE;

	hLockupEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hLockupEvent == INVALID_HANDLE_VALUE) return FALSE;

	// 60 seconds or so needed for new purify runs...
	switch (WaitForMultipleObjects(2, hProcessArray, FALSE, 60000))
	{
	case WAIT_OBJECT_0:
		// all is good, the event was signaled
		break;
	default:
		// timeout, thread exited, etc
		return FALSE;
	}
	
	// at this point event_index was created, set by CreateWindowAndProcessMessages new thread, and waited/cleared above...

	hThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)LockupDetector, 0, 0, &dwThreadId);
	if (hThread == INVALID_HANDLE_VALUE) return FALSE;
	//	Higher thread priority
	SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL);
	CloseHandle(hThread);

	if (Config_Get_Bool(&IniConfigFile, _T("Threads"), _T("Restart_On_Deadlock"), &bRestartOnDeadlock))
	{
		bRestartOnDeadlock = FALSE;
	}
	if (bRestartOnDeadlock)	return Start_Deadlock_Process();
	return TRUE;
}



VOID Windows_DeInit(VOID)
{
	DWORD n;

	//	Free resources
	if (!PostMessage(hMessageWindow, WM_CLOSE, 0, 0))
	{
		n = GetLastError();
	}
	if (WaitForSingleObject(hProcessArray[WAIT_THREAD_INDEX], 5000) != WAIT_OBJECT_0)
	{
		n = GetLastError();
	}

	if (hRestartEvent != INVALID_HANDLE_VALUE) CloseHandle(hRestartEvent);
	if (hRestartProcess != INVALID_HANDLE_VALUE) CloseHandle(hRestartProcess);
	if (hRestartHeartbeat != INVALID_HANDLE_VALUE) CloseHandle(hRestartHeartbeat);

	if (tszClassName != tszDefaultClassName) Free(tszClassName);
	CloseHandle(hSingleMutex);
	CloseHandle(hProcessArray[WAIT_EVENT_INDEX]);
	hProcessArray[WAIT_EVENT_INDEX] = INVALID_HANDLE_VALUE;
	CloseHandle(hProcessArray[WAIT_THREAD_INDEX]);
	hProcessArray[WAIT_THREAD_INDEX] = INVALID_HANDLE_VALUE;
	SetEvent(hLockupEvent);

	for (n=0;n<1024;n++)
	{
		if (lpMessageProc[n]) Free(lpMessageProc[n]);
	}
}

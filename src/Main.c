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
#include <pdh.h>

#define DEFAULT_CONFIG_FILE _T("ioFTPD.ini")

volatile DWORD    dwDaemonStatus;

//  Device function calls
static INT      InitPos;
static LPTSTR    tszCommandLine;
static HINSTANCE  ghInstance;
static HANDLE    hInitGuard;
static DWORD     dwMyPid;
// tszExeName points inside tszExePath to just beyond last \\ which is set to 0
TCHAR           *tszExeName; 
TCHAR            tszExePath[MAX_PATH];
static TCHAR     tszConfigFile[MAX_PATH];
DWORD            dwIoVersion[3];
UINT64           u64WindowsStartTime;
UINT64           u64FtpStartTime;

static SERVICE_STATUS          ServiceStatus; 
static SERVICE_STATUS_HANDLE   ServiceStatusHandle;

static DWORD bRunningAsService = FALSE;

typedef struct _INIT_TABLE
{
  LPSTR  szName;
  LPVOID  InitCommand;
  LPVOID  DeInitCommand;
  DWORD  dwContext;
  DWORD  dwLateStop;

} INIT_TABLE;


#define  INIT  BOOL (__cdecl *)(BOOL)
#define INITP  BOOL (__cdecl *)(BOOL, LPVOID)


static INIT_TABLE Init_Table[] =
{
  "Memory",          Memory_Init,          Memory_DeInit,          0, 3,
  "Time",            Time_Init,            NULL,                   0, 3,
  "IoProc",          IoProc_Init,          IoProc_DeInit,          0, 0,
  "Timer",           Timer_Init,           Timer_DeInit,           0, 1,
  "Config",          Config_Init,          Config_DeInit,          1, 3,
  "File",            File_Init,            File_DeInit,            0, 3,
  "LogSystem",       LogSystem_Init,       LogSystem_DeInit,       0, 3,
  "Thread",          Thread_Init,          Thread_DeInit,          0, 2,
  "Windows",         Windows_Init,         Windows_DeInit,         2, 0,
  "Debug",           Debug_Init,           Debug_DeInit,           0, 3,
  "Client",          Client_Init,          Client_DeInit,          0, 0,
  "DataCopy",        DataCopy_Init,        DataCopy_DeInit,        0, 0,
  "Security",        Security_Init,        Security_DeInit,        0, 0,
  "Socket",          Socket_Init,          Socket_DeInit,          0, 0,
  "Message",         Message_Init,         Message_DeInit,         0, 0,
  "Help",            Help_Init,            Help_DeInit,            0, 0,
  "MountFile",       MountFile_Init,       MountFile_DeInit,       0, 0,
  "DirectoryCache",  DirectoryCache_Init,  DirectoryCache_DeInit,  0, 0,
  "Group",           Group_Init,           Group_DeInit,           0, 3,
  "User",            User_Init,            User_DeInit,            0, 3,
  "Identify",        Identify_Init,        Identify_DeInit,        0, 0,
  "Event",           Event_Init,           Event_DeInit,           0, 3,
  "TransmitPackage", TransmitPackage_Init, TransmitPackage_DeInit, 0, 0,
  "FTP",             FTP_Init,             FTP_DeInit,             0, 1,
  "Services",        Services_Init,        Services_DeInit,        0, 0,
  "Scheduler",       Scheduler_Init,       Scheduler_DeInit,       0, 0,
// below are extra events we want to happen early during shutdown
  "Shutdown1",       NULL,				   Identify_Shutdown,      0, 0,
  "Shutdown2",       NULL,				   Scheduler_Shutdown,     0, 0,
  "Shutdown3",       LogSystem_Queue,      LogSystem_NoQueue,      0, 0
};


// dwExit = -1 implies GetLastError
void
ErrorExit(LPTSTR tszFunction, DWORD dwExit, LPTSTR format, ...)
{
	WCHAR   wszErrBuf[256];
	TCHAR   tszBuffer[1024];
	DWORD   dwErr, dwSize;
	int     iResult;
	LPTSTR  tszBuf;
	LPWSTR  wszErr;
	va_list argptr;


	dwErr = GetLastError();

	if (dwExit == -1)
	{
		dwExit = dwErr;
	}

	if (bRunningAsService)
	{
		ExitProcess(dwExit); 
	}

	va_start( argptr, format );

	wszErrBuf[0] = 0;
	wszErr = 0;
	if (dwErr != NO_ERROR)
	{
		dwSize = sizeof(wszErrBuf)/sizeof(wszErrBuf[0]);
		wszErr = FormatError(dwErr, wszErrBuf, &dwSize);
	}

	dwSize = sizeof(tszBuffer)/sizeof(tszBuffer[0]);

	tszBuf = tszBuffer;
	iResult = _sntprintf_s(tszBuf, dwSize, _TRUNCATE, _T("Function '%s' failed.\r\n"), tszFunction);

	if (iResult >= 0)
	{
		dwSize -= iResult;
		tszBuf += iResult;
	}
	else
	{
		dwSize = 0;
	}

	if (format && dwSize)
	{
		iResult = _vsntprintf_s(tszBuf, dwSize, _TRUNCATE, format, argptr );

		if (iResult >= 0)
		{
			dwSize -= iResult;
			tszBuf += iResult;
		}
		else
		{
			dwSize = 0;
		}
	}

	if (dwErr != NO_ERROR && dwSize && wszErr)
	{
		iResult = _sntprintf_s(tszBuf, dwSize, _TRUNCATE, _T("\r\nGetLastError: #%d (%ws)\r\n"), dwErr, wszErr);
		if (iResult >= 0)
		{
			dwSize -= iResult;
			tszBuf += iResult;
		}
		else
		{
			dwSize = 0;
		}
	}

	if (dwSize)
	{
		_sntprintf_s(tszBuf, dwSize, _TRUNCATE, _T("\r\nNOTE: More info might be available in logfiles."));
	}

	MessageBox(NULL, tszBuffer, _T("Error"), MB_OK|MB_ICONERROR); 

	ExitProcess(dwExit); 

	va_end( argptr );
}



VOID DummyEncode(PBYTE pIn, DWORD dwIn, PBYTE pOut, LPTSTR tszKey)
{
  DWORD  dwOffset, n;
  BYTE  pBuffer[20], pCopy[20];

  sha1(pBuffer, tszKey, _tcslen(tszKey));
  for (n = 0;n < 20;n++) pBuffer[n]  += (BYTE)((((n << 4) % 3) << 2) * 127);
  ZeroMemory(pOut, dwIn);
  //  Encode data
  for (dwOffset = 0;(dwIn -= 4) > 0;dwOffset++)
  {
    CopyMemory(pCopy, pBuffer, 20);
    sha1(pBuffer, pCopy, 20);
    ((LPDWORD)pOut)[0]  = ((LPDWORD)&pCopy[dwOffset % 16])[0] + ((LPDWORD)pIn)[0];
    pOut  = &pOut[4];
    pIn    = &pIn[4];
  }
}




VOID DummyDecode(PBYTE pIn, DWORD dwIn, PBYTE pOut, LPTSTR tszKey)
{
  DWORD  dwOffset, n, dwKey;
  BYTE  pBuffer[20], pCopy[20];

  dwKey  = _tcslen(tszKey);
  sha1(pBuffer, tszKey, dwKey);
  for (n = 0;n < 20;n++) pBuffer[n]  += (BYTE)((((n << 4) % 3) << 2) * 127);
  ZeroMemory(pOut, dwIn);
  //  Decode data
  for (dwOffset = 0;dwOffset < dwIn / 4;dwOffset++)
  {
    CopyMemory(pCopy, pBuffer, 20);
    sha1(pBuffer, pCopy, 20);
    ((LPDWORD)pOut)[0]  = ((LPDWORD)pIn)[0] - ((LPDWORD)&pCopy[dwOffset % 16])[0];
    pOut  = &pOut[4];
    pIn    = &pIn[4];
  }
}


BOOL InitializeDaemon(BOOL bFirstInitialization)
{
  ULONG  lOffset;
  LPVOID  lpParam;
  DWORD  n, dwError, dwSize;
  BOOL  bResult;
  WCHAR  *wszErr, wszErrBuf[256];

  if (bFirstInitialization)
  {
    InitPos  = 0;
    hInitGuard  = CreateEvent(NULL, FALSE, TRUE, NULL);
    if (! hInitGuard) return FALSE;
  }

  bResult  = TRUE;
  WaitForSingleObject(hInitGuard, INFINITE);

  if (dwDaemonStatus != DAEMON_ACTIVE)
  {
	  SetEvent(hInitGuard);
	  return FALSE;
  }

  //  Initialize daemon
  for (n = 0;n < sizeof(Init_Table) / sizeof(INIT_TABLE) && (! bFirstInitialization || bResult);n++)
  {
    lOffset  = (ULONG)Init_Table[n].InitCommand;
	if (!lOffset) continue;

    switch (Init_Table[n].dwContext)
    {
    case 0:
      bResult  = ((INIT)lOffset)(bFirstInitialization);
      continue;
    case 1:
      lpParam  = tszConfigFile;
      break;
    case 2:
      lpParam  = &ghInstance;
      break;
    }
    bResult  = ((INITP)lOffset)(bFirstInitialization, lpParam);
  }
  if (bFirstInitialization)
  {
	  InitPos  = n;
	  if (!bResult)
	  {
		  if (n > 0) n--;
		  if (n > 6)
		  {
			  // the log system is operational
			  dwError = GetLastError();
			  dwSize  = sizeof(wszErrBuf)/sizeof(*wszErrBuf);
			  wszErr = FormatError(dwError, wszErrBuf, &dwSize);
			  Putlog(LOG_ERROR, _T("Failed to initialize module '%s' (error: %ws)\r\n"), Init_Table[n].szName, wszErr);
			  // set the proper error code again as Putlog might have changed it.
			  SetLastError(dwError);
		  }
		  if (!bRunningAsService)
		  {
			  ErrorExit(_T("InitializeDaemon"), 1, _T("Failed to initialize module '%s'\r\n"), Init_Table[n].szName);
		  }
	  }
  }

  SetEvent(hInitGuard);

  return bResult;
}


/*

  DeInit() - Main DeInitialization routine

  */
VOID DaemonDeInitialize(VOID)
{
	DWORD dwPos, n;

	for (n=0; n<4; n++)
	{
		dwPos = InitPos;
		while (dwPos--)
		{
			if(Init_Table[dwPos].DeInitCommand && (Init_Table[dwPos].dwLateStop == n))
			{
				((VOID (__cdecl *)(VOID))Init_Table[dwPos].DeInitCommand)();
			}
		}
	}

	CloseHandle(hInitGuard);
}




BOOL
ServerStart(LPVOID lpNull)
{
  EVENT_COMMAND  Event;
  INT        i;

  ZeroMemory(&Event, sizeof(EVENT_COMMAND));
  //  Execute on server start event
  for (i = 0;(Event.tszCommand = Config_Get(&IniConfigFile, _TEXT("Events"), _TEXT("OnServerStart"), NULL, &i));)
  {
	  if (RunEvent(&Event))
	  {
		  Putlog(LOG_ERROR, _T("OnServerStart event '%s' return error.\r\n"), Event.tszCommand);
	  }
	  Free(Event.tszCommand);
  }
  // Mark the server as having been started by setting this to FALSE
  FtpSettings.bServerStartingUp = FALSE;
  return FALSE;
}



BOOL
ServerStop(LPVOID lpNull)
{
  EVENT_COMMAND Event;
  INT i;

  ZeroMemory(&Event, sizeof(EVENT_COMMAND));
  //  Execute on server stop event
  for (i=0;(Event.tszCommand=Config_Get(&IniConfigFile, _TEXT("Events"), _TEXT("OnServerStop"), NULL, &i));)
  {
	  if (RunEvent(&Event))
	  {
		  Putlog(LOG_ERROR, _T("OnServerStop event '%s' return error.\r\n"), Event.tszCommand);
	  }
	  Free(Event.tszCommand);
  }
  return FALSE;
}



INT
CommonMain()
{
	DWORD    dwError;
	BOOL     bDelay;

	Putlog(LOG_GENERAL, _TEXT("START: \"PID=%u\" \"CmdLine=%s\"\r\n"), dwMyPid, tszCommandLine);
	QueueJob(ServerStart, NULL, JOB_PRIORITY_HIGH);

	if (Config_Get_Bool(&IniConfigFile, _TEXT("VFS_PreLoad"), _TEXT("DELAY"), &bDelay) || !bDelay)
	{
		// it wasn't preloaded in DirCache init, so do it now as a low priority task
		QueueJob(PreLoad_VFS, (LPVOID) TRUE, JOB_PRIORITY_LOW);
	}

	dwError = ProcessMessages();

	QueueJob(ServerStop, NULL, JOB_PRIORITY_HIGH);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
	// service's module DeInit after forcing clients off sets DAEMON_SHUTDOWN status
	DaemonDeInitialize();
	// main() wants to return an int, services a DWORD... we can't win... non-negative anyway...
	return dwError;
}


VOID
NotifyServiceOfShutdown()
{
	// the stupid service manager never seems to honor the timeout and kill us if we miss it so
	// no point in setting this...
	if (bRunningAsService)
	{
		ServiceStatus.dwCurrentState  = SERVICE_STOP_PENDING;
		ServiceStatus.dwCheckPoint    = 0; 
		ServiceStatus.dwWaitHint      = 150000;
		SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
	}
}



VOID WINAPI
ServiceCtrlHandler(DWORD dwControl)
{
	switch (dwControl)
	{
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		SetDaemonStatus(DAEMON_GRACE);
		return;
	default: // including SERVICE_CONTROL_INTERROGATE:
		// just ignore it and fall through
		break;
	}
	SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
	return; 
}



BOOL
InitUptime()
{
	PDH_HQUERY           hQuery;
	PDH_HCOUNTER         hCounter;
	PDH_COUNTER_PATH_ELEMENTS pdhElems;
	PDH_FMT_COUNTERVALUE pdhValue;
	DWORD                dwSystemObjectName, dwUptimeCounterName, dwCounterPath, dwStatus;
	BOOL                 bResult = FALSE;
	TCHAR                tszSystemObjectName[80], tszUptimeCounterName[80], tszCounterPath[180];

	GetSystemTimeAsFileTime((FILETIME *) &u64FtpStartTime);

	hQuery   = NULL;
	hCounter = NULL;
	dwSystemObjectName = sizeof(tszSystemObjectName);
	if (dwStatus = PdhLookupPerfNameByIndex( NULL, 2, tszSystemObjectName, &dwSystemObjectName ))
	{
		goto FAIL;
	}

	dwUptimeCounterName = sizeof(tszUptimeCounterName);
	if (dwStatus = PdhLookupPerfNameByIndex( NULL, 674, tszUptimeCounterName, &dwUptimeCounterName ))
	{
		goto FAIL;
	}

	memset( &pdhElems, 0, sizeof(pdhElems) );
	pdhElems.szObjectName  = tszSystemObjectName;
	pdhElems.szCounterName = tszUptimeCounterName;
	dwCounterPath = sizeof(tszCounterPath);
	if (dwStatus = PdhMakeCounterPath( &pdhElems, tszCounterPath, &dwCounterPath, 0 ))
	{
		goto FAIL;
	}

	if (dwStatus = PdhOpenQuery(NULL, 0, &hQuery ))
	{
		goto FAIL;
	}

	if (dwStatus = PdhAddCounter(hQuery, tszCounterPath, 0, &hCounter ))
	{
		goto FAIL;
	}
	if (dwStatus = PdhCollectQueryData(hQuery))
	{
		goto FAIL;
	}

	if (dwStatus = PdhGetFormattedCounterValue( hCounter, PDH_FMT_LARGE , NULL, &pdhValue))
	{
		goto FAIL;
	}

	PdhRemoveCounter(hCounter);
	PdhCloseQuery(hQuery);
	hQuery = NULL;
	GetSystemTimeAsFileTime((FILETIME *) &u64WindowsStartTime);
	u64WindowsStartTime -= pdhValue.largeValue * 10000000;
	return FALSE;

FAIL:
	if (hCounter)
	{
		PdhRemoveCounter(hCounter);
	}
	if (hQuery)
	{
		PdhCloseQuery(hQuery);
	}

	// fake it... lets hope that the system hasn't been up more than 49.7 days and
	// that it hasn't been sleeping/hibernating :)
	GetSystemTimeAsFileTime((FILETIME *) &u64WindowsStartTime);
	pdhValue.largeValue = GetTickCount();
	u64WindowsStartTime -= pdhValue.largeValue * 10000;
	return TRUE;
}



void
DoSetup()
{
	DWORD                dwLen;
	BOOL                 bResult = FALSE;
	size_t               stLen;

	dwLen = GetModuleFileName(NULL, tszExePath, sizeof(tszExePath)/sizeof(TCHAR));
	if ((dwLen == 0) || (dwLen == (sizeof(tszExePath)/sizeof(TCHAR)) && GetLastError() == ERROR_INSUFFICIENT_BUFFER))
	{
		ErrorExit(_T("GetModuleFileName"), 1, NULL);
	}

	tszExeName = _tcsrchr(tszExePath, _T('\\'));
	if (! tszExeName)
	{
		ErrorExit(_T("WinMain"), 1, _T("Can't determine start directory\r\n"));
		exit(1);
	}
	stLen = ((size_t) ((char *) tszExeName - (char *) tszExePath))/sizeof(TCHAR);
	tszExeName++;

	GetFileVersion(tszExePath, &dwIoVersion[0], &dwIoVersion[1], &dwIoVersion[2], NULL);

	tszExePath[stLen] = 0;

	//  Change work directory
	if (! SetCurrentDirectory(tszExePath))
	{
		ErrorExit(_T("SetCurrentDirectory"), 1, NULL);
	}

	if ( ! tszCommandLine || ! *tszCommandLine )
	{
		if ((_sntprintf_s(tszConfigFile, sizeof(tszConfigFile)/sizeof(*tszConfigFile), _TRUNCATE, _T("%s\\%s"),
			tszExePath, DEFAULT_CONFIG_FILE)) < 0)
		{
			// just bail, not being able to read the config file is really bad...
			ErrorExit(_T("WinMain"), 1, _T("Fullpath to config file is too long\r\n"));
		}
	}
	else
	{
		stLen = _tcslen(tszCommandLine);
		if (stLen > sizeof(tszConfigFile)/sizeof(TCHAR))
		{
			ErrorExit(_T("WinMain"), 1, _T("Fullpath to config file is too long\r\n"));
			exit(1);
		}
		_tcscpy_s(tszConfigFile, sizeof(tszConfigFile)/sizeof(TCHAR), tszCommandLine);
	}


	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	SetProcessWorkingSetSize(GetCurrentProcess(), 1024 * 1024, 20 * 1024 * 1024);


	InitializeExceptionHandler(tszExePath);
	SetErrorMode(SetErrorMode(0)|SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);

	// determine the uptime
	InitUptime();

	SetDaemonStatus(DAEMON_ACTIVE);
}


VOID WINAPI
ServiceMain(DWORD dwArgc, LPTSTR *lptszArgv)
{
	ServiceStatus.dwServiceType        = SERVICE_WIN32_OWN_PROCESS; 
	ServiceStatus.dwCurrentState       = SERVICE_RUNNING;
	ServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	ServiceStatus.dwWin32ExitCode      = NO_ERROR;
	ServiceStatus.dwServiceSpecificExitCode = 0; 
	ServiceStatus.dwCheckPoint         = 0; 
	ServiceStatus.dwWaitHint           = 0; 

	ServiceStatusHandle = RegisterServiceCtrlHandler(_T("ioFTPD"), ServiceCtrlHandler);

	if (ServiceStatusHandle == (SERVICE_STATUS_HANDLE)0) 
	{
		// we failed, and no way to report error so just bail
		return;
	}

	bRunningAsService = TRUE;

	DoSetup();
	if (InitializeDaemon(TRUE))
	{
		if (SetServiceStatus(ServiceStatusHandle, &ServiceStatus))
		{
			// the next line won't return until the server stops
			ServiceStatus.dwWin32ExitCode = CommonMain();
			if (ServiceStatus.dwWin32ExitCode != NO_ERROR)
			{
				// NOTE: if we were to actually return an error by setting SERVICE_STOPPED
				// and passing a non-zero dwWin32ExitCode then the stupid service manager
				// would consider the server failed BUT won't bother to restart it if set
				// up to do so on crashes.
				// We have no choice but to exit without reporting our status...
				exit(1);
			}
		}
	}
	else
	{
		ServiceStatus.dwWin32ExitCode = GetLastError();
		if (!ServiceStatus.dwWin32ExitCode)
		{
			ServiceStatus.dwWin32ExitCode = ERROR_INVALID_FUNCTION;
		}
	}

	ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	ServiceStatus.dwCheckPoint    = 0; 
	ServiceStatus.dwWaitHint      = 0; 
	SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
} 



INT WINAPI
WinMain(HINSTANCE hInstance,
		HINSTANCE hPrevInstance,
		LPTSTR tszCmdLine,
		INT nCmdShow)
{
	SERVICE_TABLE_ENTRY  ServiceTable[2];

	ghInstance    = hInstance;
	tszCommandLine  = tszCmdLine;
	dwMyPid = GetCurrentProcessId();

	ZeroMemory(ServiceTable, sizeof(ServiceTable));
	ServiceTable[0].lpServiceName = _T("ioFTPD");
	ServiceTable[0].lpServiceProc = ServiceMain;
	if (!StartServiceCtrlDispatcher(ServiceTable))
	{
		if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
		{
			// guess we aren't a service...
			DoSetup();
			if (InitializeDaemon(TRUE))
			{
				exit(CommonMain());
			}
			ErrorExit(_T("WinMain"), 1, _T("Unspecified error in InitializeDaemon\r\n"));
		}
	}
	exit(0);
}

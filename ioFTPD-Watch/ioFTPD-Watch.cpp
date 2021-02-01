// ioFTPD-Watcher.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


static TCHAR tszLogFileName[MAX_PATH+1];


VOID WriteLogEntry(LPCSTR szFormatter, ...)
{
	CHAR		 szBuf[1024];
	SYSTEMTIME   LocalTime;
	va_list		 Arguments;
	HANDLE       hLogFile;
	int          iTimeLen, iStringLen;
	DWORD        dwWritten, dwError;

	va_start(Arguments, szFormatter);

	hLogFile = CreateFile(tszLogFileName, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
	if (hLogFile != INVALID_HANDLE_VALUE)
	{
		SetFilePointer(hLogFile, 0, 0, FILE_END);
		GetLocalTime(&LocalTime);

		iTimeLen = sprintf_s(szBuf, sizeof(szBuf), "%02d-%02d-%04d %02d:%02d:%02d ",
			LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
			LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond);
		if (iTimeLen > 0)
		{
			iStringLen = _vsnprintf_s(&szBuf[iTimeLen], sizeof(szBuf)-iTimeLen, _TRUNCATE, szFormatter, Arguments);
			if (iStringLen >= 0)
			{
				iStringLen += iTimeLen;  // count the NULL

				if (! WriteFile(hLogFile, szBuf, iStringLen, &dwWritten, NULL) || (dwWritten != iStringLen))
				{
					// we just sort of ignore this...
					dwError = GetLastError();
				}
			}
		}
		CloseHandle(hLogFile);
	}

	va_end(Arguments);
	return;
}


int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE hArray[3]; // process, event, heartbeat
	DWORD dwResult, dwTimeout, dwElapsed, dwSize;
	time_t tNow, tLast, tStart;
	TCHAR  tszBuffer[MAX_PATH+1];

	for(tLast = 0 ; tLast < 30 ; tLast++) Sleep(1000);

	if (argc < 5)
	{
		return 1;
	}

	if (!_stscanf_s(argv[1], _T("%d"), &hArray[0])) // process
	{
		return 2;
	}


	if (!_stscanf_s(argv[2], _T("%d"), &hArray[1])) // restart
	{
		return 2;
	}

	if (!_stscanf_s(argv[3], _T("%d"), &hArray[2])) // heartbeat
	{
		return 2;
	}

	if (!_stscanf_s(argv[4], _T("%d"), &dwTimeout)) // timeout
	{
		return 2;
	}

	dwSize = GetEnvironmentVariable(_T("ioFTPD_LogDir"), tszBuffer, sizeof(tszBuffer)/sizeof(*tszBuffer));
	if ((dwSize == 0) || (dwSize + 10 >= sizeof(tszBuffer)/sizeof(*tszBuffer)))
	{
		tszBuffer[0] = _T('.');
		tszBuffer[1] = 0;
	}

	_stprintf_s(tszLogFileName, sizeof(tszLogFileName)/sizeof(*tszLogFileName), _T("%s\\%s"), tszBuffer, _T("Watch.log"));

	time(&tStart);
	tLast = 0;

	while (1)
	{
		dwResult = WaitForMultipleObjectsEx(3, hArray, FALSE, 10000, TRUE);

		if (dwResult == WAIT_TIMEOUT)
		{
			time(&tNow);
			if (tLast)
			{
				// we updated it at least once which means we are watching it now...
				// preloading could take minutes so we can't start timing it out until we know that's done.
				dwElapsed = (DWORD) (tNow - tLast);
				if ((tLast < tNow) && (dwElapsed > dwTimeout))
				{
					// rut ro!  it appears like it locked up!
					WriteLogEntry("Server timeout reached (%d > %d), killing it.\r\n", dwElapsed, dwTimeout);
					TerminateProcess(hArray[0], 1);
					return 1;
				}
			}
			continue;
		}
		if (dwResult == WAIT_OBJECT_0)
		{
			// ioFTPD exited on it's own, time to commit suicide
			return 0;
		}
		if (dwResult == (WAIT_OBJECT_0 + 1))
		{
			// the deadlock event was signaled, terminate the process
			WriteLogEntry("Received deadlock signal from ioFTPD, killing it.\r\n");
			TerminateProcess(hArray[0], 1);
			return 1;
		}
		if (dwResult == (WAIT_OBJECT_0 + 2))
		{
			// we received a heartbeat signal, so it's still alive...
			time(&tLast);
			continue;
		}

		if (dwResult == WAIT_IO_COMPLETION)
		{
			// something weird going on if this happens... remote thread injection?
			WriteLogEntry("Received IO completion ?!?\r\n");
			continue;
		}

		WriteLogEntry("Wait error, aborting!.\r\n");
		return 0;
	}

	return 0;
}

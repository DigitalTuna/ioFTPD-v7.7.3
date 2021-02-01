#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>

void
ErrorExit(LPTSTR lpszFunction) 
{ 
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf)+lstrlen((LPCTSTR)lpszFunction)+60)*sizeof(TCHAR)); 
    wsprintf((LPTSTR)lpDisplayBuf, 
        _T("%s failed with error %d: %s"), 
        lpszFunction, dw, lpMsgBuf); 
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK|MB_ICONERROR); 

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(dw); 
}


INT WINAPI
WinMain(HINSTANCE hInstance,
               HINSTANCE hPrevInstance,
               LPTSTR tszCmdLine,
               INT nCmdShow)
{
	PROCESS_INFORMATION  ProcessInformation;
	STARTUPINFO          StartUpInfo;
	DWORD                dwDelay;

	ZeroMemory(&ProcessInformation, sizeof(PROCESS_INFORMATION));

	GetStartupInfo(&StartUpInfo);

	//  Set startup information
    StartUpInfo.dwFlags    = STARTF_USESHOWWINDOW;
    StartUpInfo.wShowWindow  = SW_HIDE;

	if (tszCmdLine && *tszCmdLine && (1 == _stscanf_s(tszCmdLine, _T("%u"), &dwDelay)) &&
		dwDelay > 0 && dwDelay < 300)
	{
		dwDelay *= 1000;
	}
	else
	{
		dwDelay = 10*1000;
	}

	if (!CreateProcess(0, _T("system/ioFTPD.exe"), 0, 0, 0, DETACHED_PROCESS, 0, 0,
		&StartUpInfo, &ProcessInformation))
	{
		ErrorExit(_T("CreateProcess(ioFTPD)"));
	}

	Sleep(dwDelay);
	ZeroMemory(&ProcessInformation, sizeof(PROCESS_INFORMATION));
	if (!CreateProcess(0, _T("ioGUI/ioGUI2.exe"), 0, 0, 0, DETACHED_PROCESS, 0, 0,
		&StartUpInfo, &ProcessInformation))
	{
		ErrorExit(_T("CreateProcess(ioGUI2)"));
	}
}

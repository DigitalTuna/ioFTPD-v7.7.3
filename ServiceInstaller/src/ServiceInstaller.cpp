#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>


LPTSTR
GetSystemErrorMsg(DWORD dwError)
{
    LPTSTR tszMsg;

	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		              NULL,
					  dwError,
					  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					  (LPTSTR) &tszMsg,
					  0, NULL ))
	{
		return tszMsg;
	}
	return _T("[unknown error]");
}


void
ErrorExit(LPTSTR tszFunction) 
{ 
	LPTSTR tszMsgBuf;
    LPTSTR tszDisplayBuf;
    DWORD  dwErr;
	size_t stLen;
	
	dwErr = GetLastError(); 
	tszMsgBuf = GetSystemErrorMsg(dwErr);
	if (!tszFunction)
	{
		tszFunction = _T("[unknown function]");
	}

	stLen = _tcslen(tszMsgBuf) + _tcslen(tszFunction) + 60;
	tszDisplayBuf = (LPTSTR) LocalAlloc(LMEM_ZEROINIT, stLen);
	_stprintf_s(tszDisplayBuf, stLen, _T("%s failed with error %d: %s"), 
		tszFunction, dwErr, tszMsgBuf); 
	MessageBox(NULL, tszDisplayBuf, _T("Error"), MB_OK|MB_ICONERROR); 

    LocalFree(tszMsgBuf);
    LocalFree(tszDisplayBuf);
    ExitProcess(dwErr); 
}


int
FormattedMessageBox(LPTSTR tszTitle, UINT uType, LPTSTR tszFormat, ...)
{
	va_list args;
	TCHAR tszBuffer[1024];

	va_start( args, tszFormat );
	_vstprintf_s(tszBuffer, sizeof(tszBuffer)/sizeof(TCHAR), tszFormat, args);

	return MessageBox(NULL, tszBuffer, tszTitle, uType);
}



int APIENTRY 
_tWinMain(HINSTANCE hInstance,
		  HINSTANCE hPrevInstance,
		  LPTSTR    lpCmdLine,
		  int       nCmdShow)
{
	SC_HANDLE SCM, IoService;
	TCHAR   tszFileName[MAX_PATH];
	DWORD   dwResult;
	size_t  stLen;
	TCHAR   tszCmdLine[1024], tszDepend[1024];
	LPTSTR  tszErrMsg;
	SERVICE_DESCRIPTION ServDesc;
	SERVICE_FAILURE_ACTIONS ServFail;
	SERVICE_STATUS ServiceStatus;
	SC_ACTION lpServActions[2];


	dwResult = GetCurrentDirectory(sizeof(tszFileName)/sizeof(TCHAR), tszFileName);
	if (!dwResult || dwResult > sizeof(tszFileName)/sizeof(TCHAR)-12)
	{
		ErrorExit(_T("GetCurrentDirectory"));
	}
	stLen = _tcslen(tszFileName);
	if (tszFileName[stLen-1] != _T('\\'))
	{
		tszFileName[stLen++] = _T('\\');
		tszFileName[stLen] = 0;
	}
	_tcscpy_s(&tszFileName[stLen], sizeof(tszFileName)/sizeof(TCHAR)-stLen, _T("ioFTPD.exe"));
	if (GetFileAttributes(tszFileName) == INVALID_FILE_ATTRIBUTES)
	{
		dwResult = GetLastError();
		tszErrMsg = GetSystemErrorMsg(dwResult);
		FormattedMessageBox(_T("Error"), MB_OK|MB_ICONEXCLAMATION,
			_T("Unable to locate executable\r\n%s: %s"), tszFileName, tszErrMsg);
		LocalFree(tszErrMsg);
		ExitProcess(dwResult);
	}

	SCM = OpenSCManager(0,0, SC_MANAGER_CREATE_SERVICE);
	if(!SCM)
	{
		ErrorExit(_T("OpenSCManager"));
	}

	IoService = OpenService(SCM, _T("ioFTPD"), SERVICE_ALL_ACCESS);
	if (IoService)
	{
		if (IDYES == MessageBox(NULL,
			_T("The \"ioFTPD\" Service is currently installed.  To modify the settings goto  \r\n\
Control Panel->Administrative Tools->Services->ioFTPD\r\n\r\n\
Do you want to UNINSTALL (and stop) the ioFTPD Service?"),
               _T("Service exists - Uninstall?"),
			   MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON2))
		{
			// don't really care if this fails or not, just try to stop it
			ControlService(IoService, SERVICE_CONTROL_STOP, &ServiceStatus);
			if (!DeleteService(IoService))
			{
				ErrorExit(_T("DeleteService"));
			}
			CloseServiceHandle(IoService);
			CloseServiceHandle(SCM);
			MessageBox(NULL, _T("Successfully uninstalled the \"ioFTPD\" Service"),
				_T("Service Uninstall Complete"), MB_OK);
			ExitProcess(0);
		}
		ExitProcess(ERROR_SERVICE_EXISTS);
	}

	if (IDYES != MessageBox(NULL,
		_T("Would you like to install the \"ioFTPD\" Service so that\r\n\
the FTP server starts when you turn on your computer?  "),
        _T("Install ioFTPD Service?"), MB_YESNO|MB_ICONEXCLAMATION))
	{
		ExitProcess(0);
	}

	_stprintf_s(tszCmdLine, sizeof(tszCmdLine)/sizeof(TCHAR), _T("\"%s\""), tszFileName);

	_stprintf_s(tszDepend, sizeof(tszDepend)/sizeof(TCHAR), _T("%s%c%s%c"),
		_T("tcpip"), 0, _T("afd"), 0);

	IoService = CreateService( 
        SCM,                       // default database
        _T("ioFTPD"),              // name of service 
		_T("ioFTPD"),              // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_AUTO_START,        // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        tszCmdLine,                // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        tszDepend,                 // dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 

	if(!IoService)
	{
		ErrorExit(_T("CreateService"));
	}

	ServDesc.lpDescription = _T("Provides fast, secure, and extensible support for the File Transfer Protocol (FTP).");
	if (!ChangeServiceConfig2(IoService, SERVICE_CONFIG_DESCRIPTION, &ServDesc))
	{
		ErrorExit(_T("ChangeServiceConfig2(Description)"));
	}
	ZeroMemory(&ServFail, sizeof(ServFail));
	ServFail.dwResetPeriod = 600;
	ServFail.cActions = 2;
	ServFail.lpsaActions = &lpServActions[0];
	// wait 30 seconds before restarting
	lpServActions[0].Type = SC_ACTION_RESTART;
	lpServActions[0].Delay = 60000;
	lpServActions[1].Type = SC_ACTION_NONE;
	lpServActions[1].Delay = 0;
	if (!ChangeServiceConfig2(IoService, SERVICE_CONFIG_FAILURE_ACTIONS, &ServFail))
	{
		ErrorExit(_T("ChangeServiceConfig2(Failure_Actions)"));
	}

	if (IDYES == MessageBox(NULL, 
		_T("Successfully installed the \"ioFTPD\" Service to start at boot time\r\n\
using the Local System Account.  In the event of failure the service will  \r\n\
automatically restart after a 30 second delay provided it hasn't already\r\n\
done so within the last 5 minutes.\r\n\r\n\
If you need to run as a specific user, wish to configure manual startup,\r\n\
or modify any of the various other parameters you can do so via:\r\n\
Control Panel->Administrative Tools->Services->ioFTPD.\r\n\r\n\
Would you like to start the service now?"),
        _T("Installed ioFTPD service"), MB_YESNO))
	{
		if (!StartService(IoService, 0, NULL))
		{
			ErrorExit(_T("StartService"));
		}
	}

	CloseServiceHandle(IoService);
	CloseServiceHandle(SCM);

	return 0;
}

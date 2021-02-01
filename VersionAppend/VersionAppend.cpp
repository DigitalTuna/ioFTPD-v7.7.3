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
	size_t            siCmdLine;
	DWORD             dwVerLen, dwZero, dwBytes;
	char             *Buffer;
	char              FileBuffer[9];
	VS_FIXEDFILEINFO *lpVsFileInfo;
	TCHAR             tszString[256];
	unsigned int      uFixedLen;
	DWORD             lpdwVersion[3];
	DWORD             lpdwFileVersion[3];
	HANDLE            hFile;

	siCmdLine = _tcslen(tszCmdLine);
	if (!siCmdLine)
	{
		MessageBox(NULL,
			_T("You must specify the executable on the command line\r\n\
or drag and drop it onto this executable."),
			_T("No File Specified"), MB_OK|MB_ICONEXCLAMATION); 
		ExitProcess(ERROR_INVALID_NAME);
	}
	dwVerLen = GetFileVersionInfoSize(tszCmdLine, &dwZero);
	if (!dwVerLen)
	{
		ErrorExit(_T("GetFileVersionInfoSize"));
	}
	Buffer = (char *) malloc(dwVerLen);
	if (!Buffer)
	{
		ErrorExit(_T("malloc"));
	}
	if (!GetFileVersionInfo(tszCmdLine, 0, dwVerLen, Buffer))
	{
		ErrorExit(_T("GetFileVersionInfo"));
	}
	if (!VerQueryValue(Buffer, _T("\\"), (LPVOID *) &lpVsFileInfo, &uFixedLen))
	{
		ErrorExit(_T("VerQueryValue"));
	}
	lpdwVersion[0] = HIWORD(lpVsFileInfo->dwFileVersionMS);
	lpdwVersion[1] = LOWORD(lpVsFileInfo->dwFileVersionMS);
	lpdwVersion[2] = HIWORD(lpVsFileInfo->dwFileVersionLS);

	hFile = CreateFile(tszCmdLine, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		ErrorExit(_T("CreateFile"));
	}

	if (SetFilePointer(hFile, -9, 0, FILE_END) == INVALID_SET_FILE_POINTER)
	{
		ErrorExit(_T("SetFilePointer"));
	}

	if (!ReadFile(hFile, FileBuffer, 9, &dwBytes, 0) || dwBytes != 9)
	{
		ErrorExit(_T("ReadFile"));
	}

	lpdwFileVersion[0] = lpdwFileVersion[1] = lpdwFileVersion[2] = 0;

	if (FileBuffer[8] == 0     &&
		FileBuffer[7] == 6     &&
		FileBuffer[6] == 0     &&
		FileBuffer[5] == 'r'   &&
		isdigit(FileBuffer[4]) &&
		FileBuffer[3] == '-'   &&
		isdigit(FileBuffer[2]) &&
		FileBuffer[1] == '-'   &&
		isdigit(FileBuffer[0]))
	{
		// file is already tagged, let's see if they match
		lpdwFileVersion[0] = FileBuffer[0] - '0';
		lpdwFileVersion[1] = FileBuffer[2] - '0';
		lpdwFileVersion[2] = FileBuffer[4] - '0';
		if (lpdwFileVersion[0] == lpdwVersion[0] &&
			lpdwFileVersion[1] == lpdwVersion[1] &&
			lpdwFileVersion[2] == lpdwVersion[2])
		{
			CloseHandle(hFile);
			_stprintf_s(tszString, sizeof(tszString)/sizeof(*tszString),
				_T("File Version already matches Resource Version\r\nVersion: %d-%d-%dr"),
				lpdwVersion[0], lpdwVersion[1], lpdwVersion[2]);
			MessageBox(NULL, tszString, _T("File Version Already Set"), MB_OK);
			ExitProcess(0);
		}
		else
		{
			_stprintf_s(tszString, sizeof(tszString)/sizeof(*tszString),
				_T("File Version doesn't matche Resource Version\r\n\
File    : %d-%d-%dr\r\n\
Resource: %d-%d-%dr\r\n\r\n\
Update?"),
                lpdwFileVersion[0], lpdwFileVersion[1], lpdwFileVersion[2],
				lpdwVersion[0], lpdwVersion[1], lpdwVersion[2]);
			if (MessageBox(NULL, tszString, _T("File Version Mismatch"), MB_YESNO|MB_ICONWARNING) != IDYES)
			{
				CloseHandle(hFile);
				ExitProcess(0);
			}
			// move the file pointer back so we can overwrite the existing version info
			SetFilePointer(hFile, -9, 0, FILE_END);
		}
	}

	sprintf_s(FileBuffer, sizeof(FileBuffer), "%d-%d-%dr%c%c",
		lpdwVersion[0], lpdwVersion[1], lpdwVersion[2],0,6);

	if (!WriteFile(hFile, FileBuffer, 9, &dwBytes, 0) || dwBytes != 9)
	{
		ErrorExit(_T("WriteFile"));
	}
	CloseHandle(hFile);

	_stprintf_s(tszString, sizeof(tszString)/sizeof(*tszString), _T("File Version set to %d-%d-%dr"),
		lpdwVersion[0], lpdwVersion[1], lpdwVersion[2]);

	MessageBox(NULL, tszString, _T("Update Successful!"), MB_OK);
}

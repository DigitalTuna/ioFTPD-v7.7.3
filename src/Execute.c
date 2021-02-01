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


//  Local declarations

BOOL ExecuteAsync(LPEVENT_DATA lpEventData,
				  IO_STRING *Arguments);


BOOL Redir_Command(LPEVENT_DATA lpEventData, LPSTR szCommandLine);
INT  Redir_VFS_Add(LPEVENT_DATA lpEventData, IO_STRING *Args);
INT  Redir_VFS_ChAttr(LPEVENT_DATA lpEventData, IO_STRING *Args);
INT  Redir_Change(LPEVENT_DATA lpEventData, IO_STRING *Args);
INT  Redir_Putlog(LPEVENT_DATA lpEventData, IO_STRING *Args);
INT  Redir_Unlock(LPEVENT_DATA lpEventData, IO_STRING *Args);
INT  Redir_Detach(LPEVENT_DATA lpEventData, IO_STRING *Args);
INT  Redir_Buffer(LPEVENT_DATA lpEventData, IO_STRING *Args);
INT  Redir_Prefix(LPEVENT_DATA lpEventData, IO_STRING *Args);
INT  Redir_NewLines(LPEVENT_DATA lpEventData, IO_STRING *Args);

//

Redirect_Command Redirect_Command_Table[] =
{
  "!putlog",     7,  Redir_Putlog,     // 1
  "!vfs:add",    8,  Redir_VFS_Add,    // 1
  "!vfs:chattr", 11, Redir_VFS_ChAttr,
  "!change",     7,  Redir_Change,     // 1
  "!unlock",     7,  Redir_Unlock,     //
  "!newlines",   9,  Redir_NewLines,   // 1
  "!prefix",     7,  Redir_Prefix,     // 1
  "!buffer",     7,  Redir_Buffer,     // 1
  "!detach",     7,  Redir_Detach,     // 0
  NULL,          0,  NULL
};


LPEVENT_MODULE  lpEventModules;
LPEVENT_PROC  *lpEventProc;
LPBYTE      lpEnvironmentBase;
DWORD      dwEventProc, dwEventProcSize;

DWORD volatile dwConfigCounter;
LPTSTR  tszKeepAliveText;
DWORD   dwKeepAliveText;


INT Redir_Detach(LPEVENT_DATA lpEventData, IO_STRING *Args)
{
  LPSTR  szValue;

  //  Get return value
  szValue  = GetStringIndexStatic(Args, 1);

  lpEventData->dwFlags |= EVENT_DETACH;
	  
  if (!szValue || !isdigit(*szValue) || atoi(szValue))
  {
	  lpEventData->dwFlags |= EVENT_ERROR;
  }
  return FALSE;
}


INT Redir_NewLines(LPEVENT_DATA lpEventData, IO_STRING *Args)
{
  LPSTR  szSetting;
  //  Get new settings
  szSetting  = GetStringIndexStatic(Args, 1);

  if (! strcmp(szSetting, "off"))
  {
    lpEventData->dwFlags  &= (07777 - EVENT_NEWLINE);
  }
  else lpEventData->dwFlags  |= EVENT_NEWLINE;

  return FALSE;
}






INT Redir_Buffer(LPEVENT_DATA lpEventData, IO_STRING *Args)
{
  LPSTR  szSetting;

  //  Get new setting
  szSetting  = GetStringIndexStatic(Args, 1);

  if (! strcmp(szSetting, "off") && lpEventData->lpSocket)
  {
    lpEventData->dwFlags  &= (07777 - EVENT_BUFFER);
  }
  else lpEventData->dwFlags  |= EVENT_BUFFER;

  return FALSE;
}






INT Redir_Prefix(LPEVENT_DATA lpEventData, IO_STRING *Args)
{
  LPSTR  szSetting;

  //  Get new setting
  szSetting  = GetStringIndexStatic(Args, 1);

  if (! strcmp(szSetting, "off"))
  {
    lpEventData->dwFlags  &= (07777 - EVENT_PREFIX);
  }
  else lpEventData->dwFlags  |= EVENT_PREFIX;

  return FALSE;
}





/*

  "!vfs:add <mode> <uid:gid> <path/file>

  */
INT Redir_VFS_Add(LPEVENT_DATA lpEventData, IO_STRING *Args)
{
  LPFILEINFO    lpFileInfo;
  VFSUPDATE    UpdateData;
  LPTSTR      tszFileName, tszIdData, tszFileMode;
  DWORD      dwFileName;
  TCHAR      *ptCheck;
  BOOL      bReturn;
  LPFTPUSER  lpUser;

  ZeroMemory(&UpdateData, sizeof(VFSUPDATE));
  if (GetStringItems(Args) < 4) return FALSE;

  tszFileMode  = GetStringIndexStatic(Args, 1);
  tszIdData  = GetStringIndexStatic(Args, 2);
  tszFileName  = GetStringRange(Args, 3, STR_END);
  dwFileName  = _tcslen(tszFileName);

  //  Convert strings to integers
  UpdateData.Uid  = _tcstoul(tszIdData, &ptCheck, 10);
  if ((ptCheck++)[0] != _TEXT(':')) return FALSE;
  UpdateData.Gid  = _tcstoul(ptCheck, &ptCheck, 10);
  if (ptCheck[0] != _TEXT('\0')) return FALSE;
  UpdateData.dwFileMode  = _tcstoul(tszFileMode, &ptCheck, 8);
  if (ptCheck[0] != _TEXT('\0')) return FALSE;
  //  Remove quotes
  if (tszFileName[0] == _TEXT('"') &&
    tszFileName[dwFileName - 1] == _TEXT('"'))
  {
    (tszFileName++)[dwFileName - 1]  = _TEXT('\0');
  }

  if (UpdateData.Uid >= MAX_UID) UpdateData.Uid  = 0;
  if (UpdateData.Gid >= MAX_GID) UpdateData.Gid  = 0;
  if (UpdateData.dwFileMode > 0777L) UpdateData.dwFileMode  = 0;

  //  Get fileinfo
  if (! GetFileInfo(tszFileName, &lpFileInfo)) return FALSE;

  //  Update filemode
  UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
  UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;
  UpdateData.Context.lpData   = lpFileInfo->Context.lpData;
  UpdateData.Context.dwData   = lpFileInfo->Context.dwData;
  bReturn  = UpdateFileInfo(tszFileName, &UpdateData);
  CloseFileInfo(lpFileInfo);

  if (bReturn && (lpEventData->dwData == DT_FTPUSER))
  {
	  lpUser = (LPFTPUSER)lpEventData->lpData;
	  if (lpUser)
	  {
		  MarkVirtualDir(NULL, lpUser->hMountFile);
	  }
  }

  return bReturn;
}





INT Redir_VFS_ChAttr(LPEVENT_DATA lpEventData, IO_STRING *Args)
{
  LPFILEINFO  lpFileInfo;
  VFSUPDATE  UpdateData;
  LPTSTR    tszData, tszFileName, tszType;
  DWORD    dwData, dwType, dwFileName;
  BOOL    bReturn;
  LPFTPUSER  lpUser;

  bReturn  = FALSE;
  if (GetStringItems(Args) != 4) return FALSE;

  tszType    = GetStringIndexStatic(Args, 1);
  tszFileName  = GetStringIndexStatic(Args, 2);
  dwFileName  = GetStringIndexLength(Args, 2);
  tszData    = GetStringIndexStatic(Args, 3);
  dwData    = GetStringIndexLength(Args, 3);
  dwType    = _tcstoul(tszType, NULL, 10);

  //  Remove quotes from strings
  if (tszFileName[0] == _TEXT('"') &&
    tszFileName[dwFileName - 1] == _TEXT('"'))
  {
    (tszFileName++)[dwFileName - 1]  = _TEXT('\0');
  }
  if (tszData[0] == _TEXT('"') &&
    tszData[dwData - 1] == _TEXT('"'))
  {
    (tszData++)[dwData - 1]  = _TEXT('\0');
    dwData  -= 2;
  }

  //  Get fileinfo
  if (! GetFileInfo(tszFileName, &lpFileInfo)) return FALSE;
  if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
  {
    UpdateData.Uid  = lpFileInfo->Uid;
    UpdateData.Gid  = lpFileInfo->Gid;
    UpdateData.dwFileMode  = lpFileInfo->dwFileMode;
	UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
	UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;
    //  Update file context
    if (CreateFileContext(&UpdateData.Context, &lpFileInfo->Context))
    {
      if (InsertFileContext(&UpdateData.Context, (BYTE)dwType, tszData, dwData * sizeof(TCHAR)))
      {
        bReturn  = UpdateFileInfo(tszFileName, &UpdateData);
      }
      FreeFileContext(&UpdateData.Context);
    }
  }
  CloseFileInfo(lpFileInfo);

  if (bReturn && (lpEventData->dwData == DT_FTPUSER))
  {
	  lpUser = (LPFTPUSER)lpEventData->lpData;
	  if (lpUser)
	  {
		  MarkVirtualDir(NULL, lpUser->hMountFile);
	  }
  }
  
  return bReturn;
}




/*

  "!change <user> <what> <value> <value 1>

  */
INT Redir_Change(LPEVENT_DATA lpEventData, IO_STRING *Args)
{
  Admin_Change(NULL, _T(""), Args);
  return FALSE;
}





/*

  "!putlog <what>"

  */
INT Redir_Putlog(LPEVENT_DATA lpEventData, IO_STRING *Args)
{
  return Putlog(LOG_GENERAL, _TEXT("%s\r\n"), GetStringRange(Args, 1, STR_END));
}





/*

  "!unlock <path> - unlocks file"

  */
INT Redir_Unlock(LPEVENT_DATA lpEventData, IO_STRING *Args)
{
  ReleasePath(GetStringRange(Args, 1, STR_END), NULL);
  return FALSE;
}




/*

  Seeks command from buffer

  */
INT Redir_Command(LPEVENT_DATA lpEventData, LPSTR szCommand)
{
  IO_STRING  Arguments;
  LPTSTR    tszCommand;
  DWORD    dwCommand, n;
  INT      iReturn, iTokens;

  iReturn  = -1;
  //  Tokenize string
  if (! SplitString(szCommand, &Arguments))
  {
    //  Get number of tokens
    iTokens  = GetStringItems(&Arguments);
    if (iTokens > 1)
    {
      //  Get command string
      dwCommand  = GetStringIndexLength(&Arguments, 0);
      tszCommand  = GetStringIndex(&Arguments, 0);
      _tcslwr(tszCommand);

      //  Search command from command list
      for (n = 0;Redirect_Command_Table[n].Trigger;n++)
      {
        //  Compare string and string length
        if (dwCommand == Redirect_Command_Table[n].lTrigger &&
          ! _tcsncmp(tszCommand, Redirect_Command_Table[n].Trigger, dwCommand))
        {
          //  Execute command
          Redirect_Command_Table[n].Command(lpEventData, &Arguments);
          iReturn  = 0;
          break;
        }
      }
    }
    FreeString(&Arguments);
  }
  return iReturn;
}




VOID
CopyEnvironment(LPBUFFER lpEnvironment,
                LPVOID lpContext,
                DWORD dwContextType)
{
  //  Copy Environment
  Message_Compile(lpEnvironmentBase,
                  lpEnvironment,
				  FALSE,
                  lpContext,
                  dwContextType,
                  NULL,
                  NULL);
}



BOOL CreateAsyncPipe(LPHANDLE hRead, LPHANDLE hWrite, BOOL bNotAsync)
{
  SECURITY_ATTRIBUTES  SecurityAttributes;
  CHAR        pNameBuffer[256];
  DWORD       dwFlags;

  dwFlags = (bNotAsync ? 0 : FILE_FLAG_OVERLAPPED);
  wsprintf(pNameBuffer, "\\\\.\\pipe\\ioFTPD-%u-%u", GetCurrentProcessId(), GetCurrentThreadId());
  //  Prepare security sturcture
  SecurityAttributes.lpSecurityDescriptor  = NULL;
  SecurityAttributes.bInheritHandle    = TRUE;
  SecurityAttributes.nLength        = sizeof(SECURITY_ATTRIBUTES);
  //  Create pipe
  hWrite[0]  = CreateNamedPipe(pNameBuffer,
	                           (PIPE_ACCESS_OUTBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | dwFlags),   // dwOpenMode
							   PIPE_TYPE_BYTE|PIPE_WAIT, // dwPipeMode
							   1,                        // MaxIntances
							   EVENT_BUFFER_SIZE,        // nOutBufferSize
							   EVENT_BUFFER_SIZE,        // nInBufferSize
							   0,                        // nDefaultTimeOut
							   &SecurityAttributes);     // LPSECURITY_ATTRIBUTES

  if (hWrite[0] == INVALID_HANDLE_VALUE)
  {
	  return TRUE;
  }

  //  Open pipe
  hRead[0]  = CreateFile(pNameBuffer, GENERIC_READ, 0, NULL, OPEN_EXISTING, dwFlags, NULL);

  if (hRead[0] != INVALID_HANDLE_VALUE)
  {
	  return FALSE;
  }

  CloseHandle(hWrite[0]);
  hWrite[0] = INVALID_HANDLE_VALUE;
  return TRUE;
}







INT __cdecl
EventModuleProcCompare(LPEVENT_PROC *lpProc1,
					   LPEVENT_PROC *lpProc2)
{
  //  Compare string
  return stricmp(lpProc1[0]->szName, lpProc2[0]->szName);
}


BOOL InstallEvent(LPSTR szName, EVENT_PROC_FUNC lpProc)
{
  LPEVENT_PROC  lpEvent;
  LPVOID      lpMemory;

  if (dwEventProc == dwEventProcSize)
  {
    //  Allocate memory
    lpMemory  = ReAllocate(lpEventProc, "Event:Array", sizeof(LPEVENT_PROC) * (dwEventProc + 128));
    if (! lpMemory) return TRUE;
    //  Update size
    dwEventProcSize  += 128;
    lpEventProc    = (LPEVENT_PROC *)lpMemory;
  }
  //  Allocate memory
  lpEvent  = (LPEVENT_PROC)Allocate("Event:Proc", sizeof(EVENT_PROC));
  if (! lpEvent) return TRUE;
  //  Init structure
  lpEvent->szName  = szName;  lpEvent->lpProc  = lpProc;

  if (QuickInsert(lpEventProc, dwEventProc, lpEvent, (QUICKCOMPAREPROC) EventModuleProcCompare))
  {
    //  Free memory
    Free(lpEvent);
    return TRUE;
  }
  dwEventProc++;
  return FALSE;
}



LPVOID
FindEventModule(LPTSTR tszName)
{
  LPEVENT_PROC lpSearch;
  EVENT_PROC   EventProc;
  LPVOID       lpResult;

  EventProc.szName = tszName;
  lpSearch         = &EventProc;
  //  Binary search
  lpResult=bsearch(&lpSearch,
                   lpEventProc,
                   dwEventProc,
                   sizeof(LPEVENT_PROC),
                   (QUICKCOMPAREPROC) EventModuleProcCompare);

  return (lpResult ? ((LPEVENT_PROC *)lpResult)[0]->lpProc : NULL);
}




BOOL Event_Init(BOOL bFirstInitialization)
{
  LPEVENT_MODULE  lpModule;
  HANDLE      hEnvironmentFile;
  LPSTR      szFileName;
  PCHAR      pBuffer, pNewline, pBufferEnd, pLine;
  LPVOID      lpProc;
  DWORD      dwFileSize, dwRead;
  BOOL      bReturn;
  INT        iOffset;

  if (! bFirstInitialization) 
  {
	  dwConfigCounter++;
	  return TRUE;
  }
  lpEventProc      = NULL;
  dwEventProc      = 0;
  dwEventProcSize    = 0;
  lpEnvironmentBase  = NULL;
  pBuffer        = NULL;
  bReturn        = TRUE;
  dwConfigCounter = 1;

  //  Get path for environment file
  if ((szFileName = Config_Get(&IniConfigFile, "Locations", "Environment", NULL, NULL)))
  {
    //  Open Environment file
    if ((hEnvironmentFile = CreateFile(szFileName, GENERIC_READ,
      FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0)) != INVALID_HANDLE_VALUE)
    {
      //  Check file size and allocate buffer
      if ((dwFileSize = GetFileSize(hEnvironmentFile, NULL)) != INVALID_FILE_SIZE && dwFileSize > 0 &&
        (pBuffer = (PCHAR)Allocate("Execute:Init:Environment", dwFileSize + 4)))
      {
        if (ReadFile(hEnvironmentFile, pBuffer, dwFileSize, &dwRead, NULL))
        {
          //  Prepend buffer with "\0\0\n\0"
          CopyMemory(&pBuffer[dwRead], "\0\0\n", 4);
          pBufferEnd  = &pBuffer[dwRead + 4];
          pLine  = pBuffer;
          bReturn  = FALSE;

          //  Remove newlines from environment
          while ((pNewline = strchr(pLine, '\n')))
          {
            //  Lines must be longer than 3chars a=b<newline>
            if (pNewline > &pLine[2])
            {
              //  Replace "\r\n" or "\n" with "\0"
              if (pNewline[-1] == '\r')
              {
                //  Remove carriage feed
                MoveMemory(pNewline, &pNewline[1], pBufferEnd - &pNewline[1]);
                pNewline--;
                pBufferEnd--;
              }
              pNewline[0]  = '\0';
            }
            pLine  = &pNewline[1];
          }
        }
      }
      //  Close file
      CloseHandle(hEnvironmentFile);
    }
  }

  if (! bReturn &&
    ! (lpEnvironmentBase = Message_PreCompile(pBuffer, NULL))) bReturn  = TRUE;
  //  Free memory
  Free(szFileName);
  Free(pBuffer);
  //  Init tcl
  if (bReturn ||
    Tcl_ModuleInit()) return FALSE;
  //  Install built-in handlers
  InstallEvent("EXEC", ExecuteAsync);
  InstallEvent("TCL", TclExecute);
  //  Load modules
  for (iOffset = 0;(szFileName = Config_Get(&IniConfigFile, "Modules", "EventModule", NULL, &iOffset));)
  {
    //  Allocate memory
    lpModule  = (LPEVENT_MODULE)Allocate("Event:Module", sizeof(EVENT_MODULE));
    if (lpModule)
    {
      //  Initialize structue
      ZeroMemory(lpModule, sizeof(EVENT_MODULE));
      lpModule->lpGetProc      = GetProc;
      lpModule->lpInstallEvent  = InstallEvent;
      lpModule->hModule      = LoadLibrary(szFileName);
      //  Check module handle
      if (lpModule->hModule &&
        lpModule->hModule != INVALID_HANDLE_VALUE)
      {
        //  Find startup proc
        lpProc  = GetProcAddress(lpModule->hModule, "EventInit");
        //  Execute startup proc
        if (lpProc &&
          ! ((BOOL (__cdecl *)(LPEVENT_MODULE))lpProc)(lpModule))
        {
          lpModule->lpNext  = lpEventModules;
          lpEventModules    = lpModule;
          lpModule      = NULL;
        }
        else FreeLibrary(lpModule->hModule);
      }
    }
    //  Free memory
    Free(szFileName);
    Free(lpModule);
  }

  dwKeepAliveText = 0;
  if (tszKeepAliveText = Config_Get(&IniConfigFile, _T("Threads"), _T("Keep_Alive_Text"), NULL, NULL))
  {
	  dwKeepAliveText = _tcslen(tszKeepAliveText);
  }

  return TRUE;
}


VOID Event_DeInit(VOID)
{
  LPEVENT_MODULE  lpModule;
  LPVOID      lpProc;

  //  Release libraries
  for (;lpModule = lpEventModules;)
  {
    lpEventModules  = lpEventModules->lpNext;
    //  Find proc
    lpProc  = GetProcAddress(lpModule->hModule, "EventDeInit");
    //  Execute shutdown proc
    if (lpProc) ((VOID (__cdecl *)(LPEVENT_MODULE))lpProc)(lpModule);
    //  Unload library
    FreeLibrary(lpModule->hModule);
    Free(lpModule);
  }

  //  Tcl deinit
  Tcl_ModuleDeInit();

  //  Free memory
  for (;dwEventProc--;) Free(lpEventProc[dwEventProc]);
  Free(lpEventProc);
  Free(lpEnvironmentBase);
  Free(tszKeepAliveText);
}



BOOL ExecuteAsync(LPEVENT_DATA lpEventData, IO_STRING *Arguments)
{
	PROCESS_INFORMATION  ProcessInformation;
	STARTUPINFO          StartUpInfo;
	DWORD        dwError, dwExitCode, dwCID;
	BUFFER       Environment;
	LPTSTR       tszCommandLine;
	HANDLE       hReadPipe, hWritePipe, hEvent;
	BOOL         bReturn;
	LPCLIENT     lpClient;
	CHAR         szBuffer[EVENT_BUFFER_SIZE], *pNewLine, *pTemp;
	OVERLAPPED   Overlapped;
	BUFFER       OutBuffer;
	DWORD        dwBufferSize, dwNextPos, dwCurrentPos, dwBytesRead, dwInCounter, dwAliveCounter, dwToCopy, dwLen, n;
	BOOL         bCancel, bOutputNow, bKeepAlive, bOutputAlways;

	bReturn   = TRUE;
	dwError   = NO_ERROR;

	//  Reset memory
	ZeroMemory(&ProcessInformation, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&Environment, sizeof(BUFFER));
	dwCID = -1;
	lpClient = NULL;

	//  Get arguments
	tszCommandLine  = GetStringRange(Arguments, STR_BEGIN, STR_END);

	//  Set startup information
	GetStartupInfo(&StartUpInfo);
	StartUpInfo.dwFlags    = STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;
	StartUpInfo.wShowWindow  = SW_HIDE;
	StartUpInfo.hStdInput = INVALID_HANDLE_VALUE;

	//  Copy default environment
	Environment.size  = 1024;
	Environment.buf    = (PCHAR)Allocate("Execute:Environment", Environment.size);
	if (!Environment.buf) return TRUE;
	CopyEnvironment(&Environment, lpEventData->lpData, lpEventData->dwData);

	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!hEvent) 
	{
		Free(Environment.buf);
		return TRUE;
	}


	// Create pipe, but hold handle lock until creation because we don't want to accidentally
	// pass inheritable handles to multiple processes...
	AcquireHandleLock();
	if (CreateAsyncPipe(&hReadPipe, &hWritePipe, FALSE))
	{
		ReleaseHandleLock();
		Free(Environment.buf);
		CloseHandle(hEvent);
		return TRUE;
	}

	//  Set startup information
	StartUpInfo.hStdOutput  = hWritePipe;
	StartUpInfo.hStdError  = hWritePipe;

	//  Create process
	if (CreateProcess(0, tszCommandLine, 0, 0, TRUE,
		0, Environment.buf, 0, &StartUpInfo, &ProcessInformation))
	{
		// don't need this anymore
		CloseHandle(hWritePipe);

		ReleaseHandleLock();

		// TODO: what about scheduler events?
		if (lpEventData->dwData == C_FTP)
		{
			dwCID = ((LPFTPUSER) lpEventData->lpData)->Connection.dwUniqueId;
			lpClient = LockClient(dwCID);
			if (lpClient)
			{
				lpClient->Static.dwFlags |= S_SCRIPT;
				UnlockClient(dwCID);
			}
		}

		ZeroMemory(&OutBuffer, sizeof(OutBuffer));
		dwInCounter     = EXE_INPUT_INTERVALS;
		dwAliveCounter  = EXE_DIRECT_INTERVALS;
		bCancel         = FALSE;
		bOutputNow      = FALSE;
		dwBufferSize    = sizeof(szBuffer);
		dwNextPos       = 0;
		bKeepAlive      = FALSE;
		bOutputAlways   = FALSE;

		SetBlockingThreadFlag();

		while (!(lpEventData->dwFlags & EVENT_DETACH))
		{
			ZeroMemory(&Overlapped, sizeof(Overlapped));
			Overlapped.hEvent = hEvent;

			if (!bCancel && !ReadFile(hReadPipe, &szBuffer[dwNextPos], dwBufferSize, NULL, &Overlapped) && (dwError = GetLastError()) != ERROR_IO_PENDING)
			{
				break;
			}
			bCancel = TRUE;
			if (WaitForSingleObject(Overlapped.hEvent, EXE_CHECK_INTERVAL) != WAIT_TIMEOUT)
			{
				// process input...
				bCancel = FALSE;
				if (!GetOverlappedResult(hReadPipe, &Overlapped, &dwBytesRead, FALSE))
				{
					// broken pipe most likely which means process finished...
					dwError = GetLastError();
					break;
				}
				else
				{
					dwBufferSize -= dwBytesRead;
					dwNextPos    += dwBytesRead;
					dwCurrentPos  = 0;
					n             = dwNextPos;

					while (n > 0)
					{
						pNewLine = memchr(&szBuffer[dwCurrentPos], _TEXT('\n'), n);

						if (!pNewLine)
						{
							if (dwBufferSize || !dwCurrentPos)
							{
								// go and try to grab some more text to hopefuly get a full line since
								// we have room in buffer or will have after handling already processed lines
								break;
							}
							// no newline in an entirely full buffer, just print the whole thing
							// and start over with an empty buffer...
							dwLen = dwToCopy = dwNextPos;
						}
						else
						{
							dwToCopy = pNewLine - &szBuffer[dwCurrentPos];
							dwLen = dwToCopy + 1;
							if (dwToCopy > 1 && pNewLine[-1] == _T('\r')) dwToCopy--;

							if ((dwToCopy > 2) && (szBuffer[dwCurrentPos] == _T('!')) && (szBuffer[dwCurrentPos+1] != _T('!')))
							{
								szBuffer[dwCurrentPos+dwToCopy] = 0;
								if (!Redir_Command(lpEventData, &szBuffer[dwCurrentPos]))
								{
									// we executed a command, so don't copy string at all
									n            -= dwLen;
									dwCurrentPos += dwLen;
									continue;
								}
							}
						}

						if (!(lpEventData->dwFlags & EVENT_SILENT))
						{
							//  Copy to output buffer
							if (lpEventData->dwFlags & EVENT_PREFIX) Put_Buffer(&OutBuffer, lpEventData->tszPrefix, lpEventData->dwPrefix);
							Put_Buffer(&OutBuffer, &szBuffer[dwCurrentPos], dwToCopy);
							// Yil: lines NEED to be newline terminated... deal with it...
							Put_Buffer(&OutBuffer, _TEXT("\r\n"), 2);
						}

						n            -= dwLen;
						dwCurrentPos += dwLen;
					}

					// shove whatever is left over down and setup variables again
					dwLen         = dwNextPos - dwCurrentPos;
					dwBufferSize += dwCurrentPos;
					dwNextPos     = dwLen;
					MoveMemory(szBuffer, &szBuffer[dwCurrentPos], dwLen);

					if (!(lpEventData->dwFlags & EVENT_BUFFER))
					{
						bOutputNow = TRUE;
					}
				}
			}
			else
			{
				if (! dwAliveCounter--)
				{
					bOutputAlways = TRUE;
				}
				if (! dwInCounter--)
				{
					bOutputNow = TRUE;
					bKeepAlive = TRUE;
				}
			}
			if ((lpClient && (lpClient->Static.dwFlags & S_TERMINATE)) || (dwDaemonStatus == DAEMON_SHUTDOWN))
			{
				// rut ro, we are being forced to exit...
				lpEventData->dwFlags |= EVENT_DETACH;
				Putlog(LOG_SYSTEM, _TEXT("Abandoned EXE process (pid=%d): %s\r\n"), ProcessInformation.dwProcessId, tszCommandLine);
				continue;
			}

			if (bOutputAlways) bOutputNow = TRUE;

			if (bOutputNow)
			{
				// we need to output something...

				// If we have a user output buffer then we need to look at the last line and see if it's
				// an FTP final response code, if so don't send that part of buffer since we to leave it
				// so it can be returned when the function finishes and site commands / etc can look at
				// the value...
				if ((lpEventData->lpBuffer) && (OutBuffer.len > 5))
				{
					// start at len-1 so we don't find the terminating newline
					for(n = OutBuffer.len-1 ; n-- ; )
					{
						if ((n != 0) && (OutBuffer.buf[n] != '\n')) continue;
						if (n + 4 > OutBuffer.len) continue;
						if (n == 0)
						{
							pTemp = OutBuffer.buf;
						}
						else
						{
							pTemp = &OutBuffer.buf[n+1];
						}
						if ((pTemp[3] == ' ') && (pTemp[0] >= '0') && (pTemp[0] <= '5') &&
							isdigit(pTemp[1]) && isdigit(pTemp[2]))
						{
							// it's a valid response code
							if (n == 0)
							{
								// doh, we can't send this!
								bOutputNow = FALSE;
								break;
							}
							dwToCopy = OutBuffer.len - n;
							SendQuick(lpEventData->lpSocket, OutBuffer.buf, OutBuffer.len - dwToCopy);
							OutBuffer.len = dwToCopy;
							MoveMemory(OutBuffer.buf, &OutBuffer.buf[n+1], dwToCopy);
							bOutputNow     = FALSE;
							bKeepAlive     = FALSE;
							dwInCounter    = EXE_INPUT_INTERVALS;
							dwAliveCounter = EXE_DIRECT_INTERVALS;
							break;
						}
					}
				}

				if (bOutputNow && bKeepAlive)
				{
					bKeepAlive = FALSE;
					if (dwKeepAliveText && !(lpEventData->dwFlags & EVENT_SILENT))
					{
						if (lpEventData->dwPrefix)
						{
							Put_Buffer(&OutBuffer, lpEventData->tszPrefix, lpEventData->dwPrefix);
						}
						else
						{
							Put_Buffer(&OutBuffer, tszKeepAliveText, dwKeepAliveText);
						}
						Put_Buffer(&OutBuffer, _TEXT("\r\n"), 2);
					}
				}

				if (bOutputNow && OutBuffer.len)
				{
					// we didn't send just part of it above...
					SendQuick(lpEventData->lpSocket, OutBuffer.buf, OutBuffer.len);
					OutBuffer.len  = 0;
					bOutputNow     = FALSE;
					bKeepAlive     = FALSE;
					dwInCounter    = EXE_INPUT_INTERVALS;
					dwAliveCounter = EXE_DIRECT_INTERVALS;
				}
			}
	    }

		if (bCancel)
		{
			CancelIo(hReadPipe);
		}
		CloseHandle(hReadPipe);

		// copy/move whatever hasn't been sent to the user already to the output buffer
		if (lpEventData->lpBuffer)
		{
			if (lpEventData->lpBuffer->len == 0)
			{
				// Free old buf since it's emtpy and copy over current info
				Free(lpEventData->lpBuffer->buf);
				CopyMemory(lpEventData->lpBuffer, &OutBuffer, sizeof(OutBuffer));
				OutBuffer.buf = NULL;
			}
			else
			{
				Put_Buffer(lpEventData->lpBuffer, OutBuffer.buf, OutBuffer.len);
			}
		}

		if (lpEventData->dwFlags & EVENT_DETACH)
		{
			dwError = NO_ERROR;
			if (!(lpEventData->dwFlags & EVENT_ERROR)) bReturn = FALSE;
		}
		else if ((dwError == NO_ERROR) || (dwError == ERROR_BROKEN_PIPE))
		{
			//  Wait for process to stop (500msecs max)
			WaitForSingleObject(ProcessInformation.hProcess, 500);
			//  Get exit code
			dwError = NO_ERROR;
			if (GetExitCodeProcess(ProcessInformation.hProcess, &dwExitCode))
			{
				if (! dwExitCode) bReturn = FALSE;
			}
		}

		SetNonBlockingThreadFlag();

		//  Free resources
		CloseHandle(ProcessInformation.hThread);
		CloseHandle(ProcessInformation.hProcess);
		Free(OutBuffer.buf);

		lpClient = LockClient(dwCID);
		if (lpClient)
		{
			lpClient->Static.dwFlags &= ~S_SCRIPT;
			UnlockClient(dwCID);
		}
	}
	else
	{
		dwError = GetLastError();
		CloseHandle(hWritePipe);
		CloseHandle(hReadPipe);
		ReleaseHandleLock();

		Putlog(LOG_SYSTEM, _TEXT("CreateProcess failure: %s (error = %u)\r\n"), tszCommandLine, dwError);
		if (dwError == ERROR_FILE_NOT_FOUND)
		{
			// just so we don't confuse users thinking a site command referred to a missing file we'll
			// change the response... the logfile will have the real one...
			dwError = ERROR_SCRIPT_MISSING;
		}
	}
	Free(Environment.buf);
	CloseHandle(hEvent);
	SetLastError(dwError);
	return bReturn;
}




BOOL
RunEvent(LPEVENT_COMMAND lpCommandData)
{
  EVENT_DATA         EventData;
  IO_STRING          Arguments, Parameters;
  LPTSTR             tszEvent;
  BUFFER             Message;
  LPBYTE             lpMessageBuffer;
  LPVOID             lpModule;
  DWORD              dwError;
  BOOL               bReturn;

  dwError  = NO_ERROR;

  //  Initialize structure
  ZeroMemory(&Arguments, sizeof(IO_STRING));
  EventData.dwFlags  = EVENT_NEWLINE;
  EventData.lpData   = lpCommandData->lpDataSource;
  EventData.dwData   = lpCommandData->dwDataSource;
  EventData.lpSocket = lpCommandData->lpOutputSocket;

  if (EventData.tszPrefix = lpCommandData->tszOutputPrefix)
  {
    EventData.dwFlags  |= EVENT_PREFIX;
    EventData.dwPrefix  = _tcslen(EventData.tszPrefix);
  }
  if (EventData.lpBuffer = lpCommandData->lpOutputBuffer)
  {
    EventData.dwFlags  |= EVENT_BUFFER;
  }
  if (! EventData.lpSocket || EventData.lpSocket->Socket == INVALID_SOCKET)
  {
    if (! EventData.lpBuffer) EventData.dwFlags  |= EVENT_SILENT;
    EventData.lpSocket  = NULL;
  }

  if (! lpCommandData->tszCommand)
  {
    dwError  = ERROR_INVALID_ARGUMENTS;
  }
  else if (lpCommandData->tszCommand[0] == _TEXT('%'))
  {
    //  Convert cookies
    Message.size  = (_tcslen(lpCommandData->tszCommand) + 2) * sizeof(TCHAR);
    Message.len    = 0;
    Message.dwType  = TYPE_CHAR;
    Message.buf    = (PCHAR)Allocate("Execute:CookieString", Message.size);

    if (Message.buf)
    {
      //  Copy string to temporary buffer
      CopyMemory(Message.buf, &lpCommandData->tszCommand[1], Message.size - 2 * sizeof(TCHAR));
      _tcscpy(&Message.buf[Message.size - 2 * sizeof(TCHAR)], _TEXT("\n"));

      lpMessageBuffer  = Message_PreCompile(Message.buf, NULL);
      Message_Compile(lpMessageBuffer, &Message, FALSE, lpCommandData->lpDataSource, lpCommandData->dwDataSource, NULL, NULL);

      if (Message.len >= 2)
      {
        //  Replace carriage feed with '\0'
        Message.buf[Message.len - 2]  = '\0';
        if (SplitString(Message.buf, &Arguments)) dwError  = GetLastError();
      }
      else dwError  = ERROR_INVALID_ARGUMENTS;
      Free(lpMessageBuffer);
      Free(Message.buf);
    } else {
      dwError  = ERROR_NOT_ENOUGH_MEMORY;
    }
  }
  else if (SplitString(lpCommandData->tszCommand, &Arguments)) {
    dwError  = GetLastError();
  }

  if (dwError != NO_ERROR) ERROR_RETURN(dwError, FALSE);

  //  Append parameters to arguments
  if (lpCommandData->tszParameters)
  {
	ZeroMemory(&Parameters, sizeof(IO_STRING));
    if (! SplitString(lpCommandData->tszParameters, &Parameters))
    {
      if (ConcatString(&Arguments, &Parameters)) dwError  = GetLastError();
      FreeString(&Parameters);
    }

    if (dwError != NO_ERROR)
    {
      FreeString(&Arguments);
      ERROR_RETURN(dwError, TRUE);
    }
  }

  //  Find event proc
  if ((tszEvent = GetStringIndexStatic(&Arguments, 0)) &&
    (lpModule = FindEventModule(tszEvent)))
  {
    PushString(&Arguments, 1);
    //  Execute event proc
    bReturn=((BOOL (__cdecl *)(LPEVENT_DATA, IO_STRING *))lpModule)(&EventData,
                                                                    &Arguments);
    if (bReturn) dwError=GetLastError();
    PullString(&Arguments, 1);
  }
  else
  {
	  dwError  = ERROR_FILE_NOT_FOUND;
	  bReturn  = TRUE;
  }

  FreeString(&Arguments);
  if (dwError != NO_ERROR) SetLastError(dwError);
  return bReturn;
}


BOOL
RunTclEventWithResult(LPEVENT_DATA lpEventData, LPIO_STRING Arguments, LPINT lpiResult, LPVIRTUALDIR lpVirtualDir)
{
	DWORD              dwError;
	BOOL               bReturn;

	dwError  = NO_ERROR;

	//  Initialize structure
	lpEventData->dwFlags  |= EVENT_NEWLINE | EVENT_SILENT;

	bReturn=TclExecute2(lpEventData, Arguments, lpiResult, lpVirtualDir);
	if (bReturn) dwError=GetLastError();

	if (dwError != NO_ERROR) SetLastError(dwError);
	return bReturn;
}

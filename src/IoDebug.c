/*
* Copyright(c) 2006 Yil@Wondernet.nu
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
#include <DbgHelp.h>

#define MINIDUMP_DIR   _T("\\Debugging Tools for Windows")
#define MINIDUMP_DLL   _T("DbgHelp.dll")
#define CRASH_LOG      _T("CRASH-Log.txt")

// I don't want to change _WIN32_WINNT so define this if not defined
#ifndef SM_TABLETPC
#  define SM_TABLETPC             86
#  define SM_MEDIACENTER          87
#  define SM_STARTER              88
#  define SM_SERVERR2             89
#endif

#define ARRAY_SIZE(X)		 (sizeof(X)/sizeof(*X))

#define MINI_NO_OUTPUT      0x1
#define MINI_SYMBOLS_OK     0x2
#define MINI_PROMPT_OK      0x4
#define MINI_START_HEADER   0x10
#define MINI_MODULES_HEADER 0x20
#define MINI_THREADS_HEADER 0x40
#define MINI_SYMBOLS_TRIED  0x80

typedef BOOL  (WINAPI *MiniDumpWriteDump_t)(HANDLE hProcess, DWORD ProcessId, HANDLE hFile,
											MINIDUMP_TYPE DumpType,
											PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
											PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
											PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

typedef DWORD (WINAPI *SymGetOptions_t)(VOID);

typedef DWORD (WINAPI *SymSetOptions_t)(DWORD SymOptions);

typedef BOOL  (WINAPI *SymInitialize_t)(HANDLE hProcess, PCTSTR UserSearchPath, BOOL fInvadeProcess);

typedef BOOL  (WINAPI *StackWalk64_t)(DWORD MachineType, HANDLE hProcess, HANDLE hThread,
									  LPSTACKFRAME64 StackFrame, PVOID ContextRecord,
									  PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
									  PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
									  PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
									  PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);

typedef BOOL  (WINAPI *SymGetModuleInfo64_t)(HANDLE hProcess, DWORD64 qwAddr, PIMAGEHLP_MODULE64 ModuleInfo);

typedef BOOL  (WINAPI *SymFromAddr_t)(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement,
									  PSYMBOL_INFO Symbol);

typedef BOOL  (WINAPI *SymGetLineFromAddr64_t)(HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement,
											   PIMAGEHLP_LINE64 Line);

typedef DWORD64 (WINAPI *SymLoadModule64_t)(HANDLE hProcess, HANDLE hFile, PCSTR ImageName,
											PCSTR ModuleName, DWORD64 BaseOfDll, DWORD SizeOfDll);

typedef struct _MINI_DUMP {
	CRITICAL_SECTION  csDebugLock;
	HANDLE            hDebugDll;
	HANDLE            hProcess;
	HANDLE            hSymSrv;
	DWORD64           dw64SymSrvBase;
	BOOL              dwFlags;
	HANDLE            hCrashLog;
	DWORD             dwMajorVer;
	DWORD             dwMinorVer;
	WCHAR             wszStartDir[MAX_PATH];
	size_t            stStartDirLen;
	BOOL              bTinyOnly;
	TCHAR             tszCrashDir[MAX_PATH];
	LPTSTR            tszSymbolPath;
	LPTOP_LEVEL_EXCEPTION_FILTER lpOldExceptionFilter;

	// pointers to functions in DbgHelp.dll
	MiniDumpWriteDump_t              pMiniDumpWriteDump;
	SymGetOptions_t                  pSymGetOptions;
	SymSetOptions_t                  pSymSetOptions;
	SymInitialize_t                  pSymInitialize;
	StackWalk64_t                    pStackWalk64;
	PFUNCTION_TABLE_ACCESS_ROUTINE64 pSymFuncTableAccess64;
	PGET_MODULE_BASE_ROUTINE64       pSymGetModuleBase64;
	SymGetModuleInfo64_t             pSymGetModuleInfo64;
	SymFromAddr_t                    pSymFromAddr;
	SymGetLineFromAddr64_t           pSymGetLineFromAddr64;
	SymLoadModule64_t                pSymLoadModule64;
	
} MINI_DUMP, *LPMINI_DUMP;

static MINI_DUMP MiniDump;

static LONG volatile lStackLogCount;


// Return -1 on error, or else size of written string in characters
static INT
iCrashPrintf(LPTSTR format, ...)
{
	TCHAR   tszBuf[1024];
	int     iReturn;
	DWORD   dwWrote;
	va_list argptr;

	if (MiniDump.hCrashLog == 0 || MiniDump.hCrashLog == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	va_start( argptr, format );
	iReturn = _vsntprintf_s(tszBuf, ARRAY_SIZE(tszBuf), _TRUNCATE, format, argptr );
	va_end( argptr );

	if (iReturn < 0)
	{
		// invalid arguments to the about printf throw an exception which can't be
		// handled since we are already in the exception handler and thus will
		// exit the process.  To get here the string must have been larger than the
		// buffer size.
		return -1;
	}
	if (!WriteFile(MiniDump.hCrashLog, tszBuf, iReturn * sizeof(TCHAR), &dwWrote, 0 ))
	{
		return -1;
	}

	return iReturn;
}


static BOOL
GetAccessToken(HANDLE *phToken)
{
	*phToken = NULL;

	if(!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, TRUE, phToken))
	{
		if(GetLastError() == ERROR_NO_TOKEN)
		{
			// No thread token, get process one
			if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, phToken))
			{
				return FALSE;
			}
		}
		else
		{
			return FALSE;
		}
	}
	return TRUE;
}


static BOOL
TestTokenPriv(HANDLE hToken, LPTSTR tszPrivilegeName, BOOL *pbState)
{
	LUID              luid;
	CHAR              pBuffer[512];
	TOKEN_PRIVILEGES *pPrivs;
	DWORD             dwLen, i;

	if ( !LookupPrivilegeValue(0, tszPrivilegeName, &luid ) )
	{
		return FALSE;
	}

	pPrivs = (TOKEN_PRIVILEGES *) pBuffer;
	if (!GetTokenInformation(hToken, TokenPrivileges, pPrivs, sizeof(pBuffer), &dwLen))
	{
		return FALSE;
	}

	for (i=0 ; i < pPrivs->PrivilegeCount ; i++)
	{
		if ((pPrivs->Privileges[i].Luid.HighPart == luid.HighPart) &&
			(pPrivs->Privileges[i].Luid.LowPart == luid.LowPart))
		{
			if (pPrivs->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED)
			{
				*pbState = TRUE;
			}
			else
			{
				*pbState = FALSE;
			}
			return TRUE;
		}
	}

	return FALSE;
}


static BOOL
EnableTokenPriv(HANDLE hToken, LPTSTR tszPrivilegeName)
{
	TOKEN_PRIVILEGES TP;

	TP.PrivilegeCount = 1;
	if (!LookupPrivilegeValue(0, tszPrivilegeName, &TP.Privileges[0].Luid))
	{
		return FALSE;
	}

	TP.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if ( !AdjustTokenPrivileges(hToken, FALSE, &TP, sizeof(TOKEN_PRIVILEGES), 0, 0) )
	{
		return FALSE;
	}
	if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
	{
		return FALSE;
	}
	return TRUE;
}


static LPTSTR
tszGetExceptionName(DWORD dwCode)
{
	switch (dwCode)
	{
	case EXCEPTION_ACCESS_VIOLATION:
		return _T("Access Violation");
	case EXCEPTION_DATATYPE_MISALIGNMENT:
		return _T("Datatype Misalignment ");
	case EXCEPTION_BREAKPOINT:
		return _T("Breakpoint");
	case EXCEPTION_SINGLE_STEP:
		return _T("Single Step");
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		return _T("Array Bounds Exceeded");
	case EXCEPTION_FLT_DENORMAL_OPERAND:
		return _T("Float - Denormal Operand");
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		return _T("Float - Divide by Zero");
	case EXCEPTION_FLT_INEXACT_RESULT:
		return _T("Float - Inexact Result");
	case EXCEPTION_FLT_INVALID_OPERATION:
		return _T("Float - Invalid Operation");
	case EXCEPTION_FLT_OVERFLOW:
		return _T("Float - Overflow");
	case EXCEPTION_FLT_STACK_CHECK:
		return _T("Float - Stack Check");
	case EXCEPTION_FLT_UNDERFLOW:
		return _T("Float - Underflow");
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		return _T("Integer - Divide by Zero");
	case EXCEPTION_INT_OVERFLOW:
		return _T("Integer - Overflow");
	case EXCEPTION_PRIV_INSTRUCTION:
		return _T("Privileged Instruction");
	case EXCEPTION_IN_PAGE_ERROR:
		return _T("In Page Error");
	case EXCEPTION_ILLEGAL_INSTRUCTION:
		return _T("Illegal Instruction");
	case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		return _T("Noncontinuable Exception");
	case EXCEPTION_STACK_OVERFLOW:
		return _T("Stack_overflow");
	case EXCEPTION_INVALID_DISPOSITION:
		return _T("Invalid Disposition");
	case EXCEPTION_GUARD_PAGE:
		return _T("Guard Page");
	case EXCEPTION_INVALID_HANDLE:
		return _T("Invalid Handle");
#ifdef STATUS_POSSIBLE_DEADLOCK // include ntstatus.h if needed
	case EXCEPTION_POSSIBLE_DEADLOCK: 
		return _T("Possible Deadlock");
#endif // STATUS_POSSIBLE_DEADLOCK
	}
	return _T("Unknown/User Defined");
}


static int
iWriteVersionInfo(LPTSTR tszBuf, DWORD dwLen, LPOSVERSIONINFOEX lpOSVI)
{
	LPTSTR  tszText;
	int     i, iLen;

	switch (lpOSVI->dwMajorVersion)
	{
	case 4:
		switch (lpOSVI->dwMinorVersion)
		{
		case 0:
			if (lpOSVI->dwPlatformId == VER_PLATFORM_WIN32_NT)
			{
				tszText = _T("NT4");
			}
			else
			{
				tszText = _T("95");
			}
			break;
		case 10:
			if (lpOSVI->szCSDVersion[1] == _T('A'))
			{
				tszText = _T("98SE");
			}
			else
			{
				tszText = _T("98");
			}
			break;
		case 90:
			tszText = _T("ME");
			break;
		}
		break;

	case 5:
		switch (lpOSVI->dwMinorVersion)
		{
		case 0:
			tszText = _T("2000");
			break;
		case 1:
			tszText = _T("XP");
			break;
		case 2:
			tszText = _T("Server 2003");
			break;
		}
		break;

	case 6:
		tszText = _T("Vista");
		break;
	}
	if (!tszText)
	{
		tszText = _T("UNKNOWN");
	}

	iLen = i = _sntprintf_s(tszBuf, dwLen, _TRUNCATE, _T("%s"), tszText);
	if (i >= 0 && GetSystemMetrics(SM_SERVERR2))
	{
		i = _sntprintf_s(tszBuf+iLen, dwLen-iLen, _TRUNCATE, _T(" R2"));
		iLen += i;
	}
	if (i >= 0 && lpOSVI->wSuiteMask & VER_SUITE_EMBEDDEDNT)
	{
		i = _sntprintf_s(tszBuf+iLen, dwLen-iLen, _TRUNCATE, _T(" Embedded"));
		iLen+=i;
	}
	if (i >= 0 && lpOSVI->wSuiteMask & VER_SUITE_PERSONAL)
	{
		i = _sntprintf_s(tszBuf+iLen, dwLen-iLen, _TRUNCATE, _T(" Home"));
		iLen+=i;
	}
	if (i >= 0 && GetSystemMetrics(SM_MEDIACENTER))
	{
		i = _sntprintf_s(tszBuf+iLen, dwLen-iLen, _TRUNCATE, _T(" Media Center Edition"));
		iLen+=i;
	}
	if (i >= 0 && GetSystemMetrics(SM_TABLETPC))
	{
		i = _sntprintf_s(tszBuf+iLen, dwLen-iLen, _TRUNCATE, _T(" Tablet PC Edition"));
		iLen+=i;
	}
	if (i >= 0 && GetSystemMetrics(SM_STARTER))
	{
		i = _sntprintf_s(tszBuf+iLen, dwLen-iLen, _TRUNCATE, _T(" Starter Edition"));
		iLen+=i;
	}
	if (i >= 0 && lpOSVI->wSuiteMask & VER_SUITE_BLADE)
	{
		i = _sntprintf_s(tszBuf+iLen, dwLen-iLen, _TRUNCATE, _T(" Web Edition"));
		iLen+=i;
	}
	if (i >= 0 && lpOSVI->wSuiteMask & VER_SUITE_COMPUTE_SERVER)
	{
		i = _sntprintf_s(tszBuf+iLen, dwLen-iLen, _TRUNCATE, _T(" Compute Cluster Edition"));
		iLen+=i;
	}
	if (i >= 0 && lpOSVI->wSuiteMask & VER_SUITE_DATACENTER)
	{
		i = _sntprintf_s(tszBuf+iLen, dwLen-iLen, _TRUNCATE, _T(" Datacenter Edition"));
		iLen+=i;
	}
	if (i >= 0 && lpOSVI->wSuiteMask & VER_SUITE_ENTERPRISE)
	{
		i = _sntprintf_s(tszBuf+iLen, dwLen-iLen, _TRUNCATE, _T(" Enterprise Edition"));
		iLen+=i;
	}
	if (i >= 0 && lpOSVI->wSuiteMask & VER_SUITE_STORAGE_SERVER)
	{
		i = _sntprintf_s(tszBuf+iLen, dwLen-iLen, _TRUNCATE, _T(" Storage"));
		iLen+=i;
	}

	if (i >= 0 && lpOSVI->szCSDVersion[0])
	{
		i = _sntprintf_s(tszBuf+iLen, dwLen-iLen, _TRUNCATE, _T(" - %s"), lpOSVI->szCSDVersion);
		iLen+=i;
	}


	if (i<0)
	{
		// an error of some sort occurred, return -1
		return -1;
	}
	return iLen;
}


static BOOL
GetLogicalAddress(PVOID addr, LPTSTR tszModuleName, DWORD dwNameLen, PDWORD pdwSectionNum, PDWORD pdwOffset)
{
	MEMORY_BASIC_INFORMATION MBI;
	HANDLE                   hModule;
	PIMAGE_DOS_HEADER        pDosHdr;
	PIMAGE_NT_HEADERS        pNtHdr;
	PIMAGE_SECTION_HEADER    pSectionHdr;
	DWORD                    pOffset; // SectionHdr uses DWORD for addresses in winnt.h - no idea on 64bit...
	DWORD                    i;

	if ( !VirtualQuery(addr, &MBI, sizeof(MBI)) )
	{
		return FALSE;
	}

	hModule = (HMODULE) MBI.AllocationBase;
	if ( !GetModuleFileName(hModule, tszModuleName, dwNameLen) )
	{
		return FALSE;
	}

	pDosHdr      = (PIMAGE_DOS_HEADER)hModule;
	pNtHdr       = (PIMAGE_NT_HEADERS)((PCHAR) hModule + pDosHdr->e_lfanew);
	pSectionHdr  = IMAGE_FIRST_SECTION( pNtHdr );

	pOffset      = (DWORD) addr - (DWORD) hModule;

	for (i=0 ; i < pNtHdr->FileHeader.NumberOfSections ; i++, pSectionHdr++ )
	{
		if ((pOffset >= pSectionHdr->VirtualAddress) &&
			(pOffset <= pSectionHdr->VirtualAddress + max(pSectionHdr->SizeOfRawData, pSectionHdr->Misc.VirtualSize)) )
		{
			// address is in this section
			*pdwSectionNum = i+1;
			*pdwOffset     = pOffset - pSectionHdr->VirtualAddress;
			return TRUE;
		}
	}
	return FALSE;
}


VOID 
EnableSymSrvPrompt()
{
	// enable the prompt for accessing MS's symbol server
	// indicate this a requested dump
	MiniDump.dwFlags |= MINI_PROMPT_OK;
}



static VOID
InitializeSymbolServer(BOOL bErrors)
{
	typedef UINT_PTR (CALLBACK *SymbolServerGetOptions_t)(void);
	typedef BOOL (CALLBACK *SymbolServerSetOptions_t)(UINT_PTR options, ULONG64 data);
	DWORD dwOpts;

	if (!MiniDump.pSymGetOptions)
	{
		return;
	}

	if (MiniDump.dwFlags & MINI_SYMBOLS_TRIED)
	{
		return;
	}

	MiniDump.dwFlags |= MINI_SYMBOLS_TRIED;

	dwOpts = (MiniDump.pSymGetOptions)();

	dwOpts |= (SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_LINES);
	// check to see if we are here because of "site LoadSymbols"
	if ( !(MiniDump.dwFlags & MINI_PROMPT_OK) )
	{
		dwOpts |= (SYMOPT_DEFERRED_LOADS | SYMOPT_NO_PROMPTS);
	}
	// get real names
	dwOpts &= ~SYMOPT_UNDNAME;

	MiniDump.pSymSetOptions(dwOpts);

	if (!(MiniDump.pSymInitialize)(MiniDump.hProcess, MiniDump.tszSymbolPath, TRUE))
	{
		if (bErrors)
		{
			iCrashPrintf("SymInitialize %u\r\n", GetLastError());
			return;
		}
	}

	MiniDump.dwFlags |= MINI_SYMBOLS_OK;
}



static VOID
WriteStackTrace(MINIDUMP_THREAD_CALLBACK *pThread)
{
	STACKFRAME64       SF;
	CONTEXT            Context;
	IMAGEHLP_MODULE64  ImgHlpModule;
	IMAGEHLP_LINE64    ImgHlpLine;
	DWORD              dwMachineType, dwLineDisplacement;
	DWORD64            dw64SymDisplacement;
	BYTE               SymbolBuf[sizeof(SYMBOL_INFO) + 1024];
	PSYMBOL_INFO       pSymbol;
	DWORD              dwFrame, dwSection, dwOffset;
	TCHAR              tszModulePath[MAX_PATH], *tszModuleName;
	BOOL               bHasLogical, bHasModule;


	ZeroMemory(&SF, sizeof(SF));

	// this only works for I386 machines.
	dwMachineType = IMAGE_FILE_MACHINE_I386;
	SF.AddrPC.Mode    = AddrModeFlat;
	SF.AddrPC.Offset    = pThread->Context.Eip;
	SF.AddrFrame.Mode = AddrModeFlat;
	SF.AddrFrame.Offset = pThread->Context.Ebp;
	SF.AddrStack.Mode = AddrModeFlat;
	SF.AddrStack.Offset = pThread->Context.Esp;

	// StackWalk64 claims it can modify the context.  Not sure if we are allowed to modify
	// the one passed to us via MiniDumpCallback
	CopyMemory(&Context, &pThread->Context, sizeof(Context));

	pSymbol               = (PSYMBOL_INFO) SymbolBuf;
	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen   = 1024;

	ImgHlpModule.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
	ImgHlpLine.SizeOfStruct = sizeof(IMAGEHLP_LINE);

	for(dwFrame = 1 ; ; dwFrame++)
	{
		// Get the next stack frame
		if ( ! (MiniDump.pStackWalk64)(  dwMachineType,
			MiniDump.hProcess,
			pThread->ThreadHandle,
			&SF,
			&pThread->Context,
			0,
			MiniDump.pSymFuncTableAccess64,
			MiniDump.pSymGetModuleBase64,
			0 ) )
		{
			break;
		}
		if (SF.AddrPC.Offset == SF.AddrReturn.Offset)
		{
			iCrashPrintf(_T("StackWalk64: ERROR - Recursive Callstack, address = 0x%I64X\r\n"),
				SF.AddrPC.Offset);
			break;
		}
#if 0
		if ( !SF.AddrFrame.Offset )
		{
			iCrashPrintf(_T("StackWalk64: ERROR - Bad frame, address = 0x%I64X\r\n"),
				SF.AddrFrame.Offset);
			break;
		}
#endif

		tszModuleName = 0;
		if (GetLogicalAddress((PVOID) SF.AddrPC.Offset, tszModulePath, ARRAY_SIZE(tszModulePath), &dwSection, &dwOffset))
		{
			bHasLogical = TRUE;
			tszModuleName = _tcsrchr(tszModulePath, _T('\\'));
			if (!tszModuleName)
			{
				tszModuleName = tszModulePath;
			}
		}
		else
		{
			bHasLogical = FALSE;
		}

		if ( (MiniDump.pSymGetModuleInfo64)(MiniDump.hProcess, SF.AddrPC.Offset, &ImgHlpModule) )
		{
			bHasModule = TRUE;
			tszModuleName = ImgHlpModule.ModuleName;
		}
		else if (!tszModuleName)
		{
			bHasModule = FALSE;
			tszModuleName = _T("[unknown module]");
		}

		if (bHasLogical)
		{
			if (dwSection > 1)
			{
				iCrashPrintf(_T("  #%2u: %08I64X -> [%s + %X:%08X]"), dwFrame, SF.AddrPC.Offset, tszModuleName, 
					dwSection, dwOffset);
			}
			else
			{
				iCrashPrintf(_T("  #%2u: %08I64X -> [%s + %X]"), dwFrame, SF.AddrPC.Offset, tszModuleName, dwOffset);
			}
		}
		else
		{
			iCrashPrintf(_T("  #%2u: %08I64X -> [%s]"), dwFrame, SF.AddrPC.Offset, tszModuleName);
		}

		if ( (MiniDump.pSymFromAddr)(MiniDump.hProcess, SF.AddrPC.Offset, &dw64SymDisplacement, pSymbol) )
		{
			if (bHasModule && ImgHlpModule.LineNumbers &&
				(MiniDump.pSymGetLineFromAddr64)(MiniDump.hProcess, SF.AddrPC.Offset, &dwLineDisplacement, &ImgHlpLine) )
			{
				iCrashPrintf(_T(" %s() + 0x%I64X\r\n"), pSymbol->Name, dw64SymDisplacement);
				iCrashPrintf(_T("                    [%s, line %u]\r\n"),
					ImgHlpLine.FileName, ImgHlpLine.LineNumber); 
			}
			else
			{
				iCrashPrintf(_T(" %s%s() + 0x%I64X\r\n"), (bHasModule && ImgHlpModule.SymType == SymPdb ? _T("") : _T("? ")),
					pSymbol->Name, dw64SymDisplacement);
			}
		}
		else
		{
			iCrashPrintf(_T("\r\n"));
		}
	}
	iCrashPrintf("\r\n\r\n");
}




VOID
LogStackTrace(LPTSTR tszFormat, ...)
{
	STACKFRAME64       SF;
	CONTEXT            Context;
	IMAGEHLP_MODULE64  ImgHlpModule;
	IMAGEHLP_LINE64    ImgHlpLine;
	DWORD              dwMachineType, dwLineDisplacement;
	DWORD64            dw64SymDisplacement;
	BYTE               SymbolBuf[sizeof(SYMBOL_INFO) + 1024];
	CHAR               szBuf[512];
	PSYMBOL_INFO       pSymbol;
	DWORD              dwFrame, dwSection, dwOffset;
	TCHAR              tszModulePath[MAX_PATH], *tszModuleName;
	BOOL               bHasLogical, bHasModule;
	LONG               lStackNum;
	va_list            argptr;

	va_start( argptr, tszFormat );
	_vsntprintf_s(SymbolBuf, ARRAY_SIZE(SymbolBuf), _TRUNCATE, tszFormat, argptr );
	va_end( argptr );

	Putlog(LOG_ERROR, _T("%s"), SymbolBuf);
	if (!MiniDump.pSymGetOptions)
	{
		return;
	}

	EnterCriticalSection(&MiniDump.csDebugLock);

	lStackNum = ++lStackLogCount;
	Putlog(LOG_ERROR, _T("(%d) %s"), lStackNum, SymbolBuf);

	ZeroMemory(&SF, sizeof(SF));
	ZeroMemory(&Context, sizeof(Context));
	Context.ContextFlags = CONTEXT_ALL;
	RtlCaptureContext(&Context);

	// this only works for I386 machines.
	dwMachineType = IMAGE_FILE_MACHINE_I386;
	SF.AddrPC.Mode      = AddrModeFlat;
	SF.AddrPC.Offset    = Context.Eip;
	SF.AddrFrame.Mode   = AddrModeFlat;
	SF.AddrFrame.Offset = Context.Ebp;
	SF.AddrStack.Mode   = AddrModeFlat;
	SF.AddrStack.Offset = Context.Esp;

	pSymbol               = (PSYMBOL_INFO) SymbolBuf;
	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen   = 1024;

	ImgHlpModule.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
	ImgHlpLine.SizeOfStruct = sizeof(IMAGEHLP_LINE);

	InitializeSymbolServer(FALSE);

	for(dwFrame = 1 ; ; dwFrame++)
	{
		// Get the next stack frame
		if ( ! (MiniDump.pStackWalk64)(  dwMachineType,
			GetCurrentProcess(),
			GetCurrentThread(),
			&SF,
			&Context,
			0,
			MiniDump.pSymFuncTableAccess64,
			MiniDump.pSymGetModuleBase64,
			0 ) )
		{
			break;
		}
		if (SF.AddrPC.Offset == SF.AddrReturn.Offset)
		{
			Putlog(LOG_DEBUG, _T("StackWalk64(%d): ERROR - Recursive Callstack, address = 0x%I64X\r\n"), lStackNum, SF.AddrPC.Offset);
			break;
		}
#if 0
		if ( !SF.AddrFrame.Offset )
		{
			iCrashPrintf(_T("StackWalk64: ERROR - Bad frame, address = 0x%I64X\r\n"),
				SF.AddrFrame.Offset);
			break;
		}
#endif

		tszModuleName = 0;
		if (GetLogicalAddress((PVOID) SF.AddrPC.Offset, tszModulePath, ARRAY_SIZE(tszModulePath), &dwSection, &dwOffset))
		{
			bHasLogical = TRUE;
			tszModuleName = _tcsrchr(tszModulePath, _T('\\'));
			if (!tszModuleName)
			{
				tszModuleName = tszModulePath;
			}
		}
		else
		{
			bHasLogical = FALSE;
		}

		if ( (MiniDump.pSymGetModuleInfo64)(MiniDump.hProcess, SF.AddrPC.Offset, &ImgHlpModule) )
		{
			bHasModule = TRUE;
			tszModuleName = ImgHlpModule.ModuleName;
		}
		else if (!tszModuleName)
		{
			bHasModule = FALSE;
			tszModuleName = _T("[unknown module]");
		}

		if (bHasLogical)
		{
			if (dwSection > 1)
			{
				sprintf_s(szBuf, sizeof(szBuf), "(%d)  #%2u: %08I64X -> [%s + %X:%08X]", lStackNum, dwFrame, SF.AddrPC.Offset, tszModuleName, dwSection, dwOffset);
			}
			else
			{
				sprintf_s(szBuf, sizeof(szBuf), "(%d)  #%2u: %08I64X -> [%s + %X]", lStackNum, dwFrame, SF.AddrPC.Offset, tszModuleName, dwOffset);
			}
		}
		else
		{
			sprintf_s(szBuf, sizeof(szBuf), "(%d)  #%2u: %08I64X -> [%s]", lStackNum, dwFrame, SF.AddrPC.Offset, tszModuleName);
		}

		if ( (MiniDump.pSymFromAddr)(MiniDump.hProcess, SF.AddrPC.Offset, &dw64SymDisplacement, pSymbol) )
		{
			if (bHasModule && ImgHlpModule.LineNumbers &&
				(MiniDump.pSymGetLineFromAddr64)(MiniDump.hProcess, SF.AddrPC.Offset, &dwLineDisplacement, &ImgHlpLine) )
			{
				Putlog(LOG_DEBUG, _T("%s %s() + 0x%I64X [%s, line %u]\r\n"),
					szBuf, pSymbol->Name, dw64SymDisplacement, ImgHlpLine.FileName, ImgHlpLine.LineNumber); 
			}
			else
			{
				Putlog(LOG_DEBUG, _T("%s %s%s() + 0x%I64X\r\n"), szBuf, (bHasModule && ImgHlpModule.SymType == SymPdb ? _T("") : _T("? ")),
					pSymbol->Name, dw64SymDisplacement);
			}
		}
		else
		{
			Putlog(LOG_DEBUG, _T("%s\r\n"), szBuf);
		}
	}
	LeaveCriticalSection(&MiniDump.csDebugLock);
}



static BOOL CALLBACK
ioMiniDumpCallback(PVOID                            pParam, 
				   PMINIDUMP_CALLBACK_INPUT         pInput, 
				   PMINIDUMP_CALLBACK_OUTPUT        pOutput 
				   ) 
{
	IMAGEHLP_MODULE64  ImgHlpModule;
	TCHAR             *ptszSymType[NumSymTypes] = { _T("none"), _T("COFF"), _T("CV"), _T("PDB"), _T("export"),
		                                            _T("deferred"), _T("SYM"), _T("DIA"), _T("virtual") };
	DWORD              dwAddr;
	CHAR               szTemp[MAX_PATH];

	switch(pInput->CallbackType)
	{
	case ModuleCallback:
		if (_wcsnicmp(pInput->Module.FullPath, MiniDump.wszStartDir, MiniDump.stStartDirLen))
		{
			// dll not loaded from startup directory, so don't dump non-referenced globals
			pOutput->ModuleWriteFlags &= ~ModuleWriteDataSeg;
		}
		if (MiniDump.dwFlags & MINI_NO_OUTPUT)
		{
			return TRUE;
		}
		if (!(MiniDump.dwFlags & MINI_MODULES_HEADER))
		{
			MiniDump.dwFlags |= MINI_MODULES_HEADER;
			iCrashPrintf(_T("\r\nModules:\r\n--------\r\n"));
		}
		dwAddr = (DWORD) pInput->Module.BaseOfImage;
		iCrashPrintf(_T("[%08x - %08x]: %ws (v%u.%u.%u.%u)\r\n"),
			dwAddr,
			dwAddr + pInput->Module.SizeOfImage,
			pInput->Module.FullPath,
			HIWORD(pInput->Module.VersionInfo.dwFileVersionMS),
			LOWORD(pInput->Module.VersionInfo.dwFileVersionMS),
			HIWORD(pInput->Module.VersionInfo.dwFileVersionLS),
			LOWORD(pInput->Module.VersionInfo.dwFileVersionLS));
		// only continue if executing a "site crashnow" since resolving the pdb's forces
		// the symbols of all the modules to be loaded and/or looked up via the symbol server.
		if ( !(MiniDump.dwFlags & MINI_SYMBOLS_OK) || !(MiniDump.dwFlags & MINI_PROMPT_OK))
		{
			return TRUE;
		}

		// make sure the module is registered
		_snprintf_s(szTemp,ARRAY_SIZE(szTemp), _TRUNCATE, "%ws", pInput->Module.FullPath);
		if ( !(MiniDump.pSymLoadModule64)(MiniDump.hProcess, 0, szTemp, 0,
			pInput->Module.BaseOfImage, pInput->Module.SizeOfImage))
		{
			if (GetLastError() != ERROR_SUCCESS)
			{
				iCrashPrintf(_T("                       Load Error: %u\r\n"), GetLastError());
				return TRUE;
			}
		}

		ImgHlpModule.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
		if ( (MiniDump.pSymGetModuleInfo64)(MiniDump.hProcess, pInput->Module.BaseOfImage, &ImgHlpModule) )
		{
			if (ImgHlpModule.SymType == SymPdb)
			{
				iCrashPrintf(_T("                       Symbol Type: PDB - %s\r\n"), ImgHlpModule.LoadedPdbName);
			}
			else
			{
				iCrashPrintf(_T("                       Symbol Type: %s\r\n"), ptszSymType[ImgHlpModule.SymType]);
			}
		}
		else
		{
			iCrashPrintf(_T("                       Error: %d\r\n"), GetLastError());
		}
		return TRUE;

	case ThreadCallback:
		if (pInput->Thread.ThreadId != GetCurrentThreadId())
		{
			// increment the suspend count for all threads but ours so that TINY dump and
			// MINI dump end up the same.  Since we are going to exit after creating
			// the dumps it doesn't matter...
			SuspendThread(pInput->Thread.ThreadHandle);
		}
		if (MiniDump.dwFlags & MINI_NO_OUTPUT)
		{
			return TRUE;
		}
		if (!(MiniDump.dwFlags & MINI_THREADS_HEADER))
		{
			MiniDump.dwFlags |= MINI_THREADS_HEADER;
			iCrashPrintf(_T("\r\n\r\nThreads:\r\n--------\r\n"));
		}
		iCrashPrintf(_T("ID: %-5u [%08x-%08x]\r\n"),
			pInput->Thread.ThreadId,
			(DWORD) pInput->Thread.StackBase,
			(DWORD) pInput->Thread.StackEnd);
		if (MiniDump.dwFlags & MINI_SYMBOLS_OK)
		{
			WriteStackTrace(&pInput->Thread);
		}
		return TRUE;

	case ThreadExCallback:
		return TRUE;

	case IncludeThreadCallback:
		// filter here if necessary
		return TRUE;

	case IncludeModuleCallback:
		// filter here if necessary
		return TRUE;

	case MemoryCallback:
		return TRUE;

	case CancelCallback:
		return TRUE;

	case WriteKernelMinidumpCallback:
		// this is for open handle data
		return TRUE;

	case KernelMinidumpStatusCallback:
		return TRUE;

	case RemoveMemoryCallback:
		return FALSE;

	case IncludeVmRegionCallback:
		pOutput->Continue = TRUE;
		return TRUE;

	case IoStartCallback:
	case IoWriteAllCallback: // shouldn't happen
	case IoFinishCallback:   // shouldn't happen
		// we don't want to do our own I/O
		return FALSE;

	case ReadMemoryFailureCallback:
		// ignore error
		pOutput->Status = S_OK;
		return TRUE;

	case SecondaryFlagsCallback:
		return TRUE;

	default:
		return FALSE;
	}
}


// NOTES:
// 1) Get the most important information, the crash address, output to the
//    CrashLog before any call which requires acquiring the Dll loader lock
//    since the crash/deadlock could result in this being held...
// 2) Generate the larger minidump.
// 3) Initialize the symbol resolving code
// 4) Generate the tiny minidump while also outputting information about the
//    threads and modules to the CrashLog.  Thread stacks will be walked and
//    addresses translated given available information.

static LONG WINAPI
UnhandledExceptionLogger(LPEXCEPTION_POINTERS lpExceptionInfo)
{
	SYSTEMTIME	SystemTime;
	SYSTEM_INFO siSysInfo;
	TCHAR       tszTemp[1024];
	DWORD       n, m, dwPid;
	INT         iLen;
	HANDLE      hDump, hAccessToken;
	LPTSTR      tszExceptionName;
	size_t      stCrash;
	LONG        lRet;
	HKEY       hKey;
	OSVERSIONINFOEX  OSVI;
	MINIDUMP_EXCEPTION_INFORMATION MiniExceptInfo;
	MINIDUMP_TYPE                  MiniDumpType;
	MINIDUMP_CALLBACK_INFORMATION  MiniCallInfo;


	// no need to ever release this since we plan on dieing anyway...
	EnterCriticalSection(&MiniDump.csDebugLock);

	// uncomment this to allow attaching a debugger since we can't get here if code was already under one
	// MessageBox(NULL, _T("Waiting..."), _T("Debug Me!!!!!"), MB_OK);

	stCrash = _tcslen(MiniDump.tszCrashDir);
	if (stCrash >= MAX_PATH-1)
	{
		exit(1);
	}
	MiniDump.tszCrashDir[stCrash++] = _T('\\');
	MiniDump.tszCrashDir[stCrash] = 0;
	if (_tcsncpy_s(&MiniDump.tszCrashDir[stCrash], MAX_PATH-stCrash, CRASH_LOG, _TRUNCATE) < 0)
	{
		exit(1);
	}

	MiniDump.hCrashLog  = CreateFile(MiniDump.tszCrashDir, GENERIC_WRITE, FILE_SHARE_READ, 0,
		OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH, 0);
	if (MiniDump.hCrashLog == 0 || MiniDump.hCrashLog == INVALID_HANDLE_VALUE)
	{
		exit(1);
	}
	SetFilePointer(MiniDump.hCrashLog, 0, 0, FILE_END);

	tszExceptionName = tszGetExceptionName(lpExceptionInfo->ExceptionRecord->ExceptionCode);

	GetLocalTime(&SystemTime);
	// format: Wed Jan 02 02:03:55 1980
	iCrashPrintf(_T("%hs %hs %02u %02u:%02u:%02u %04u - ioFTPD v%u.%u.%u\r\nUnhandled exception: %s (0x%08X)\r\n"),
		WeekDay3[SystemTime.wDayOfWeek], Months[SystemTime.wMonth], SystemTime.wDay,
		SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond, SystemTime.wYear,
		dwIoVersion[0], dwIoVersion[1], dwIoVersion[2],
		tszExceptionName, lpExceptionInfo->ExceptionRecord->ExceptionCode);


	switch (lpExceptionInfo->ExceptionRecord->ExceptionCode)
	{
	case EXCEPTION_ACCESS_VIOLATION:
	case EXCEPTION_IN_PAGE_ERROR:
		if (lpExceptionInfo->ExceptionRecord->NumberParameters >= 2)
		{
			switch (lpExceptionInfo->ExceptionRecord->ExceptionInformation[0])
			{
			case 0:
				iCrashPrintf(_T("Address: 0x%08X [attempting to read data from 0x%08X]\r\n"),
					lpExceptionInfo->ExceptionRecord->ExceptionAddress,
					lpExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
				break;
			case 1:
				iCrashPrintf(_T("Address: 0x%08X [attempting to write data to 0x%08X]\r\n"),
					lpExceptionInfo->ExceptionRecord->ExceptionAddress,
					lpExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
				break;
			case 8:
				iCrashPrintf(_T("Address: 0x%08X [data execution prevention (DEP) violation for data at 0x%08X]\r\n"),
					lpExceptionInfo->ExceptionRecord->ExceptionAddress,
					lpExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
				break;
			default:
				iCrashPrintf(_T("Address: 0x%08X [UNKNOWN action]\r\n"),
					lpExceptionInfo->ExceptionRecord->ExceptionAddress);
				break;
			}
			break;
		}
	default:
		iCrashPrintf(_T("Address: 0x%08X\r\n"),
			lpExceptionInfo->ExceptionRecord->ExceptionAddress);
		break;
	}

	// At this point the most important information has been printed to the Crash Log.
	// Now feel free to call functions that may try to acquire locks and to generate
	// the minidump.

	dwPid = GetCurrentProcessId();
	iCrashPrintf(_T("PID=%u, PATH=%s\\%s\r\n"), dwPid, tszExePath, tszExeName);
	iCrashPrintf(_T("Thread ID: %u\r\n"), GetCurrentThreadId());

	iCrashPrintf(_T("\r\nSystem information:\r\n"));

	ZeroMemory(&siSysInfo, sizeof(SYSTEM_INFO));
	GetSystemInfo(&siSysInfo);

	hKey = INVALID_HANDLE_VALUE;
	for(m=0 ; m<siSysInfo.dwNumberOfProcessors ; m++)
	{
		if (hKey && hKey != INVALID_HANDLE_VALUE)
		{
			RegCloseKey( hKey );
		}
		_sntprintf_s(tszTemp, ARRAY_SIZE(tszTemp), _TRUNCATE,
			_T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\%d"), m);
		lRet = RegOpenKeyEx( HKEY_LOCAL_MACHINE, tszTemp, 0, KEY_QUERY_VALUE, &hKey );
		if ( lRet != ERROR_SUCCESS ) continue;

		n = sizeof(tszTemp); // in bytes not TCHARs
		lRet = RegQueryValueEx( hKey, _T("ProcessorNameString"), NULL, NULL,(LPBYTE) tszTemp, &n);
		if ( lRet != ERROR_SUCCESS ) continue;

		iCrashPrintf(_T("Processor #%u Name: %s\r\n"), m, tszTemp);

		n = sizeof(tszTemp); // in bytes not TCHARs
		lRet = RegQueryValueEx( hKey, _T("Identifier"), NULL, NULL,(LPBYTE) tszTemp, &n);
		if ( lRet != ERROR_SUCCESS ) continue;

		iCrashPrintf(_T("Processor #%u Identifier: %s\r\n"), m, tszTemp);
	}
	if (hKey && hKey != INVALID_HANDLE_VALUE)
	{
		RegCloseKey( hKey );
	}

	ZeroMemory(&OSVI, sizeof(OSVERSIONINFOEX));
	OSVI.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

	n = GetVersionEx( (OSVERSIONINFO *) &OSVI);
	if (!n)
	{
		OSVI.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		n = GetVersionEx( (OSVERSIONINFO *) &OSVI);
	}

	iCrashPrintf(_T("OS: Windows %u.%u (build %u)\r\n"),
		OSVI.dwMajorVersion, OSVI.dwMinorVersion, OSVI.dwBuildNumber);

	lRet = RegOpenKeyEx( HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows NT\\CurrentVersion"), 
		0, KEY_QUERY_VALUE, &hKey );
	if ( lRet == ERROR_SUCCESS )
	{
		n = sizeof(tszTemp); // in bytes not TCHARs
		lRet = RegQueryValueEx( hKey, _T("ProductName"), 0, 0,(LPBYTE) tszTemp, &n);
		if ( lRet == ERROR_SUCCESS )
		{
			iCrashPrintf(_T("Registry: %s\r\n"), tszTemp);
		}
		RegCloseKey( hKey );
	}

	iLen = iWriteVersionInfo(tszTemp, ARRAY_SIZE(tszTemp), &OSVI);
	if (iLen > 0)
	{
		iCrashPrintf(_T("Decoded: %s\r\n"), tszTemp);
	}

	iCrashPrintf(_T("Page size: %u\r\n"), siSysInfo.dwPageSize);

	// Now generate the actual minidump, it will call the callback for each thread/module/etc
	if (MiniDump.pMiniDumpWriteDump)
	{
		MiniExceptInfo.ThreadId          = GetCurrentThreadId();
		MiniExceptInfo.ExceptionPointers = lpExceptionInfo;
		MiniExceptInfo.ClientPointers    = TRUE;

		MiniCallInfo.CallbackRoutine = ioMiniDumpCallback;
		MiniCallInfo.CallbackParam   = 0;

		if (GetAccessToken(&hAccessToken))
		{
			if ( !EnableTokenPriv(hAccessToken, SE_DEBUG_NAME) )
			{
				iCrashPrintf(_T("Notice: Unable to acquire Debug Rights\r\n"));
			}
		}
		else
		{
			iCrashPrintf(_T("ERROR: Unable to examine Access Token\r\n"));
		}

		// Write the best MiniDump we can minus the read-only code sections of modules...
		if (MiniDump.dwMajorVer >= 6 && MiniDump.dwMinorVer > 1)
		{
			MiniDumpType = (MiniDumpWithPrivateReadWriteMemory | MiniDumpWithDataSegs | MiniDumpWithHandleData |
				MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
		}
		else if (MiniDump.dwMajorVer >= 5 && MiniDump.dwMinorVer > 1)
		{
			MiniDumpType = (MiniDumpWithPrivateReadWriteMemory | MiniDumpWithDataSegs | MiniDumpWithHandleData |
				MiniDumpWithUnloadedModules);
		}
		else // must be 5.1 since MiniDumpWriteDump didn't exist prior
		{
			MiniDumpType = (MiniDumpWithDataSegs | MiniDumpWithHandleData);
		}

		// ...\MINIDUMP-<PID>-YYYYMMDD-HHMMSS.dmp
		_sntprintf_s(&MiniDump.tszCrashDir[stCrash], MAX_PATH-stCrash, _TRUNCATE,
			"MINIDUMP-%04d%02d%02d.%02d%02d%02d-%d.dmp", 
			SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay,
			SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond, dwPid);

		if (!MiniDump.bTinyOnly)
		{
			hDump = CreateFile(MiniDump.tszCrashDir, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0,
				CREATE_NEW, FILE_FLAG_WRITE_THROUGH, 0);

			if (hDump && hDump != INVALID_HANDLE_VALUE)
			{
				// the first dump shouldn't try to use the symbol server...
				// It will however increment the suspend count of all threads but this one so TinyDump
				// will match.
				MiniDump.dwFlags |= MINI_NO_OUTPUT;

				// create the dump...
				if (!(MiniDump.pMiniDumpWriteDump)(GetCurrentProcess(), GetCurrentProcessId(),
					hDump, MiniDumpType, &MiniExceptInfo, 0, &MiniCallInfo))
				{
					iCrashPrintf(_T("\r\nMiniDumpError = %u\r\n"), GetLastError());
				}

				FlushFileBuffers(hDump);
				CloseHandle(hDump);
				hDump = 0;
			}
		}


		// now create a really small dump of just stuff on the stack, but this time output module/thread
		// details to the crash log file using the symbol server.
		if (MiniDump.dwMajorVer >= 6 && MiniDump.dwMinorVer > 1)
		{
			// MiniDumpWithoutOptionalData seems to remove memory regions that IndirectlyReferencedMemory
			// would like included which means you can't evaluate pointers on the stack...
			// FilterMemory and FilterModulePaths might eliminate any remaining private data, but the paranoid
			// wouldn't trust it anyway.  Thus stack traces in text form via CrashLog.txt so people can see everything.
			MiniDumpType = (MiniDumpScanMemory | MiniDumpWithIndirectlyReferencedMemory |
				MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo );
		}
		else if (MiniDump.dwMajorVer >= 5 && MiniDump.dwMinorVer > 1)
		{
			MiniDumpType = (MiniDumpScanMemory | MiniDumpWithIndirectlyReferencedMemory);
		}
		else // must be 5.1 since MiniDumpWriteDump didn't exist prior
		{
			MiniDumpType = MiniDumpScanMemory;
		}

		_sntprintf_s(&MiniDump.tszCrashDir[stCrash], MAX_PATH-stCrash, _TRUNCATE,
			"TINYDUMP-%04d%02d%02d.%02d%02d%02d-%d.dmp", 
			SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay,
			SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond, dwPid);

		hDump = CreateFile(MiniDump.tszCrashDir, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0,
			CREATE_NEW, FILE_FLAG_WRITE_THROUGH, 0);

		if (hDump && hDump != INVALID_HANDLE_VALUE)
		{
			// we want to write to CrashLog this time
			MiniDump.dwFlags &= ~MINI_NO_OUTPUT;

			InitializeSymbolServer(TRUE);

			// create the dump...
			if (!(MiniDump.pMiniDumpWriteDump)(GetCurrentProcess(), GetCurrentProcessId(),
				hDump, MiniDumpType, &MiniExceptInfo, 0, &MiniCallInfo))
			{
				iCrashPrintf(_T("TinyDumpError = %u\r\n"), GetLastError());
			}

			FlushFileBuffers(hDump);
			CloseHandle(hDump);
		}
	}

	iCrashPrintf(_T("\r\n-------------------------------------------------------------------------------\r\n\r\n"));

	FlushFileBuffers(MiniDump.hCrashLog);
	CloseHandle(MiniDump.hCrashLog);
	//  Terminate process
	exit(1);
}



// note: using major.minor.revision.build not major.minor.build.revision (MSDN)
BOOL
GetFileVersion(LPTSTR tszFilePath, DWORD *pdwMajor, DWORD *pdwMinor,
			   DWORD *pdwRev, DWORD *pdwBuild)
{
	DWORD                dwVerLen, dwZero;
	VS_FIXEDFILEINFO    *lpVsFileInfo;
	UINT                 uFixedLen;
	BOOL                 bResult = FALSE;
	char                *lpVersionBuf;

	if (pdwMajor)  *pdwMajor = 0;
	if (pdwMinor)  *pdwMinor = 0;
	if (pdwRev)    *pdwRev   = 0;
	if (pdwBuild)  *pdwBuild = 0;

	dwVerLen = GetFileVersionInfoSize(tszFilePath, &dwZero);
	if ( dwVerLen && (dwVerLen < 10*1024) && (lpVersionBuf = (char *) _alloca(dwVerLen)))
	{
		if (GetFileVersionInfo(tszFilePath, 0, dwVerLen, lpVersionBuf))
		{
			if (VerQueryValue(lpVersionBuf, _T("\\"), (LPVOID *) &lpVsFileInfo, &uFixedLen))
			{
				if (pdwMajor) *pdwMajor = HIWORD(lpVsFileInfo->dwFileVersionMS);
				if (pdwMinor) *pdwMinor = LOWORD(lpVsFileInfo->dwFileVersionMS);
				if (pdwRev)   *pdwRev   = HIWORD(lpVsFileInfo->dwFileVersionLS);
				if (pdwBuild) *pdwBuild = LOWORD(lpVsFileInfo->dwFileVersionLS);
				return FALSE;
			}
		}
	}
	return TRUE;
}


static int
iCompareFileVersions(DWORD *pdwVersion1, DWORD *pdwVersion2)
{
	if (pdwVersion1[0] != pdwVersion2[0])
	{
		return pdwVersion1[0] - pdwVersion2[0];
	}

	if (pdwVersion1[1] != pdwVersion2[1])
	{
		return pdwVersion1[1] - pdwVersion2[1];
	}

	if (pdwVersion1[2] != pdwVersion2[2])
	{
		return pdwVersion1[2] - pdwVersion2[2];
	}

	if (pdwVersion1[3] != pdwVersion2[3])
	{
		return pdwVersion1[3] - pdwVersion2[3];
	}
	return 0;
}



// returns NULL on error
HANDLE
LoadLatestLibrary(LPTSTR tszDllName, LPTSTR tszExtraDir)
{
	TCHAR    tszLocalPath[MAX_PATH], tszSysPath[MAX_PATH], tszExtraPath[MAX_PATH];
	TCHAR   *tszLoad;
	DWORD    pdwLocalVer[4], pdwSysVer[4], pdwExtraVer[4], *pdwVer;
	DWORD    dwLocalAttr, dwExtraAttr, dwSysAttr, dwAttr;
	size_t   stLen;
	HANDLE   hDll;


	if (_sntprintf_s(tszLocalPath, ARRAY_SIZE(tszLocalPath), _TRUNCATE, "%s\\%s", tszExePath, tszDllName) < 0)
	{
		dwLocalAttr = INVALID_FILE_ATTRIBUTES;
	}
	else
	{
		dwLocalAttr = GetFileAttributes(tszLocalPath);
		if (dwLocalAttr != INVALID_FILE_ATTRIBUTES &&
			GetFileVersion(tszLocalPath, &pdwLocalVer[0], &pdwLocalVer[1], &pdwLocalVer[2], &pdwLocalVer[3]))
		{
			dwLocalAttr = INVALID_FILE_ATTRIBUTES;
		}
	}

	if (!tszExtraDir || (_sntprintf_s(tszExtraPath, ARRAY_SIZE(tszExtraPath), _TRUNCATE, "%s\\%s",
		tszExtraDir, tszDllName) < 0))
	{
		dwExtraAttr = INVALID_FILE_ATTRIBUTES;
	}
	else
	{
		dwExtraAttr = GetFileAttributes(tszExtraPath);
		if (GetFileVersion(tszExtraPath, &pdwExtraVer[0], &pdwExtraVer[1], &pdwExtraVer[2], &pdwExtraVer[3]))
		{
			dwExtraAttr = INVALID_FILE_ATTRIBUTES;
		}
	}

	stLen = GetSystemDirectory(tszSysPath, ARRAY_SIZE(tszSysPath));
	if (stLen == 0 || stLen > ARRAY_SIZE(tszSysPath) ||
		(_sntprintf_s(tszSysPath, ARRAY_SIZE(tszSysPath), _TRUNCATE, "%s\\%s", tszSysPath, tszDllName) < 0))
	{
		dwSysAttr = INVALID_FILE_ATTRIBUTES;
	}
	else
	{
		dwSysAttr = GetFileAttributes(tszSysPath);
		if (GetFileVersion(tszSysPath, &pdwSysVer[0], &pdwSysVer[1], &pdwSysVer[2], &pdwSysVer[3]))
		{
			dwSysAttr = INVALID_FILE_ATTRIBUTES;
		}
	}

	tszLoad = tszLocalPath;
	pdwVer  = pdwLocalVer;
	dwAttr  = dwLocalAttr;
	if (dwExtraAttr != INVALID_FILE_ATTRIBUTES)
	{
		if (dwLocalAttr == INVALID_FILE_ATTRIBUTES || (iCompareFileVersions(pdwLocalVer, pdwExtraVer) < 0))
		{
			tszLoad = tszExtraPath;
			pdwVer  = pdwExtraVer;
			dwAttr  = dwExtraAttr;
		}
	}

	if (dwSysAttr != INVALID_FILE_ATTRIBUTES)
	{
		if (dwAttr == INVALID_FILE_ATTRIBUTES || (iCompareFileVersions(pdwVer, pdwSysVer) < 0))
		{
			tszLoad = tszSysPath;
			dwAttr  = dwSysAttr;
		}
	}

	if (dwAttr != INVALID_FILE_ATTRIBUTES)
	{
		if ( hDll = LoadLibrary(tszLoad) )
		{
			return hDll;
		}
	}
	// just try the plain name...
	return LoadLibrary(tszDllName);
}




VOID
InitializeExceptionHandler(LPTSTR tszStartDir)
{
	typedef LPAPI_VERSION (WINAPI *ImagehlpApiVersionEx_t)(LPAPI_VERSION);

	API_VERSION             ApiVersionWanted, *pApiVer;
	ImagehlpApiVersionEx_t  pApiVersionEx;
	TCHAR    tszPath[MAX_PATH];
	DWORD    dwSize;
	LONG     lRet;
	HKEY     hKey;


	ZeroMemory(&MiniDump, sizeof(MiniDump));

	MiniDump.hProcess = GetCurrentProcess();
	InitializeCriticalSection(&MiniDump.csDebugLock);

	_tcscpy_s(MiniDump.tszCrashDir, MAX_PATH, tszStartDir);
	swprintf_s(MiniDump.wszStartDir, ARRAY_SIZE(MiniDump.wszStartDir), L"%hs", tszStartDir);
	MiniDump.stStartDirLen = wcslen(MiniDump.wszStartDir);

	tszPath[0] = 0;
	lRet = RegOpenKeyEx( HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion"), 
		0, KEY_QUERY_VALUE, &hKey );
	if ( lRet == ERROR_SUCCESS )
	{
		dwSize = sizeof(tszPath); // in bytes not TCHARs
		lRet = RegQueryValueEx( hKey, _T("ProgramFilesDir"), 0, 0,(LPBYTE) tszPath, &dwSize);
		if ( lRet == ERROR_SUCCESS && ( (dwSize+sizeof(MINIDUMP_DIR)) < sizeof(tszPath) ) )
		{
			dwSize /= sizeof(TCHAR);
			if (dwSize > 0)
			{
				dwSize--;
				_tcscpy_s(&tszPath[dwSize], ARRAY_SIZE(tszPath)-dwSize, MINIDUMP_DIR);
			}
		}
		else
		{
			tszPath[0] = 0;
		}
		RegCloseKey( hKey );
	}


	MiniDump.hDebugDll = LoadLatestLibrary(MINIDUMP_DLL, (tszPath[0] ? tszPath : 0));

	if (MiniDump.hDebugDll)
	{
		ApiVersionWanted.MajorVersion = 4;
		ApiVersionWanted.MinorVersion = 0;
		ApiVersionWanted.Revision     = 5;
		ApiVersionWanted.Reserved     = 0;

		dwSize = GetModuleFileName(MiniDump.hDebugDll, tszPath, ARRAY_SIZE(tszPath));
		if (dwSize != 0 && dwSize != ARRAY_SIZE(tszPath))
		{
			GetFileVersion(tszPath, &MiniDump.dwMajorVer, &MiniDump.dwMinorVer, 0, 0);
		}

		pApiVersionEx = (ImagehlpApiVersionEx_t) GetProcAddress(MiniDump.hDebugDll, "ImagehlpApiVersionEx");
		if (pApiVersionEx)
		{
			pApiVer = (pApiVersionEx)(&ApiVersionWanted);
		}

		// Everything below is available in v5.1+ so default XP will have.
		MiniDump.pMiniDumpWriteDump    = (MiniDumpWriteDump_t)    GetProcAddress(MiniDump.hDebugDll, "MiniDumpWriteDump");
		MiniDump.pSymGetOptions        = (SymGetOptions_t)        GetProcAddress(MiniDump.hDebugDll, "SymGetOptions");
		MiniDump.pSymSetOptions        = (SymSetOptions_t)        GetProcAddress(MiniDump.hDebugDll, "SymSetOptions");
		MiniDump.pSymInitialize        = (SymInitialize_t)        GetProcAddress(MiniDump.hDebugDll, "SymInitialize");
		MiniDump.pStackWalk64          = (StackWalk64_t)          GetProcAddress(MiniDump.hDebugDll, "StackWalk64");
		MiniDump.pSymFuncTableAccess64 = (PFUNCTION_TABLE_ACCESS_ROUTINE64) GetProcAddress(MiniDump.hDebugDll, "SymFunctionTableAccess64");
		MiniDump.pSymGetModuleBase64   = (PGET_MODULE_BASE_ROUTINE64)       GetProcAddress(MiniDump.hDebugDll, "SymGetModuleBase64");
		MiniDump.pSymGetModuleInfo64   = (SymGetModuleInfo64_t)   GetProcAddress(MiniDump.hDebugDll, "SymGetModuleInfo64");
		MiniDump.pSymFromAddr          = (SymFromAddr_t)          GetProcAddress(MiniDump.hDebugDll, "SymFromAddr");
		MiniDump.pSymGetLineFromAddr64 = (SymGetLineFromAddr64_t) GetProcAddress(MiniDump.hDebugDll, "SymGetLineFromAddr64");
		// SymLoadModuleEx is 6.0+ so using the older version
		MiniDump.pSymLoadModule64      = (SymLoadModule64_t)      GetProcAddress(MiniDump.hDebugDll, "SymLoadModule64");

		if (!MiniDump.pSymGetOptions || !MiniDump.pSymSetOptions || !MiniDump.pSymInitialize || !MiniDump.pStackWalk64 ||
			!MiniDump.pSymFuncTableAccess64 || !MiniDump.pSymGetModuleBase64 || !MiniDump.pSymGetModuleInfo64 ||
			!MiniDump.pSymFromAddr || !MiniDump.pSymGetLineFromAddr64 || !MiniDump.pSymLoadModule64)
		{
			// we can test just this one option later
			MiniDump.pSymGetOptions = 0;
		}
	}

	MiniDump.lpOldExceptionFilter = SetUnhandledExceptionFilter(UnhandledExceptionLogger);
}


BOOL
Debug_Init(BOOL bFirstInitialization)
{
	TCHAR   tszTemp[MAX_PATH];
	LPTSTR  tszPath, tszSymPath;
	DWORD   dwResult;
	size_t  stLen;
	int     iWrote;

	if (!bFirstInitialization)
	{
		return TRUE;
	}

#if 0
	HANDLE  hAccessToken;
	BOOL bStatus;
	if (!GetAccessToken(&hAccessToken) ||
		!TestTokenPriv(hAccessToken, SE_DEBUG_NAME, &bStatus) ||
		bStatus)
	{
		Putlog(LOG_GENERAL, _T("NOTICE: \"Process does not have debug permissions - tiny/minidump disabled.\"\r\n"));
	}
#endif

	tszPath = Config_Get(&IniConfigFile, _T("Locations"), _T("Crash_Dir"), 0, 0);
	if (tszPath)
	{
		if ((dwResult = GetFullPathName(tszPath, ARRAY_SIZE(tszTemp), tszTemp, 0)) &&
			(dwResult < MAX_PATH) &&
			((dwResult = GetFileAttributes(tszTemp)) != INVALID_FILE_ATTRIBUTES) &&
			(dwResult & FILE_ATTRIBUTE_DIRECTORY))
		{
			// it's valid so store it... In theory if the server crashed during the
			// copy of a DIFFERENT path this could write the file in the wrong place.
			strcpy_s(MiniDump.tszCrashDir, ARRAY_SIZE(MiniDump.tszCrashDir), tszTemp);
		}
		Free(tszPath);
	}

	Config_Get_Bool(&IniConfigFile, _T("Locations"), _T("TinyDump_Only"), &MiniDump.bTinyOnly);


	// Under what situation should we use http://msdl.microsoft.com/download/symbols ?

	tszPath = Config_Get(&IniConfigFile, _T("Locations"), _T("Symbol_Path"), 0, 0);
	stLen = MiniDump.stStartDirLen +1;
	if (tszPath)
	{
		stLen += _tcslen(tszPath) + 1; // for the ;
	}
	tszSymPath = Allocate("Location:Symbol_Path", stLen*sizeof(TCHAR));
	if (!tszSymPath)
	{
		return FALSE;
	}
	if (tszPath)
	{
		iWrote = _sntprintf_s(tszSymPath, stLen, _TRUNCATE, _T("%ws;%s"), MiniDump.wszStartDir, tszPath);
	}
	else
	{
		iWrote = _sntprintf_s(tszSymPath, stLen, _TRUNCATE, _T("%ws"), MiniDump.wszStartDir);
	}
	if (iWrote < 0)
	{
		return FALSE;
	}
	MiniDump.tszSymbolPath = tszSymPath;

	return TRUE;

}


VOID Debug_DeInit(VOID)
{
#ifdef _DEBUG
	DWORD dwError;

	// restore original exception hander
	SetUnhandledExceptionFilter(MiniDump.lpOldExceptionFilter);

	if (MiniDump.hDebugDll) 
	{
		dwError = FreeLibrary(MiniDump.hDebugDll);
	}

	DeleteCriticalSection(&MiniDump.csDebugLock);
	Free(MiniDump.tszSymbolPath);
#endif
}

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


static LPMESSAGE_VARIABLE	*lpMessageVariable;
static LPVARIABLE_MODULE	lpMessageVariableModules;
static DWORD				dwMessageVariables, dwMessageVariablesAllocated;
static DWORD                dwAlwaysZero;




/*

  %[$groupname]

  */
LPVOID MessageVariable_GroupName(LPMESSAGEDATA lpData)
{	
	return Gid2Group(GETOFFSET(lpData->DataOffsets, DATA_GROUPFILE)->Gid);
}


/*

  %[$creditsection]

  */
LPVOID MessageVariable_CreditSection(LPMESSAGEDATA lpData)
{
	if (lpData->dwData == DT_USERFILE_PLUS)
	{
		return &GETOFFSET(lpData->DataOffsets, DATA_CCHANNEL)->Path.CreditSection;
	}
	return &GETOFFSET(lpData->DataOffsets, DATA_CSECTION);
}


/*

  %[$statssection]

  */
LPVOID MessageVariable_StatsSection(LPMESSAGEDATA lpData)
{
	if (lpData->dwData == DT_USERFILE_PLUS)
	{
		return &GETOFFSET(lpData->DataOffsets, DATA_CCHANNEL)->Path.StatsSection;
	}
	return &GETOFFSET(lpData->DataOffsets, DATA_SSECTION);
}



/*

%[$sharesection]

*/
LPVOID MessageVariable_ShareSection(LPMESSAGEDATA lpData)
{
	if (lpData->dwData == DT_USERFILE_PLUS)
	{
		return &GETOFFSET(lpData->DataOffsets, DATA_CCHANNEL)->Path.ShareSection;
	}
	return &GETOFFSET(lpData->DataOffsets, DATA_SHARESECTION);
}



LPVOID MessageVariable_HomeDirectory(LPMESSAGEDATA lpData)
{
	return GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->Home;
}


/*

%[$VFS]

*/
LPVOID MessageVariable_VFS(LPMESSAGEDATA lpData)
{
	LPSTR szVFS;

	szVFS = GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->MountFile;

	if (!szVFS[0])
	{
		return "<default>";
	}
	else
	{
		return szVFS;
	}
}



/*

  %[$gid]

  */
LPVOID MessageVariable_Gid(LPMESSAGEDATA lpData)
{
	return &GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->Gid;
}


/*

%[$uid]

*/
LPVOID MessageVariable_Uid(LPMESSAGEDATA lpData)
{
	return &GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->Uid;
}



/*

%[$Downloads]

*/
LPVOID MessageVariable_Downloads(LPMESSAGEDATA lpData)
{
	LPPARENT_USERFILE  lpParentUserFile;
	LPUSERFILE         lpUserFile;
	
	lpUserFile = GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	//  Get parent userfile
	lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile->lpParent;

	if (!lpParentUserFile) return &dwAlwaysZero;
	return (LPVOID) &lpParentUserFile->lDownloads;
}


/*

%[$Uploads]

*/
LPVOID MessageVariable_Uploads(LPMESSAGEDATA lpData)
{
	LPPARENT_USERFILE  lpParentUserFile;
	LPUSERFILE         lpUserFile;

	lpUserFile = GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	//  Get parent userfile
	lpParentUserFile  = (LPPARENT_USERFILE)lpUserFile->lpParent;

	if (!lpParentUserFile) return &dwAlwaysZero;
	return (LPVOID) &lpParentUserFile->lUploads;
}



/*

%[$MaxDownloads]

*/
LPVOID MessageVariable_MaxDownloads(LPMESSAGEDATA lpData)
{
	return &GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->MaxDownloads;
}


/*

%[$MaxUploads]

*/
LPVOID MessageVariable_MaxUploads(LPMESSAGEDATA lpData)
{
	return &GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->MaxUploads;
}

/*

%[$LogonCount]

*/
LPVOID MessageVariable_LogonCount(LPMESSAGEDATA lpData)
{
	return &GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->LogonCount;
}


/*

%[$LogonLast]

*/
LPVOID MessageVariable_LogonLast(LPMESSAGEDATA lpData)
{
	// this is really a 64 bit value getting trimmed to 32...
	return &GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->LogonLast;
}


/*

%[$FTPLogins]

*/
LPVOID MessageVariable_FTPLogins(LPMESSAGEDATA lpData)
{
	LPPARENT_USERFILE  lpParentUserFile;

	//  Get parent userfile
	lpParentUserFile  = (LPPARENT_USERFILE)(GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->lpParent);

	if (!lpParentUserFile) return &dwAlwaysZero;
	return (LPVOID) &lpParentUserFile->lLoginCount[C_FTP];
}


/*

%[$CreatedOn]

*/
LPVOID MessageVariable_CreatedOn(LPMESSAGEDATA lpData)
{
	// this is really a 64 bit value getting trimmed to 32...
	return &GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->CreatedOn;
}


/*

%[$ExpiresAt]

*/
LPVOID MessageVariable_ExpiresAt(LPMESSAGEDATA lpData)
{
	// this is really a 64 bit value getting trimmed to 32...
	return &GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->ExpiresAt;
}


/*

%[$DeletedOn]

*/
LPVOID MessageVariable_DeletedOn(LPMESSAGEDATA lpData)
{
	// this is really a 64 bit value getting trimmed to 32...
	return &GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->DeletedOn;
}


/*

%[$DeletedBy]

*/
LPVOID MessageVariable_DeletedBy(LPMESSAGEDATA lpData)
{
	return Uid2User(GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->DeletedBy);
}


/*

%[$LimitPerIP]

*/
LPVOID MessageVariable_LimitPerIP(LPMESSAGEDATA lpData)
{
	// this is really a 64 bit value getting trimmed to 32...
	return &GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->LimitPerIP;
}




/*

  %[$logintime]

  */
LPVOID MessageVariable_LoginTime(LPMESSAGEDATA lpData)
{
	// this is really a 64 bit value getting trimmed to 32...
	return &GETOFFSET(lpData->DataOffsets, DATA_CONNECTION)->tLogin;
}




/*

%[$ClosedOn]

*/
LPVOID MessageVariable_ClosedOn(LPMESSAGEDATA lpData)
{
	// this is really a 64 bit value getting trimmed to 32
	return (DWORD *) &FtpSettings.tmSiteClosedOn;
}



/*

%[UpTime]

*/
LPVOID MessageVariable_UpTime(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	UINT64 u64Time;

	GetSystemTimeAsFileTime((FILETIME *) &u64Time);
	u64Time -= u64FtpStartTime;
	u64Time /= 10000000;
	lpData->dwTempValue = (DWORD) u64Time;
	return &lpData->dwTempValue;
}


/*

%[SysUpTime]

*/
LPVOID MessageVariable_SysUpTime(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	UINT64 u64Time;

	GetSystemTimeAsFileTime((FILETIME *) &u64Time);
	u64Time -= u64WindowsStartTime;
	u64Time /= 10000000;
	lpData->dwTempValue = (DWORD) u64Time;
	return &lpData->dwTempValue;
}



/*

  %[$ident]

  */
LPVOID MessageVariable_Ident(LPMESSAGEDATA lpData)
{
	LPSTR	szIdent;

	szIdent	= (LPSTR)GETOFFSET(lpData->DataOffsets, DATA_CONNECTION)->szIdent;

	return (szIdent ? szIdent : "*");
}




/*

  %[$lastcommand]

  */
LPVOID MessageVariable_LastCommand(LPMESSAGEDATA lpData)
{
	return GETOFFSET(lpData->DataOffsets, DATA_CCHANNEL)->Action;
}




/*

  %[$ip]

  */
LPVOID MessageVariable_Ip(LPMESSAGEDATA lpData)
{
	return inet_ntoa(GETOFFSET(lpData->DataOffsets, DATA_CONNECTION)->ClientAddress.sin_addr);
}



/*

  %[$hostname]

  */
LPVOID MessageVariable_Hostname(LPMESSAGEDATA lpData)
{
	PCONNECTION_INFO	pConnectionInfo;

	//	Get connection
	pConnectionInfo	= GETOFFSET(lpData->DataOffsets, DATA_CONNECTION);
	//	Return hostname
	if (pConnectionInfo->szHostName) return pConnectionInfo->szHostName;
	//	Return ip
	return MessageVariable_Ip(lpData);
}


/*

  %[$pwd]

  */
LPVOID MessageVariable_Pwd(LPMESSAGEDATA lpData)
{
	return GETOFFSET(lpData->DataOffsets, DATA_CCHANNEL)->Path.pwd;
}



/*

  %[$cwd]

  */
LPVOID MessageVariable_Cwd(LPMESSAGEDATA lpData)
{
	MOUNTFILE hMountFile;

	hMountFile = (GETOFFSET(lpData->DataOffsets, DATA_MOUNTTABLE));

	if (hMountFile && hMountFile->lpFtpUser && hMountFile->lpFtpUser->FtpVariables.bKeepLinksInPath)
	{
		return GETOFFSET(lpData->DataOffsets, DATA_CCHANNEL)->Path.Symbolic;
	}
	// else just act like $pwd
	return GETOFFSET(lpData->DataOffsets, DATA_CCHANNEL)->Path.pwd;
}



/*

  %[$path]

  */
LPVOID MessageVariable_Path(LPMESSAGEDATA lpData)
{
	return GETOFFSET(lpData->DataOffsets, DATA_CCHANNEL)->Path.RealPath;
}




/*

  %[$user]

  */
LPVOID MessageVariable_User(LPMESSAGEDATA lpData)
{
	return Uid2User(GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->Uid);
}




/*

  %[$unfo]

  */
LPVOID MessageVariable_Unfo(LPMESSAGEDATA lpData)
{
	return GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->Tagline;
}



/*

%[$creator]

*/
LPVOID MessageVariable_Creator(LPMESSAGEDATA lpData)
{
	LPSTR szUser;

	if (GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->CreatorUid == -1)
	{
		if (GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->CreatorName[0] == 0)
		{
			return "<unknown>";
		}
		return GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->CreatorName;
	}

	szUser = Uid2User(GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->CreatorUid);

	if (szUser)
	{
		return szUser;
	}

	return "<unknown>";
}




/*

%[$opaque]

*/
LPVOID MessageVariable_Opaque(LPMESSAGEDATA lpData)
{
	return GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->Opaque;
}


/*

%[$DeletedMsg]

*/
LPVOID MessageVariable_DeletedMsg(LPMESSAGEDATA lpData)
{
	return GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->DeletedMsg;
}


/*

%[$LogonHost]

*/
LPVOID MessageVariable_LogonHost(LPMESSAGEDATA lpData)
{
	return GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->LogonHost;
}


/*

  %[$group]

  */
LPVOID MessageVariable_Group(LPMESSAGEDATA lpData)
{
	return Gid2Group(GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->Gid);
}



/*

  %[$flags]

  */
LPVOID MessageVariable_Flags(LPMESSAGEDATA lpData)
{
	return GETOFFSET(lpData->DataOffsets, DATA_USERFILE)->Flags;
}



/*

  %[$section]

  */
LPVOID MessageVariable_Section(LPMESSAGEDATA lpData)
{
	return GETOFFSET(lpData->DataOffsets, DATA_CCHANNEL)->Path.SectionName;
}



/*

  %[$service]

  */
LPVOID MessageVariable_Service(LPMESSAGEDATA lpData)
{
	LPIOSERVICE lpService;
	lpService = GETOFFSET(lpData->DataOffsets, DATA_CONNECTION)->lpService;
	if (lpService) return lpService->tszName;
	return "";
}


LPVOID MessageVariable_ShowService(LPMESSAGEDATA lpData)
{
	LPIOSERVICE lpService;
	lpService = GETOFFSET(lpData->DataOffsets, DATA_CONNECTION)->lpDoService;
	if (lpService) return lpService->tszName;
	return "";
}


/*

%[$device]

*/
LPVOID MessageVariable_Device(LPMESSAGEDATA lpData)
{
	LPIODEVICE lpDevice;
	lpDevice = GETOFFSET(lpData->DataOffsets, DATA_CONNECTION)->lpDevice;
	if (lpDevice) return lpDevice->tszName;
	return "";
}


LPVOID MessageVariable_ShowDevice(LPMESSAGEDATA lpData)
{
	LPIODEVICE lpDevice;
	lpDevice = GETOFFSET(lpData->DataOffsets, DATA_CONNECTION)->lpDoDevice;
	if (lpDevice) return lpDevice->tszName;
	return "";
}


INT __cdecl MessageVariableCompare(LPMESSAGE_VARIABLE *lpVar1, LPMESSAGE_VARIABLE *lpVar2)
{
	//	Compare args
	if (lpVar1[0]->bArgs > lpVar2[0]->bArgs) return 1;
	if (lpVar1[0]->bArgs < lpVar2[0]->bArgs) return -1;
	//	Compare length
	if (lpVar1[0]->dwName > lpVar2[0]->dwName) return 1;
	if (lpVar1[0]->dwName < lpVar2[0]->dwName) return -1;
	//	Compare name
	return memicmp(lpVar1[0]->tszName, lpVar2[0]->tszName, lpVar1[0]->dwName * sizeof(TCHAR));
}




WORD FindMessageVariable(LPTSTR tszName, DWORD dwName, BOOL bArgs, LPBYTE lpType)
{
	MESSAGE_VARIABLE	Variable;
	LPMESSAGE_VARIABLE	lpVariable;
	LPVOID				lpResult;

	//	Prepare seek item
	Variable.dwName		= dwName;
	Variable.tszName	= tszName;
	Variable.bArgs	= bArgs;
	lpVariable		= &Variable;
	//	Execute binary search
	lpResult	= bsearch(&lpVariable, lpMessageVariable, dwMessageVariables,
		                  sizeof(LPMESSAGE_VARIABLE), (QUICKCOMPAREPROC) MessageVariableCompare);
	if (! lpResult) return (WORD)-1;

	if (lpType) lpType[0]	= (BYTE)((LPMESSAGE_VARIABLE *)lpResult)[0]->dwType;

	return ((LPMESSAGE_VARIABLE *)lpResult) - lpMessageVariable;
}


LPVOID GetVariable(WORD wIndex)
{
	return (LPVOID)lpMessageVariable[wIndex];
}






USHORT MessageVariable_Compile(LPMESSAGEDATA lpData, LPBYTE lpBuffer)
{
	LPMESSAGE_VARIABLE	lpVariable;
	LPVOID				lpResult;
	LPSTR				szFormatString;
	BYTE				bFormatString;

	lpVariable		= lpMessageVariable[((LPWORD)lpBuffer)[0]];
	bFormatString	= lpBuffer[2];
	szFormatString	= (LPSTR)&lpBuffer[3];

	if (HASDATAEX(lpData->DataOffsets, lpVariable->dwRequiredData))
	{
		if ((lpResult = ((LPVOID (__cdecl *)(const LPMESSAGEDATA ))lpVariable->AllocProc)(lpData)))
		{
			switch (lpVariable->dwType)
			{
			case C_STRING_VARIABLE:
				//	Insert to output buffer
				Put_Buffer_Format(lpData->lpOutBuffer, szFormatString, (LPSTR)lpResult);
				break;
			case C_INTEGER_VARIABLE:
				//	Insert to output buffer
				Put_Buffer_Format(lpData->lpOutBuffer, szFormatString, ((LPINT)lpResult)[0]);
				break;
			case C_FLOAT_VARIABLE:
				//	Insert to output buffer
				Put_Buffer_Format(lpData->lpOutBuffer, szFormatString, ((DOUBLE *)lpResult)[0]);
				break;
			}
			//	Deallocate resources
			if (lpVariable->FreeProc) ((VOID (__cdecl *)(LPVOID))lpVariable->FreeProc)(lpResult);
		}
	}
	else
	{
		switch (lpVariable->dwType)
		{
		case C_STRING_VARIABLE:
			FormatString(lpData->lpOutBuffer, szFormatString, "");
			break;
		case C_INTEGER_VARIABLE:
			FormatString(lpData->lpOutBuffer, szFormatString, 0);
			break;
		case C_FLOAT_VARIABLE:
			FormatString(lpData->lpOutBuffer, szFormatString, 0.);
			break;
		}
	}
	return bFormatString + 3;
}





BOOL MessageVariable_Precompile(LPBUFFER lpBuffer, LPSTR szName, DWORD dwName, LPSTR szFormat, BYTE bFormat)
{
	LPMESSAGE_VARIABLE	lpVariable;
	WORD				wVariable;
	LPSTR				szSuffix;
	BYTE				pHeader[1 + sizeof(WORD)];

	if ((wVariable = FindMessageVariable(szName, dwName, FALSE, NULL)) != (WORD)-1)
	{
		//	Type is variable
		lpVariable	= lpMessageVariable[wVariable];

		pHeader[0]	= VARIABLE;
		CopyMemory(&pHeader[1], &wVariable, sizeof(WORD));
		//	Insert to buffer
		Put_Buffer(lpBuffer, &pHeader, 1 + sizeof(WORD));
		//	Format string length + '?' + '\0'
		bFormat	+= (2 * sizeof(CHAR));
		Put_Buffer(lpBuffer, &bFormat, sizeof(BYTE));
		bFormat	-= (2 * sizeof(CHAR));
		//	Insert format string to buffer
		Put_Buffer(lpBuffer, szFormat, bFormat);
		//	Add prefix suffix
		switch (lpVariable->dwType)
		{
		case C_INTEGER_VARIABLE:
			szSuffix	= "i";
			break;
		case C_STRING_VARIABLE:
			szSuffix	= "s";
			break;
		case C_FLOAT_VARIABLE:
			szSuffix	= "f";
			break;
		}
		Put_Buffer(lpBuffer, szSuffix, 2 * sizeof(CHAR));
		return FALSE;
	}
	return TRUE;
}




BOOL InstallMessageVariable(LPTSTR tszName, LPVOID AllocProc,
							LPVOID FreeProc, DWORD dwRequiredData, DWORD dwType, ...)
{
	LPMESSAGE_VARIABLE	lpVariable;
	LPARG_PROC			lpArgProc;
	LPVOID				lpMemory, Proc;
	BOOL				bError;
	va_list				Arguments;
	DWORD				dwName;

	if (! tszName || ! (dwName = _tcslen(tszName)) ||
		! AllocProc || (dwMessageVariables == (USHORT)-1)) return TRUE;

	if (dwMessageVariables == dwMessageVariablesAllocated)
	{
		//	Allocate more memory
		lpMemory	= ReAllocate(lpMessageVariable, "Message:VariableArray",
			sizeof(LPMESSAGE_VARIABLE) * (dwMessageVariables + 128));
		if (! lpMemory) return TRUE;
		//	Update array size
		dwMessageVariablesAllocated	+= 128;
		lpMessageVariable			= (LPMESSAGE_VARIABLE *)lpMemory;
	}

	//	Allocate memory for variable
	lpVariable	= (LPMESSAGE_VARIABLE)Allocate("Message:Variable", sizeof(MESSAGE_VARIABLE));
	if (! lpVariable) return TRUE;

	//	Update structure contents
	lpVariable->dwName		= dwName;
	lpVariable->tszName		= tszName;
	lpVariable->bArgs		= (dwType & C_ARGS ? TRUE : FALSE);
	lpVariable->dwType		= (dwType & (0xFFFFFFFF - C_ARGS));
	lpVariable->AllocProc	= AllocProc;
	lpVariable->FreeProc	= FreeProc;
	lpVariable->lpArgProc	= NULL;
	lpVariable->dwRequiredData	= dwRequiredData;

	bError	= FALSE;
	va_start(Arguments, dwType);
	//	Get procs
	while ((Proc = va_arg(Arguments, LPVOID)))
	{
		//	Allocate memory
		if (! (lpArgProc = (LPARG_PROC)Allocate("Arg:Proc", sizeof(ARG_PROC))))
		{
			bError	= TRUE;
			break;
		}
		//	Add to list
		lpArgProc->lpProc	= Proc;
		lpArgProc->lpNext	= (LPARG_PROC)lpVariable->lpArgProc;
		lpVariable->lpArgProc	= lpArgProc;
	}
	va_end(Arguments);
	//	Insert new variable to array
	if (bError || QuickInsert(lpMessageVariable, dwMessageVariables, lpVariable, (QUICKCOMPAREPROC) MessageVariableCompare))
	{
		//	Free memory
		for (;lpArgProc = lpVariable->lpArgProc;)
		{
			lpVariable->lpArgProc	= (LPARG_PROC)lpArgProc->lpNext;
			Free(lpArgProc);
		}
		Free(lpVariable);
		return TRUE;
	}
	dwMessageVariables++;
	return FALSE;
}







BOOL MessageVariables_Init(VOID)
{
	LPVARIABLE_MODULE	lpModule;
	LPSTR				szFileName;
	LPVOID				lpProc;
	INT					iOffset;

	dwMessageVariables			= 0;
	dwMessageVariablesAllocated	= 0;
	lpMessageVariableModules	= NULL;
	lpMessageVariable			= NULL;
	dwAlwaysZero                = 0;
	//	String variables
	InstallMessageVariable("PWD", MessageVariable_Pwd, NULL, B_COMMAND, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("CWD", MessageVariable_Cwd, NULL, B_COMMAND, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("PATH", MessageVariable_Path, NULL, B_COMMAND|B_MOUNTTABLE, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("USER", MessageVariable_User, NULL, B_USERFILE, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("UNFO", MessageVariable_Unfo, NULL, B_USERFILE, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("GROUP", MessageVariable_Group, NULL, B_USERFILE, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("FLAGS", MessageVariable_Flags, NULL, B_USERFILE, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("HOME", MessageVariable_HomeDirectory, NULL, B_USERFILE, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("CREATOR", MessageVariable_Creator, NULL, B_USERFILE, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("VFS", MessageVariable_VFS, NULL, B_USERFILE, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("OPAQUE", MessageVariable_Opaque, NULL, B_USERFILE, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("DELETEDBY", MessageVariable_DeletedBy, NULL, B_USERFILE, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("DELETEDMSG", MessageVariable_DeletedMsg, NULL, B_USERFILE, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("LOGONHOST", MessageVariable_LogonHost, NULL, B_USERFILE, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("HOSTNAME", MessageVariable_Hostname, NULL, B_CONNECTION, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("COMMAND", MessageVariable_LastCommand, NULL, B_COMMAND, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("IP", MessageVariable_Ip, NULL, B_CONNECTION, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("IDENT", MessageVariable_Ident, NULL, B_CONNECTION, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("SECTION", MessageVariable_Section, NULL, B_COMMAND, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("SERVICE", MessageVariable_Service, NULL, B_CONNECTION, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("DEVICE", MessageVariable_Device, NULL, B_CONNECTION, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("SHOWSERVICE", MessageVariable_ShowService, NULL, B_CONNECTION, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("SHOWDEVICE", MessageVariable_ShowDevice, NULL, B_CONNECTION, C_STRING_VARIABLE, NULL);
	InstallMessageVariable("GROUPNAME", MessageVariable_GroupName, NULL, B_GROUPFILE, C_STRING_VARIABLE, NULL);

	//	Integer variables
	InstallMessageVariable("CLOSEDON", MessageVariable_ClosedOn, NULL, B_ANY, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("LOGINTIME", MessageVariable_LoginTime, NULL, B_CONNECTION, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("UID", MessageVariable_Uid, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("GID", MessageVariable_Gid, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("DOWNLOADS", MessageVariable_Downloads, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("UPLOADS", MessageVariable_Uploads, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("MAXDOWNLOADS", MessageVariable_MaxDownloads, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("MAXUPLOADS", MessageVariable_MaxUploads, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("LOGONCOUNT", MessageVariable_LogonCount, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("LOGONLAST", MessageVariable_LogonLast, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("FTPLOGINS", MessageVariable_FTPLogins, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("CREATEDON", MessageVariable_CreatedOn, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("EXPIRESAT", MessageVariable_ExpiresAt, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("DELETEDON", MessageVariable_DeletedOn, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("LIMITPERIP", MessageVariable_LimitPerIP, NULL, B_USERFILE, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("STATSSECTION", MessageVariable_StatsSection, NULL, B_COMMAND, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("CREDITSECTION", MessageVariable_CreditSection, NULL, B_COMMAND, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("UPTIME", MessageVariable_UpTime, NULL, B_ANY, C_INTEGER_VARIABLE, NULL);
	InstallMessageVariable("SYSUPTIME", MessageVariable_SysUpTime, NULL, B_ANY, C_INTEGER_VARIABLE, NULL);
	//	Modules
	for (iOffset = 0;(szFileName = Config_Get(&IniConfigFile, "Modules", "MessageVariableModule", NULL, &iOffset));)
	{
		//	Allocate memory
		lpModule	= (LPVARIABLE_MODULE)Allocate("Variable:Module", sizeof(VARIABLE_MODULE));
		if (lpModule)
		{
			//	Initialize structue
			ZeroMemory(lpModule, sizeof(VARIABLE_MODULE));
			lpModule->GetProc					= GetProc;
			lpModule->InstallMessageVariable	= InstallMessageVariable;
			lpModule->hModule					= LoadLibrary(szFileName);
			//	Check module handle
			if (lpModule->hModule &&
				lpModule->hModule != INVALID_HANDLE_VALUE)
			{
				//	Find startup proc
				lpProc	= GetProcAddress(lpModule->hModule, "MessageVariableInit");
				//	Execute startup proc
				if (lpProc &&
					! ((BOOL (__cdecl *)(LPVARIABLE_MODULE))lpProc)(lpModule))
				{
					lpModule->lpNext			= lpMessageVariableModules;
					lpMessageVariableModules	= lpModule;
					lpModule	= NULL;
				}
				else FreeLibrary(lpModule->hModule);
			}
		}
		//	Free memory
		Free(szFileName);
		Free(lpModule);
	}
	return FALSE;
}





VOID MessageVariables_DeInit(VOID)
{
	LPVARIABLE_MODULE	lpModule;
	LPARG_PROC			lpArgProc;
	LPVOID				lpProc;

	//	Release libraries
	for (;lpModule = lpMessageVariableModules;)
	{
		lpMessageVariableModules	= lpMessageVariableModules->lpNext;
		//	Find proc
		lpProc	= GetProcAddress(lpModule->hModule, "MessageVariableDeInit");
		//	Execute shutdown proc
		if (lpProc) ((VOID (__cdecl *)(LPVARIABLE_MODULE))lpProc)(lpModule);
		//	Unload library
		FreeLibrary(lpModule->hModule);
		Free(lpModule);
	}
	//	Free memory
	for (;dwMessageVariables--;)
	{
		for (;lpArgProc = lpMessageVariable[dwMessageVariables]->lpArgProc;)
		{
			lpMessageVariable[dwMessageVariables]->lpArgProc	= (LPARG_PROC)lpArgProc->lpNext;
			Free(lpArgProc);
		}
		Free(lpMessageVariable[dwMessageVariables]);
	}
	Free(lpMessageVariable);
}

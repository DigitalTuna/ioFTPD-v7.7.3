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

LPTSTR Admin_CrashNow(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_LoadSymbols(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_MakeCert(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_RemoveCert(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Verify(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_RevertGroup(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_MyInfo(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Permissions(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Uptime(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_SectionNums(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Stat(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);


static ADMINCOMMAND	AdminCommand[] = {
	_TEXT("ChAttr"),	Admin_ChangeFileAttributes,
	_TEXT("ChMod"),		Admin_ChangeFileMode,
	_TEXT("ChOwn"),		Admin_ChangeFileOwner,
	_TEXT("Perms"),     Admin_Permissions,
	_TEXT("Symlink"),   Admin_CreateSymLink,
	_TEXT("Size"),		Admin_DirSize,
	_TEXT("Freespace"), Admin_FreeSpace,
	_TEXT("Color"),     Admin_Color,
	_TEXT("Help"),      Admin_Help,
	_TEXT("Stat"),      Admin_Stat,

	_TEXT("SWho"),		Admin_UsersOnline,
	_TEXT("Who"),       Admin_Who,
	_TEXT("Users"),		Admin_Users,
	_TEXT("Groups"),	Admin_Groups,
	_TEXT("Stats"),		Admin_UserStats,
	_TEXT("UInfo"),		Admin_UserInfo,
	_TEXT("MyInfo"),	Admin_MyInfo,
	_TEXT("GInfo"),		Admin_GroupInfo,
	_TEXT("Change"),	Admin_Change,
	_TEXT("Config"),	Admin_Config,
	_TEXT("Kick"),		Admin_Kick,
	_TEXT("Kill"),		Admin_Kill,
	_TEXT("Bans"),		Admin_Bans,
	_TEXT("SectionsNums"),   Admin_SectionNums,

	_TEXT("Close"),     Admin_Close,
	_TEXT("Open"),      Admin_Open,
	_TEXT("Shutdown"),	Admin_Shutdown,

	_TEXT("AddUser"),	Admin_AddUser,
	_TEXT("GAddUser"),	Admin_AddUserToGroup,
	_TEXT("DelUser"),	Admin_DeleteUser,
	_TEXT("ReAdd"),	    Admin_ReAdd,
	_TEXT("Purge"),	    Admin_Purge,
	_TEXT("RenUser"),	Admin_RenameUser,
	_TEXT("GrpAdd"),	Admin_AddGroup,
	_TEXT("GrpDel"),	Admin_DeleteGroup,
	_TEXT("GrpRen"),	Admin_RenameGroup,
	_TEXT("GrpRevert"), Admin_RevertGroup,

	_TEXT("ChGrp"),		Admin_ChangeUserGroups,
	_TEXT("AddIp"),		Admin_AddIp,
	_TEXT("DelIp"),		Admin_DeleteIp,
	_TEXT("FindIp"),	Admin_FindIp,
	_TEXT("Passwd"),	Admin_SetOwnPassword,
	_TEXT("Tagline"),	Admin_SetOwnTagline,
	_TEXT("Uptime"),    Admin_Uptime,

	_TEXT("Services"),  Admin_Services,
	_TEXT("Devices"),   Admin_Devices,

	_TEXT("ioVersion"),   Admin_Version,
	_TEXT("ioVerify"),    Admin_Verify,
	_TEXT("CrashNow"),    Admin_CrashNow,
	_TEXT("LoadSymbols"), Admin_LoadSymbols,
	_TEXT("MakeCert"),    Admin_MakeCert,
	_TEXT("RemoveCert"),  Admin_RemoveCert,
	_TEXT("Ciphers"),     Admin_Ciphers,

	_TEXT("DirCache"),    Admin_DirCache,
	_TEXT("Refresh"),	  Admin_Refresh,
};

// global, but protected by locks
static LPBUFFER lpVerifyBuffer;
static LPTSTR   tszVerifyPrefix;
static DWORD    dwVerifyBufferLock;


BOOL IsName(LPTSTR tszName)
{
	DWORD	dwName;

	if (!tszName)
	{
		return FALSE;
	}

	switch (tszName[0])
	{
	case _T('='):
	case _T('-'):
	case _T('+'):
		return FALSE;
	}

	for (dwName = 0;dwName <= _MAX_NAME;dwName++)
	{
		switch ((tszName++)[0])
		{
		//	List of disallowed character follows
		case _TEXT('|'):
		case _TEXT('"'):
		case _TEXT('*'):
		case _TEXT(':'):
		case _TEXT('\\'):
		case _TEXT('/'):
		case _TEXT('?'):
		case _TEXT('.'):
		case _TEXT('>'):
		case _TEXT('<'):
		case _TEXT(' '):
			return FALSE;
		case _TEXT('\0'):
			return TRUE;
		}
	}
	//	Name too long
	return FALSE;
}




BOOL IsGroupName(LPTSTR tszGroupName)
{
	//	Validate groupname
	if (IsName(tszGroupName)) return FALSE;
	SetLastError(ERROR_GROUPNAME);
	return TRUE;
}




BOOL IsUserName(LPTSTR tszUserName)
{
	//	Validate username
	if (IsName(tszUserName)) return FALSE;
	SetLastError(ERROR_USERNAME);
	return TRUE;
}



BOOL
IsLocalAdmin(LPUSERFILE lpUserFile, PCONNECTION_INFO lpConnectionInfo)
{
	if (lpUserFile && lpConnectionInfo &&
		!HasFlag(lpUserFile, _T("M")) &&
		lpConnectionInfo->ClientAddress.sin_addr.s_addr == 0x0100007F)
	{
		return FALSE;
	}
	SetLastError(ERROR_MASTER);
	return TRUE;
}


DWORD
CheckForMasterAccount(LPFTPUSER lpUser, LPUSERFILE lpUserFile)
{
	if (lpUser->UserFile->Uid == lpUserFile->Uid)
	{
		return NO_ERROR;
	}

	if (! HasFlag(lpUserFile, _TEXT("M")) &&
		IsLocalAdmin(lpUser->UserFile, &lpUser->Connection))
	{
		return ERROR_MASTER;
	}
	else if (User_IsAdmin(lpUser->UserFile, lpUserFile, NULL))
	{
		return GetLastError();
	}
	return NO_ERROR;
}


LPTSTR
LookupUserName(LPUSERFILE lpUserFile)
{
	LPTSTR tszUser;

	tszUser = Uid2User(lpUserFile->Uid);
	if (tszUser) return tszUser;

	return _T("<Unknown>");
}


ADMINPROC
FindAdminCommand(LPTSTR tszCommand, LPTSTR *ptszName)
{
	DWORD	n;
	//	Find command
	for (n = 0;n < sizeof(AdminCommand) / sizeof(ADMINCOMMAND);n++)
	{
		if (! _tcsicmp(tszCommand, AdminCommand[n].tszCommand))
		{
			if (ptszName)
			{
				*ptszName = AdminCommand[n].tszCommand;
			}
			return AdminCommand[n].lpProc;
		}
	}
	return NULL;
}



LPTSTR Admin_RenameUser(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPUSERFILE		lpUserFile;
	LPTSTR			tszUserName, tszNewUserName, tszAdmin;
	DWORD			dwError;

	dwError	= NO_ERROR;
	if (GetStringItems(Args) < 3) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));
	if (GetStringItems(Args) > 3) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 3, STR_END));

	//	Get arguments
	tszUserName	= GetStringIndexStatic(Args, 1);
	tszNewUserName	= GetStringIndexStatic(Args, 2);

	if (IsUserName(tszNewUserName)) return tszNewUserName;
	//	Check permissions
	if (UserFile_Open(tszUserName, &lpUserFile, 0)) return tszUserName;

	dwError = CheckForMasterAccount(lpUser, lpUserFile);

	UserFile_Close(&lpUserFile, 0);
	if (dwError != NO_ERROR) ERROR_RETURN(dwError, tszUserName);

	//	Rename user
	if (RenameUser(tszUserName, tszNewUserName)) return GetStringIndexStatic(Args, 0);

	tszAdmin = LookupUserName(lpUser->UserFile);

	Putlog(LOG_SYSOP, _TEXT("'%s' renamed user '%s' to '%s'.\r\n"), tszAdmin, tszUserName, tszNewUserName);

	return NULL;
}



LPTSTR Admin_SetOwnPassword(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPUSERFILE		lpUserFile;
	DWORD			dwError;
	LPTSTR			tszPassword, tszUserName;

	dwError	= NO_ERROR;
	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));

	//	Get arguments
	tszPassword	= GetStringRange(Args, 1, STR_END);
	tszUserName	= Uid2User(lpUser->UserFile->Uid);
	if (! tszUserName) ERROR_RETURN(ERROR_USER_NOT_FOUND, _TEXT("Self"));

	if (! UserFile_OpenPrimitive(lpUser->UserFile->Uid, &lpUserFile, 0))
	{
		//	Change password
		if (! UserFile_Lock(&lpUserFile, 0))
		{
			HashString(tszPassword, lpUserFile->Password);
			UserFile_Unlock(&lpUserFile, 0);
		}
		else dwError	= GetLastError();
		UserFile_Close(&lpUserFile, 0);
		if (dwError == NO_ERROR) return NULL;
	}
	else dwError	= GetLastError();
	ERROR_RETURN(dwError, tszUserName);
}






LPTSTR Admin_DeleteUser(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszUserName, tszAdmin, tszMsg;
	LPUSERFILE		lpUserFile;
	LPBUFFER        lpBuffer;
	DWORD			dwError;

	dwError	= NO_ERROR;
	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));

	//	Get arguments
	lpBuffer	= &lpUser->CommandChannel.Out;
	tszUserName	= GetStringIndexStatic(Args, 1);
	if (GetStringItems(Args) > 2) 
	{
		tszMsg = GetStringRange(Args, 2, STR_END);
	}
	else
	{
		tszMsg = _T("");
	}

	//	Open userfile
	if (! UserFile_Open(tszUserName, &lpUserFile, 0))
	{
		if (! UserFile_Lock(&lpUserFile, 0))
		{
			//	Check permissions
			dwError = CheckForMasterAccount(lpUser, lpUserFile);

			if (dwError == NO_ERROR)
			{
				// update deleted message even if already deleted
				strncpy_s(lpUserFile->DeletedMsg, sizeof(lpUserFile->DeletedMsg), tszMsg, _TRUNCATE);

				tszAdmin = LookupUserName(lpUser->UserFile);

				if (lpUserFile->DeletedOn == 0)
				{
					// mark user as deleted
					lpUserFile->DeletedOn = time((time_t *) NULL);
					lpUserFile->DeletedBy = lpUser->UserFile->Uid;
					Putlog(LOG_SYSOP, _TEXT("'%s' marked user '%s' for deletion with DeletedMsg='%s'.\r\n"),
						tszAdmin, tszUserName, lpUserFile->DeletedMsg);
					FormatString(lpBuffer, _TEXT("%sDisabled user account '%s' and marked it for deletion.\r\n"), tszMultilinePrefix, tszUserName);
					if (tszMsg[0])
					{
						FormatString(lpBuffer, _TEXT("%sDeletedMsg set to '%s'.\r\n"), tszMultilinePrefix, tszMsg);
					}
				}
				else
				{
					Putlog(LOG_SYSOP, _TEXT("'%s' re-marked user '%s' for deletion with DeletedMsg='%s'\r\n"),
						tszAdmin, tszUserName, lpUserFile->DeletedMsg);
					FormatString(lpBuffer, _TEXT("%sRe-Deleted user '%s', DeletedMsg updated to '%s'\r\n"), tszMultilinePrefix, tszUserName, tszMsg);
				}
			}
			UserFile_Unlock(&lpUserFile, 0);
		}
		else dwError	= GetLastError();
		UserFile_Close(&lpUserFile, 0);
		if (dwError == NO_ERROR) return NULL;
	}
	else dwError	= GetLastError();

	ERROR_RETURN(dwError, tszUserName);
}



// internal command
LPTSTR DisplayDeletedUsers(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, BOOL bReAdd)
{
	LPTSTR			tszUserName, tszBasePath, tszFileName;
	LPUSERFILE		lpUserFile;
	USERFILE_PLUS   UserFile_Plus;
	LPBUFFER        lpBuffer;
	PBYTE			pBuf;
	DWORD			n, dwError, dwUsers, dwFileName;
	INT			    Uid, *pUsers;
	time_t          tNow;

	//	Get arguments
	lpBuffer = &lpUser->CommandChannel.Out;

	pUsers = GetUsers(&dwUsers);
	if (dwUsers == 0)
	{
		if (pUsers) Free(pUsers);
		ERROR_RETURN(ERROR_USER_NOT_FOUND, NULL);
	}

	tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
	if (! tszBasePath) return NULL;
	dwFileName	= aswprintf(&tszFileName, _TEXT("%s\\UserList.Header"), tszBasePath);
	FreeShared(tszBasePath);
	if (! dwFileName) return NULL;
	//	Show header
	MessageFile_Show(tszFileName, lpBuffer, lpUser, DT_FTPUSER, tszMultilinePrefix, NULL);

	dwError = NO_ERROR;

	//	Load body
	_tcscpy(&tszFileName[dwFileName - 6], _TEXT("Body"));
	pBuf = Message_Load(tszFileName);
	tNow = time((time_t) NULL);

	UserFile_Plus.lpCommandChannel = &lpUser->CommandChannel;
	UserFile_Plus.lpFtpUserCaller = lpUser;

	for(n=0 ; pBuf && dwError == NO_ERROR && n<dwUsers ; n++)
	{
		Uid = pUsers[n];

		if (!UserFile_OpenPrimitive(Uid, &lpUserFile, 0))
		{
			if (!lpUserFile->DeletedOn && (!lpUserFile->ExpiresAt || (lpUserFile->ExpiresAt > tNow)))
			{
				// user wasn't deleted or account hasn't expired
				UserFile_Close(&lpUserFile, 0);
				continue;
			}
			tszUserName = Uid2User(Uid);

			SetLastError(NO_ERROR);
			// check permissions, 1M's can re-add anyone, G's can re-add admin group members provided they don't have the 1M flag
			// check permissions, M can purge anyone, 1's can purge non 1's, G's can purge other G's or users as long as admin of group in common
			if ((bReAdd && (!HasFlag(lpUser->UserFile, _T("1M")) ||
				            (!HasFlag(lpUser->UserFile, _T("G")) && HasFlag(lpUserFile, _T("1M")) && User_IsAdmin(lpUser->UserFile, lpUserFile, NULL))))
				||
				((!bReAdd) && ((!HasFlag(lpUser->UserFile, _T("M")) ||
				               (!HasFlag(lpUser->UserFile, _T("1")) && HasFlag(lpUserFile, _T("1M"))) ||
							   (HasFlag(lpUser->UserFile, _T("1")) && !HasFlag(lpUser->UserFile, _T("G")) &&
							    HasFlag(lpUserFile, _T("G1M")) && User_IsAdmin(lpUser->UserFile, lpUserFile, NULL))))))
			{
				UserFile_Plus.lpUserFile = lpUserFile;
				Message_Compile(pBuf, lpBuffer, FALSE, &UserFile_Plus, DT_USERFILE_PLUS, tszMultilinePrefix, NULL);
			}
			UserFile_Close(&lpUserFile, 0);
		}
		else dwError = GetLastError();
	}

	//	Show footer
	_tcscpy(&tszFileName[dwFileName - 6], _TEXT("Footer"));
	MessageFile_Show(tszFileName, lpBuffer, lpUser, DT_FTPUSER, tszMultilinePrefix, NULL);

	if (pBuf) Free(pBuf);
	Free(pUsers);
	Free(tszFileName);
	return NULL;
}



LPTSTR Admin_Purge(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszUserName, tszAdmin;
	LPUSERFILE		lpUserFile;
	LPGROUPFILE		lpGroupFile;
	LPBUFFER        lpBuffer;
	DWORD			n, dwError, dwUsers;
	INT			    i, Uid, *pUsers, iFake;
	time_t          tNow;

	if (GetStringItems(Args) > 2) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 2, STR_END));

	//	Get arguments
	lpBuffer = &lpUser->CommandChannel.Out;
	tszAdmin = LookupUserName(lpUser->UserFile);

	if (GetStringItems(Args) == 1)
	{
		// print names which can be re-added.
		FormatString(lpBuffer, _TEXT("%sUsers you can purge...\r\n%s\r\n"), tszMultilinePrefix, tszMultilinePrefix);
		return DisplayDeletedUsers(lpUser, tszMultilinePrefix, FALSE);
	}

	tszUserName	= GetStringIndexStatic(Args, 1);

	if (!_tcsicmp(tszUserName, _T("*")))
	{
		pUsers = GetUsers(&dwUsers);
		if (dwUsers == 0)
		{
			if (!pUsers) Free(pUsers);
			ERROR_RETURN(ERROR_USER_NOT_FOUND, GetStringIndexStatic(Args, 0));
		}
		iFake = -1;
	}
	else
	{
		iFake = User2Uid(tszUserName);
		if (iFake == -1) ERROR_RETURN(ERROR_USER_NOT_FOUND, GetStringIndexStatic(Args, 0));
		dwUsers = 1;
		pUsers = &iFake;
	}

	dwError = NO_ERROR;
	tNow = time((time_t) NULL);
	for(n=0 ; dwError == NO_ERROR && n<dwUsers ; n++)
	{
		Uid = pUsers[n];

		if (!UserFile_OpenPrimitive(Uid, &lpUserFile, 0))
		{
			if (!lpUserFile->DeletedOn && (!lpUserFile->ExpiresAt || (lpUserFile->ExpiresAt > tNow)))
			{
				// user wasn't deleted or account hasn't expired
				UserFile_Close(&lpUserFile, 0);
				if (iFake != -1)
				{
					// if specified by name return an error
					ERROR_RETURN(ERROR_USER_NOT_DELETED, tszUserName);
				}

				continue;
			}
			tszUserName = Uid2User(Uid);

			// check permissions, M can purge anyone, 1's can purge non 1's, G's can purge other G's or users as long as admin of group in common
			SetLastError(NO_ERROR);
			if (!HasFlag(lpUser->UserFile, _T("M")) ||
				(!HasFlag(lpUser->UserFile, _T("1")) && HasFlag(lpUserFile, _T("1M"))) ||
				(HasFlag(lpUser->UserFile, _T("1")) && !HasFlag(lpUser->UserFile, _T("G")) &&
				 HasFlag(lpUserFile, _T("G1M")) && User_IsAdmin(lpUser->UserFile, lpUserFile, NULL)))
			{
				if (! UserFile_Lock(&lpUserFile, 0))
				{
					if (! DeleteUser(tszUserName))
					{
						Putlog(LOG_SYSOP, _TEXT("'%s' purged user '%s'.\r\n"), tszAdmin, tszUserName);
						FormatString(lpBuffer, _TEXT("%sPurged: '%s'\r\n"), tszMultilinePrefix, tszUserName);

						//	Update group statistics
						for (i = 0;i < MAX_GROUPS && lpUserFile->Groups[i] != -1;i++)
						{
							if (! GroupFile_OpenPrimitive(lpUserFile->Groups[i], &lpGroupFile, 0))
							{
								if (! GroupFile_Lock(&lpGroupFile, 0))
								{
									//	Alter counters
									lpGroupFile->Users--;
									if ((i == 0) && (lpGroupFile->Slots[0] != -1)) lpGroupFile->Slots[0]++;
									GroupFile_Unlock(&lpGroupFile, 0);
								}
								GroupFile_Close(&lpGroupFile, 0);
							}
						}
					}
					else dwError = GetLastError();
					UserFile_Unlock(&lpUserFile, 0);
				}
				else dwError = GetLastError();
			}
			else
			{
				dwError = GetLastError();
				// only report permission failures for single user calls
				if (iFake == -1)
				{
					dwError = NO_ERROR;
				}
				else if (dwError != IO_NOT_GADMIN) dwError = IO_NO_ACCESS;
			}
			UserFile_Close(&lpUserFile, 0);
		}
		else
		{
			dwError = GetLastError();
		}
	}

	if (iFake == -1)
	{
		Free(pUsers);
		if (dwError != NO_ERROR) tszUserName = GetStringIndexStatic(Args, 0);
	}
	if (dwError == NO_ERROR) return NULL;
	ERROR_RETURN(dwError, tszUserName);
}




LPTSTR Admin_ReAdd(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszUserName, tszAdmin;
	LPUSERFILE		lpUserFile;
	LPBUFFER        lpBuffer;
	DWORD			dwError, n, dwUsers;
	time_t          tNow;
	BOOL            bModified;
	INT             Uid, *pUsers, iFake;


	if (GetStringItems(Args) > 2) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 2, STR_END));

	//	Get arguments
	lpBuffer = &lpUser->CommandChannel.Out;
	tszAdmin = LookupUserName(lpUser->UserFile);

	if (GetStringItems(Args) == 1)
	{
		// print names which can be re-added.
		FormatString(lpBuffer, _TEXT("%sUsers you can re-add...\r\n%s\r\n"), tszMultilinePrefix, tszMultilinePrefix);
		return DisplayDeletedUsers(lpUser, tszMultilinePrefix, TRUE);
	}

	tszUserName	= GetStringIndexStatic(Args, 1);

	if (!_tcsicmp(tszUserName, _T("*")))
	{
		pUsers = GetUsers(&dwUsers);
		if (dwUsers == 0)
		{
			if (!pUsers) Free(pUsers);
			ERROR_RETURN(ERROR_USER_NOT_FOUND, GetStringIndexStatic(Args, 0));
		}
		iFake = -1;
	}
	else
	{
		iFake = User2Uid(tszUserName);
		if (iFake == -1) ERROR_RETURN(ERROR_USER_NOT_FOUND, GetStringIndexStatic(Args, 0));
		dwUsers = 1;
		pUsers = &iFake;
	}

	tNow = time((time_t) NULL);
	dwError = NO_ERROR;

	for(n=0 ; dwError == NO_ERROR && n<dwUsers ; n++)
	{
		Uid = pUsers[n];

		if (!UserFile_OpenPrimitive(Uid, &lpUserFile, 0))
		{
			if (!lpUserFile->DeletedOn && (!lpUserFile->ExpiresAt || (lpUserFile->ExpiresAt > tNow)))
			{
				// user wasn't deleted or account hasn't expired
				UserFile_Close(&lpUserFile, 0);
				if (iFake != -1)
				{
					// if specified by name return an error
					ERROR_RETURN(ERROR_USER_NOT_DELETED, tszUserName);
				}

				continue;
			}

			tszUserName = Uid2User(Uid);


			// check permissions, M can re-add anyone, 1's can re-add non 1's, G's can re-add other G's or users as long as admin of group in common
			SetLastError(NO_ERROR);
			if (!HasFlag(lpUser->UserFile, _T("1M")) ||
				(!HasFlag(lpUser->UserFile, _T("G")) && HasFlag(lpUserFile, _T("1M")) && User_IsAdmin(lpUser->UserFile, lpUserFile, NULL)))
			{
				if (! UserFile_Lock(&lpUserFile, 0))
				{
					bModified = FALSE;

					if (lpUserFile->DeletedOn)
					{
						bModified = TRUE;
						lpUserFile->DeletedOn = 0;
						lpUserFile->DeletedBy = -1;
						ZeroMemory(lpUserFile->DeletedMsg, sizeof(lpUserFile->DeletedMsg));

						Putlog(LOG_SYSOP, _TEXT("'%s' re-added deleted user '%s'.\r\n"), tszAdmin, tszUserName);
						FormatString(lpBuffer, _TEXT("%sRe-added deleted user: '%s'\r\n"), tszMultilinePrefix, tszUserName);
					}
					if (lpUserFile->ExpiresAt && (lpUserFile->ExpiresAt < tNow))
					{
						bModified = TRUE;
						lpUserFile->ExpiresAt = 0;
						Putlog(LOG_SYSOP, _TEXT("'%s' re-added expired user '%s'.\r\n"), tszAdmin, tszUserName);
						FormatString(lpBuffer, _TEXT("%sRe-added expired user: '%s'\r\n"), tszMultilinePrefix, tszUserName);
					}
					UserFile_Unlock(&lpUserFile, 0);

					if (!bModified)
					{
						dwError = ERROR_USER_NOT_DELETED;
					}
				}
				else dwError = GetLastError();
			}
			else
			{
				dwError = GetLastError();
				// only report permission failures for single user calls
				if (iFake == -1)
				{
					dwError = NO_ERROR;
				}
				else if (dwError != IO_NOT_GADMIN) dwError = IO_NO_ACCESS;
			}

			UserFile_Close(&lpUserFile, 0);
		}
		else dwError = GetLastError();
	}

	if (iFake == -1) Free(pUsers);
	if (dwError == NO_ERROR) return NULL;
	ERROR_RETURN(dwError, tszUserName);
}




LPTSTR Admin_AddGroup(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszGroupName, tszDescription, tszAdmin;
	LPGROUPFILE		lpGroupFile;
	DWORD			dwError;
	INT				Gid;

	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));

	//	Get arguments
	tszGroupName	= GetStringIndexStatic(Args, 1);
	tszDescription	= (GetStringItems(Args) > 2 ? GetStringRange(Args, 2, STR_END) : NULL);

	if (IsGroupName(tszGroupName)) return tszGroupName;
	if (Group2Gid(tszGroupName) != -1) ERROR_RETURN(ERROR_GROUP_EXISTS, tszGroupName);

	//	Create group
	Gid	= CreateGroup(tszGroupName);
	if (Gid == -1) return tszGroupName;
	tszAdmin = LookupUserName(lpUser->UserFile);

	Putlog(LOG_SYSOP, _TEXT("'%s' created group '%s'.\r\n"), tszAdmin, tszGroupName);

	dwError	= NO_ERROR;
	//	Change description
	if (tszDescription)
	{
		if (! GroupFile_OpenPrimitive(Gid, &lpGroupFile, 0))
		{
			if (! GroupFile_Lock(&lpGroupFile, 0))
			{
				_tcsncpy(lpGroupFile->szDescription,
					tszDescription, sizeof(lpGroupFile->szDescription) / sizeof(TCHAR) - 1);
				GroupFile_Unlock(&lpGroupFile, 0);
			}
			else dwError	= GetLastError();
			GroupFile_Close(&lpGroupFile, 0);
		}
		else dwError	= GetLastError();
	}
	if (dwError == NO_ERROR) return NULL;
	ERROR_RETURN(dwError, tszGroupName);
}





LPTSTR Admin_RenameGroup(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszGroupName, tszNewGroupName, tszAdmin;

	if (GetStringItems(Args) < 3) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));
	if (GetStringItems(Args) > 3) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 3, STR_END));

	//	Get arguments
	tszGroupName	= GetStringIndexStatic(Args, 1);
	tszNewGroupName	= GetStringIndexStatic(Args, 2);
	if (IsGroupName(tszNewGroupName)) return tszNewGroupName;
	if (RenameGroup(tszGroupName, tszNewGroupName)) return GetStringIndexStatic(Args, 0);

	tszAdmin = LookupUserName(lpUser->UserFile);
	Putlog(LOG_SYSOP, _TEXT("'%s' renamed group '%s' to '%s'.\r\n"), tszAdmin, tszGroupName, tszNewGroupName);

	return NULL;
}


LPTSTR Admin_RevertGroup(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszGroupName, tszAdmin, tszFileName;
	LPPARENT_GROUPFILE lpParent;
	DWORD			dwError;
	INT32           Gid;
	LPBUFFER        lpBuffer;

	dwError	= NO_ERROR;

	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));
	if (GetStringItems(Args) > 2) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 2, STR_END));

	//	Get arguments
	tszGroupName = GetStringIndexStatic(Args, 1);
	tszAdmin     = LookupUserName(lpUser->UserFile);
	lpBuffer     = &lpUser->CommandChannel.Out;

	if ((Gid = Group2Gid(tszGroupName)) == -1)
	{
		ERROR_RETURN(ERROR_GROUP_NOT_FOUND, tszGroupName);
	}

	if (!(lpParent = GetParentGroupFileSafely(Gid)) || !lpParent->tszDefaultName)
	{
		ERROR_RETURN(ERROR_GROUP_NOT_FOUND, tszGroupName);
	}

	if (tszFileName = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("User_Files"), lpParent->tszDefaultName, NULL))
	{
		if (GetFileAttributes(tszFileName) != INVALID_FILE_ATTRIBUTES)
		{
			if (!DeleteFile(tszFileName))
			{
				dwError = GetLastError();
				Free(tszFileName);
				return tszGroupName;
			}
			Putlog(LOG_SYSOP, _TEXT("'%s' reverted group '%s' customizations.\r\n"), tszAdmin, tszGroupName);
			Free(tszFileName);
			return NULL;
		}
		Free(tszFileName);
		FormatString(lpBuffer, _TEXT("%sGroup '%s' was not specialized.\r\n"), tszMultilinePrefix, tszGroupName);
		dwError = ERROR_COMMAND_FAILED;
		return tszGroupName;
	}
	return tszGroupName;
}





LPTSTR Admin_DeleteGroup(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszGroupName, tszAdmin;
	LPGROUPFILE		lpGroupFile;
	DWORD			dwError;

	dwError	= NO_ERROR;
	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));
	if (GetStringItems(Args) > 2) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 2, STR_END));

	//	Get arguments
	tszGroupName = GetStringIndexStatic(Args, 1);

	//	Delete group
	if (GroupFile_Open(tszGroupName, &lpGroupFile, 0)) return tszGroupName;

	if (lpGroupFile->Gid == NOGROUP_ID)
	{
		GroupFile_Close(&lpGroupFile, 0);
		ERROR_RETURN(ERROR_NOGROUP, tszGroupName);
	}

	if (! GroupFile_Lock(&lpGroupFile, 0))
	{
		if (! lpGroupFile->Users)
		{
			if (DeleteGroup(tszGroupName)) dwError	= GetLastError();
		}
		else dwError	= ERROR_GROUP_NOT_EMPTY;
		GroupFile_Unlock(&lpGroupFile, 0);
	}
	else dwError	= GetLastError();
	GroupFile_Close(&lpGroupFile, 0);

	if (dwError == NO_ERROR)
	{
		tszAdmin = LookupUserName(lpUser->UserFile);
		Putlog(LOG_SYSOP, _TEXT("'%s' deleted group '%s'.\r\n"), tszAdmin, tszGroupName);
		return NULL;
	}
	ERROR_RETURN(dwError, tszGroupName);
}








/*

  User_DeleteIp() - Deletes ip (or list of ips) from user

  */
LPTSTR Admin_DeleteIp(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPBUFFER		lpBuffer;
	LPUSERFILE		lpUserFile;
	LPTSTR			tszUserName, tszIp, tszAdmin;
	DWORD			dwError;
	UINT			i, j;
	CHAR            szObscuredMask[_IP_LINE_LENGTH+1];

	dwError	= NO_ERROR;
	if (GetStringItems(Args) < 3) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));

	//	Get arguments
	tszUserName	= GetStringIndexStatic(Args, 1);
	lpBuffer	= &lpUser->CommandChannel.Out;

	//	Open userfile
	if (UserFile_Open(tszUserName, &lpUserFile, 0)) return tszUserName;
	if (! UserFile_Lock(&lpUserFile, 0))
	{
		//	Check permissions
		dwError = CheckForMasterAccount(lpUser, lpUserFile);

		if (dwError == NO_ERROR)
		{
			dwError	= ERROR_NO_MATCH;
			tszAdmin = LookupUserName(lpUser->UserFile);

			for (i = 2U;i < GetStringItems(Args);i++)
			{
				tszIp	= GetStringIndexStatic(Args, i);

				for (j = 0U;j < MAX_IPS && lpUserFile->Ip[j][0] != _TEXT('\0');j++)
				{
					//	Remove ip from account
					if (! iCompare(tszIp, lpUserFile->Ip[j]))
					{
						dwError	= NO_ERROR;
						//	Show output
						FormatString(lpBuffer, _TEXT("%sRemoved: '%s'\r\n"), tszMultilinePrefix, lpUserFile->Ip[j]);
						Obscure_Mask(szObscuredMask, lpUserFile->Ip[j]);
						Putlog(LOG_SYSOP, _TEXT("'%s' removed ip '%s' from user '%s'.\r\n"), tszAdmin, szObscuredMask, tszUserName);
						//	Update ip list
						MoveMemory(&lpUserFile->Ip[j], &lpUserFile->Ip[j + 1], (MAX_IPS - j - 1) * sizeof(lpUserFile->Ip[0]));
						lpUserFile->Ip[MAX_IPS - 1][0]	= _TEXT('\0');
						j--;
					}
				}
			}
		}
		UserFile_Unlock(&lpUserFile, 0);
	}
	else dwError	= GetLastError();
	UserFile_Close(&lpUserFile, 0);

	switch (dwError)
	{
	case NO_ERROR:
		return NULL;
	case ERROR_NO_MATCH:
		ERROR_RETURN(ERROR_NO_MATCH, GetStringRange(Args, 2, STR_END));
	}
	ERROR_RETURN(dwError, tszUserName);
}


/*

  User_FindIp() - Return list of users matching an ip/host

  */
LPTSTR Admin_FindIp(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPBUFFER		lpBuffer;
	LPTSTR			tszHost;
	DWORD			dwError;

	dwError	= NO_ERROR;
	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));
	if (GetStringItems(Args) > 2) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 2, STR_END));

	//	Get arguments
	tszHost  = GetStringIndexStatic(Args, 1);
	lpBuffer = &lpUser->CommandChannel.Out;

	if (UserIpHostMaskMatches(lpBuffer, tszMultilinePrefix, tszHost))
	{
		return NULL;
	}
	ERROR_RETURN(ERROR_NO_MATCH, tszHost);
}



BOOL
AddIpToUser(LPUSERFILE lpAdmin, LPTSTR tszAdminName, LPUSERFILE lpUserFile, LPTSTR tszUserName,
			LPBUFFER lpBuffer, LPTSTR tszMultilinePrefix, LPSTR tszIp)
{
	TCHAR   tszLine[_INI_LINE_LENGTH+1], tszPerms[_INI_LINE_LENGTH+1], tszSecure[20];
	CHAR    szObscuredMask[_IP_LINE_LENGTH+1], *szMask;
	LPTSTR  tszTemp, tszMark;
	DWORD   dwFields, n, dwMinFields, dwHostType, dwIdentRequired;
	INT32   iOffset;
	BOOL    bIdent, bWild, bNumeric, bDynamic, bAllowed, bHasRules;

	if (!tszIp)
	{
		return NO_ERROR;
	}

	if (*tszIp == _T(':'))
	{
		bDynamic = TRUE;
	}
	else
	{
		bDynamic = FALSE;
	}

	if (!(tszTemp = _tcschr(tszIp, _T('@'))))
	{
		if (lpBuffer)
		{
			FormatString(lpBuffer, _T("%sInvalid hostmask: No @ specified ('%s').\r\n"), tszMultilinePrefix, tszIp);
		}
		ERROR_RETURN(ERROR_INVALID_ARGUMENTS, FALSE);
	}

	if (tszTemp == tszIp)
	{
		if (lpBuffer)
		{
			FormatString(lpBuffer, _T("%sInvalid hostmask: No ident (or *) specified before @ ('%s').\r\n"), tszMultilinePrefix, tszIp);
		}
		ERROR_RETURN(ERROR_INVALID_ARGUMENTS, FALSE);
	}

	*tszTemp = _T('\0');
	bIdent = (_tcschr(tszIp, _T('*')) ? FALSE : TRUE);
	*tszTemp = _T('@');

	if (!*tszTemp++)
	{
		if (lpBuffer)
		{
			FormatString(lpBuffer, _T("%sInvalid hostmask: No address specified ('%s').\r\n"), tszMultilinePrefix, tszIp);
		}
		ERROR_RETURN(ERROR_INVALID_ARGUMENTS, FALSE);
	}

	bWild    = FALSE;
	bNumeric = TRUE;
	dwFields = 0;
	tszMark = tszTemp-1;

	for( ; *tszTemp ; tszTemp++ )
	{
		if (*tszTemp == _T('.'))
		{
			if ((*tszMark == _T('.') || *tszMark == _T('@')) && tszMark != tszTemp)
			{
				dwFields++;
			}
			tszMark = tszTemp;
			continue;
		}

		if (*tszTemp == _T('*'))
		{
			bWild = TRUE;
			tszMark = tszTemp;
			continue;
		}

		if (bNumeric && !_istdigit(*tszTemp))
		{
			bNumeric = FALSE;
		}
	}

	if (*tszMark == _T('.'))
	{
		dwFields++;
	}

	iOffset   = 0;
	bAllowed  = FALSE;
	bHasRules = FALSE;

	for (n=1 ; !bAllowed && n <= 20 ; n++)
	{
		_stprintf_s(tszSecure, sizeof(tszSecure)/sizeof(*tszSecure), _T("Secure_Ip_%u"), n);
		if (!Config_Get(&IniConfigFile, _T("Network"), tszSecure, tszLine, &iOffset))
		{
			break;
		}
		if (4 != _stscanf_s(tszLine, _T("%u %u %u %[^\n]"), &dwIdentRequired, &dwHostType, &dwMinFields, 
			tszPerms, sizeof(tszPerms)))
		{
			// bad settings
			Putlog(LOG_ERROR, _TEXT("Bad .ini file settings: '%s'\r\n"), tszSecure);
			continue;
		}
		bHasRules = TRUE;
		if (!HavePermission(lpAdmin, tszPerms) &&
			(!dwIdentRequired || bIdent) &&
			(((dwHostType == 0 || dwHostType == 3) && bNumeric && ( dwFields >= dwMinFields )) ||
			(dwHostType == 1 && !bNumeric && !bWild && !bDynamic )                            ||
			((dwHostType == 2 || dwHostType == 3) && !bNumeric && !bWild && bDynamic )        ||
			(dwHostType == 3 && !bNumeric && !bDynamic && ( dwFields >= dwMinFields ))    ))
		{
			bAllowed = TRUE;
			break;
		}
	}


	if (!bHasRules)
	{
		// no rule backward compatibility
		bAllowed = TRUE;
	}

	if (!bAllowed)
	{
		if (lpAdmin && HasFlag(lpAdmin, _T("M")))
		{
			if (lpBuffer)
			{
				FormatString(lpBuffer, _T("%sDenied: hostmask '%s' failed IP requirements.\r\n"), tszMultilinePrefix, tszIp);
			}
			ERROR_RETURN(ERROR_NO_MATCH, FALSE);
		}
		// we're allowed to override as master
	}

	for (n = 0 ; n < MAX_IPS && lpUserFile->Ip[n][0] ; n++)
	{
		szMask = lpUserFile->Ip[n];

		//	Find existing entries
		if (!stricmp(tszIp, szMask))
		{
			// identical match, just ignore
			if (lpBuffer)
			{
				FormatString(lpBuffer, _T("%sIgnored hostmask: '%s' identical to existing '%s'.\r\n"),
					tszMultilinePrefix, tszIp, szMask);
			}
			// non-fatal
			ERROR_RETURN(ERROR_ALREADY_EXISTS, TRUE);
		}
		// Look for existing now obsolete entries using wildcard.  identical caught above
		else if (! iCompare(tszIp, szMask))
		{
			//	Show output
			if (lpBuffer)
			{
				FormatString(lpBuffer, _T("%sRemoved hostmask: '%s'\r\n"), tszMultilinePrefix, szMask);
			}
			Obscure_Mask(szObscuredMask, szMask);
			Putlog(LOG_SYSOP, _TEXT("'%s' removed ip '%s' from user '%s'.\r\n"), tszAdminName, szObscuredMask, tszUserName);
			//	Update IP list
			MoveMemory(szMask, &lpUserFile->Ip[n + 1], (MAX_IPS - n - 1) * sizeof(lpUserFile->Ip[0]));
			lpUserFile->Ip[MAX_IPS - 1][0]	= 0;
			n--;
		}
		// Look for an already existing wildcard match for new entry
		else if (! iCompare(szMask, tszIp))
		{
			if (lpBuffer)
			{
				FormatString(lpBuffer, _T("%sIgnored hostmask: '%s' matches existing '%s'.\r\n"),
					tszMultilinePrefix, szMask, tszIp);
			}
			// non-fatal
			ERROR_RETURN(ERROR_ALREADY_EXISTS, TRUE);
		}
	}

	if (n >= MAX_IPS)
	{
		if (lpBuffer)
		{
			FormatString(lpBuffer, _TEXT("%sDenied hostmask: '%s' - Maximum ips per account limit reached!\r\n"),
				tszMultilinePrefix, tszIp);
		}
		ERROR_RETURN(ERROR_INVALID_ARGUMENTS, FALSE);
	}

	//	Add new ip to array
	_tcsncpy(lpUserFile->Ip[n], tszIp, _IP_LINE_LENGTH);
	//	Show output
	if (lpBuffer)
	{
		FormatString(lpBuffer, _TEXT("%sAdded hostmask: '%s'%s\r\n"), tszMultilinePrefix, lpUserFile->Ip[n],
			(bAllowed ? _T("") : _T(" (master override)")));
	}
	Obscure_Mask(szObscuredMask, tszIp);
	Putlog(LOG_SYSOP, _TEXT("'%s' added ip '%s'%s to user '%s'.\r\n"), tszAdminName, szObscuredMask,
		(bAllowed ? _T("") : _T(" (master override)")), tszUserName);
	ERROR_RETURN(NO_ERROR, TRUE);
}



VOID
ShowAddIpRules(LPUSERFILE lpUserFile, LPBUFFER lpBuffer, LPTSTR tszMultilinePrefix)
{
	TCHAR   tszLine[_INI_LINE_LENGTH+1], tszPerms[_INI_LINE_LENGTH+1], tszSecure[20];
	LPTSTR  tszTemp;
	DWORD   n, m, dwMinFields, dwHostType, dwIdentRequired;
	INT32   iOffset;
	BOOL    bHeaderShown;


	bHeaderShown = FALSE;
	iOffset = 0;
	for (n=m=1 ; n <= 20 ; n++)
	{
		_stprintf_s(tszSecure, sizeof(tszSecure)/sizeof(*tszSecure), "Secure_Ip_%d", n);
		if (!Config_Get(&IniConfigFile, _T("Network"), tszSecure, tszLine, &iOffset))
		{
			break;
		}
		if (4 != _stscanf_s(tszLine, "%u %u %u %[^\n]", &dwIdentRequired, &dwHostType, &dwMinFields, 
			tszPerms, sizeof(tszPerms)))
		{
			continue;
		}
		if (!bHeaderShown)
		{
			bHeaderShown = TRUE;
			FormatString(lpBuffer, _TEXT("%sList of IP/host mask rules:\r\n"), tszMultilinePrefix);
		}
		if (4 != _stscanf_s(tszLine, "%u %u %u %[^\n]", &dwIdentRequired, &dwHostType, &dwMinFields, 
			tszPerms, sizeof(tszPerms)))
		{
			continue;
		}
		if (!lpUserFile || !HavePermission(lpUserFile, tszPerms))
		{
			tszTemp = (dwIdentRequired ? _T("ident") : _T("*"));
			switch (dwHostType)
			{
			case 0:
				FormatString(lpBuffer, _TEXT("%s  %d: %s@numeric-only-IP  [ %d levels required ]\r\n"),
					tszMultilinePrefix, m, tszTemp, dwMinFields);
				break;
			case 1:
				FormatString(lpBuffer, _TEXT("%s  %d: %s@fully-qualified-name\r\n"),
					tszMultilinePrefix, m, tszTemp, dwMinFields);
				break;
			case 2:
				FormatString(lpBuffer, _TEXT("%s  %d: :%s@dynamically-resolved-fullname\r\n"),
					tszMultilinePrefix, m, tszTemp, dwMinFields);
				break;
			case 3:
				FormatString(lpBuffer, _TEXT("%s  %d: %s@wildcard-hostname  [ %d levels required ]\r\n"),
					tszMultilinePrefix, m, tszTemp, dwMinFields);
				break;
			}
			m++;
		}
	}
	if (!bHeaderShown)
	{
		FormatString(lpBuffer, _TEXT("%sThere are no IP/host mask rules that apply to you.\r\n"), tszMultilinePrefix);
	}

}


/*

  User_AddIp() - Adds ip to user

  */
LPTSTR Admin_AddIp(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPBUFFER		lpBuffer;
	LPUSERFILE		lpUserFile;
	LPTSTR			tszUserName, tszIp, tszAdmin, tszError;
	DWORD			dwError;
	UINT			i;
	BOOL            bAllowed, bShowRules;


	dwError	= NO_ERROR;
	lpBuffer	= &lpUser->CommandChannel.Out;

	if ((GetStringItems(Args) == 1) && lpBuffer)
	{
		ShowAddIpRules(lpUser->UserFile, lpBuffer, tszMultilinePrefix);
		return NULL;
	}

	if (GetStringItems(Args) < 3) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));

	//	Get arguments
	tszUserName	= GetStringIndexStatic(Args, 1);

	//	Open userfile
	if (UserFile_Open(tszUserName, &lpUserFile, 0)) return tszUserName;
	tszError = tszUserName;
	bShowRules = FALSE;
	if (! UserFile_Lock(&lpUserFile, 0))
	{
		//	Check permissions
		dwError = CheckForMasterAccount(lpUser, lpUserFile);

		if (dwError == NO_ERROR)
		{
			tszAdmin = LookupUserName(lpUser->UserFile);

			bAllowed = TRUE;
			for (i=2U ; bAllowed && i < GetStringItems(Args) ; i++)
			{
				tszIp	= GetStringIndexStatic(Args, i);
				if (!tszIp) break;

				if (!AddIpToUser(lpUser->UserFile, tszAdmin, lpUserFile, tszUserName,
					lpBuffer, tszMultilinePrefix, tszIp))
				{
					dwError = GetLastError();
					if (GetLastError() != ERROR_NO_MATCH)
					{
						break;
					}
					dwError = NO_ERROR;
					bShowRules = TRUE;
				}
			}
		}
		UserFile_Unlock(&lpUserFile, 0);
	}
	else dwError	= GetLastError();
	UserFile_Close(&lpUserFile, 0);

	if (bShowRules)
	{
		ShowAddIpRules(lpUser->UserFile, lpBuffer, tszMultilinePrefix);
	}

	if (dwError == NO_ERROR) return NULL;
	ERROR_RETURN(dwError, tszError);
}






LPTSTR Admin_AddUserX(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args, BOOL bToGroup)
{
	LPUSERFILE		lpUserFile;
	LPGROUPFILE		lpGroupFile, lpGroupFile2;
	LPBUFFER        lpBuffer;
	BOOL			bUpdateGroup;
	LPTSTR			tszGroupName, tszUserName, tszPassword, tszIp, tszAdmin, tszGroupName2;
	INT				Gid, Uid;
	DWORD			n, i, dwUserError, dwGroupError;
	BOOL            bIsAdmin, bLogged, bAdded;

	lpBuffer	= &lpUser->CommandChannel.Out;

	if (!HasFlag(lpUser->UserFile, _T("1M")))
	{
		bIsAdmin = TRUE;
	}
	else
	{
		bIsAdmin = FALSE;
	}

	if (bToGroup)
	{
		if (GetStringItems(Args) < 4) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));

		//	Get arguments
		tszUserName	= GetStringIndexStatic(Args, 2);
		tszGroupName	= GetStringIndexStatic(Args, 1);
		tszPassword	= GetStringIndexStatic(Args, 3);

		Gid	= Group2Gid(tszGroupName);
		if (Gid < 0) ERROR_RETURN(ERROR_GROUP_NOT_FOUND, tszGroupName);
	}
	else
	{
		if (GetStringItems(Args) < 3) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));

		if ((lpUser->UserFile->AdminGroups[0] == -1) && !bIsAdmin)
		{
			ERROR_RETURN(IO_GADMIN_EMPTY, GetStringIndexStatic(Args, 0));
		}
		if (lpUser->UserFile->AdminGroups[0] != -1)
		{
			Gid = lpUser->UserFile->AdminGroups[0];
		}
		else
		{
			Gid	= NOGROUP_ID;
		}

		//	Get arguments
		tszUserName	 = GetStringIndexStatic(Args, 1);
		tszGroupName = Gid2Group(Gid);
		tszPassword	 = GetStringIndexStatic(Args, 2);
	}

	if (IsUserName(tszUserName)) return tszUserName;

	tszAdmin = LookupUserName(lpUser->UserFile);
	bUpdateGroup	= FALSE;

	//	Handle group-admin
	if (!bIsAdmin)
	{
		//	Check admin group
		for (n = 0;n < MAX_GROUPS && lpUser->UserFile->AdminGroups[n] != -1;n++)
		{
			if (lpUser->UserFile->AdminGroups[n] == Gid) break;
		}
		if (n == MAX_GROUPS || lpUser->UserFile->AdminGroups[n] != Gid)
		{
			ERROR_RETURN(IO_NOT_GADMIN, tszGroupName);
		}
		bUpdateGroup	= TRUE;
	}


	dwUserError	= NO_ERROR;
	dwGroupError	= NO_ERROR;
	tszGroupName	= Gid2Group(Gid);
	if (! tszGroupName) tszGroupName	= _TEXT("<deleted>");
	//	Open groupfile
	if (GroupFile_OpenPrimitive(Gid, &lpGroupFile, 0)) return tszGroupName;
	if (! GroupFile_Lock(&lpGroupFile, 0))
	{
		//	Check slots
		if (! bUpdateGroup || lpGroupFile->Slots[0] != 0)
		{
			//	Create user
			Uid	= CreateUser(tszUserName, Gid);
			if (Uid != -1)
			{
				bLogged = FALSE;
				if (! UserFile_OpenPrimitive(Uid, &lpUserFile, 0))
				{
					if (! UserFile_Lock(&lpUserFile, 0))
					{
						bLogged = TRUE;
						//	Apply modifications to userfile
						HashString(tszPassword, lpUserFile->Password);

						// remove all references to NOGROUP_ID
						for (i = 0 ; (i < MAX_GROUPS) && (lpUserFile->Groups[i] != -1) ; i++)
						{
							if (lpUserFile->Groups[i] == NOGROUP_ID)
							{
								MoveMemory(&lpUserFile->Groups[i], &lpUserFile->Groups[i + 1], sizeof(INT32) * (MAX_GROUPS - 1 - i));
								lpUserFile->Groups[MAX_GROUPS-1] = -1;
							}
						}

						// replace reference if no groups left...
						if (lpUserFile->Groups[0] == -1)
						{
							lpUserFile->Groups[0] = NOGROUP_ID;
							lpUserFile->Groups[1] = -1;
						}

						if (Gid != NOGROUP_ID)
						{
							if (lpUserFile->Groups[0] != NOGROUP_ID)
							{
								//	Make sure user is not already added to group we're trying to add
								for (n = 0;n < MAX_GROUPS && lpUserFile->Groups[n] != -1;n++)
								{
									if (Gid == lpUserFile->Groups[n]) break;
								}
								//	Add user to group if neccessary/possible
								if (n < MAX_GROUPS && lpUserFile->Groups[n] == -1)
								{
									// Make room for new group at the front of array
									MoveMemory(&lpUserFile->Groups[1], &lpUserFile->Groups[0], n * sizeof(int));
									lpUserFile->Groups[0]	= Gid;
									if (n + 1 < MAX_GROUPS) lpUserFile->Groups[n + 1]	= -1;
								}
								else if (n < MAX_GROUPS)
								{
									// user already a member of group, just make sure it's at front
									MoveMemory(&lpUserFile->Groups[1], &lpUserFile->Groups[0], n * sizeof(int));
									lpUserFile->Groups[0]	= Gid;
								}
								else
								{
									// make room at front for new group, drop the last group
									MoveMemory(&lpUserFile->Groups[1], &lpUserFile->Groups[0], (n-1) * sizeof(int));
									lpUserFile->Groups[0]	= Gid;
								}
							}
							else lpUserFile->Groups[0]	= Gid;
						}

						if (bUpdateGroup && lpGroupFile->Slots[0] != -1) lpGroupFile->Slots[0]--;

						for (n = 0;n < MAX_GROUPS && lpUserFile->Groups[n] != -1;n++)
						{
							bAdded = FALSE;
							// it's the currently open/locked group
							if (Gid == lpUserFile->Groups[n])
							{
								bAdded = TRUE;
								lpGroupFile->Users++;
								tszGroupName2 = tszGroupName;

							}
							else
							{
								tszGroupName2 = NULL;
								if (!GroupFile_OpenPrimitive(lpUserFile->Groups[n], &lpGroupFile2, 0))
								{
									if (! GroupFile_Lock(&lpGroupFile2, 0))
									{
										lpGroupFile2->Users++;
										bAdded = TRUE;
										tszGroupName2 = Gid2Group(lpUserFile->Groups[n]);
										if (!tszGroupName2) tszGroupName2 = _T("<Unknown>");
										GroupFile_Unlock(&lpGroupFile2, 0);
									}
									GroupFile_Close(&lpGroupFile2, 0);
								}
							}

							if (!n)
							{
								// first group is default group
								FormatString(lpBuffer, _TEXT("%sCreated user '%s' in group '%s'%s.\r\n"),
									tszMultilinePrefix, tszUserName, tszGroupName2, (lpUserFile->Groups[0] == NOGROUP_ID ? _T(" (default)") : _T("")));
								Putlog(LOG_SYSOP, _TEXT("'%s' created user '%s' in group '%s'.\r\n"),
									tszAdmin, tszUserName, tszGroupName2);
							}
							else if (bAdded)
							{
								FormatString(lpBuffer, _TEXT("%sAdded new user '%s' to group '%s' automatically.\r\n"),
									tszMultilinePrefix, tszUserName, tszGroupName2);
								Putlog(LOG_SYSOP, _TEXT("'%s' added new user '%s' to group '%s' automatically.\r\n"),
									tszAdmin, tszUserName, tszGroupName2);
							}
							else
							{
								FormatString(lpBuffer, _TEXT("%sError adding new user '%s' to group '%s' automatically.\r\n"),
									tszMultilinePrefix, tszUserName, tszGroupName2);
								Putlog(LOG_ERROR, _TEXT("'%s' failed to add new user '%s' to group '%s' automatically.\r\n"),
									tszAdmin, tszUserName, tszGroupName2);
							}
						}

						//	Add New Ips
						for (i = (bToGroup ? 4 : 3);i < GetStringItems(Args);i++)
						{
							tszIp	= GetStringIndexStatic(Args, i);

							if (!AddIpToUser(lpUser->UserFile, tszAdmin, lpUserFile, tszUserName,
								lpBuffer, tszMultilinePrefix, tszIp))
							{
								if (GetLastError() != ERROR_NO_MATCH)
								{
									break;
								}
							}
						}

						strcpy(lpUserFile->CreatorName, tszAdmin);
						lpUserFile->CreatorUid = lpUser->UserFile->Uid;
						lpUserFile->CreatedOn = time((time_t *) NULL);

						UserFile_Unlock(&lpUserFile, 0);
					}
					else dwUserError	= GetLastError();

					tszGroupName	= Gid2Group(lpUserFile->Groups[0]);
					if (! tszGroupName) tszGroupName	= _TEXT("<deleted>");

					UserFile_Close(&lpUserFile, 0);
				}
				else dwUserError	= GetLastError();

				if (!bLogged)
				{
					DeleteUser(tszUserName);
					if (dwUserError == NO_ERROR) dwUserError = ERROR_COMMAND_FAILED;
				}
			}
			else dwUserError	= GetLastError();
		}
		else dwGroupError	= IO_NO_SLOTS;
				
		GroupFile_Unlock(&lpGroupFile, 0);
	}
	else dwGroupError	= GetLastError();
	GroupFile_Close(&lpGroupFile, 0);

	if (dwUserError != NO_ERROR) ERROR_RETURN(dwUserError, tszUserName);
	if (dwGroupError != NO_ERROR) ERROR_RETURN(dwGroupError, tszGroupName);

	return NULL;
}



LPTSTR Admin_AddUser(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	return Admin_AddUserX(lpUser, tszMultilinePrefix, Args, FALSE);
}



LPTSTR Admin_AddUserToGroup(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	return Admin_AddUserX(lpUser, tszMultilinePrefix, Args, TRUE);
}



LPTSTR Admin_ChangeUserGroups(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPGROUPFILE		lpGroupFile;
	LPBUFFER		lpBuffer;
	LPUSERFILE		lpUserFile;
	PINT32			lpGroups;
	DWORD			dwError;
	LPTSTR			tszUserName, tszGroupName, tszAdmin;
	BOOL			bDefault, bFirst, bForce, bAdd;
	USERFILE        UserFile;
	DWORD			i, n, dwInNoGroup;
	INT32           Gid;


	dwError	= NO_ERROR;
	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));

	tszUserName	= GetStringIndexStatic(Args, 1);
	if (!_tcsnicmp(tszUserName, _T("/Default"), 8))
	{
		bDefault = TRUE;
		// only M flagged users can change the default accounts
		if (HasFlag(lpUser->UserFile, _TEXT("M")))
		{
			ERROR_RETURN(IO_NO_ACCESS, GetStringIndexStatic(Args, 1));
		}

		if (!_tcsicmp(tszUserName, _T("/Default.User")))
		{
			Gid = -1;
		}
		else if (tszUserName[8] == _T('='))
		{
			if ((Gid = Group2Gid(&tszUserName[9])) == -1)
			{
				ERROR_RETURN(ERROR_GROUP_NOT_FOUND, &tszUserName[9]);
			}
		}
		else
		{
			ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringIndexStatic(Args, 1));
		}

		if (User_Default_Open(&UserFile, Gid))
		{
			return tszUserName;
		}

		lpUserFile = &UserFile;
	}
	else
	{
		bDefault = FALSE;

		//	Open userfile
		if (UserFile_Open(tszUserName, &lpUserFile, 0))
		{
			return tszUserName;
		}

		if (UserFile_Lock(&lpUserFile, 0))
		{
			dwError	= GetLastError();
			UserFile_Close(&lpUserFile, 0);
			ERROR_RETURN(dwError, tszUserName);
		}

	}

	//	Get argumentss
	lpBuffer	= &lpUser->CommandChannel.Out;
	tszAdmin    = LookupUserName(lpUser->UserFile);
	lpGroups    = lpUserFile->Groups;

	//	Remove user from 'nogroup' group for the moment
	lpGroups = lpUserFile->Groups;

	dwInNoGroup	= 0;
	for (i = 0 ; (i < MAX_GROUPS) && (lpGroups[i] != -1) ; i++)
	{
		if (lpGroups[i] == NOGROUP_ID)
		{
			dwInNoGroup++;
			MoveMemory(&lpGroups[i], &lpGroups[i + 1], sizeof(INT32) * (MAX_GROUPS - 1 - i));
			lpGroups[MAX_GROUPS-1] = -1;
		}
	}

	//	Perform chgroup on any supplied parameters, if none we just want to print group membership
	lpGroupFile = NULL;
	for (n = 2 ; n < GetStringItems(Args) ; n++)
	{
		tszGroupName	= GetStringIndexStatic(Args, n);

		if (!tszGroupName)
		{
			break;
		}

		if (lpGroupFile)
		{
			GroupFile_Unlock(&lpGroupFile, 0);
			GroupFile_Close(&lpGroupFile, 0);
			lpGroupFile = NULL;
		}

		switch (tszGroupName[0])
		{
		case _T('.'):
			tszGroupName++;
			bForce = FALSE;
			bFirst = TRUE;
			break;
		case _T('+'):
			tszGroupName++;
			bForce = TRUE;
			bFirst = FALSE;
			bAdd   = TRUE;
			break;
		case _T('-'):
			tszGroupName++;
			bForce = TRUE;
			bFirst = FALSE;
			bAdd   = FALSE;
			if ((tszGroupName[0] == _T('*')) && !tszGroupName[1])
			{
				// special case '-*' option to remove all groups!
				for (i = 0 ; (i < MAX_GROUPS) && (lpGroups[i] != -1) ; )
				{
					tszGroupName = Gid2Group(lpGroups[i]);
					if (!tszGroupName) tszGroupName = _T("<Unknown>");
					if (GroupFile_OpenPrimitive(lpGroups[i], &lpGroupFile, 0) || !lpGroupFile || GroupFile_Lock(&lpGroupFile, 0))
					{
						dwError	= GetLastError();
						if (lpGroupFile)
						{
							GroupFile_Close(&lpGroupFile, 0);
							lpGroupFile = NULL;
						}
						FormatString(lpBuffer, _TEXT("%s%2TERROR: Unable to access/lock group '%5T%s%2T': %E.%0T\r\n"),
							tszMultilinePrefix, tszGroupName, dwError);
						i++;
						continue;
					}
					lpGroupFile->Users--;
					FormatString(lpBuffer, _TEXT("%s%4TREMOVED:%0T User '%5T%s%0T' has been %4TREMOVED%0T from group '%5T%s%0T'.\r\n"),
						tszMultilinePrefix, tszUserName, tszGroupName);
					//	Log event
					Putlog(LOG_SYSOP, _TEXT("'%s' removed user '%s' from group '%s'.\r\n"),
						tszAdmin, tszUserName, tszGroupName);
					//	Update group array
					MoveMemory(&lpGroups[i], &lpGroups[i + 1], sizeof(INT32) * (MAX_GROUPS - 1 - i));
					lpGroups[MAX_GROUPS-1] = -1;
					GroupFile_Unlock(&lpGroupFile, 0);
					GroupFile_Close(&lpGroupFile, 0);
					lpGroupFile = NULL;
					// not incrementing i since we just shifted stuff down...
				}
				continue;
			}
			break;
		default:
			bForce  = FALSE;
			bFirst  = FALSE;
		}

		//	Open and lock groupfile
		if (GroupFile_Open(tszGroupName, &lpGroupFile, 0) || !lpGroupFile || GroupFile_Lock(&lpGroupFile, 0))
		{
			dwError	= GetLastError();
			if (lpGroupFile)
			{
				GroupFile_Close(&lpGroupFile, 0);
				lpGroupFile = NULL;
			}
			FormatString(lpBuffer, _TEXT("%s%2TERROR: Unable to access/lock group '%5T%s%2T': %E.%0T\r\n"),
				tszMultilinePrefix, tszGroupName, dwError);
			continue;
		}

		if (lpGroupFile->Gid == NOGROUP_ID)
		{
			// you can't add/remove this group explicitly
			FormatString(lpBuffer, _TEXT("%s%2TERROR: Cannot add/remove default membership group (id=%5T%d%2T, name='%5T%s%2T').%0T\r\n"),
				tszMultilinePrefix, NOGROUP_ID, tszGroupName);
			continue;
		}

		for (i = 0 ; (i < MAX_GROUPS) && (lpGroups[i] != -1) ; i++)
		{
			if (lpGroups[i] == lpGroupFile->Gid)
			{
				break;
			}
		}

		if (bFirst && (i == 0) && (lpGroups[i] == lpGroupFile->Gid))
		{
			// we are specifying the default group but it's already the default...
			FormatString(lpBuffer, _TEXT("%sSKIP: User '%5T%s%0T' already has '%5T%s%0T' as the DEFAULT group.%0T\r\n"),
				tszMultilinePrefix, tszUserName, tszGroupName);
			continue;
		}
		else if (bForce && bAdd && (lpGroups[i] == lpGroupFile->Gid))
		{
			// we are already a member
			FormatString(lpBuffer, _TEXT("%sSKIP: User '%5T%s%0T' already a member of group '%5T%s%0T'.\r\n"),
				tszMultilinePrefix, tszUserName, tszGroupName);
			continue;
		}
		else if ((i >= MAX_GROUPS) && (!bForce || (bForce && bAdd)))
		{
			//	Could not add user to group because we're out of room
			FormatString(lpBuffer, _TEXT("%s%2TERROR: Could not add user '%5T%s%2T' to group '%5T%s%2T': max groups/user limit reached!%0T\r\n"),
				tszMultilinePrefix, tszUserName, tszGroupName);
			continue;
		}
		else if ((i < MAX_GROUPS) && (lpGroups[i] == lpGroupFile->Gid) && (bFirst || !bForce || (bForce && !bAdd)))
		{
			//	Update groupfile
			if (!bDefault) lpGroupFile->Users--;

			FormatString(lpBuffer, _TEXT("%s%4TREMOVED:%0T User '%5T%s%0T' has been %4TREMOVED%0T from group '%5T%s%0T'.\r\n"),
				tszMultilinePrefix, tszUserName, tszGroupName);
			//	Log event
			Putlog(LOG_SYSOP, _TEXT("'%s' removed user '%s' from group '%s'.\r\n"),
				tszAdmin, tszUserName, tszGroupName);
			//	Update group array
			MoveMemory(&lpGroups[i], &lpGroups[i + 1], sizeof(INT32) * (MAX_GROUPS - 1 - i));
			lpGroups[MAX_GROUPS-1] = -1;

			if (!bFirst)
			{
				// we just wanted to remove the group, so we are done now
				continue;
			}
		}
		else if (bForce && !bAdd)
		{
			// not a member of the group so nothing to remove
			FormatString(lpBuffer, _TEXT("%sSKIP: User '%5T%s%0T' is not a member of group '%5T%s%0T'.\r\n"),
				tszMultilinePrefix, tszUserName, tszGroupName);
			continue;
		}

		//	Add user to group...
		if (bFirst || !i)
		{
			FormatString(lpBuffer, _TEXT("%s%4TADDED:%0T User '%5T%s%0T' has been %4TADDED%0T to group '%5T%s%0T' as %4TDEFAULT%0T.\r\n"),
				tszMultilinePrefix, tszUserName, tszGroupName);
		}
		else
		{
			FormatString(lpBuffer, _TEXT("%s%4TADDED:%0T User '%5T%s%0T' has been %4TADDED%0T to group '%5T%s%0T'.\r\n"),
				tszMultilinePrefix, tszUserName, tszGroupName);
		}
		//	Log event
		Putlog(LOG_SYSOP, _TEXT("'%s' added user '%s' to group '%s'%s.\r\n"),
			tszAdmin, tszUserName, tszGroupName,
			(bFirst ? _T(" as DEFAULT") : _T("")));

		//	Update group counter
		if (!bDefault) lpGroupFile->Users++;

		//	Update group list
		if (bFirst && (i != 0))
		{
			MoveMemory(&lpGroups[1], &lpGroups[0], i * sizeof(INT));
			lpGroups[0]	= lpGroupFile->Gid;
			if (++i < MAX_GROUPS) lpGroups[i]	= -1;
		}
		else
		{
			lpGroups[i]	= lpGroupFile->Gid;
			if (++i < MAX_GROUPS) lpGroups[i]	= -1;
		}

	}

	if (lpGroupFile)
	{
		GroupFile_Unlock(&lpGroupFile, 0);
		GroupFile_Close(&lpGroupFile, 0);
	}

	// now update NoGroup group data if needed
	dwError = NO_ERROR;
	if (!bDefault && ((lpGroups[0] == -1) && !dwInNoGroup) || ((lpGroups[0] != -1) && dwInNoGroup))
	{
		// we need to modify the user count for the NoGroup group.
		if (! GroupFile_OpenPrimitive(NOGROUP_ID, &lpGroupFile, 0))
		{
			if (! GroupFile_Lock(&lpGroupFile, 0))
			{
				//	Update user count
				if (dwInNoGroup)
				{
					lpGroupFile->Users -= dwInNoGroup;
				}
				else
				{
					lpGroupFile->Users++;
				}
				GroupFile_Unlock(&lpGroupFile, 0);
			}
			else dwError	= GetLastError();
			GroupFile_Close(&lpGroupFile, 0);
		}
		else dwError	= GetLastError();

		//	Print error
		if (dwError != NO_ERROR)
		{
			FormatString(lpBuffer, _TEXT("%s%2TERROR: Cannot update built-in group id #%5T%d%2T '%5T%s%2T': %E.%0T\r\n"),
				tszMultilinePrefix, NOGROUP_ID, (tszGroupName = Gid2Group(NOGROUP_ID) ? tszGroupName : _T("*MISSING*")), dwError);
		}
	}

	if (!bDefault && (lpGroups[0] == -1))
	{
		lpGroups[0] = NOGROUP_ID;
		lpGroups[1] = -1;
	}

	// display current group memberships for user
	FormatString(lpBuffer, _TEXT("%s\r\n%sNew group list: %5T"), tszMultilinePrefix, tszMultilinePrefix);
	for (i = 0;i < MAX_GROUPS && lpGroups[i] != -1;i++)
	{
		tszGroupName = Gid2Group(lpGroups[i]);
		if (tszGroupName)
		{
			FormatString(lpBuffer, _TEXT("%s "), tszGroupName);
		}
		else
		{
			FormatString(lpBuffer, _TEXT("(gid=%d) "), lpGroups[i]);
		}
	}
	FormatString(lpBuffer, _TEXT("%0T\r\n"));


	if (bDefault)
	{
		if (User_Default_Write(lpUserFile))
		{
			dwError = GetLastError();
			if (dwError == ERROR_USER_DELETED)
			{
				FormatString(lpBuffer, _TEXT("%s\r\n%s%4TGroup '%5T%s%4T' is no longer specialized.  Reverting to 'Default.User' for new users.%0T\r\n"),
					tszMultilinePrefix, tszMultilinePrefix, tszUserName);
				dwError = NO_ERROR;
			}
		}

		if (!User_Default_Close(lpUserFile) && !dwError)
		{
			dwError = GetLastError();
		}
	}
	else
	{
		if (UserFile_Unlock(&lpUserFile, 0))
		{
			dwError = GetLastError();
		}
		if (UserFile_Close(&lpUserFile, 0) && !dwError)
		{
			dwError = GetLastError();
		}
	}

	if (dwError == NO_ERROR) return NULL;
	SetLastError(dwError);
	return tszUserName;
}



LPTSTR Admin_Close(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszUserName, tszGroupName, tszMsg, tszCloseExempt, tszNew;
	BOOL            bKick, bNewOnly, bDiffer, bSingle, bSingleChange;
	LPBUFFER		lpBuffer;
	DWORD           n, dwError, dwKicked, dwExempt;
	INT             Uid;
	LPCLIENT        lpClient;
	LPUSERFILE      lpUserFile;


	//	Get arguments
	lpBuffer	 = &lpUser->CommandChannel.Out;
	tszUserName  = LookupUserName(lpUser->UserFile);
	tszGroupName = Gid2Group(lpUser->UserFile->Gid);
	if (!tszGroupName) tszGroupName = _T("<System>");

	bKick = FALSE;
	bSingle = FALSE;
	bNewOnly = FALSE;
	tszMsg = NULL;
	if (GetStringItems(Args) > 1)
	{
		n = 1;
		if (!_tcsicmp(GetStringIndexStatic(Args, 1), "-single"))
		{
			if (HasFlag(lpUser->UserFile, _T("M")))
			{
				return GetStringIndexStatic(Args, 1);
			}
			bSingle = TRUE;
			n++;
		}

		if (GetStringItems(Args) > n)
		{
			if (!_tcsicmp(GetStringIndexStatic(Args, n), "-kick"))
			{
				bKick = TRUE;
				if (GetStringItems(Args) > ++n)
				{
					tszMsg = GetStringRange(Args, n, STR_END);
				}
			}
			else if (!_tcsicmp(GetStringIndexStatic(Args, n), "-new"))
			{
				bNewOnly = TRUE;
				if (GetStringItems(Args) > ++n)
				{
					tszMsg = GetStringRange(Args, n, STR_END);
				}
			}
			else
			{
				tszMsg = GetStringRange(Args, n, STR_END);
			}
		}
	}

	while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();

	if (tszMsg && !tszMsg[0]) tszMsg = NULL;

	dwError = NO_ERROR;
	bDiffer = ((FtpSettings.tszCloseMsg && tszMsg && _tcsicmp(FtpSettings.tszCloseMsg, tszMsg)) ||
		       (FtpSettings.tszCloseMsg && !tszMsg) || (!FtpSettings.tszCloseMsg && tszMsg));
	bSingleChange = ( (bSingle && (FtpSettings.iSingleCloseUID == -1)) || (!bSingle && (FtpSettings.iSingleCloseUID != -1)) );

	if (FtpSettings.tmSiteClosedOn)
	{
		// the site is already closed... perhaps just kicking users or updating the message...
		if (bSingleChange)
		{
			FormatString(lpBuffer, _TEXT("%sServer already closed, but now %s.\r\n"),
				tszMultilinePrefix, (bSingle ? _T("closed to *** everyone except YOU **") : _T("open to exempt users")));
		}
		if (bDiffer)
		{
			FormatString(lpBuffer, _TEXT("%sServer already closed, updating reason to '%s'.\r\n"),
				tszMultilinePrefix, (tszMsg ? tszMsg : _T("<none>")));
		}
		else if (!bSingleChange && !bDiffer && !bKick)
		{
			dwError = ERROR_ALREADY_CLOSED;
		}
	}
	else
	{
		FormatString(lpBuffer, _TEXT("%sClosing server "), tszMultilinePrefix);
		if (bSingle)
		{
			FormatString(lpBuffer, _TEXT("( *** to ALL users except YOU *** ) "));
		}
		else if (bNewOnly)
		{
			FormatString(lpBuffer, _TEXT("(only to NEW logins) "));
		}
		FormatString(lpBuffer, _TEXT("with reason '%s'.\r\n"), (tszMsg ? tszMsg : _T("<none>")));
		FtpSettings.tmSiteClosedOn = time((time_t) NULL);
	}

	if (!dwError)
	{
		if (bSingle)
		{
			FtpSettings.iSingleCloseUID = lpUser->UserFile->Uid;
		}
		else
		{
			FtpSettings.iSingleCloseUID = -1;
		}

		if (FtpSettings.tszCloseMsg)
		{
			FreeShared(FtpSettings.tszCloseMsg);
			FtpSettings.tszCloseMsg = NULL;
		}

		if (tszMsg)
		{
			n = _tcslen(tszMsg)+sizeof(*tszMsg);
			tszNew = AllocateShared(NULL, "CloseMsg", n);
			if (tszNew)
			{
				_tcscpy_s(tszNew, n, tszMsg);
				FtpSettings.tszCloseMsg = tszNew;
			}
		}

		FtpSettings.bKickNonExemptOnClose = (bNewOnly ? FALSE : TRUE);
	}

	tszCloseExempt = NULL;
	if (bKick && !dwError && FtpSettings.tszCloseExempt)
	{
		tszCloseExempt = (LPTSTR) AllocateShared(FtpSettings.tszCloseExempt, NULL, 0);
	}

	// can release the lock now
	InterlockedExchange(&FtpSettings.lStringLock, FALSE);

	if (dwError != NO_ERROR)
	{
		ERROR_RETURN(dwError, GetStringIndexStatic(Args, 0));
	}

	//	Close daemon
	Putlog(LOG_GENERAL, _TEXT("CLOSE: \"%s\" \"%s\" \"%s\"\r\n"), tszUserName, tszGroupName, (tszMsg ? tszMsg : _T("<none>")));

	if (bKick)
	{
		dwKicked = 0;
		dwExempt = 0;
		for ( n = 0 ; n <= dwMaxClientId ; n++ )
		{
			if ((lpClient = LockClient(n)))
			{
				Uid = lpClient->Static.Uid;
				UnlockClient(n);

				if (lpUser->Connection.dwUniqueId == n)
				{
					// don't kill me!
					continue;
				}
				if (bSingle && ( FtpSettings.iSingleCloseUID != Uid ) && ( FtpSettings.iServerSingleExemptUID != Uid ) )
				{
					if (!KillUser(n))
					{
						dwKicked++;
					}
					continue;
				}
				if (!UserFile_OpenPrimitive(Uid, &lpUserFile, 0))
				{
					if (!tszCloseExempt || HavePermission(lpUserFile, tszCloseExempt))
					{
						if (!KillUser(n))
						{
							dwKicked++;
						}
					}
					else dwExempt++;
					UserFile_Close(&lpUserFile, 0);
				}
			}
		}
		FormatString(lpBuffer, _TEXT("%sKicked %d users (%d exempt besides me).\r\n"),	tszMultilinePrefix, dwKicked, dwExempt);
	}

	FreeShared(tszCloseExempt);
	return NULL;
}



LPTSTR Admin_Open(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszUserName, tszGroupName, tszMsg;
	LPBUFFER        lpBuffer;
	time_t          tClosed;
	TCHAR           pBuffer[100];

	//	Get arguments
	lpBuffer	 = &lpUser->CommandChannel.Out;
	tszUserName  = LookupUserName(lpUser->UserFile);
	tszGroupName = Gid2Group(lpUser->UserFile->Gid);
	if (!tszGroupName) tszGroupName = _T("<System>");

	while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();

	if (!FtpSettings.tmSiteClosedOn)
	{
		InterlockedExchange(&FtpSettings.lStringLock, FALSE);
		ERROR_RETURN(ERROR_ALREADY_OPEN, GetStringIndexStatic(Args, 0));
	}

	tClosed = FtpSettings.tmSiteClosedOn;
	FtpSettings.tmSiteClosedOn = 0;
	tszMsg = FtpSettings.tszCloseMsg;
	FtpSettings.tszCloseMsg = NULL;

	tClosed = time((time_t) NULL) - tClosed;

	InterlockedExchange(&FtpSettings.lStringLock, FALSE);

	Putlog(LOG_GENERAL, _TEXT("OPEN: \"%s\" \"%s\" \"%d\" \"%s\"\r\n"), 
		tszUserName, tszGroupName, (DWORD) tClosed, (tszMsg ? tszMsg : _T("<none>")));

	if (tszMsg)
	{
		FreeShared(tszMsg);
	}

	Time_Duration(pBuffer, sizeof(pBuffer), tClosed, 0, 0, 2, 0, 0, ", ");

	FormatString(lpBuffer, _TEXT("%sServer opened.  Was closed for %s.\r\n"),	tszMultilinePrefix, pBuffer);

	return NULL;
}


DWORD Shutdown_TimerProc(LPVOID lpIgnored, LPTIMER lpTimer)
{
	LPTSTR   tszUserName;
	LPCLIENT lpClient;
	DWORD n, dwUsers;
	INT   Uid;

	while (InterlockedExchange(&FtpSettings.dwShutdownLock, TRUE)) SwitchToThread();

	if (FtpSettings.dwShutdownTimeLeft > SHUTDOWN_TIMER_DELAY)
	{
		FtpSettings.dwShutdownTimeLeft -= SHUTDOWN_TIMER_DELAY;
		InterlockedExchange(&FtpSettings.dwShutdownLock, FALSE);
		// count users
		dwUsers = 0;
		for ( n = 0 ; n <= dwMaxClientId ; n++ )
		{
			if ((lpClient = LockClient(n)))
			{
				Uid = lpClient->Static.Uid;
				UnlockClient(n);

				if (FtpSettings.dwShutdownCID != n)
				{
					dwUsers++;
				}
			}
		}

		if (dwUsers)
		{
			// people still connected, try again later
			return SHUTDOWN_TIMER_DELAY;
		}
	}

	// ran out of time or everybody offline, tell server to shutdown..

	//	Close daemon
	tszUserName = Uid2User(FtpSettings.ShutdownUID);
	if (!tszUserName) tszUserName = _T("<System>");

	Putlog(LOG_GENERAL, _TEXT("SHUTDOWN: \"%s\"\r\n"), tszUserName);

	FtpSettings.lpShutdownTimer = NULL;

	InterlockedExchange(&FtpSettings.dwShutdownLock, FALSE);

	SetDaemonStatus(DAEMON_GRACE);

	// cancel timer
	return INFINITE;
}




LPTSTR Admin_Shutdown(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszUserName, tszArg, tszTemp;
	LPBUFFER        lpBuffer;
	INT             iGrace;

	//	Get arguments
	lpBuffer	 = &lpUser->CommandChannel.Out;
	tszUserName	= LookupUserName(lpUser->UserFile);

	iGrace = -1;
	if (GetStringItems(Args) > 1)
	{
		tszArg = GetStringIndexStatic(Args, 1);

		if (!_tcsicmp(tszArg, _T("cancel")))
		{
			while (InterlockedExchange(&FtpSettings.dwShutdownLock, TRUE)) SwitchToThread();

			if (!FtpSettings.lpShutdownTimer)
			{
				InterlockedExchange(&FtpSettings.dwShutdownLock, FALSE);
				FormatString(lpBuffer, _TEXT("%sNothing to cancel - server not shutting down.\r\n"), tszMultilinePrefix);
				ERROR_RETURN(ERROR_COMMAND_FAILED, GetStringIndexStatic(Args, 0));
			}
			if (StopIoTimer(FtpSettings.lpShutdownTimer, FALSE) == 1)
			{
				InterlockedExchange(&FtpSettings.dwShutdownLock, FALSE);
				// rut ro, it probably just fired as we about to die...
				ERROR_RETURN(ERROR_COMMAND_FAILED, GetStringIndexStatic(Args, 0));
			}
			FtpSettings.lpShutdownTimer = NULL;
			FtpSettings.ShutdownUID = -1;
			FtpSettings.dwShutdownCID = -1;
			FtpSettings.dwShutdownTimeLeft = 0;
			InterlockedExchange(&FtpSettings.dwShutdownLock, FALSE);
			FormatString(lpBuffer, _TEXT("%sShutdown cancelled.\r\n"),	tszMultilinePrefix);
			return NULL;
		}

		if (!_tcsicmp(tszArg, _T("now")))
		{
			iGrace = 0;
		}
		else
		{
			iGrace = _tcstol(tszArg, &tszTemp, 10);
			if (!tszTemp || *tszTemp || iGrace < 0)
			{
				ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszArg);
			}
		}
	}

	if (iGrace == 0)
	{
		// immediate hard shutdown
		Putlog(LOG_GENERAL, _TEXT("SHUTDOWN: \"%s\"\r\n"), tszUserName);

		SetDaemonStatus(DAEMON_GRACE);
		return NULL;
	}

	if (iGrace == -1) iGrace = SHUTDOWN_DEFAULT_GRACE;

	while (InterlockedExchange(&FtpSettings.dwShutdownLock, TRUE)) SwitchToThread();

	if (FtpSettings.lpShutdownTimer)
	{
		FormatString(lpBuffer, _TEXT("%sServer already shutting down, grace period changed to '%d' seconds.\r\n"),
			tszMultilinePrefix, iGrace);
		FtpSettings.dwShutdownTimeLeft = iGrace * 1000;
		InterlockedExchange(&FtpSettings.dwShutdownLock, FALSE);
		return NULL;
	}

	FtpSettings.ShutdownUID = lpUser->UserFile->Uid;
	FtpSettings.dwShutdownCID = lpUser->Connection.dwUniqueId;
	FtpSettings.dwShutdownTimeLeft = iGrace * 1000;
	FtpSettings.lpShutdownTimer = StartIoTimer(NULL, Shutdown_TimerProc, NULL, 500);
	if (!FtpSettings.lpShutdownTimer)
	{
		InterlockedExchange(&FtpSettings.dwShutdownLock, FALSE);
		return GetStringIndexStatic(Args, 0);
	}

	InterlockedExchange(&FtpSettings.dwShutdownLock, FALSE);

	Putlog(LOG_GENERAL, _TEXT("SHUTDOWN-GRACE: \"%s\" \"%d\"\r\n"), tszUserName, iGrace);

	FormatString(lpBuffer, _TEXT("%sStarted server shutdown, grace period set to '%d' seconds.\r\n"),
		tszMultilinePrefix, iGrace);
	return NULL;
}



LPTSTR Admin_Config(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPBUFFER		lpBuffer;
	LPTSTR			tszCommand, tszError;
	DWORD			dwTokens, dwError;
	BOOL			bResult, bAllowed;


	dwTokens	= GetStringItems(Args);
	dwError		= ERROR_MISSING_ARGUMENT;
	tszError	= GetStringIndexStatic(Args, 0);
	if (dwTokens < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));

	//	Get arguments
	tszCommand	= GetStringIndexStatic(Args, 1);
	lpBuffer	= &lpUser->CommandChannel.Out;

	bAllowed = FALSE;
	Config_Get_Bool(&IniConfigFile, _TEXT("FTP"), _TEXT("Enable_Config_Commands"), (PINT) &bAllowed);

	if (! _tcsicmp(tszCommand, _TEXT("del")))
	{
		if (!bAllowed) ERROR_RETURN(IO_NO_ACCESS, tszCommand);
		if (dwTokens == 4)
		{
			bResult	= Config_Set(&IniConfigFile, GetStringIndexStatic(Args, 2),
				_ttoi(GetStringIndexStatic(Args, 3)), NULL, CONFIG_DEL);
			if (! bResult) return NULL;
			dwError	= GetLastError();
		}
		else if (dwTokens > 4)
		{
			tszError	= GetStringRange(Args, 4, STR_END);
			dwError	= ERROR_INVALID_ARGUMENTS;
		}
	}
	else if (! _tcsicmp(tszCommand, _TEXT("add")))
	{
		if (!bAllowed) ERROR_RETURN(IO_NO_ACCESS, tszCommand);
		if (dwTokens >= 4)
		{
			bResult	= Config_Set(&IniConfigFile, GetStringIndexStatic(Args, 2), -1,
				GetStringRange(Args, 3, STR_END), CONFIG_INSERT);
			if (! bResult) return NULL;
			dwError	= GetLastError();
		}
		else if (dwTokens > 4)
		{
			tszError	= GetStringRange(Args, 4, STR_END);
			dwError	= ERROR_INVALID_ARGUMENTS;
		}
	}
	else if (! _tcsicmp(tszCommand, _TEXT("save")))
	{
		if (!bAllowed) ERROR_RETURN(IO_NO_ACCESS, tszCommand);
		if (dwTokens == 2)
		{
			if (! Config_Write(&IniConfigFile)) return NULL;
			dwError	= GetLastError();
		}
		else
		{
			tszError	= GetStringRange(Args, 2, STR_END);
			dwError	= ERROR_INVALID_ARGUMENTS;
		}
	}
	else if (! _tcsicmp(tszCommand, _TEXT("show")) && lpBuffer && tszMultilinePrefix)
	{
		Config_Print(&IniConfigFile, lpBuffer, (dwTokens == 2 ? NULL : GetStringRange(Args, 2, STR_END)), tszMultilinePrefix);
		return NULL;
	}
	else if (! _tcsicmp(tszCommand, _TEXT("insert")))
	{
		if (!bAllowed) ERROR_RETURN(IO_NO_ACCESS, tszCommand);
		if (dwTokens >= 5)
		{
			bResult	= Config_Set(&IniConfigFile, GetStringIndexStatic(Args, 2),
				_ttoi(GetStringIndexStatic(Args, 3)),
				GetStringRange(Args, 4, STR_END), CONFIG_INSERT);
			if (! bResult) return NULL;
			dwError	= GetLastError();
		}
	}
	else if (! _tcsicmp(tszCommand, _TEXT("rehash")))
	{
		if (dwTokens == 2)
		{
			if (! Config_Read(&IniConfigFile))
			{
				InitializeDaemon(FALSE);
				return NULL;
			}
			dwError	= GetLastError();
		}
		else
		{
			tszError	= GetStringRange(Args, 2, STR_END);
			dwError	= ERROR_INVALID_ARGUMENTS;		
		}
	}
	else if (! _tcsicmp(tszCommand, _TEXT("replace")))
	{
		if (!bAllowed) ERROR_RETURN(IO_NO_ACCESS, tszCommand);
		if (dwTokens >= 5)
		{
			bResult	= Config_Set(&IniConfigFile, GetStringIndexStatic(Args, 2),
				_ttoi(GetStringIndexStatic(Args, 3)),
				GetStringRange(Args, 4, STR_END), CONFIG_REPLACE);
			if (! bResult) return NULL;
			dwError	= GetLastError();
		}
	}
	else
	{
		tszError	= GetStringRange(Args, 1, STR_END);
		dwError	= ERROR_INVALID_ARGUMENTS;
	}
	ERROR_RETURN(dwError, tszError);
}






LPTSTR Admin_SetOwnTagline(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPUSERFILE		lpUserFile;
	DWORD			dwError;
	LPTSTR			tszTagline, tszUserName;

	dwError	= NO_ERROR;
	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));

	//	Get arguments
	tszTagline	= GetStringRange(Args, 1, STR_END);
	tszUserName	= LookupUserName(lpUser->UserFile);

	if (! UserFile_OpenPrimitive(lpUser->UserFile->Uid, &lpUserFile, 0))
	{
		//	Change password
		if (! UserFile_Lock(&lpUserFile, 0))
		{
			_tcsncpy(lpUserFile->Tagline, tszTagline, 128);
			lpUserFile->Tagline[128] = 0;
			UserFile_Unlock(&lpUserFile, 0);
		}
		else dwError	= GetLastError();
		UserFile_Close(&lpUserFile, 0);
		if (dwError == NO_ERROR) return NULL;
	}
	else dwError	= GetLastError();
	ERROR_RETURN(dwError, tszUserName);
}


LPTSTR Admin_Uptime(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPBUFFER    lpBuffer;
	UINT64      u64FileTime, u64FileTime2;
	TCHAR       tszBuffer[100];


	lpBuffer = &lpUser->CommandChannel.Out;
	GetSystemTimeAsFileTime((FILETIME *) &u64FileTime);
	u64FileTime2 = u64FileTime;

	u64FileTime -= u64WindowsStartTime;
	u64FileTime /= 10000000;

	tszBuffer[0] = 0;
	Time_Duration(tszBuffer, sizeof(tszBuffer), u64FileTime,  _T('m'), _T('d'), 2, 0, 0, _T(", "));
	FormatString(lpBuffer, _T("%s OS: %s.\r\n"), tszMultilinePrefix, tszBuffer);

	u64FileTime2 -= u64FtpStartTime;
	u64FileTime2 /= 10000000;

	tszBuffer[0] = 0;
	Time_Duration(tszBuffer, sizeof(tszBuffer), u64FileTime2, _T('m'), _T('d'), 2, 0, 0, _T(", "));
	FormatString(lpBuffer, _T("%sFTP: %s.\r\n"), tszMultilinePrefix, tszBuffer);

	return NULL;
}

	
// if bIgnoreMountPoints is TRUE then do not descend or enumerate mount points
//
// BOOL ModifyProc(PVIRTUALPATH lpVPath, LPFILEINFO lpFileInfo, LPFILEINFO lpParentInfo,
//                 BOOL bIgnoreMountPoints, LPVOID lpContext)
//   -- lVPath.pwd will have the virtual path, and lpVPath.RealPath will have the real path.
VOID RecursiveAction(LPUSERFILE lpUserFile, MOUNTFILE hMountFile, LPTSTR lpPath, BOOL bIgnoreMountPoints,
					 BOOL bIgnoreFiles, DWORD dwMaxDepth,
					 BOOL (* lpModifyProc)(PVIRTUALPATH, LPFILEINFO, LPFILEINFO, LPVOID),
					 LPVOID lpContext)
{
	VIRTUALPATH      vpPath;
	TCHAR            tszRealPath[MAX_PATH+1];
	DWORD            dwVirtLastPos, dwRealLastPos;
	LPFILEINFO       lpFileInfo;
	LPDIRECTORYINFO  lpDirInfo, lpFirstDirInfo;
	LPTSTR           tszResolvedPath;
	LPMOUNT_TABLE	 lpMountTable;
	LPMOUNT_POINT    lpMountPoint;
	MOUNT_DATA       MountData;
	DWORD            i,j;
	size_t           stPWD, stReal;

	if (dwDaemonStatus != DAEMON_ACTIVE) return;

	// NOTE: I cheat a bit and manage a VIRTUALPATH's RealPath field myself by using a local array
	// to avoid all the allocate/free's using PWD_CWD would generate.
	// NOTE: using TCHAR filenames even though virtualpath uses chars since that would be updated
	// if we ever went to wchars.

	tszResolvedPath = 0;
	lpFirstDirInfo = 0;
	lpDirInfo = 0;
	if (dwMaxDepth) dwMaxDepth--;

	stPWD = sizeof(vpPath.pwd)/sizeof(*vpPath.pwd);
	if (_tcsncpy_s(vpPath.pwd, stPWD, lpPath, _TRUNCATE) == STRUNCATE)
	{
		SetLastError(ERROR_BUFFER_OVERFLOW);
		goto PWD_ISSUE;
	}
	vpPath.len = _tcslen(vpPath.pwd);
	stPWD -= vpPath.len;

	dwVirtLastPos = 0;
	for(i=0;i<vpPath.len;i++)
	{
		if (vpPath.pwd[i] == _T('/'))
		{
			dwVirtLastPos = i;
		}
	}
	if (!vpPath.len || (dwVirtLastPos+1 != vpPath.len) || vpPath.pwd[vpPath.len])
	{
		// virtual paths for directories always end in /
		SetLastError(ERROR_INVALID_NAME);
		goto PWD_ISSUE;
	}
	dwVirtLastPos++; // make it point to the NULL now that we know it ends in a /

	// make it point to the static array and not dynamically allocated as it normally is
	vpPath.RealPath = tszRealPath;
	vpPath.l_RealPath = 0;

	ZeroMemory(&MountData, sizeof(MOUNT_DATA));
	while (tszResolvedPath = PWD_Resolve(vpPath.pwd, hMountFile, &MountData, TRUE, MAX_PATH + 1))
	{
		stReal = sizeof(tszRealPath)/sizeof(*tszRealPath);
		if (_tcsncpy_s(vpPath.RealPath, stReal, tszResolvedPath, _TRUNCATE) == STRUNCATE)
		{
			SetLastError(ERROR_BUFFER_OVERFLOW);
			goto REAL_ISSUE;
		}
		vpPath.l_RealPath = _tcslen(tszResolvedPath);
		stReal -= vpPath.l_RealPath;
		if (stReal <= 1)
		{
			SetLastError(ERROR_BUFFER_OVERFLOW);
			goto REAL_ISSUE;
		}
		vpPath.RealPath[vpPath.l_RealPath++] = _T('\\');
		vpPath.RealPath[vpPath.l_RealPath]   = 0;
		stReal--;
		dwRealLastPos = vpPath.l_RealPath;

		if (lpDirInfo = OpenDirectory(tszResolvedPath, TRUE, FALSE, FALSE, NULL, NULL))
		{
			if (!lpFirstDirInfo)
			{
				// the first resolved dir is the one that controls permissions on the directory
				// so record it.
				lpFirstDirInfo = lpDirInfo;
			}
			for(i=0;i<lpDirInfo->dwDirectorySize;i++)
			{
				lpFileInfo = lpDirInfo->lpFileInfo[i];

				// update paths
				if (_tcsncpy_s(&vpPath.pwd[dwVirtLastPos], stPWD, lpFileInfo->tszFileName, _TRUNCATE) == STRUNCATE)
				{
					SetLastError(ERROR_BUFFER_OVERFLOW);
					goto PWD_ISSUE;
				}
				vpPath.len += lpFileInfo->dwFileName;
				// note: not tracking stPWD here since we just going to reset it in a second.

				if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if (stPWD-lpFileInfo->dwFileName <= 1)
					{
						SetLastError(ERROR_BUFFER_OVERFLOW);
						goto PWD_ISSUE;
					}
					vpPath.pwd[vpPath.len++] = _T('/');
					vpPath.pwd[vpPath.len] = 0;
				}

				if (_tcsncpy_s(&vpPath.RealPath[dwRealLastPos], stReal, lpFileInfo->tszFileName, _TRUNCATE ) == STRUNCATE)
				{
					SetLastError(ERROR_BUFFER_OVERFLOW);
					goto REAL_ISSUE;
				}
				vpPath.l_RealPath += lpFileInfo->dwFileName;
				// note: not tracking stReal here since we just going to reset it in a second.

				if ((lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !bIgnoreFiles)
				{
					// call the function on directory or file
					if (lpModifyProc(&vpPath, lpFileInfo, lpFirstDirInfo->lpRootEntry, lpContext))
					{
						// it didn't return an error, so descend if a directory
						if ((lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && dwMaxDepth)
						{
							RecursiveAction(lpUserFile, hMountFile, vpPath.pwd, bIgnoreMountPoints, bIgnoreFiles, dwMaxDepth,
								lpModifyProc, lpContext);
						}
					}
				}

				// reset paths
				vpPath.pwd[dwVirtLastPos] = 0;
				vpPath.len = dwVirtLastPos;

				vpPath.RealPath[dwRealLastPos] = 0;
				vpPath.l_RealPath = dwRealLastPos;
			}
			if (lpFirstDirInfo != lpDirInfo)
			{
				// need to leave the first directory open for permission checking
				CloseDirectory(lpDirInfo);
				lpDirInfo = 0;
			}
		}
		FreeShared(tszResolvedPath);
		// tszResolvedPath will be cleared/set in while
	}

	if (!bIgnoreMountPoints && (lpMountTable = PWD_GetTable(vpPath.pwd, hMountFile)))
	{
		for (i = 0;i < lpMountTable->dwMountPoints;i++)
		{
			lpMountPoint = lpMountTable->lpMountPoints[i];

			// check for no submounts or no name
			if (! lpMountPoint->dwSubMounts && !lpMountPoint->dwName) continue;

			for (j = 0; j < lpMountPoint->dwSubMounts; j++)
			{
				if (GetFileInfo(lpMountPoint->lpSubMount[j].szFileName, &lpFileInfo))
				{
					// don't check access, lpModifyProc should do that
					if (_tcsncpy_s(&vpPath.pwd[dwVirtLastPos], stPWD, lpMountPoint->szName, _TRUNCATE) == STRUNCATE)
					{
						CloseFileInfo(lpFileInfo);
						SetLastError(ERROR_BUFFER_OVERFLOW);
						goto PWD_ISSUE;
					}
					vpPath.len += lpMountPoint->dwName;

					if ((stPWD - lpMountPoint->dwName -1) <= 1)
					{
						CloseFileInfo(lpFileInfo);
						SetLastError(ERROR_BUFFER_OVERFLOW);
						goto PWD_ISSUE;
					}
					vpPath.pwd[vpPath.len++] = _T('/');
					vpPath.pwd[vpPath.len] = 0;

					if (_tcsncpy_s(vpPath.RealPath, sizeof(tszRealPath)/sizeof(*tszRealPath), lpMountPoint->lpSubMount[j].szFileName, _TRUNCATE) == STRUNCATE)
					{
						CloseFileInfo(lpFileInfo);
						SetLastError(ERROR_BUFFER_OVERFLOW);
						goto REAL_ISSUE;
					}
					vpPath.l_RealPath = lpMountPoint->lpSubMount[j].dwFileName;

					// call the function on the virtual mount point
					if (lpModifyProc(&vpPath, lpFileInfo, lpFirstDirInfo->lpRootEntry, lpContext) && dwMaxDepth)
					{
						// virtual mount points are directories, so descend if
						// function returned TRUE...
						RecursiveAction(lpUserFile, hMountFile, vpPath.pwd, bIgnoreMountPoints, bIgnoreFiles, dwMaxDepth,
							lpModifyProc, lpContext);
					}

					// reset virtual path, real one is just copied completely
					vpPath.pwd[dwVirtLastPos] = 0;
					vpPath.len = dwVirtLastPos;
					
					CloseFileInfo(lpFileInfo);
				}
			}
		} // for i loop
	}
	if (lpFirstDirInfo) CloseDirectory(lpFirstDirInfo);
	return;

PWD_ISSUE:
	Putlog(LOG_ERROR, _T("Recursive action error with virtual name: err=%d (Name = '%s')\r\n"), GetLastError(), vpPath.pwd);
	goto COMMON;

REAL_ISSUE:
	Putlog(LOG_ERROR, _T("Recursive action error with disk name: err=%d (Name = '%s')\r\n"), GetLastError(), vpPath.RealPath);

COMMON:
	if (tszResolvedPath) FreeShared(tszResolvedPath);
	if (lpDirInfo && (lpDirInfo != lpFirstDirInfo)) CloseDirectory(lpDirInfo);
	if (lpFirstDirInfo) CloseDirectory(lpFirstDirInfo);
}


// glob but no path except for possible trailing /
static BOOL isValidWildcard(LPTSTR tszName, LPDWORD lpLen)
{
	LPTSTR lpPos        = tszName;
	DWORD  dwLastSlash  = 0;
	DWORD  dwFirstGlob  = 0;
	DWORD  dwLen        = 0;
	BOOL   bWild        = FALSE;

	if (lpLen)      *lpLen = 0;

	for (;*lpPos; lpPos++)
	{
		dwLen++;
		if (*lpPos == _T('/') && *(lpPos+1))
		{
			// there is a slash and it's not at the end
			dwLastSlash = dwLen;
			continue;
		}
		if (!dwFirstGlob && (*lpPos == _T('*') || *lpPos == _T('?')))
		{
			dwFirstGlob = dwLen;
		}
	}

	if (lpLen)	     *lpLen = dwLen;

	if (!dwFirstGlob || dwLastSlash)
	{
		// no glob, or it is a path it's not a valid wildcard for recursive
		return FALSE;
	}

	return TRUE;
}




static BOOL Admin_ChangeFileOwnerProc(PVIRTUALPATH lpVPath, LPFILEINFO lpFileInfo,
									  LPFILEINFO lpParentInfo, LPADMIN_UPDATE lpAdminUpdate)
{
	VFSUPDATE	UpdateData;

	if (lpAdminUpdate->Progress.lpCommand && GetTickCount() > lpAdminUpdate->Progress.dwTicks)
	{
		Progress_Update(&lpAdminUpdate->Progress);
	}

	if (lpAdminUpdate->bDirOnly && !(lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		return TRUE;
	}

	if (lpAdminUpdate->bFileOnly && (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		return TRUE;
	}

	if (lpAdminUpdate->tszWildcard)
	{
		if (spCompare(lpAdminUpdate->tszWildcard, lpFileInfo->tszFileName))
		{
			// no match, so skip but return OK to continue
			return TRUE;
		}
	}

	if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		lpAdminUpdate->Progress.dwArg1++;
	}
	else
	{
		lpAdminUpdate->Progress.dwArg2++;
	}

	UpdateData.Uid	=
		(lpAdminUpdate->UpdateData.Uid == INVALID_USER ? lpFileInfo->Uid : lpAdminUpdate->UpdateData.Uid);
	UpdateData.Gid	=
		(lpAdminUpdate->UpdateData.Gid == INVALID_GROUP ? lpFileInfo->Gid : lpAdminUpdate->UpdateData.Gid);
	if ((UpdateData.Uid == lpFileInfo->Uid) && (UpdateData.Gid == lpFileInfo->Gid))
	{
		// no change made
		return TRUE;
	}

	UpdateData.dwFileMode	    = lpFileInfo->dwFileMode;
	UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
	UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;
	UpdateData.Context.dwData	= lpFileInfo->Context.dwData;
	UpdateData.Context.lpData	= lpFileInfo->Context.lpData;
	//	Call update
	if (UpdateFileInfo(lpVPath->RealPath, &UpdateData))
	{
		lpAdminUpdate->Progress.dwArg3++;
		return TRUE;
	}
	lpAdminUpdate->Progress.dwArg4++;
	return FALSE;
}





LPTSTR Admin_ChangeFileOwner(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszCommand, tszFileName, tszToUpdate;
	LPFILEINFO		lpFileInfo;
	VIRTUALPATH		Path;
	ADMIN_UPDATE	AdminUpdate;
	BOOL			bRecursive, bResult, bWild;
	DWORD			dwError, dwLen, dwDepth;
	LPTSTR			tszIdData;
	TCHAR			*tpCheck;
	TCHAR           tszWildName[_MAX_PATH+1];
	LPBUFFER        lpBuffer;


	dwError	= NO_ERROR;
	//	Recursive
	if (GetStringItems(Args) > 2 && ! _tcscmp(GetStringIndexStatic(Args, 1), _T("-R")))
	{
		bRecursive	= TRUE;
	}
	else
	{
		bRecursive	= FALSE;
	}

	//	Check arguments
	tszCommand = GetStringIndexStatic(Args, 0);
	if (GetStringItems(Args) < (bRecursive ? 4U : 3U) ) 
	{
		ERROR_RETURN(ERROR_MISSING_ARGUMENT, tszCommand);
	}

	tszIdData = GetStringIndex(Args, (bRecursive ? 2 : 1));
	tpCheck	  = _tcschr(tszIdData, _TEXT(':'));
	lpBuffer  = &lpUser->CommandChannel.Out;
	ZeroMemory(&AdminUpdate, sizeof(ADMIN_UPDATE));

	//	Get user and group ids
	if (tpCheck)
	{
		tpCheck[0]	= _TEXT('\0');
		AdminUpdate.UpdateData.Gid	= Group2Gid(&tpCheck[1]);

		if (AdminUpdate.UpdateData.Gid == INVALID_GROUP)
		{
			ERROR_RETURN(ERROR_GROUP_NOT_FOUND, &tpCheck[1]);
		}
	}
	else
	{
		AdminUpdate.UpdateData.Gid	= INVALID_GROUP;
	}
	AdminUpdate.UpdateData.Uid	= User2Uid(tszIdData);

	if (tszIdData != tpCheck && AdminUpdate.UpdateData.Uid == INVALID_USER)
	{
		ERROR_RETURN(ERROR_USER_NOT_FOUND, tszIdData);
	}
	
	//	Resolve target filename
	tszToUpdate = GetStringRange(Args, (bRecursive ? 3 : 2), STR_END);
	if (!tszToUpdate) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszCommand);

	PWD_Zero(&Path);
	PWD_Copy(&lpUser->CommandChannel.Path, &Path, FALSE);

	// decide if this is a filename or a wildcard
	bWild = isValidWildcard(tszToUpdate, &dwLen);
	if (!dwLen || dwLen >= _MAX_PATH)
	{
		ERROR_RETURN(ERROR_PATH_NOT_FOUND, tszToUpdate);
	}

	AdminUpdate.bDirOnly  = FALSE;
	AdminUpdate.bFileOnly = FALSE;
	dwDepth = (bRecursive ? -1 : 1);

	_tcscpy_s(tszWildName, _MAX_PATH+1, tszToUpdate);
	if (bWild)
	{
		if (tszWildName[dwLen-1] == _T('/'))
		{
			// Must have a wildcard to handle a filename of just / properly!
			AdminUpdate.bDirOnly = TRUE;
			tszWildName[dwLen-1] = 0;
		}
		else if (tszWildName[dwLen-1] == _T(':'))
		{
			AdminUpdate.bFileOnly = TRUE;
			tszWildName[dwLen-1] = 0;
		}
		AdminUpdate.tszWildcard = tszWildName;
		tszFileName	= PWD_CWD(lpUser->UserFile, &Path, _T("."), lpUser->hMountFile, EXISTS|TYPE_LINK|VIRTUAL_PWD);
	}
	else
	{
		AdminUpdate.tszWildcard = NULL;
		tszFileName	= PWD_CWD(lpUser->UserFile, &Path, tszToUpdate, lpUser->hMountFile, EXISTS|TYPE_LINK|VIRTUAL_PWD);
	}
	if (!tszFileName)
	{
		dwError = GetLastError();
		PWD_Free(&Path);
		ERROR_RETURN(dwError, tszToUpdate);
	}

	//	Get fileinfo
	if (!tszFileName || !GetFileInfo(tszFileName, &lpFileInfo))
	{
		dwError = GetLastError();
		PWD_Free(&Path);
		ERROR_RETURN(dwError, tszToUpdate);
	}

	//	Update fileinfo
	bResult = FALSE;
	AdminUpdate.lpUserFile = lpUser->UserFile;
	AdminUpdate.Progress.lpCommand = &lpUser->CommandChannel;
	AdminUpdate.Progress.lpClient  = lpUser->Connection.lpClient;
	AdminUpdate.Progress.tszMultilinePrefix = tszMultilinePrefix;
	AdminUpdate.Progress.dwDelay  = 10000;
	AdminUpdate.Progress.dwTicks  = GetTickCount() + AdminUpdate.Progress.dwDelay;
	AdminUpdate.Progress.tszFormatString = _T("Still updating... %u dirs, %u files examined: %u modified, %u errors.\r\n");

	if (AdminUpdate.tszWildcard)
	{
		bResult = TRUE;
		RecursiveAction(lpUser->UserFile, lpUser->hMountFile, Path.pwd, FALSE, AdminUpdate.bDirOnly, dwDepth,
			Admin_ChangeFileOwnerProc, &AdminUpdate);
	}
	else if (Admin_ChangeFileOwnerProc(&Path, lpFileInfo, NULL, &AdminUpdate))
	{
		bResult = TRUE;
		//	Recurse to subdirectories
		if (bRecursive && (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			RecursiveAction(lpUser->UserFile, lpUser->hMountFile, Path.pwd, FALSE, AdminUpdate.bDirOnly, -1,
				Admin_ChangeFileOwnerProc, &AdminUpdate);
		}
	}
	else
	{
		dwError	= GetLastError();
	}

	CloseFileInfo(lpFileInfo);
	MarkVirtualDir(&Path, lpUser->hMountFile);
	PWD_Free(&Path);

	if (bRecursive || AdminUpdate.tszWildcard)
	{
		FormatString(lpBuffer, _T("%s%u dirs examined, %u files examined: %u modifications, %u errors.\r\n"),
			tszMultilinePrefix, AdminUpdate.Progress.dwArg1, AdminUpdate.Progress.dwArg2, AdminUpdate.Progress.dwArg3, AdminUpdate.Progress.dwArg4); 
	}

	if (!bResult) ERROR_RETURN(dwError, tszToUpdate);
	return NULL;
}


static BOOL Admin_ChangeFileModeProc(PVIRTUALPATH lpVPath, LPFILEINFO lpFileInfo,
									 LPFILEINFO lpParentInfo, LPADMIN_UPDATE lpAdminUpdate)
{
	VFSUPDATE vfsUpdate;

	if (lpAdminUpdate->Progress.lpCommand && GetTickCount() > lpAdminUpdate->Progress.dwTicks)
	{
		Progress_Update(&lpAdminUpdate->Progress);
	}

	if (lpAdminUpdate->bDirOnly && !(lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		return TRUE;
	}

	if (lpAdminUpdate->bFileOnly && (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		return TRUE;
	}

	if (lpAdminUpdate->tszWildcard)
	{
		if (spCompare(lpAdminUpdate->tszWildcard, lpFileInfo->tszFileName))
		{
			// no match, so skip but return OK to continue
			return TRUE;
		}
	}

	if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		lpAdminUpdate->Progress.dwArg1++;
	}
	else
	{
		lpAdminUpdate->Progress.dwArg2++;
	}

	if (lpAdminUpdate->dwAddModes)
	{
		vfsUpdate.dwFileMode = lpFileInfo->dwFileMode | lpAdminUpdate->dwAddModes;

	}
	else if (lpAdminUpdate->dwRemoveModes)
	{
		vfsUpdate.dwFileMode = lpFileInfo->dwFileMode & ~lpAdminUpdate->dwRemoveModes;
	}
	else
	{
		vfsUpdate.dwFileMode = lpAdminUpdate->UpdateData.dwFileMode;
	}

	if (vfsUpdate.dwFileMode == (lpFileInfo->dwFileMode & S_ACCESS))
	{
		// no change
		return TRUE;
	}

	//	Check access
	if ((FtpSettings.dwChmodCheck == 2) ||
		((FtpSettings.dwChmodCheck == 1 || Access(lpAdminUpdate->lpUserFile, lpFileInfo, _I_OWN)) &&
		  Access(lpAdminUpdate->lpUserFile, lpParentInfo, _I_WRITE)))
	{
		//	Update filemode
		vfsUpdate.Uid	= lpFileInfo->Uid;
		vfsUpdate.Gid	= lpFileInfo->Gid;
		vfsUpdate.ftAlternateTime  = lpFileInfo->ftAlternateTime;
		vfsUpdate.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;
		vfsUpdate.Context.dwData   = lpFileInfo->Context.dwData;
		vfsUpdate.Context.lpData   = lpFileInfo->Context.lpData;
		if (UpdateFileInfo(lpVPath->RealPath, &vfsUpdate))
		{
			lpAdminUpdate->Progress.dwArg3++;
			return TRUE;
		}
	}
	lpAdminUpdate->Progress.dwArg4++;
	return FALSE;
}




LPTSTR Admin_ChangeFileMode(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPFILEINFO		lpFileInfo, lpParentInfo;
	ADMIN_UPDATE	AdminUpdate;
	VIRTUALPATH		Path;
	LPTSTR			tszCommand, tszFileName, tszFileMode, tszToUpdate;
	TCHAR			*tpCheck;
	BOOL			bResult, bRecursive, bWild;
	DWORD			dwError, dwLen, dwDepth;
	TCHAR           tszWildName[_MAX_PATH+1];
	LPBUFFER        lpBuffer;

	dwError	= NO_ERROR;
	//	Recursive
	if (GetStringItems(Args) > 2 && ! _tcscmp(GetStringIndexStatic(Args, 1), _T("-R")))
	{
		bRecursive	= TRUE;
	}
	else
	{
		bRecursive	= FALSE;
	}

	tszCommand = GetStringIndexStatic(Args, 0);
	if (GetStringItems(Args) < (bRecursive ? 4U : 3U) )
	{
		ERROR_RETURN(ERROR_MISSING_ARGUMENT, tszCommand);
	}

	tszFileMode	= GetStringIndexStatic(Args, (bRecursive ? 2 : 1));
	tszToUpdate	= GetStringRange(Args, (bRecursive ? 3 : 2), STR_END);
	if (!tszToUpdate || !tszFileMode) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszCommand);

	lpBuffer = &lpUser->CommandChannel.Out;

	//	Get FileMode
	ZeroMemory(&AdminUpdate, sizeof(AdminUpdate));
	if (tszFileMode[0] == '+')
	{
		// we want to force some bits
		AdminUpdate.dwAddModes = _tcstoul(&tszFileMode[1], &tpCheck, 8);

		if (tpCheck <= tszFileMode || tpCheck[0] != _TEXT('\0') || AdminUpdate.dwAddModes > 0777L || !AdminUpdate.dwAddModes)
		{
			// you can't ADD nothing...
			ERROR_RETURN(ERROR_INVALID_FILEMODE, tszFileMode);
		}
	}
	else if (tszFileMode[0] == '-')
	{
		AdminUpdate.dwRemoveModes = _tcstoul(&tszFileMode[1], &tpCheck, 8);

		if (tpCheck <= tszFileMode || tpCheck[0] != _TEXT('\0') || AdminUpdate.dwRemoveModes > 0777L || !AdminUpdate.dwRemoveModes)
		{
			// you can't remove nothing...
			ERROR_RETURN(ERROR_INVALID_FILEMODE, tszFileMode);
		}
	}
	else
	{
		AdminUpdate.UpdateData.dwFileMode	= _tcstoul(tszFileMode, &tpCheck, 8);

		if (tpCheck <= tszFileMode || tpCheck[0] != _TEXT('\0') || AdminUpdate.UpdateData.dwFileMode > 0777L)
		{
			ERROR_RETURN(ERROR_INVALID_FILEMODE, tszFileMode);
		}
	}

	//	Prepare to resolve target filename
	PWD_Zero(&Path);
	PWD_Copy(&lpUser->CommandChannel.Path, &Path, FALSE);

	bWild = isValidWildcard(tszToUpdate, &dwLen);
	if (!dwLen || dwLen >= _MAX_PATH)
	{
		ERROR_RETURN(ERROR_PATH_NOT_FOUND, tszToUpdate);
	}

	// decide if this is a filename or a wildcard
	AdminUpdate.bDirOnly  = FALSE;
	AdminUpdate.bFileOnly = FALSE;
	dwDepth = (bRecursive ? -1 : 1);

	_tcscpy_s(tszWildName, sizeof(tszWildName), tszToUpdate);
	if (bWild)
	{
		if (tszWildName[dwLen-1] == _T('/'))
		{
			// Must have a wildcard to handle a filename of just / properly!
			AdminUpdate.bDirOnly = TRUE;
			tszWildName[dwLen-1] = 0;
		}
		else if (tszWildName[dwLen-1] == _T(':'))
		{
			AdminUpdate.bFileOnly = TRUE;
			tszWildName[dwLen-1] = 0;
		}

		AdminUpdate.tszWildcard = tszWildName;
		tszFileName	= PWD_CWD(lpUser->UserFile, &Path, _T("."),	lpUser->hMountFile, EXISTS|TYPE_LINK|VIRTUAL_PWD);
	}
	else
	{
		AdminUpdate.tszWildcard = NULL;
		tszFileName	= PWD_CWD(lpUser->UserFile, &Path, tszToUpdate,	lpUser->hMountFile, EXISTS|TYPE_LINK|VIRTUAL_PWD);
	}
	if (!tszFileName || !GetFileInfo(tszFileName, &lpFileInfo))
	{
		dwError = GetLastError();
		PWD_Free(&Path);
		ERROR_RETURN(dwError, tszToUpdate);
	}

	bResult = FALSE;
	// AdminUpdate.UpdateData.dwFileMode was set above, everything else
	// the same and copied over in change Proc().
	AdminUpdate.lpUserFile = lpUser->UserFile;
	AdminUpdate.Progress.lpCommand = &lpUser->CommandChannel;
	AdminUpdate.Progress.lpClient  = lpUser->Connection.lpClient;
	AdminUpdate.Progress.tszMultilinePrefix = tszMultilinePrefix;
	AdminUpdate.Progress.dwDelay  = 10000;
	AdminUpdate.Progress.dwTicks  = GetTickCount() + AdminUpdate.Progress.dwDelay;
	AdminUpdate.Progress.tszFormatString = _T("Still updating... %u dirs, %u files examined: %u modified, %u errors.\r\n");

	if (AdminUpdate.tszWildcard)
	{
		bResult = TRUE;
		RecursiveAction(lpUser->UserFile, lpUser->hMountFile, Path.pwd, FALSE, AdminUpdate.bDirOnly, dwDepth,
			Admin_ChangeFileModeProc, &AdminUpdate);
	}
	else if (GetVfsParentFileInfo(lpUser->UserFile, lpUser->hMountFile, &Path, &lpParentInfo, TRUE))
	{
		if (Admin_ChangeFileModeProc(&Path, lpFileInfo, lpParentInfo, &AdminUpdate))
		{
			bResult = TRUE;
			if (bRecursive && (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				RecursiveAction(lpUser->UserFile, lpUser->hMountFile, Path.pwd, FALSE, AdminUpdate.bDirOnly, -1,
					Admin_ChangeFileModeProc, &AdminUpdate);
			}
		}
		else
		{
			dwError = GetLastError();
		}
		CloseFileInfo(lpParentInfo);
	}
	else
	{
		dwError = GetLastError();
	}

	CloseFileInfo(lpFileInfo);
	MarkVirtualDir(&Path, lpUser->hMountFile);
	PWD_Free(&Path);

	if (bRecursive || AdminUpdate.tszWildcard)
	{
		FormatString(lpBuffer, _T("%s%u dirs examined, %u files examined: %u modifications, %u errors.\r\n"),
			tszMultilinePrefix, AdminUpdate.Progress.dwArg1, AdminUpdate.Progress.dwArg2, AdminUpdate.Progress.dwArg3, AdminUpdate.Progress.dwArg4); 
	}

	if (!bResult) ERROR_RETURN(dwError, tszToUpdate);
	return NULL;
}





LPTSTR Admin_ChangeFileAttributes(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPFILEINFO		lpFileInfo;
	VFSUPDATE		UpdateData;
	VIRTUALPATH		Path;
	LPBUFFER		lpBuffer;
	LPTSTR			tszFileName, tszFileToUpdate, tszData, tszCommand, tszError;
	DWORD			dwData, dwFileToUpdate, dwError, dwContextType;

	dwError	= NO_ERROR;
	tszError	= NULL;
	dwContextType	= (DWORD)-1;
	ZeroMemory(&UpdateData, sizeof(VFSUPDATE));
	lpBuffer = &lpUser->CommandChannel.Out;

	switch (GetStringItems(Args))
	{
	case 3:
		tszCommand	= GetStringIndexStatic(Args, 1);
		tszFileToUpdate	= GetStringIndex(Args, 2);
		dwFileToUpdate	= GetStringIndexLength(Args, 2);
		//	Remove quotes
		if (tszFileToUpdate[0] == _TEXT('\"') &&
			tszFileToUpdate[dwFileToUpdate - 1] == _TEXT('\"'))
		{
			(tszFileToUpdate++)[dwFileToUpdate - 1]	= _TEXT('\0');
		}
		//	Resolve virtual path
		PWD_Zero(&Path);
		PWD_Copy(&lpUser->CommandChannel.Path, &Path, FALSE);
		tszFileName	= PWD_CWD(lpUser->UserFile, &Path, tszFileToUpdate, lpUser->hMountFile, TYPE_LINK|EXISTS|VIRTUAL_PWD);

		if (tszFileName)
		{
			//	Get fileinfo
			if (GetFileInfo(tszFileName, &lpFileInfo))
			{
				//	Check context type
				if (tszCommand[1] == _TEXT('h')) dwContextType	= PRIVATE;
				if (tszCommand[1] == _TEXT('l')) dwContextType	= SYMBOLICLINK;
				if (dwContextType == (DWORD)-1)
				{
					tszError	= tszCommand;
					CloseFileInfo(lpFileInfo);
					dwError	= ERROR_INVALID_ARGUMENTS;
					break;
				}
				
				switch (tszCommand[0])
				{
				case _TEXT('-'):
					//	Remove selected context
					if (CreateFileContext(&UpdateData.Context, &lpFileInfo->Context))
					{
						DeleteFileContext(&UpdateData.Context, (BYTE)dwContextType);
						//	Update fileinfo
						UpdateData.Uid	= lpFileInfo->Uid;
						UpdateData.Gid	= lpFileInfo->Gid;
						UpdateData.dwFileMode	= lpFileInfo->dwFileMode;
						UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
						UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;

						if (! UpdateFileInfo(tszFileName, &UpdateData))
						{
							dwError	= GetLastError();
							tszError	= tszFileToUpdate;
						}
						FreeFileContext(&UpdateData.Context);
					}
					else
					{
						dwError	= GetLastError();
						tszError	= GetStringIndexStatic(Args, 0);
					}
					break;
				case _TEXT('+'):
					//	Show selected context
					tszData	= FindFileContext((BYTE)dwContextType, &lpFileInfo->Context);
					if (! tszData) tszData	= _TEXT("<value not set>");

					FormatString(lpBuffer, _TEXT("%sCHATTR: %s\r\n"), tszMultilinePrefix, tszData);
					break;
				default:
					dwError	= ERROR_INVALID_ARGUMENTS;
					tszError	= tszCommand;
				}
				CloseFileInfo(lpFileInfo);
			}
			PWD_Free(&Path);
		}
		else
		{
			dwError	= GetLastError();
			tszError	= tszFileToUpdate;
		}
		break;

	case 4:
		tszCommand	= GetStringIndexStatic(Args, 1);
		tszFileToUpdate	= GetStringIndexStatic(Args, 2);
		dwFileToUpdate	= GetStringIndexLength(Args, 2);
		tszData	= GetStringIndexStatic(Args, 3);
		dwData	= GetStringIndexLength(Args, 3);

		//	Remove quotes
		if (tszFileToUpdate[0] == _TEXT('\"') &&
			tszFileToUpdate[dwFileToUpdate - 1] == _TEXT('\"'))
		{
			(tszFileToUpdate++)[dwFileToUpdate - 1]	= _TEXT('\0');
		}
		if (tszData[0] == _TEXT('\"') &&
			tszData[dwData - 1] == _TEXT('\"'))
		{
			(tszData++)[dwData - 1]	= _TEXT('\0');
			dwData	-= 2;
		}
		//	Resolve virtual path
		PWD_Zero(&Path);
		PWD_Copy(&lpUser->CommandChannel.Path, &Path, FALSE);
		tszFileName	= PWD_CWD(lpUser->UserFile, &Path, tszFileToUpdate, lpUser->hMountFile, TYPE_LINK|EXISTS|VIRTUAL_PWD);

		if (tszFileName)
		{
			//	Get fileinfo
			if (GetFileInfo(tszFileName, &lpFileInfo))
			{
				//	Check context type
				if (tszCommand[1] == _TEXT('h')) dwContextType	= PRIVATE;
				if (tszCommand[1] == _TEXT('l')) dwContextType	= SYMBOLICLINK;
				if (dwContextType == (DWORD)-1)
				{
					CloseFileInfo(lpFileInfo);
					dwError	= ERROR_INVALID_ARGUMENTS;
					tszError	= tszCommand;
					break;
				}
			
				if (tszCommand[0] == _TEXT('+'))
				{
					UpdateData.Uid	= lpFileInfo->Uid;
					UpdateData.Gid	= lpFileInfo->Gid;
					UpdateData.dwFileMode	= lpFileInfo->dwFileMode;
					UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
					UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;

					//	Duplicate context
					if (CreateFileContext(&UpdateData.Context, &lpFileInfo->Context))
					{
						//	Update fileinfo
						if (InsertFileContext(&UpdateData.Context, (BYTE)dwContextType, tszData, dwData * sizeof(TCHAR)))
						{
							if (! UpdateFileInfo(tszFileName, &UpdateData))
							{
								dwError	= GetLastError();
								tszError	= GetStringIndexStatic(Args, 2);
							}
						}
						else
						{
							dwError	= GetLastError();
							tszError	= GetStringIndexStatic(Args, 0);
						}
						FreeFileContext(&UpdateData.Context);
					}
					else
					{
						dwError	= GetLastError();
						tszError	= GetStringIndexStatic(Args, 0);
					}
				}
				else
				{
					dwError	= ERROR_INVALID_ARGUMENTS;
					tszError	= tszCommand;
				}
				CloseFileInfo(lpFileInfo);
			}
			else
			{
				tszError	= GetStringIndexStatic(Args, 2);
				dwError	= GetLastError();
			}
			PWD_Free(&Path);
		}
		else
		{
			tszError	= GetStringIndexStatic(Args, 2);
			dwError	= GetLastError();
		}
		break;
	case 1:
	case 2:
		dwError	= ERROR_MISSING_ARGUMENT;
		tszError	= GetStringIndexStatic(Args, 0);
		break;
	default:
		dwError	= ERROR_INVALID_ARGUMENTS;
		tszError	= GetStringRange(Args, 4, STR_END);
	}


	if (dwError != NO_ERROR) ERROR_RETURN(dwError, tszError);
	return NULL;
}




LPTSTR Admin_Bans(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPBANINFO		lpBanInfo, lpBanList;
	IN_ADDR			InetAddress;
	LPBUFFER		lpBuffer;
	LPTSTR			tszNetworkAddress, tszCommand;

	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));
	if (GetStringItems(Args) > 3) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 3, STR_END));

	lpBuffer = &lpUser->CommandChannel.Out;
	//	Get arguments
	tszCommand	= GetStringIndexStatic(Args, 1);

	if (! _tcsicmp(tszCommand, _TEXT("remove")) && GetStringItems(Args) == 3)
	{
		if (GetStringItems(Args) != 3)
		{
			ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));
		}
		tszNetworkAddress	= GetStringIndexStatic(Args, 2);

		//	Lift ban
		UnbanNetworkAddress(tszNetworkAddress, lpBuffer, tszMultilinePrefix);
	}
	else if (! _tcsicmp(tszCommand, _TEXT("list")))
	{
		//	List network bans
		lpBanList	= GetNetworkBans();
		while (lpBanInfo = lpBanList)
		{
			lpBanList	= lpBanList->lpNext;
			switch (lpBanInfo->dwNetworkAddress)
			{
			case 4:
				InetAddress.s_addr	= ((PULONG)lpBanInfo->pNetworkAddress)[0];
				tszNetworkAddress	= inet_ntoa(InetAddress);
				if (tszNetworkAddress) break;
			default:
				tszNetworkAddress	= _TEXT("Unknown address");
			}
			FormatString(lpBuffer, _TEXT("%sBanned IP: %s for %u seconds (%u attempt(s))\r\n"),
				tszMultilinePrefix, tszNetworkAddress, lpBanInfo->dwBanDuration / 1000, lpBanInfo->dwConnectionAttempts);
			Free(lpBanInfo);
		}
	}
	else if (! _tcsicmp(tszCommand, _T("immune")))
	{
		ImmuneMaskList(lpBuffer, tszMultilinePrefix);
	}
	else ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszCommand);
	return NULL;
}



VOID Display_UserInfo(LPFTPUSER lpUser, LPUSERFILE lpUserFile, LPTSTR tszBasePath, LPTSTR tszBaseName, LPBUFFER lpBuffer, BOOL bShowStats, LPTSTR tszMultilinePrefix)
{
	LPSECTION_INFO  lpSection;
	LPTSTR			tszFileName, tszPath;
	DWORD           dwLen, dwSectionCount, n, m, dwError;
	TCHAR		    pBuffer[_MAX_NAME+1];
	USERFILE        UserFileTotals;
	LPBYTE          pMsgBuf;
	INT             i;
	BOOL            bPrinted, bIsVfsAdmin;
	VIRTUALPATH     vpOriginal, vpPath, *pvpPath;
	TCHAR           PathBuffer[_MAX_PWD + 1];
	MOUNT_DATA      MountData;
	USERFILE_PLUS   UserFile_Plus;


	//	Show userinfo file
	if (!aswprintf(&tszFileName, _TEXT("%s\\%s.Header "), tszBasePath, tszBaseName))
	{
		return;
	}
	// allocated with an extra space to handle the longer name below, so trim it off now
	dwLen = _tcslen(tszFileName);
	tszFileName[dwLen-1] = 0;

	UserFile_Plus.lpUserFile = lpUserFile;
	UserFile_Plus.lpFtpUserCaller = lpUser;
	UserFile_Plus.lpCommandChannel = &lpUser->CommandChannel;
	UserFile_Plus.iGID = 0;

	MessageFile_Show(tszFileName, lpBuffer, &UserFile_Plus, DT_USERFILE_PLUS, tszMultilinePrefix, NULL);

	ZeroMemory(pBuffer, sizeof(pBuffer));
	CopyMemory(&UserFileTotals, lpUserFile, sizeof(UserFileTotals));
	// these are actually contiguous, but just going to play it safe...
	ZeroMemory(&UserFileTotals.AllUp, sizeof(UserFileTotals.AllUp));
	ZeroMemory(&UserFileTotals.AllDn, sizeof(UserFileTotals.AllDn));
	ZeroMemory(&UserFileTotals.MonthUp, sizeof(UserFileTotals.MonthUp));
	ZeroMemory(&UserFileTotals.MonthDn, sizeof(UserFileTotals.MonthDn));
	ZeroMemory(&UserFileTotals.WkUp, sizeof(UserFileTotals.WkUp));
	ZeroMemory(&UserFileTotals.WkDn, sizeof(UserFileTotals.WkDn));
	ZeroMemory(&UserFileTotals.DayUp, sizeof(UserFileTotals.DayUp));
	ZeroMemory(&UserFileTotals.DayDn, sizeof(UserFileTotals.DayDn));
	UserFileTotals.Credits[0] = 0;

	_tcscpy(&tszFileName[dwLen-7], "Section");
	pMsgBuf = Message_Load(tszFileName);

	dwSectionCount = 0;
	Config_Lock(&IniConfigFile, FALSE);

	bIsVfsAdmin = !HasFlag(lpUser->UserFile, _T("MV"));

	// now fake out the Command channel path stuff for these calls
	pvpPath = &lpUser->CommandChannel.Path;
	CopyMemory(&vpOriginal, pvpPath, sizeof(vpOriginal));

	for (i=0 ; i < MAX_SECTIONS ; i++)
	{
		bPrinted = FALSE;

		for(n=0 ; !bPrinted && (n < dwSectionInfoArray) ; n++)
		{
			lpSection = lpSectionInfoArray[n];
			if (lpSection->iStat != i)
			{
				continue;
			}

			if (!bIsVfsAdmin)
			{
				// path matches, but double check full paths are visible to the user requesting info
				// so we don't give out hidden paths.
				_tcscpy(PathBuffer, lpSection->tszPath);
				tszPath = &PathBuffer[1];
				while (*tszPath)
				{
					if ((*tszPath == _T('*')) || (*tszPath == _T('?')) || (*tszPath == _T('[')) || (*tszPath == _T(']')))
					{
						// found a wildcard, now back to last /
						while ((tszPath != PathBuffer) && (*tszPath != _T('/'))) tszPath--;
						tszPath[1] = 0;
						break;
					}
					tszPath++;
				}

				ZeroMemory(&MountData, sizeof(MountData));
				PWD_Reset(&vpPath);
				PWD_Set(&vpPath, "/");
				if (!PWD_CWD2(lpUser->UserFile, &vpPath, PathBuffer, lpUser->hMountFile, &MountData, TYPE_LINK, NULL, NULL, NULL))
				{
					dwError = GetLastError();
					// TODO: test for virtual dirs here?  if (MountData.Initialised && MountData.lpVirtualDirEvent)?
					// no access, or wasn't found
					continue;
				}
				PWD_Free(&vpPath);
			}

			bPrinted = TRUE;
			dwSectionCount++;
			if (bShowStats && pMsgBuf)
			{
				_tcscpy(pvpPath->SectionName, lpSection->tszSectionName);
				pvpPath->CreditSection = lpSection->iCredit;
				pvpPath->StatsSection  = lpSection->iStat;
				pvpPath->ShareSection  = lpSection->iShare;
				Message_Compile(pMsgBuf, lpBuffer, FALSE, &UserFile_Plus, DT_USERFILE_PLUS, tszMultilinePrefix, NULL);
			}
			for ( m=0; m < 3 ; m ++)
			{
				UserFileTotals.AllUp[m]   += lpUserFile->AllUp[n*3+m];
				UserFileTotals.AllDn[m]   += lpUserFile->AllDn[n*3+m];
				UserFileTotals.MonthUp[m] += lpUserFile->MonthUp[n*3+m];
				UserFileTotals.MonthDn[m] += lpUserFile->MonthDn[n*3+m];
				UserFileTotals.WkUp[m]    += lpUserFile->WkUp[n*3+m];;
				UserFileTotals.WkDn[m]    += lpUserFile->WkDn[n*3+m];
				UserFileTotals.DayUp[m]   += lpUserFile->DayUp[n*3+m];
				UserFileTotals.DayDn[m]   += lpUserFile->DayDn[n*3+m];
			}
			UserFileTotals.Credits[n] += lpUserFile->Credits[n];
		}
	}
	Config_Unlock(&IniConfigFile, FALSE);

	if (pMsgBuf) Free(pMsgBuf);
	UserFile_Plus.lpUserFile = &UserFileTotals;
	_tcscpy(pvpPath->SectionName, "*TOTALS*");

	if (!bShowStats || dwSectionCount > 1)
	{
		pvpPath->ShareSection = dwSectionCount;
		if (!bShowStats)
		{
			pvpPath->CreditSection = pvpPath->StatsSection = 0;
		}
		else
		{
			pvpPath->CreditSection = pvpPath->StatsSection = -1;
		}
		strcpy(&tszFileName[dwLen-7], "Totals");
		MessageFile_Show(tszFileName, lpBuffer, &UserFile_Plus, DT_USERFILE_PLUS, tszMultilinePrefix, NULL);
	}

	// restore command channel path
	CopyMemory(pvpPath, &vpOriginal, sizeof(vpOriginal));

	_tcscpy(&tszFileName[dwLen-7], "Footer");
	MessageFile_Show(tszFileName, lpBuffer, &UserFile_Plus, DT_USERFILE_PLUS, tszMultilinePrefix, NULL);

	Free(tszFileName);
}






LPTSTR Admin_UserInfo(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPUSERFILE		lpUserFile;
	LPBUFFER		lpBuffer;
	LPTSTR			tszUserName, tszBasePath;
	DWORD           dwError;
	BOOL            bDefault, bShowStats;
	USERFILE        UserFile;
	INT32           Gid;


	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));
	if (GetStringItems(Args) > 3) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 3, STR_END));

	//	Get arguments
	tszUserName	= GetStringIndexStatic(Args, 1);
	lpBuffer	= &lpUser->CommandChannel.Out;

	bShowStats  = TRUE;

	if ((GetStringItems(Args) == 3) && !_tcsicmp(GetStringIndexStatic(Args,2), "totals"))
	{
		bShowStats = FALSE;
	}

	if (!_tcsnicmp(tszUserName, _T("/Default"), 8))
	{
		bDefault = TRUE;
		if (!_tcsicmp(tszUserName, _T("/Default.User")))
		{
			Gid = -1;
		}
		else
		{
			if (tszUserName[8] != _T('='))
			{
				SetLastError(ERROR_INVALID_ARGUMENTS);
				return tszUserName;
			}
			if ((Gid = Group2Gid(&tszUserName[9])) == -1)
			{
				SetLastError(ERROR_GROUP_NOT_FOUND);
				return tszUserName;
			}
		}

		if (User_Default_Open(&UserFile, Gid))
		{
			return tszUserName;
		}
		if (UserFile.lpInternal == NULL)
		{
			FormatString(lpBuffer, _T("%sThe group '%s' is not customized yet.  See '/Default.User' instead.\r\n"),
				tszMultilinePrefix, &tszUserName[9]);
			User_Default_Close(&UserFile);
			return NULL;
		}
		lpUserFile = &UserFile;
	}
	else
	{
		bDefault = FALSE;
		if (UserFile_Open(tszUserName, &lpUserFile, 0))
		{
			return tszUserName;
		}
	}

	dwError = NO_ERROR;
	if (User_IsAdmin(lpUser->UserFile, lpUserFile, NULL))
	{
		dwError = GetLastError();
	}
	else
	{
		tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
		if (tszBasePath)
		{
			//	Show UserInfo.* files
			Display_UserInfo(lpUser, lpUserFile, tszBasePath, _T("UserInfo"), lpBuffer, bShowStats, tszMultilinePrefix);
			FreeShared(tszBasePath);
		}
	}

	if (bDefault)
	{
		User_Default_Close(&UserFile);
	}
	else
	{
		UserFile_Close(&lpUserFile, 0);
	}
	
	if (!dwError)
	{
		return NULL;
	}
	return tszUserName;
}



LPTSTR Admin_MyInfo(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPBUFFER		lpBuffer;
	LPTSTR			tszUserName, tszBasePath;

	if (GetStringItems(Args) > 1) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 1, STR_END));

	//	Get arguments
	tszUserName = Uid2User(lpUser->UserFile->Uid);
	if (!tszUserName)
	{
		ERROR_RETURN(ERROR_USER_NOT_FOUND, GetStringIndexStatic(Args, 0));
	}

	lpBuffer = &lpUser->CommandChannel.Out;

	tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
	if (tszBasePath)
	{
		//	Show MyInfo.* files which by default are the same as UserInfo.*
		Display_UserInfo(lpUser, lpUser->UserFile, tszBasePath, _T("MyInfo"), lpBuffer, TRUE, tszMultilinePrefix);
		FreeShared(tszBasePath);
	}

	return NULL;
}




LPTSTR Admin_GroupInfo(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	TCHAR			tszSearch[_MAX_NAME + 2];
	PBYTE			pBuffer;
	DWORD			dwFileName;
	LPGROUPFILE		lpGroupFile;
	LPUSERFILE		lpUserFile;
	USERFILE_PLUS   UserFile_Plus;
	LPUSERSEARCH	hFind;
	LPBUFFER		lpBuffer;
	LPTSTR			tszFileName, tszBasePath;
	LPTSTR			tszGroupName;
	DWORD           n;
	INT32           Gid;
	GROUPFILE       GroupFile;
	BOOL            bDefault;

	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));
	if (GetStringItems(Args) > 2) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 2, STR_END));

	//	Get arguments
	tszGroupName = GetStringIndexStatic(Args, 1);
	lpBuffer     = &lpUser->CommandChannel.Out;

	if (!_tcsicmp(tszGroupName, _T("/Default.Group")))
	{
		bDefault = TRUE;

		if (HasFlag(lpUser->UserFile, _T("M")))
		{
			return tszGroupName;
		}

		if (Group_Default_Open(&GroupFile))
		{
			return tszGroupName;
		}
		lpGroupFile = &GroupFile;
	}
	else
	{
		bDefault = FALSE;
		Gid = Group2Gid(tszGroupName);
		if (Gid == -1)
		{
			ERROR_RETURN(ERROR_NO_SUCH_GROUP, tszGroupName);
		}

		if (GroupFile_Open(tszGroupName, &lpGroupFile, 0))
		{
			return tszGroupName;
		}

		if (HasFlag(lpUser->UserFile, _T("M1")))
		{
			//	Check admin group
			for (n = 0;n < MAX_GROUPS && lpUser->UserFile->AdminGroups[n] != -1;n++)
			{
				if (lpUser->UserFile->AdminGroups[n] == Gid) break;
			}
			if (n == MAX_GROUPS || lpUser->UserFile->AdminGroups[n] != Gid)
			{
				GroupFile_Close(&lpGroupFile, 0);
				ERROR_RETURN(IO_GADMIN_EMPTY, tszGroupName);
			}
		}
	}

	//	Show groupinfo files
	tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
	if (tszBasePath)
	{
		dwFileName	= aswprintf(&tszFileName, _TEXT("%s\\GroupInfo.Header"), tszBasePath);
		if (dwFileName)
		{
			//	Show header
			MessageFile_Show(tszFileName, lpBuffer, lpGroupFile, DT_GROUPFILE, tszMultilinePrefix, NULL);

			//	Show body
			_tcscpy(&tszFileName[dwFileName - 6], _TEXT("Body"));
			pBuffer	= Message_Load(tszFileName);
			if (pBuffer)
			{
				if (!bDefault)
				{
					_stprintf(tszSearch, _TEXT("=%.*s"), _MAX_NAME, tszGroupName);
					hFind	= FindFirstUser(tszSearch, &lpUserFile, NULL, NULL, NULL);
					if (hFind)
					{
						UserFile_Plus.lpFtpUserCaller = lpUser;
						UserFile_Plus.lpCommandChannel = &lpUser->CommandChannel;
						UserFile_Plus.iGID = Gid;
						do
						{
							UserFile_Plus.lpUserFile = lpUserFile;
							Message_Compile(pBuffer, lpBuffer, FALSE, &UserFile_Plus, DT_USERFILE_PLUS, tszMultilinePrefix, NULL);
							UserFile_Close(&lpUserFile, 0);
						} while (! FindNextUser(hFind, &lpUserFile));
					}
				}
				Free(pBuffer);
			}
			//	Show footer
			_tcscpy(&tszFileName[dwFileName - 6], _TEXT("Footer"));
			MessageFile_Show(tszFileName, lpBuffer, lpGroupFile, DT_GROUPFILE, tszMultilinePrefix, NULL);

			Free(tszFileName);
		}				
		FreeShared(tszBasePath);
	}

	if (bDefault)
	{
		Group_Default_Close(&GroupFile);
	}
	else
	{
		GroupFile_Close(&lpGroupFile, 0);
	}
	return NULL;
}


BOOL Admin_SizeAdd(PVIRTUALPATH lpVPath, LPFILEINFO lpFileInfo, LPFILEINFO lpParentInfo,
				   LPADMIN_SIZE lpAdminSize)
{
	if (lpAdminSize->Progress.lpCommand && GetTickCount() > lpAdminSize->Progress.dwTicks)
	{
		lpAdminSize->Progress.dwArg1 = lpAdminSize->dwDirCount + lpAdminSize->dwNoAccess;
		lpAdminSize->Progress.dwArg2 = lpAdminSize->dwFileCount;
		lpAdminSize->Progress.dwArg3 = lpAdminSize->dwNoAccess;
		Progress_Update(&lpAdminSize->Progress);
	}

	if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		if (!lpAdminSize->lpUserFile || Access(lpAdminSize->lpUserFile, lpFileInfo, _I_READ))
		{
			lpAdminSize->dwDirCount++;
			return TRUE;
		}
		lpAdminSize->dwNoAccess++;
		return FALSE;
	}

	lpAdminSize->dwFileCount++;
	lpAdminSize->u64Size += lpFileInfo->FileSize;
	return TRUE;
}


VOID PrettyPrintSize(LPTSTR lpBuf, DWORD dwBufLen, UINT64 u64Size)
{
	UINT64 temp;
	LPTSTR tszSize;
	DWORD dwNum, dwFraction;

	if (!(u64Size >> 10))
	{
		// < 1 KB
		temp = 1;
		tszSize = _T("B");
	}
	else if (!(u64Size >> 20))
	{
		// < 1 MB
		temp = 1 << 10;
		tszSize = _T("KB");
	}
	else if (!(u64Size >> 30))
	{
		// << 1 GB
		temp = 1 << 20;
		tszSize = _T("MB");
	}
	else if (!(u64Size >> 40))
	{
		// << 1 TB
		temp = 1 << 30;
		tszSize = _T("GB");
	}
	else
	{
		// > 1 TB
		temp = (UINT64) 1 << 40;
		tszSize = _T("TB");
	}

	// it's small enough to fit into a long now...
	dwNum = (DWORD) (u64Size*100/temp);
	dwFraction = dwNum % 100;
	dwNum = dwNum / 100;

	if (!dwFraction || dwNum > 100)
	{
		sprintf_s(lpBuf, dwBufLen, "%u %s", dwNum, tszSize);
		return;
	}
	if (dwNum > 10)
	{
		sprintf_s(lpBuf, dwBufLen, "%u.%u %s", dwNum, dwFraction/10, tszSize);
		return;
	}
	sprintf_s(lpBuf, dwBufLen, "%u.%02u %s", dwNum, dwFraction, tszSize);
	return;
}



LPTSTR Admin_DirSize(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR          tszDirName, tszPath, tszCommand;
	LPBUFFER        lpBuffer;
	VIRTUALPATH     Path;
	LPFILEINFO      lpFileInfo;
	BOOL            bResult = FALSE;
	ADMIN_SIZE      AdminSize;
	TCHAR           tszTemp[12];

	tszCommand  = GetStringIndexStatic(Args, 0);
	if (GetStringItems(Args) < 2) 
	{
		tszDirName = _T(".");
	}
	else
	{
		tszDirName	= GetStringRange(Args, 1, STR_END);
	}

	lpBuffer = &lpUser->CommandChannel.Out;

	PWD_Zero(&Path);
	PWD_Copy(&lpUser->CommandChannel.Path, &Path, FALSE);

	if (!(tszPath = PWD_CWD(lpUser->UserFile, &Path, tszDirName, lpUser->hMountFile, EXISTS|VIRTUAL_PWD)))
	{
		return tszCommand;
	}

	//  Get file attrbiutes
	if (!GetFileInfo(tszPath, &lpFileInfo))
	{
		PWD_Free(&Path);
		return tszCommand;
	}

	if (!(lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		CloseFileInfo(lpFileInfo);
		PWD_Free(&Path);
		ERROR_RETURN(ERROR_DIRECTORY, tszDirName);
	}

	ZeroMemory(&AdminSize, sizeof(AdminSize));
	AdminSize.lpUserFile = lpUser->UserFile;
	AdminSize.Progress.lpCommand = &lpUser->CommandChannel;
	AdminSize.Progress.lpClient  = lpUser->Connection.lpClient;
	AdminSize.Progress.tszMultilinePrefix = tszMultilinePrefix;
	AdminSize.Progress.dwDelay  = 10000;
	AdminSize.Progress.dwTicks  = GetTickCount() + AdminSize.Progress.dwDelay;
	AdminSize.Progress.tszFormatString = _T("Still sizing... %u dirs, %u files processed, %u access errors.\r\n");

	// do recursive summation
	RecursiveAction(lpUser->UserFile, lpUser->hMountFile, Path.pwd, FALSE, FALSE, -1,
		Admin_SizeAdd, &AdminSize);

	FormatString(lpBuffer, _T("%sReport for \"%s\":\r\n"), tszMultilinePrefix, Path.pwd);
	FormatString(lpBuffer, _T("%sSubDirectories: %u\r\n"), tszMultilinePrefix, AdminSize.dwDirCount);
	FormatString(lpBuffer, _T("%sFiles: %u\r\n"), tszMultilinePrefix, AdminSize.dwFileCount);
	PrettyPrintSize(tszTemp, sizeof(tszTemp)/sizeof(TCHAR), AdminSize.u64Size);
	FormatString(lpBuffer, _T("%sTotal Size: %s\r\n"), tszMultilinePrefix, tszTemp);
	FormatString(lpBuffer, _T("%sNoAccess: %u\r\n"), tszMultilinePrefix, AdminSize.dwNoAccess);

	CloseFileInfo(lpFileInfo);
	PWD_Free(&Path);
	return NULL;
}


LPTSTR Admin_FreeSpace(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR          tszDirName, tszCommand;
	LPBUFFER        lpBuffer;
	VIRTUALPATH     Path;
	BOOL            bResult = FALSE;
	TCHAR           tszTemp[12];
	UINT64          u64Free, u64Size;

	tszCommand  = GetStringIndexStatic(Args, 0);
	lpBuffer    = &lpUser->CommandChannel.Out;

	PWD_Zero(&Path);
	// copy the real path as well
	PWD_Copy(&lpUser->CommandChannel.Path, &Path, TRUE);

	tszDirName = NULL;
	if (GetStringItems(Args) > 1)
	{
		tszDirName	= GetStringRange(Args, 1, STR_END);
		// return directory specifier instead of function name now
		tszCommand = tszDirName;

		PWD_Free(&Path);
		if (!PWD_CWD(lpUser->UserFile, &Path, tszDirName, lpUser->hMountFile, EXISTS|VIRTUAL_PWD))
		{
			return tszCommand;
		}
	}

	if (!GetDiskFreeSpaceEx(Path.RealPath, (PULARGE_INTEGER) &u64Free, (PULARGE_INTEGER) &u64Size, NULL))
	{
		PWD_Free(&Path);
		return tszCommand;
	}
	PrettyPrintSize(tszTemp, sizeof(tszTemp)/sizeof(TCHAR), u64Free);
	FormatString(lpBuffer, _TEXT("%sFree space under this directory: %s\r\n"), tszMultilinePrefix, tszTemp);

	PWD_Free(&Path);
	return NULL;
}

LPTSTR Admin_Stat(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR          tszCommand, tszBasePath, tszFileName;
	LPBUFFER        lpBuffer;
	BOOL            bShowDefault = FALSE;

	tszCommand  = GetStringIndexStatic(Args, 0);
	lpBuffer    = &lpUser->CommandChannel.Out;

	// Show status message
	MaybeDisplayStatus(lpUser, _T("200-"));

	//	Show completion message
	bShowDefault	= TRUE;
	tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
	if (tszBasePath)
	{
		if (aswprintf(&tszFileName, _TEXT("%s\\TransferComplete"), tszBasePath))
		{
			if (! MessageFile_Show(tszFileName,	lpBuffer, lpUser, DT_FTPUSER, _TEXT("200-"), _TEXT("200 ")))
			{
				bShowDefault	= FALSE;
			}
			Free(tszFileName);
		}
		FreeShared(tszBasePath);
	}
	if (bShowDefault)
	{
		FormatString(&lpUser->CommandChannel.Out,
			_TEXT("%s"), _TEXT("200 Stat complete.\r\n"));
	}
	return NULL;
}


// tszPathBuf must be at least _MAX_PWD in size
static BOOL ParseVfsPerms(LPCONFIG_LINE lpLine, LPTSTR tszPathBuf, LPTSTR *ptszPerms)
{
	LPTSTR tszPath, tszAccessList;
	DWORD dwPath;

	tszPath	= lpLine->Value;
	if (tszPath[0] == _TEXT('"'))
	{
		//	Find second quote
		tszAccessList	= (LPTSTR)_tmemchr(++tszPath, _TEXT('"'), lpLine->Value_l - 1);
		dwPath	= &tszAccessList[-1] - tszPath;
	}
	else
	{
		//	Find first space from string
		if (! (tszAccessList = (LPSTR)_tmemchr(&tszPath[1], _TEXT(' '), lpLine->Value_l - 1)))
		{
			//	Find first '\t'
			tszAccessList	= (LPSTR)_tmemchr(&tszPath[1], _TEXT('\t'), lpLine->Value_l - 1);
		}
		dwPath	= tszAccessList - tszPath;
	}

	if (!tszAccessList++)
	{
		SetLastError(ERROR_INVALID_ARGUMENTS);
		return TRUE;
	}

	//	Limit path length
	if (dwPath > _MAX_PWD) dwPath	= _MAX_PWD;
	//	Copy path to local variable
	CopyMemory(tszPathBuf, tszPath, dwPath * sizeof(TCHAR));
	tszPathBuf[dwPath]	= 0;

	*ptszPerms = tszAccessList;
	return FALSE;
}


static LPTSTR GetFilePermMatch(LPUSERFILE lpUserFile, LPFILEINFO lpFileInfo, DWORD dwPerms)
{
	INT32 Gid;
	DWORD n;

	// duplicate part of the logic used in Access() function
	if (lpFileInfo->Uid == lpUserFile->Uid)
	{
		if (((dwPerms & _I_READ)  && (lpFileInfo->dwFileMode & S_IRUSR)) ||
			((dwPerms & _I_WRITE) && (lpFileInfo->dwFileMode & S_IWUSR)))
		{
			return _T("u");
		}
	}
	else if (!(dwPerms & _I_OWN))
	{
		//  User is not owner of file
		for (n = 0 ; n < MAX_GROUPS && ((Gid = lpUserFile->Groups[n]) != -1) ; n++)
		{
			if (lpFileInfo->Gid == Gid) break;
		}
		if (n < MAX_GROUPS && lpFileInfo->Gid == Gid)
		{
			// user a member of the group of the file
			if (((dwPerms & _I_READ)  && (lpFileInfo->dwFileMode & S_IRGRP)) ||
				((dwPerms & _I_WRITE) && (lpFileInfo->dwFileMode & S_IWGRP)))
			{
				return _T("g");
			}
		}
		else
		{
			if (((dwPerms & _I_READ)  && (lpFileInfo->dwFileMode & S_IROTH)) ||
				((dwPerms & _I_WRITE) && (lpFileInfo->dwFileMode & S_IWOTH)))
			{
				return _T("o");
			}
		}
	}

	if (!HasFlag(lpUserFile, _T("M")))
	{
		return _T("M");
	}
	if (dwPerms & _I_READ)
	{
		if (!HasFlag(lpUserFile, _T("V")))
		{
			return _T("V");
		}
	}

	return NULL;
}


// +r for the entire parent path is required for everything since resolver mandates that!
static ACTION_TEST ActionTestList[] = {
	// activity,      owner activity,      file,  dir,   symlink,  nomount, parentperms,  item perms,
	_T("Download"),   NULL,                TRUE,  FALSE, FALSE,    FALSE,   0,            _I_READ,
	_T("Upload"),     NULL,                TRUE,  FALSE, FALSE,    FALSE,   _I_WRITE,     0,
	_T("Resume"),     NULL,                TRUE,  FALSE, FALSE,    FALSE,   0,            _I_WRITE,
	_T("OverWrite"),  NULL,                TRUE,  FALSE, FALSE,    FALSE,   0,            (_I_WRITE | _I_OWN),
	_T("Rename"),     _T("RenameOwn"),     TRUE,  TRUE,  TRUE,     TRUE,    _I_WRITE,     0,
	_T("Delete"),     _T("DeleteOwn"),     TRUE,  FALSE, TRUE,     FALSE,   _I_WRITE,     0,
	_T("MakeDir"),    NULL,                FALSE, TRUE,  TRUE,     FALSE,   _I_WRITE,     0,
	_T("RemoveDir"),  _T("RemoveOwnDir"),  FALSE, TRUE,  FALSE,    FALSE,   _I_WRITE,     0,
	_T("TimeStamp"),  _T("TimeStampOwn"),  TRUE,  FALSE, FALSE,    FALSE,   _I_WRITE,     _I_WRITE,
	_T("NoStats"),    NULL,                TRUE,  FALSE, FALSE,    FALSE,   0,            0,
	_T("NoFxpOut"),   NULL,                TRUE,  FALSE, FALSE,    FALSE,   0,            0,
	_T("NoFxpIn"),    NULL,                TRUE , FALSE, FALSE,    FALSE,   0,            0,
	NULL,             NULL,                FALSE, FALSE, FALSE,    FALSE,   0,            0
};


LPTSTR Admin_Permissions(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPCONFIG_LINE	lpLine, lpLine2;
	LPUSERFILE      lpUserFile;
	LPBUFFER        lpBuffer;
	LPTSTR          tszDirName, tszUserName, tszAccessList1, tszAccessList2, tszRealPath, tszGroup, tszAction;
	LPTSTR          tszItemMatch, tszParentMatch, tszPath, tszSlash, tszDirPath, tszPrivate, tszWhy, tszTemp;
	VIRTUALPATH     vpPath, vpCheck;
	TCHAR			PathBuffer1[_MAX_PWD + 1], PathBuffer2[_MAX_PWD+1], tTemp;
	INT				iOffset, iOffset2, iUid;
	DWORD           n, dwError, dwLen, dwMinPath;
	BOOL            bIsVfsAdmin, bIsAdmin, bIsNormal, bIsMountPoint, bResult, bResult2, bAccess, bPathOk, bPrinted, bMatch;
	LPACTION_TEST   lpActionTest;
	LPFILEINFO      lpFileInfo, lpParentInfo, lpPathInfo;
	MOUNT_DATA      MountData;

	lpBuffer    = &lpUser->CommandChannel.Out;

	tszDirName   = NULL;
	tszUserName  = NULL;
	lpUserFile   = NULL;
	lpFileInfo   = NULL;
	lpParentInfo = NULL;

	bIsNormal = HasFlag(lpUser->UserFile, _T("MV1G"));

	if (GetStringItems(Args) > 1)
	{
		tszUserName = GetStringIndexStatic(Args,1);
		if (tszUserName[0] == _T('-'))
		{
			tszUserName++;
			if (bIsNormal)
			{
				ERROR_RETURN(IO_NO_ACCESS, tszUserName);
			}
			iUid = User2Uid(tszUserName);
			if (iUid == -1)
			{
				ERROR_RETURN(ERROR_USER_NOT_FOUND, tszUserName);
			}
			if (iUid != lpUser->UserFile->Uid)
			{
				if (UserFile_OpenPrimitive(iUid, &lpUserFile, 0))
				{
					return tszUserName;
				}
				if (HasFlag(lpUser->UserFile, _T("V")) && User_IsAdmin(lpUser->UserFile, lpUserFile, NULL))
				{
					dwError = GetLastError();
					UserFile_Close(&lpUserFile, 0);
					ERROR_RETURN(dwError, tszUserName);
				}
			}
			else
			{
				lpUserFile = lpUser->UserFile;
			}
			if (GetStringItems(Args) > 2)
			{
				tszDirName = GetStringRange(Args, 2, STR_END);
			}
		}
		else
		{
			tszUserName = NULL;
			tszDirName  = GetStringRange(Args, 1, STR_END);
		}
	}

	if (!tszUserName)
	{
		tszUserName = Uid2User(lpUser->UserFile->Uid);
		if (!tszUserName) tszUserName = _T("*unknown*");
		lpUserFile  = lpUser->UserFile;
	}

	FormatString(lpBuffer, _T("%s\r\n%sAccess report for user '%s'.  Flags: %s\r\n"), tszMultilinePrefix, tszMultilinePrefix, tszUserName, lpUserFile->Flags);
	FormatString(lpBuffer, _T("%sGroup memberships: "), tszMultilinePrefix);
	for (n = 0 ; n < MAX_GROUPS && (lpUserFile->Groups[n] != -1) ; n++)
	{
		if (n) FormatString(lpBuffer, _T(", "));
		tszGroup = Gid2Group(lpUserFile->Groups[n]);
		if (!tszGroup) tszGroup = _T("*unknown*");
		FormatString(lpBuffer, _T("%s"), tszGroup);
	}
	FormatString(lpBuffer, _T("\r\n"));

	PWD_Reset(&vpPath);
	if (tszDirName)
	{
		ZeroMemory(&MountData, sizeof(MountData));
		PWD_Copy(&lpUser->CommandChannel.Path, &vpPath, FALSE);
		tszRealPath = PWD_CWD2(lpUserFile, &vpPath, tszDirName, lpUser->hMountFile, &MountData, IGNORE_PERMS|EXISTS|TYPE_LINK|VIRTUAL_PWD, NULL, NULL, NULL);
		if (!tszRealPath)
		{
			// it really must not exist...
			dwError = GetLastError();
			if (lpUserFile != lpUser->UserFile) UserFile_Close(&lpUserFile, 0);
			ERROR_RETURN(dwError, tszDirName);
		}
	}
	else
	{
		PWD_Copy(&lpUser->CommandChannel.Path, &vpPath, TRUE);
		tszRealPath = vpPath.RealPath;
	}

	bIsVfsAdmin = !HasFlag(lpUser->UserFile, _T("MV"));
	bIsAdmin    = !HasFlag(lpUser->UserFile, _T("MV1"));

	if (!bIsVfsAdmin)
	{
		// pass it through again to check the whole path for hidden dirs so we don't leak info if
		// target user can see the item but requesting user can't
		PWD_Reset(&vpCheck);
		PWD_Set(&vpCheck, "/");
		if (!PWD_CWD(lpUser->UserFile, &vpCheck, vpPath.pwd, lpUser->hMountFile, EXISTS|TYPE_LINK|VIRTUAL_PWD))
		{
			dwError = GetLastError();
			if (lpUserFile != lpUser->UserFile) UserFile_Close(&lpUserFile, 0);
			if (tszDirName)
			{
				ERROR_RETURN(dwError, tszDirName);
			}
			ERROR_RETURN(dwError, GetStringIndexStatic(Args, 0));
		}
		PWD_Free(&vpCheck);
	}

	// pass it through again to check the whole path as we might be checking perms as a different user...
	PWD_Reset(&vpCheck);
	PWD_Set(&vpCheck, "/");
	if (!PWD_CWD(lpUserFile, &vpCheck, vpPath.pwd, lpUser->hMountFile, EXISTS|TYPE_LINK|VIRTUAL_PWD))
	{
		bPathOk = FALSE;
		if (!bIsVfsAdmin)
		{
			dwError = GetLastError();
			if (lpUserFile != lpUser->UserFile) UserFile_Close(&lpUserFile, 0);
			if (tszDirName)
			{
				ERROR_RETURN(dwError, tszDirName);
			}
			ERROR_RETURN(dwError, GetStringIndexStatic(Args, 0));
		}
	}
	else
	{
		PWD_Free(&vpCheck);
		bPathOk = TRUE;
	}

	FormatString(lpBuffer, _T("%s\r\n%sPath Check:\r\n"), tszMultilinePrefix, tszMultilinePrefix);
	
	bAccess = TRUE;
	tszDirPath = NULL;
	ZeroMemory(&MountData, sizeof(MountData));

	for(tszPath = vpPath.pwd ; tszSlash = _tcschr(tszPath, _T('/')) ; tszPath = tszSlash)
	{
		ZeroMemory(&MountData, sizeof(MountData));
		tTemp = *++tszSlash;
		*tszSlash = 0;
		FreeShared(tszDirPath);
		tszDirPath = PWD_Resolve(vpPath.pwd, lpUser->hMountFile, &MountData, TRUE, 0);
		FormatString(lpBuffer, _T("%s  "), tszMultilinePrefix);
		tszPrivate = NULL;
		if (!tszDirPath)
		{
			if (MountData.lpVirtualDirEvent)
			{
				FormatString(lpBuffer, _T("VIRTUAL    :"), tszMultilinePrefix);
			}
			else
			{
				FormatString(lpBuffer, _T("NO-DIR     :"), tszMultilinePrefix);
			}
		}
		else
		{
			if (GetFileInfoNoCheck(tszDirPath, &lpPathInfo))
			{
				tszWhy = NULL;
				if ((lpPathInfo->dwFileMode & S_PRIVATE) && (tszPrivate = (LPTSTR)FindFileContext(PRIVATE, &lpPathInfo->Context)))
				{
					if (HavePermission(lpUserFile, tszPrivate))
					{
						if (tszPrivate[0] == _T(':'))
						{
							tszWhy = _T(":");
						}
						else if (!HasFlag(lpUserFile, "M"))
						{
							tszWhy = _T("M");
						}
						else if (!HasFlag(lpUserFile, "V"))
						{
							tszWhy = _T("V");
						}
					}
					else
					{
						tszWhy = _T("H");
					}
				}
				if (bIsNormal)
				{
					// if we are here then we know the tests on the whole path passed at the very beginning
					// so lets not give out info on what exactly is a hidden path... just clear it.
					tszPrivate = NULL;
					tszWhy = NULL;
				}
				if (tszPrivate && !tszWhy)
				{
					FormatString(lpBuffer, _T("FAIL-HIDDEN:"));
					bAccess = FALSE;
				}
				else
				{
					if (tszItemMatch = GetFilePermMatch(lpUserFile, lpPathInfo, _I_READ))
					{
						if (tszWhy)
						{
							FormatString(lpBuffer, _T("OK %s+r [%s] :"), tszItemMatch, tszWhy);
						}
						else
						{
							FormatString(lpBuffer, _T("OK %s+r     :"), tszItemMatch);
						}
					}
					else
					{
						bAccess = FALSE;
						if (tszWhy)
						{
							FormatString(lpBuffer, _T("FAIL +r [%s]:"), tszWhy);
						}
						else
						{
							FormatString(lpBuffer, _T("FAIL +r    :"));
						}

					}
				}
				CloseFileInfo(lpPathInfo);
			}
			else
			{
				bAccess = FALSE;
				FormatString(lpBuffer, _T("NO-INFO    :"));
			}
		}
		if (tszPrivate && bIsVfsAdmin)
		{
			FormatString(lpBuffer, _T(" %s  | [%s]\r\n"), vpPath.pwd, tszPrivate);
		}
		else
		{
			FormatString(lpBuffer, _T(" %s\r\n"), vpPath.pwd);
		}
		*tszSlash = tTemp;
	}

	if (!tszDirPath && MountData.lpVirtualDirEvent)
	{
		FormatString(lpBuffer, _T("\r\n%s\r\n%sTarget is under a virtual directory.  Aborting.\r\n%s\r\n"),
			tszMultilinePrefix, tszMultilinePrefix, tszMultilinePrefix);
		goto cleanup;
	}
	FreeShared(tszDirPath);

	bIsMountPoint = PWD_IsMountPoint(vpPath.pwd, lpUser->hMountFile);

	if (!GetFileInfo(tszRealPath, &lpFileInfo))
	{
		FormatString(lpBuffer, _T("%sERROR: Unable to get file/dir owner info.\r\n"), tszMultilinePrefix);
		goto cleanup;
	}

	if (vpPath.len == 1)
	{
		// we are at root, so parent = dir
		lpParentInfo = lpFileInfo;
	}
	else if (!GetVfsParentFileInfo(lpUser->UserFile, lpUser->hMountFile, &vpPath, &lpParentInfo, FALSE))
	{
		FormatString(lpBuffer, _T("%sERROR: Unable to get parent directory info.\r\n"), tszMultilinePrefix);
		lpParentInfo = NULL;
	}

	lpLine = NULL;
	lpLine2 = NULL;
	Config_Lock(&IniConfigFile, FALSE);

	if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		if (lpFileInfo->dwFileMode & S_SYMBOLIC)
		{
			tszWhy = _T("SYMBOLIC LINK");
		}
		else 
		{
			tszWhy = _T("DIRECTORY");
		}
	}
	else
	{
		tszWhy = ("FILE");
	}
	FormatString(lpBuffer, _T("%s\r\n%sDetails for %s: %s\r\n%s\r\n"), tszMultilinePrefix, tszMultilinePrefix, tszWhy, vpPath.pwd, tszMultilinePrefix);

	if (bIsAdmin)
	{
		FormatString(lpBuffer, _T("%sAction:   | OK? | Perms    | Parent  | Rule     <path> -> <flag-perms>\r\n"), tszMultilinePrefix);
		FormatString(lpBuffer, _T("%s--------- | --- | -------- | ------- | -------------------------------\r\n"), tszMultilinePrefix);
	}
	else
	{
		FormatString(lpBuffer, _T("%sAction:   | OK? | Perms    | Parent  | Rule     <path>\r\n"), tszMultilinePrefix);
		FormatString(lpBuffer, _T("%s--------- | --- | -------- | ------- | -------------------------------\r\n"), tszMultilinePrefix);
	}

	for (lpActionTest = ActionTestList ; lpActionTest->tszAction ; lpActionTest++)
	{
		// skip stuff that doesn't apply to the type of thing we are dealing with
		if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (lpFileInfo->dwFileMode & S_SYMBOLIC)
			{
				if (!lpActionTest->bSymbolicActivity) continue;
			}
			else if (!lpActionTest->bDirectoryActivity) continue;
		}
		else if (!lpActionTest->bFileActivity) continue;

		iOffset = 0;
		lpLine2 = NULL;

		while (lpLine = Config_Get_Primitive(&IniConfigFile, "VFS", lpActionTest->tszAction, &iOffset))
		{
			if (ParseVfsPerms(lpLine, PathBuffer1, &tszAccessList1))
			{
				continue;
			}

			//	Compare current path with config path
			if (! PathCompare(PathBuffer1, vpPath.pwd))
			{
				//	Get permission
				bResult = HavePermission(lpUserFile, tszAccessList1);
				// if we have permission we are done.
				if (!bResult) break;
				// if there isn't an owner override we are done.
				if (!lpActionTest->tszOwnerAction) break;
				// if we don't own the object in question we are done.
				if (lpFileInfo->Uid != lpUserFile->Uid) break;

				// ok, time to test the alternate owner permissions
				iOffset2 = 0;
				while (lpLine2 = Config_Get_Primitive(&IniConfigFile, "VFS", lpActionTest->tszOwnerAction, &iOffset2))
				{
					if (ParseVfsPerms(lpLine2, PathBuffer2, &tszAccessList2))
					{
						continue;
					}

					//	Compare current path with config path
					if (! PathCompare(PathBuffer2, vpPath.pwd))
					{
						//	Get permission
						bResult2 = HavePermission(lpUserFile, tszAccessList2);
						break;
					}
				}
				break;
			}
		}

		bAccess = bPathOk;

		if (!lpLine || (lpLine && bResult && (!lpLine2 || (lpLine2 && bResult2))))
		{
			bAccess = FALSE;
		}

		if (!bIsAdmin)
		{
			tszAccessList2 = tszAccessList1 = _T("*HIDDEN*");
		}

		tszItemMatch = NULL;
		if (lpActionTest->bNoMount && bIsMountPoint)
		{
			bAccess = FALSE;
		}

		if (lpActionTest->dwRequiredPerms)
		{
			if (lpFileInfo) tszItemMatch = GetFilePermMatch(lpUserFile, lpFileInfo, lpActionTest->dwRequiredPerms);
			if (!tszItemMatch) bAccess = FALSE;
		}

		tszParentMatch = NULL;
		if (lpActionTest->dwParentRequiredPerms)
		{
			if (lpParentInfo) tszParentMatch = GetFilePermMatch(lpUserFile, lpParentInfo, lpActionTest->dwParentRequiredPerms);
			if (!tszParentMatch) bAccess = FALSE;
		}

		if (bAccess)
		{
			FormatString(lpBuffer, _T("%s%-9s | %1TYES%0T | "), tszMultilinePrefix, lpActionTest->tszAction);
		}
		else
		{
			FormatString(lpBuffer, _T("%s%-9s | %2TNO%0T  | "), tszMultilinePrefix, lpActionTest->tszAction);
		}

		if (lpActionTest->bNoMount && bIsMountPoint)
		{
			FormatString(lpBuffer, _T("FAIL-MNT"));
		}
		else if (lpActionTest->dwRequiredPerms)
		{
			if (tszItemMatch)
			{
				FormatString(lpBuffer, _T("OK  %s+"), tszItemMatch);
			}
			else
			{
				FormatString(lpBuffer, _T("FAIL +"));
			}
			if (lpActionTest->dwRequiredPerms & _I_WRITE)
			{
				FormatString(lpBuffer, _T("w "));
			}
			else
			{
				FormatString(lpBuffer, _T("r "));
			}
		}
		else
		{
			FormatString(lpBuffer, _T("        "));
		}
		FormatString(lpBuffer, _T(" | "));

		if (lpActionTest->dwParentRequiredPerms)
		{
			if (tszParentMatch)
			{
				FormatString(lpBuffer, _T("OK  %s+"), tszParentMatch);
			}
			else
			{
				FormatString(lpBuffer, _T("FAIL +"));
			}
			if (lpActionTest->dwParentRequiredPerms & _I_WRITE)
			{
				FormatString(lpBuffer, _T("w"));
			}
			else
			{
				FormatString(lpBuffer, _T("r"));
			}
		}
		else
		{
			FormatString(lpBuffer, _T("       "));
		}
		FormatString(lpBuffer, _T(" | "));

		tszTemp = NULL;
		if (lpLine2)
		{
			// only here if we are owner and plain check failed
			tszTemp = tszAccessList2;
			if (!bResult)
			{
				// owner OK
				FormatString(lpBuffer, _T("OK[OWN]  '%s'"), PathBuffer2);
			}
			else
			{
				// owner and plain denied!
				FormatString(lpBuffer, _T("FAIL+OWN '%s'"), PathBuffer2);
			}
		}
		else if (lpLine)
		{
			tszTemp = tszAccessList1;
			if (!bResult)
			{
				// it's OK
				FormatString(lpBuffer, _T("OK       '%s'"), PathBuffer1);
			}
			else if (lpActionTest->tszOwnerAction && (lpFileInfo->Uid == lpUserFile->Uid))
			{
				// denied, BUT no owner line was found to test against but it COULD have existed
				FormatString(lpBuffer, _T("FAIL-OWN '%s'"), PathBuffer1);
			}
			else
			{
				// denied
				FormatString(lpBuffer, _T("FAIL     '%s'"), PathBuffer1);
			}
		}
		else 
		{
			// no matching entry found to allow or deny!
			FormatString(lpBuffer,     _T("FAIL    *NO PATH MATCH*\r\n"));
		}
		if (tszTemp)
		{
			if (bIsAdmin)
			{
				while ((*tszTemp == _T(' ')) || (*tszTemp == _T('\t'))) tszTemp++;
				FormatString(lpBuffer, _T(" -> '%s'\r\n"), tszTemp);
			}
			else
			{
				FormatString(lpBuffer, _T("\r\n"));
			}
		}
	}

	if ((lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(lpFileInfo->dwFileMode & S_SYMBOLIC) &&
		(bIsAdmin || (vpPath.len+5 <= _MAX_PWD)))
	{
		// show details for things IN the directory
		FormatString(lpBuffer, _T("%s\r\n%s\r\n%sRules for items under the directory...\r\n%s\r\n"),
			tszMultilinePrefix, tszMultilinePrefix, tszMultilinePrefix, tszMultilinePrefix);

		if (bIsAdmin)
		{
			FormatString(lpBuffer, _T("%sAction:      | OK? | Dir     | Rule <path> -> <flag-perms>\r\n"), tszMultilinePrefix);
			FormatString(lpBuffer, _T("%s------------ | --- | ------- | ---------------------------\r\n"), tszMultilinePrefix);
		}
		else
		{
			FormatString(lpBuffer, _T("%sAction:      | OK? | Dir     | Rule <path>\r\n"), tszMultilinePrefix);
			FormatString(lpBuffer, _T("%s------------ | --- | ------- | ---------------------------\r\n"), tszMultilinePrefix);

			// now append "_@#%" as an unlikely pathname to test against that, we already made sure we have room above.
			_tcscpy(&vpPath.pwd[vpPath.len], _T("/_@#%"));
		}
		for (lpActionTest = ActionTestList ; lpActionTest->tszAction ; lpActionTest++)
		{
			for (tszAction = lpActionTest->tszAction ; tszAction ; tszAction = (tszAction == lpActionTest->tszAction ? lpActionTest->tszOwnerAction : NULL))
			{
				bPrinted  = FALSE;
				bMatch    = FALSE;
				iOffset   = 0;
				lpLine2   = NULL;
				dwMinPath = 0;

				while (lpLine = Config_Get_Primitive(&IniConfigFile, "VFS", tszAction, &iOffset))
				{
					if (ParseVfsPerms(lpLine, PathBuffer1, &tszAccessList1))
					{
						continue;
					}

					// simple test to remove most info leakage
					if (!bIsAdmin && (PathBuffer1[0] != _T('/'))) continue;

					if (PathBuffer1[0] == _T('/'))
					{
						_tcscpy(PathBuffer2, PathBuffer1);
						tszPath = &PathBuffer2[1];
						while (*tszPath)
						{
							if ((*tszPath == _T('*')) || (*tszPath == _T('?')) || (*tszPath == _T('[')) || (*tszPath == _T(']')))
							{
								// found a wildcard, now back to last /
								while ((tszPath != PathBuffer2) && (*tszPath != _T('/'))) tszPath--;
								tszPath[1] = 0;
								break;
							}
							tszPath++;
						}

						dwLen = _tcslen(PathBuffer2);
						if (bMatch && (dwLen < vpPath.len) && (dwLen < dwMinPath))
						{
							// skip rules that are more general than the ones already displayed as they
							// will never be used because the first found is the controlling rule.
							continue;
						}
						if (dwMinPath < vpPath.len) dwMinPath = vpPath.len;

						if (PathCompare(PathBuffer1, vpPath.pwd))
						{
							// not a match, but let's give it a second try on the basepath for admins
							if (!bIsAdmin || _tcsnicmp(PathBuffer2, vpPath.pwd, vpPath.len))
							{
								continue;
							}
						}

						if (!bIsVfsAdmin)
						{
							// path matches, but double check full paths are visible to the user requesting info
							// so we don't give out hidden paths.
							ZeroMemory(&MountData, sizeof(MountData));
							PWD_Reset(&vpCheck);
							PWD_Set(&vpCheck, "/");
							if (!PWD_CWD2(lpUser->UserFile, &vpCheck, PathBuffer2, lpUser->hMountFile, &MountData, EXISTS|TYPE_LINK, NULL, NULL, NULL))
							{
								// TODO: test for virtual dirs here?  if (MountData.Initialised && MountData.lpVirtualDirEvent)?
								// no access, or wasn't found
								continue;
							}
							PWD_Free(&vpCheck);
						}
						bMatch = TRUE;
					}
					else
					{
						// if it doesn't match the path then skip
						if (PathCompare(PathBuffer1, vpPath.pwd))
						{
							if (!bIsAdmin || (PathBuffer1[0] != _T('*')))
							{
								continue;
							}
						}
					}

					bPrinted = TRUE;

					bResult = HavePermission(lpUserFile, tszAccessList1);

					bAccess = bPathOk;

					if (bResult)
					{
						bAccess = FALSE;
					}

					tszParentMatch = NULL;
					if (lpActionTest->dwParentRequiredPerms)
					{
						if (lpFileInfo) tszParentMatch = GetFilePermMatch(lpUserFile, lpFileInfo, lpActionTest->dwParentRequiredPerms);
						if (!tszParentMatch) bAccess = FALSE;
					}

					if (bAccess)
					{
						FormatString(lpBuffer, _T("%s%-12s | %1TYES%0T | "), tszMultilinePrefix, tszAction);
					}
					else
					{
						FormatString(lpBuffer, _T("%s%-12s | %2TNO%0T  | "), tszMultilinePrefix, tszAction);
					}

					if (lpActionTest->dwParentRequiredPerms)
					{
						if (tszParentMatch)
						{
							FormatString(lpBuffer, _T("OK  %s+"), tszParentMatch);
						}
						else
						{
							FormatString(lpBuffer, _T("FAIL +"));
						}
						if (lpActionTest->dwParentRequiredPerms & _I_WRITE)
						{
							FormatString(lpBuffer, _T("w"));
						}
						else
						{
							FormatString(lpBuffer, _T("r"));
						}
					}
					else
					{
						FormatString(lpBuffer, _T("       "));
					}

					if (!bResult)
					{
						// it's OK
						FormatString(lpBuffer, _T(" | OK   '%s'"), PathBuffer1);
					}
					else
					{
						// denied
						FormatString(lpBuffer, _T(" | FAIL '%s'"), PathBuffer1);
					}
					if (bIsAdmin)
					{
						while ((*tszAccessList1 == _T(' ')) || (*tszAccessList1 == _T('\t'))) tszAccessList1++;
						FormatString(lpBuffer, _T("-> '%s'\r\n"), tszAccessList1);
					}
					else
					{
						FormatString(lpBuffer, _T("\r\n"));
						// Can stop printing answers now!
						break;
					}
				}
				if (!bPrinted)
				{
					FormatString(lpBuffer, _T("%s%-12s | %2TNO%0T  |         | FAIL *NO PATH MATCH*\r\n"), tszMultilinePrefix, tszAction);
				}
			}
		}
	}


	Config_Unlock(&IniConfigFile, FALSE);

	FormatString(lpBuffer, _T("%s\r\n"), tszMultilinePrefix);

cleanup:
	if (lpParentInfo && (lpFileInfo != lpParentInfo)) CloseFileInfo(lpParentInfo);
	if (lpFileInfo) CloseFileInfo(lpFileInfo);
	if (lpUserFile != lpUser->UserFile) UserFile_Close(&lpUserFile, 0);

	PWD_Free(&vpPath);
	return NULL;
}


// cwd = /test
// site symlink ../foo              : valid
// site symlink ../foo|foo2         : valid
// site symlink ../foo   |   foo3   : valid
// site symlink /foo | foo4         : valid
// site symlink /foo/ | foo5        : valid
// site symlink /foo/missing        : invalid, dir is missing
// site symlink "../foo6"           : invalid, bad filename filtered
// site symlink ../foo | "foo7"     : invalid, bad filename filtered
// site symlink ../foo | /foo8      : invalid, ../foo invalid from / dir
// site symlink foo | /foo8         : valid, created under /, points to /foo
// site symlink ../../foo | exists/foo9 : valid, created in /tests/exists/ and
//                                        points to ../../foo which makes sense
//                                        from the subdir.
LPTSTR Admin_CreateSymLink(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR          tszTemp, tszName, tszCommand, tszLine, tszNext;
	LPBUFFER        lpBuffer;
	VIRTUALPATH     vpTarget, vpName;
	TCHAR           tszTarget[_MAX_PWD+1];
	DWORD           dwTarget, dwError;
	LPFILEINFO      lpFileInfo, lpParent;
	VFSUPDATE       UpdateData;
	BOOL            bSuccess;


	tszCommand  = GetStringIndexStatic(Args, 0);
	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, tszCommand);

	lpBuffer = &lpUser->CommandChannel.Out;

	// now figure out if we have target | name or just target
	tszLine = tszTemp = GetStringRange(Args, 1, STR_END);
	dwTarget = 0;
	tszNext = tszTarget;
	tszName = NULL;
	while (dwTarget < sizeof(tszTarget) && *tszTemp)
	{
		if (*tszTemp == _T('|'))
		{
			*tszNext = 0;
			tszName = tszTemp+1;
			// now go back and squash any trailing spaces
			while (dwTarget && tszNext[-1] == _T(' '))
			{
				dwTarget--;
				*tszNext-- = 0;
			}
			// now skip over leading spaces in the name
			while (*tszName == _T(' '))
			{
				tszName++;
			}
			break;
		}
		*tszNext++ = *tszTemp++;
		dwTarget++;
	}
	if (dwTarget >= sizeof(tszTarget) && *tszTemp)
	{
		SetLastError(IO_INVALID_FILENAME);
		return tszCommand;
	}
	*tszNext = 0;

	if (tszName && !*tszName)
	{
		SetLastError(IO_INVALID_ARGUMENTS);
		return tszCommand;
	}
	 
	if (!tszName)
	{
		if (!(tszTemp = _tcsrchr(tszTarget, _T('//'))) || !tszTemp[1])
		{
			SetLastError(IO_INVALID_ARGUMENTS);
			return tszCommand;
		}
		tszName = tszTemp+1;
	}


	bSuccess   = FALSE;
	lpParent   = NULL;
	lpFileInfo = NULL;
	ZeroMemory(&UpdateData, sizeof(VFSUPDATE));

	PWD_Zero(&vpName);
	PWD_Zero(&vpTarget);

	// We need to resolve the name portion first because if it a relative or absolute
	// path that doesn't resolve to the current directory a relative target will resolve
	// differently!
	PWD_Copy(&lpUser->CommandChannel.Path, &vpName, FALSE);
	if (!PWD_CWD(lpUser->UserFile, &vpName, tszName, lpUser->hMountFile, VIRTUAL_PWD))
	{
		return tszCommand;
	}

	// Make sure we aren't trying to create a symlink for the root dir.
	if (vpName.len == 1)
	{
		PWD_Free(&vpName);
		SetLastError(ERROR_FILE_EXISTS);
		return tszCommand;
	}

	// now make sure it doesn't exist
	if (GetFileInfo(vpName.RealPath, &lpFileInfo))
	{
		// doh, it exists!
		PWD_Free(&vpName);
		CloseFileInfo(lpFileInfo);
		SetLastError(ERROR_FILE_EXISTS);
		return tszCommand;
	}

	PWD_Copy(&vpName, &vpTarget, FALSE);
	// now make sure that the VFS path ends in a / since it's supposed to be a directory
	if (vpTarget.len < _MAX_PWD && vpTarget.pwd[vpTarget.len] != '/')
	{
		vpTarget.pwd[++vpTarget.len] = '/';
		vpTarget.pwd[vpTarget.len]   = 0;
	}
	// now strip off the last directory
	if (!PWD_CWD(lpUser->UserFile, &vpTarget, _T(".."), lpUser->hMountFile, EXISTS|VIRTUAL_PWD))
	{
		PWD_Free(&vpName);
	}

	// using TYPE_LINK here means we can create a symlink to a symlink
	if (!PWD_CWD(lpUser->UserFile, &vpTarget, tszTarget, lpUser->hMountFile, EXISTS|TYPE_LINK|TYPE_DIRECTORY|VIRTUAL_PWD))
	{
		goto cleanup;
	}

	if (!GetFileInfo(vpTarget.RealPath, &lpFileInfo))
	{
		goto cleanup;
	}
	UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
	UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;
	CloseFileInfo(lpFileInfo);

	if (!GetVfsParentFileInfo(lpUser->UserFile, lpUser->hMountFile, &vpName, &lpParent, FALSE))
	{
		goto cleanup;
	}

	//  Check access
	if (! Access(lpUser->UserFile, lpParent, _I_WRITE) ||
		PathCheck(lpUser->UserFile, vpName.pwd, "MakeDir") ||
		!CreateDirectory(vpName.RealPath, NULL))
	{
		goto cleanup;
	}

	//  Set directory ownership
	UpdateData.Uid         = lpUser->UserFile->Uid;
	UpdateData.Gid         = lpUser->UserFile->Gid;
	UpdateData.dwFileMode  = lpParent->dwFileMode;

	if (InsertFileContext(&UpdateData.Context, SYMBOLICLINK, tszTarget, dwTarget * sizeof(TCHAR)))
	{
		if (UpdateFileInfo(vpName.RealPath, &UpdateData))
		{
			bSuccess = TRUE;
		}
	}
	MarkVirtualDir(&vpName, lpUser->hMountFile);

cleanup:
	if (!bSuccess)
	{
		dwError = GetLastError();
	}
	PWD_Free(&vpTarget);
	PWD_Free(&vpName);
	if (lpParent)
	{
		CloseFileInfo(lpParent);
	}
	if (UpdateData.Context.dwData)
	{
		FreeFileContext(&UpdateData.Context);
	}
	if (bSuccess)
	{
		return NULL;
	}
	return tszCommand;
}



LPTSTR Admin_Version(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR            tszCommand;
	LPBUFFER          lpBuffer;

	tszCommand  = GetStringIndexStatic(Args, 0);
	lpBuffer	= &lpUser->CommandChannel.Out;

	FormatString(lpBuffer, _TEXT("%sioFTPD version: %u-%u-%ur\r\n"), tszMultilinePrefix,
		dwIoVersion[0], dwIoVersion[1], dwIoVersion[2]);
	return NULL;
}



LPTSTR Admin_CrashNow(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszUserName;

	//	Get arguments
	tszUserName	= LookupUserName(lpUser->UserFile);

	if (HasFlag(lpUser->UserFile, _TEXT("M")))
	{
		ERROR_RETURN(IO_NO_ACCESS, GetStringIndexStatic(Args, 0));
	}

	//	Close daemon
	Putlog(LOG_GENERAL, _TEXT("CRASHNOW: \"%s\"\r\n"), tszUserName);
	SleepEx(100, TRUE);

	// Generate exception
	tszUserName = 0;
	*tszUserName = 0; // crash!

	return NULL;
}


LPTSTR Admin_LoadSymbols(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszUserName;

	//	Get arguments
	tszUserName	= LookupUserName(lpUser->UserFile);

	if (HasFlag(lpUser->UserFile, _TEXT("M")))
	{
		ERROR_RETURN(IO_NO_ACCESS, GetStringIndexStatic(Args, 0));
	}

	//	Close daemon
	Putlog(LOG_GENERAL, _TEXT("LOADSYMBOLS: \"%s\"\r\n"), tszUserName);
	SleepEx(100, TRUE);

	EnableSymSrvPrompt();

	// Generate exception
	tszUserName = 0;
	*tszUserName = 0; // crash!

	return NULL;
}



LPTSTR Admin_RemoveCert(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszCommand, tszUserName, tszCertName;
	LPBUFFER        lpBuffer;
	LPIOSERVICE     lpService;
	DWORD           dwMatch;

	tszCommand  = GetStringIndexStatic(Args, 0);
	if (GetStringItems(Args) > 2) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszCommand);
	if (GetStringItems(Args) == 1) ERROR_RETURN(ERROR_MISSING_ARGUMENT, tszCommand);
	tszCertName = GetStringIndexStatic(Args, 1);

	//	Get arguments
	tszUserName	= LookupUserName(lpUser->UserFile);
	lpBuffer	= &lpUser->CommandChannel.Out;

	if (HasFlag(lpUser->UserFile, _TEXT("M")))
	{
		ERROR_RETURN(IO_NO_ACCESS, tszCommand);
	}

	lpService = lpUser->Connection.lpService;



	if (lpService->tszServiceValue && !_tcsicmp(tszCertName, lpService->tszServiceValue))
	{
		dwMatch = 1;
	}
	else if (lpService->tszHostValue && !_tcsicmp(tszCertName, lpService->tszHostValue))
	{
		dwMatch = 2;
	}
	else if (!_tcsicmp(tszCertName, _T("ioFTPD")))
	{
		dwMatch = 3;
	}
	else
	{
		// name doesn't match
		ERROR_RETURN(IO_INVALID_ARGUMENTS, tszCommand);
	}

	if (!Secure_Delete_Cert(tszCertName))
	{
		if (GetLastError() == ERROR_FILE_MISSING)
		{
			FormatString(lpBuffer, _TEXT("%sCouldn't find cert \"%s\" files to delete.\r\n"), tszMultilinePrefix, tszCertName);
		}
		return tszCommand;
	}

	// try to load a different cert...
	AcquireExclusiveLock(&lpService->loLock);

	Secure_Free_Ctx(lpService->pSecureCtx);
	lpService->pSecureCtx = NULL;
	lpService->dwFoundCredentials = 0;

	Service_GetCredentials(lpService, FALSE);

	ReleaseExclusiveLock(&lpService->loLock);


	FormatString(lpBuffer, _TEXT("%sRemoved cert \"%s\" files.\r\n"), tszMultilinePrefix, tszCertName);
	return NULL;
}



// this assumes TCHAR = CHAR... but so does a lot of other stuff...
VOID Progress_Update(LPCMD_PROGRESS lpProgress)
{
	char Buffer[256];
	DWORD dwLen;

	dwLen = 0;
	if (lpProgress->tszMultilinePrefix)
	{
		strcpy_s(Buffer, sizeof(Buffer), lpProgress->tszMultilinePrefix);
		dwLen = strlen(Buffer);
	}
	// pass it 3 args though format string may ignore them
	sprintf_s(&Buffer[dwLen], sizeof(Buffer)-dwLen, lpProgress->tszFormatString,
		lpProgress->dwArg1, lpProgress->dwArg2, lpProgress->dwArg3, lpProgress->dwArg4);
	dwLen = strlen(Buffer);

	SendQuick(&lpProgress->lpCommand->Socket, Buffer, dwLen);
	lpProgress->dwTicks = GetTickCount() + lpProgress->dwDelay;
}



static VOID
DataBase_ErrorProc(int iType, LPTSTR tszTableLocation, int Id,
				   LPTSTR tszName, LPTSTR tszModuleName)
{
	if (!lpVerifyBuffer) return;

	switch (iType)
	{
	case 1:
		FormatString(lpVerifyBuffer, _TEXT("%sDuplicate ID in '%s': id='%d' name='%s' module='%s'\r\n"),
			tszVerifyPrefix, tszTableLocation, Id, tszName, tszModuleName);
		return;
	case 2:
		FormatString(lpVerifyBuffer, _TEXT("%sUnknown module in '%s': id='%d' name='%s' module='%s'\r\n"),
			tszVerifyPrefix, tszTableLocation, Id, tszName, tszModuleName);
		return;
	case 3:
		FormatString(lpVerifyBuffer, _TEXT("%sDuplicate name in '%s': id='%d' name='%s' module='%s'\r\n"),
			tszVerifyPrefix, tszTableLocation, Id, tszName, tszModuleName);
	case 4:
		FormatString(lpVerifyBuffer, _TEXT("%sTuple parse error in '%s': line='%s'\r\n"),
			tszVerifyPrefix, tszTableLocation, tszName);
		return;
	case 5:
		FormatString(lpVerifyBuffer, _TEXT("%sBad line in '%s': line='%s'\r\n"),
			tszVerifyPrefix, tszTableLocation, tszName);
		return;
	default:
		FormatString(lpVerifyBuffer, _TEXT("%sUnknown error in '%s': id='%d' name='%s' module='%s'\r\n"),
			tszVerifyPrefix, tszTableLocation, Id, tszName, tszModuleName);
		return;
	}
}


static INT
Verify_Dummy_Open(LPVOID hModule, LPTSTR tszUserName, INT32 Uid)
{
	return DB_SUCCESS;
}



LPTSTR Admin_Verify(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	DWORD           dwGids, dwFileGids, dwModuleGids;
	DWORD           dwUids, dwFileUids, dwModuleUids;
	PINT32          pUserCount;
	DWORD           n, m, g, a, dwError, dwErrCount;
	LPBUFFER        lpBuffer;
	WIN32_FIND_DATA    FindData;
	HANDLE        hFind;
	IDDATABASE           dbGroupFake, dbUserFake;
	LPTSTR          tszGroupTableName, tszUserTableName;
	LPTSTR          tszGroupName, tszUserName;
	LPTSTR          tszGroupFileName, tszUserFileName, tszParent;
	CHAR            cTemp;
	LPIDITEM        pIdItem;
	int             iResult, Gid, Uid;
	LPPARENT_GROUPFILE lpParentGroupFile;
	LPPARENT_USERFILE  lpParentUserFile;
	LPUSERFILE         lpUserFile;
	LPGROUPFILE        lpGroupFile;
	BOOL               bKnown, bFix, bFixed;
	TCHAR              tszFileName[MAX_PATH+1];


	//	Get arguments
	lpBuffer = &lpUser->CommandChannel.Out;

	if (HasFlag(lpUser->UserFile, _TEXT("M")))
	{
		ERROR_RETURN(IO_NO_ACCESS, GetStringIndexStatic(Args, 0));
	}

	if (GetStringItems(Args) == 2)
	{
		if (_tcsicmp(GetStringIndexStatic(Args, 1), _T("fix")))
		{
			ERROR_RETURN(IO_INVALID_ARGUMENTS, GetStringIndexStatic(Args, 0));
		}
		bFix = TRUE;
	}
	else
	{
		bFix = FALSE;
	}
	bFixed = FALSE;

	// Some quick notes that I think are correct...
	//
	// 1) The IdDataBase routines are responsible for manipulating name::id::module tuples.
	//    They also read/write the tuples to a file.  The same functions are used for
	//    groups/users.  You can lookup the name of an id via a simple array lookup,
	//    however looking up a name requires a binary search but yields the full tuple.
	//
	// 2) The IdDataBase_add function assigns ids to new users and groups and it verifies the
	//    name doesn't already exist when creating new entries.  I modified the file loading
	//    function so it's impossible for duplicate names or ids to be added.  So all names
	//    and id's should be unique already.
	//
	// 4) ini::Locations::User_Id_Table points to the name:id:module tuples and every
	//    known user has an entry.  The act of registering a user, even by a module,
	//    creates an entry.  Ditto for groups.
	//
	// 5) Every STANDARD module user/group entry must have an associated file in the
	//    appropriate directory.  i.e ini::Locations::User_Files + \UID
	//
	// 6) The parsed and stored group/userfiles are kept in the lpUserFileArray and
	//    lpGroupFileArray.  Every IdDataBase entry should be represented here, however
	//    deleted user or groups which still have references will not be in the FileArrays
	//    but may still be in the IdDataBase.  Though they are flagged for
	//    deletion and this can be tested.
	//
	// 7) We'll process the groups first, and then the users so we can verify the
	//    validity of groups referenced in the userfile immediately.

	tszGroupTableName = 0;
	tszGroupFileName  = 0;
	pUserCount = 0;

	tszUserTableName = 0;
	tszUserFileName = 0;

	ZeroMemory(&dbGroupFake, sizeof(IDDATABASE));
	ZeroMemory(&dbUserFake, sizeof(IDDATABASE));

	dwError = 0;
	dwErrCount = 0;


	// only let one thread attempt this mess at a time... also needed to protect
	// the verifybuffer/prefix vars.
	while (InterlockedExchange(&dwVerifyBufferLock, TRUE)) SwitchToThread();

	lpVerifyBuffer = lpBuffer;
	tszVerifyPrefix = tszMultilinePrefix;


	// We have to be very careful here with the way we acquire locks.  First the user and group
	// array locks, then the lower level database locks and we make no modifications while holding
	// the locks since we want to acquire them in shared mode to prevent stalling anybody else,
	// and avoid any possible deadlocks if we acquired all of these exclusively.  Any changes will
	// be recorded in a to-do list for later use.
	AcquireSharedLock(&loUserFileArray);
	AcquireSharedLock(&loGroupFileArray);
	AcquireSharedLock(&dbUserId.loDataBase);
	AcquireSharedLock(&dbGroupId.loDataBase);


	//  Get path to local group id table
	tszGroupTableName  = Config_Get(&IniConfigFile, _TEXT("Locations"), _TEXT("Group_Id_Table"), NULL, NULL);
	if (! tszGroupTableName)
	{
		FormatString(lpBuffer, _TEXT("%sERROR: [Locations]::Group_Id_Table missing from .ini file.\r\n"), tszMultilinePrefix);
		dwErrCount++;
		goto CLEANUP;
	}
	FormatString(lpBuffer, _TEXT("%sParsing Group_Id_Table file: %s.\r\n"), tszMultilinePrefix, tszGroupTableName);

	if (IdDataBase_Init(tszGroupTableName, &dbGroupFake, Group_FindModule, Verify_Dummy_Open, DataBase_ErrorProc))
	{
		FormatString(lpBuffer, _TEXT("%sERROR: IdDataBase_Init returned error %E.\r\n"), tszMultilinePrefix, GetLastError());
		dwErrCount++;
		goto CLEANUP;
	}

	dwFileGids = 0;
	dwModuleGids = 0;
	dwGids = 0;

	// ok, now verify that the contents of the fake group database matches that of the real database,
	// and make sure the parent group entries exist and count standard vs other entries
	for ( n=0 ; n<dbGroupId.dwIdArrayItems ; n++ )
	{
		iResult = _tcsicmp(dbGroupId.lpIdArray[n]->tszName, dbGroupFake.lpIdArray[n]->tszName);
		if (iResult)
		{
			// TODO: could try to skip over missing or extra entry and continue...
			FormatString(lpBuffer, _TEXT("%sERROR: In memory Group DB differs from file (%s).\r\n"), tszMultilinePrefix, tszGroupTableName);
			if (bFix)
			{
				IdDataBase_Write(&dbGroupId);
				bFixed = TRUE;
				dwErrCount++;
			}
			goto CLEANUP;
		}
		if (_tcsicmp(dbGroupId.lpIdArray[n]->tszModuleName, dbGroupFake.lpIdArray[n]->tszModuleName) ||
			(dbGroupId.lpIdArray[n]->Id != dbGroupFake.lpIdArray[n]->Id))
		{
			FormatString(lpBuffer, _TEXT("%sERROR: In memory Group DB differs from file (%s).\r\n"), tszMultilinePrefix, tszGroupTableName);
			if (bFix)
			{
				IdDataBase_Write(&dbGroupId);
				bFixed = TRUE;
				dwErrCount++;
			}
			goto CLEANUP;
		}

		lpParentGroupFile = GetParentGroupFile(dbGroupId.lpIdArray[n]->Id);
		dwGids++;
		if (!_tcsicmp(dbGroupId.lpIdArray[n]->tszModuleName, _T("STANDARD")))
		{
			dwFileGids++;
		}
		else
		{
			dwModuleGids++;
		}
		if (!lpParentGroupFile)
		{
			FormatString(lpBuffer, _TEXT("%sWARNING: GroupFileArray missing GID %d (%s) -- being deleted?\r\n"), tszMultilinePrefix, dbGroupId.lpIdArray[n]->Id, dbGroupId.lpIdArray[n]->tszName);
			dwErrCount++;
		}
	}
	FormatString(lpBuffer, _TEXT("%sGroup count: %d (Standard: %d, Module: %d)\r\n"), tszMultilinePrefix, dwGids, dwFileGids, dwModuleGids);


	FormatString(lpBuffer, _TEXT("%sProcessing GroupFileArray.\r\n"), tszMultilinePrefix);
	for( n=0 ; n < dwGroupFileArrayItems ; n++ )
	{
		lpParentGroupFile = lpGroupFileArray[n];
		tszGroupName = Gid2Group(lpGroupFileArray[n]->Gid);
		
		if (tszGroupName)
		{
			bKnown = TRUE;
		}
		else
		{
			bKnown = FALSE;
			tszGroupName = _T("<Unknown>");
		}

		if (lpParentGroupFile->dwErrorFlags & WAIT_UNREGISTER)
		{
			FormatString(lpBuffer, _TEXT("%sINFO: Group waiting to be deleted: GID=%d (%s)\r\n"), tszMultilinePrefix,
				lpParentGroupFile->Gid, tszGroupName);
		}
		if (lpParentGroupFile->dwErrorFlags & WRITE_ERROR)
		{
			FormatString(lpBuffer, _TEXT("%sWARNING: Group with write error: GID=%d (%s)\r\n"), tszMultilinePrefix,
				lpParentGroupFile->Gid, tszGroupName);
		}

		if (!bKnown && !(lpParentGroupFile->dwErrorFlags & WAIT_UNREGISTER))
		{
			FormatString(lpBuffer, _TEXT("%sERROR: Group known but not in DB: GID=%d\r\n"), tszMultilinePrefix,
				lpParentGroupFile->Gid);
			dwErrCount++;
		}
	}


	tszGroupFileName  = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("Group_Files"), _T("\\Default.Group"), NULL);
	if (!tszGroupFileName) goto CLEANUP;
	// this can't fail since get_path sticks a \\ between result and appended text...
	tszParent  = _tcsrchr(tszGroupFileName,  _T('\\'));

	*tszParent = 0;
	FormatString(lpBuffer, _TEXT("%sExamining group directory: %s\r\n"), tszMultilinePrefix, tszGroupFileName);
	*tszParent = _T('\\');
	_tcscpy(tszParent+1, _T("*"));

	//  Begin directory search
	hFind  = FindFirstFile(tszGroupFileName, &FindData);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (FindData.cFileName[0] == '.')
			{
				continue;
			}
			iResult = sscanf_s(FindData.cFileName, "%d%c", &Gid, &cTemp);
			if (iResult == 1)
			{
				for( n=0 ; n<dwGids ; n++ )
				{
					pIdItem = dbGroupId.lpIdArray[n];
					if (pIdItem->Id == Gid)
					{
						if (_tcsicmp(pIdItem->tszModuleName, _T("STANDARD")))
						{
							FormatString(lpBuffer, _TEXT("%sERROR: Local group file found for module based group: GID=%d GROUPNAME=%s MODULE=\r\n"), tszMultilinePrefix,
								Gid, pIdItem->tszName, pIdItem->tszModuleName);
							dwErrCount++;
							if (bFix)
							{
								*tszParent = 0;
								_sntprintf_s(tszFileName, sizeof(tszFileName)/sizeof(*tszFileName), _TRUNCATE, _T("%s\\%s"), tszGroupFileName, FindData.cFileName);
								*tszParent = _T('\\');
								DeleteFile(tszFileName);
								bFixed++;
							}
						}
						break;
					}
				}
				if (n >= dwGids)
				{
					FormatString(lpBuffer, _TEXT("%sERROR: Unreferenced groupfile in directory: FILENAME=GID=%d\r\n"), tszMultilinePrefix, Gid);
					dwErrCount++;
					if (bFix)
					{
						*tszParent = 0;
						_sntprintf_s(tszFileName, sizeof(tszFileName)/sizeof(*tszFileName), _TRUNCATE, _T("%s\\%s"), tszGroupFileName, FindData.cFileName);
						*tszParent = _T('\\');
						DeleteFile(tszFileName);
						bFixed++;
					}

				}
				continue;
			}
			if (!stricmp("Default.Group", FindData.cFileName))
			{
				continue;
			}
			FormatString(lpBuffer, _TEXT("%sWARNING: Extra file in group directory: %s\r\n"), tszMultilinePrefix,
				FindData.cFileName);
		} while (FindNextFile(hFind, &FindData));

		dwError = GetLastError();
		if (dwError != ERROR_NO_MORE_FILES)
		{
			FormatString(lpBuffer, _TEXT("%sERROR: FindNextFile returned error: %E\r\n"), tszMultilinePrefix, dwError);
		}
		else
		{
			dwError = 0;
		}
		FindClose(hFind);
		if (dwError) goto CLEANUP;
	}



	//  Get path to local user id table
	tszUserTableName  = Config_Get(&IniConfigFile, _TEXT("Locations"), _TEXT("User_Id_Table"), NULL, NULL);
	if (! tszUserTableName)
	{
		FormatString(lpBuffer, _TEXT("%sERROR: [Locations]::User_Id_Table missing from .ini file.\r\n"), tszMultilinePrefix);
		dwErrCount++;
		goto CLEANUP;
	}
	FormatString(lpBuffer, _TEXT("%sParsing User_Id_Table file: %s.\r\n"), tszMultilinePrefix, tszUserTableName);

	if (IdDataBase_Init(tszUserTableName, &dbUserFake, User_FindModule, Verify_Dummy_Open, DataBase_ErrorProc))
	{
		FormatString(lpBuffer, _TEXT("%sERROR: IdDataBase_Init returned error %E.\r\n"), tszMultilinePrefix, GetLastError());
		dwErrCount++;
		goto CLEANUP;
	}

	dwFileUids = 0;
	dwModuleUids = 0;
	dwUids = 0;

	// ok, now verify that the contents of the fake user database matches that of the real database,
	// and make sure the parent user entries exist and count standard vs other entries
	for ( n=0 ; n<dbUserId.dwIdArrayItems ; n++ )
	{
		iResult = _tcsicmp(dbUserId.lpIdArray[n]->tszName, dbUserFake.lpIdArray[n]->tszName);
		if (iResult)
		{
			// TODO: could try to skip over missing or extra entry and continue...
			FormatString(lpBuffer, _TEXT("%sERROR: In memory User DB differs from file (%s).\r\n"), tszMultilinePrefix, tszUserTableName);
			if (bFix)
			{
				IdDataBase_Write(&dbUserId);
				bFixed = TRUE;
				dwErrCount++;
			}
			goto CLEANUP;
		}
		if (_tcsicmp(dbUserId.lpIdArray[n]->tszModuleName, dbUserFake.lpIdArray[n]->tszModuleName) ||
			(dbUserId.lpIdArray[n]->Id != dbUserFake.lpIdArray[n]->Id))
		{
			FormatString(lpBuffer, _TEXT("%sIn memory User DB differs from file (%s).\r\n"), tszMultilinePrefix, tszUserTableName);
			if (bFix)
			{
				IdDataBase_Write(&dbUserId);
				bFixed = TRUE;
				dwErrCount++;
			}
			goto CLEANUP;
		}

		lpParentUserFile = GetParentUserFile(dbUserId.lpIdArray[n]->Id);
		dwUids++;
		if (!_tcsicmp(dbUserId.lpIdArray[n]->tszModuleName, _T("STANDARD")))
		{
			dwFileUids++;
		}
		else
		{
			dwModuleUids++;
		}
		if (!lpParentUserFile)
		{
			FormatString(lpBuffer, _TEXT("%sWARNING: UserFileArray missing UID %d (%s) -- being deleted?\r\n"), tszMultilinePrefix, dbUserId.lpIdArray[n]->Id, dbUserId.lpIdArray[n]->tszName);
		}
	}
	FormatString(lpBuffer, _TEXT("%sUser count: %d (Standard: %d, Module: %d)\r\n"), tszMultilinePrefix, dwUids, dwFileUids, dwModuleUids);


	FormatString(lpBuffer, _TEXT("%sProcessing UserFileArray.\r\n"), tszMultilinePrefix);
	for( n=0 ; n < dwUserFileArrayItems ; n++ )
	{
		lpParentUserFile = lpUserFileArray[n];
		tszUserName = Uid2User(lpUserFileArray[n]->Uid);

		if (tszUserName)
		{
			bKnown = TRUE;
		}
		else
		{
			bKnown = FALSE;
			tszUserName = _T("<Unknown>");
		}

		if (lpParentUserFile->dwErrorFlags & WAIT_UNREGISTER)
		{
			FormatString(lpBuffer, _TEXT("%sINFO: User waiting to be deleted: UID=%d (%s)\r\n"), tszMultilinePrefix,
				lpParentUserFile->Uid, tszUserName);
		}
		if (lpParentUserFile->dwErrorFlags & WRITE_ERROR)
		{
			FormatString(lpBuffer, _TEXT("%sWARNING: User with write error: UID=%d (%s)\r\n"), tszMultilinePrefix,
				lpParentUserFile->Uid, tszUserName);
		}

		if (!bKnown && !(lpParentUserFile->dwErrorFlags & WAIT_UNREGISTER))
		{
			FormatString(lpBuffer, _TEXT("%sERROR: User known but not in DB: UID=%d\r\n"), tszMultilinePrefix,
				lpParentUserFile->Uid);
			dwErrCount++;
		}
	}


	tszUserFileName  = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("User_Files"), _T("\\Default.User"), NULL);
	if (!tszUserFileName) goto CLEANUP;
	// this can't fail since get_path sticks a \\ between result and appended text...
	tszParent  = _tcsrchr(tszUserFileName,  _T('\\'));

	*tszParent = 0;
	FormatString(lpBuffer, _TEXT("%sExamining user directory: %s\r\n"), tszMultilinePrefix, tszUserFileName);
	*tszParent = _T('\\');
	_tcscpy(tszParent+1, _T("*"));

	//  Begin directory search
	hFind  = FindFirstFile(tszUserFileName, &FindData);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (FindData.cFileName[0] == '.')
			{
				continue;
			}
			iResult = sscanf_s(FindData.cFileName, "%d%c", &Uid, &cTemp);
			if (iResult == 1)
			{
				for( n=0 ; n<dwUids ; n++ )
				{
					pIdItem = dbUserId.lpIdArray[n];
					if (pIdItem->Id == Uid)
					{
						if (_tcsicmp(pIdItem->tszModuleName, _T("STANDARD")))
						{
							FormatString(lpBuffer, _TEXT("%sERROR: Local user file found for module based user: UID=%d USERNAME=%s MODULE=\r\n"), tszMultilinePrefix,
								Uid, pIdItem->tszName, pIdItem->tszModuleName);
							dwErrCount++;
							if (bFix)
							{
								*tszParent = 0;
								_sntprintf_s(tszFileName, sizeof(tszFileName)/sizeof(*tszFileName), _TRUNCATE, _T("%s\\%s"), tszUserFileName, FindData.cFileName);
								*tszParent = _T('\\');
								DeleteFile(tszFileName);
								bFixed++;
							}
						}
						break;
					}
				}
				if (n >= dwUids)
				{
					FormatString(lpBuffer, _TEXT("%sERROR: Unreferenced userfile in directory: FILENAME=UID=%d\r\n"), tszMultilinePrefix, Uid);
					dwErrCount++;
					if (bFix)
					{
						*tszParent = 0;
						_sntprintf_s(tszFileName, sizeof(tszFileName)/sizeof(*tszFileName), _TRUNCATE, _T("%s\\%s"), tszUserFileName, FindData.cFileName);
						*tszParent = _T('\\');
						DeleteFile(tszFileName);
						bFixed++;
					}
				}
				continue;
			}
			if (!stricmp("Default.User", FindData.cFileName))
			{
				continue;
			}
			if (!strnicmp("Default=", FindData.cFileName, 8))
			{
				if (Group2Gid(&FindData.cFileName[8]) == -1)
				{
					dwErrCount++;
					if (bFix)
					{
						*tszParent = 0;
						_sntprintf_s(tszFileName, sizeof(tszFileName)/sizeof(*tszFileName), _TRUNCATE, _T("%s\\%s"), tszUserFileName, FindData.cFileName);
						*tszParent = _T('\\');
						DeleteFile(tszFileName);
						bFixed++;
					}
					FormatString(lpBuffer, _TEXT("%sERROR: Default user settings for a deleted group: FILENAME=%s\r\n"), tszMultilinePrefix, &FindData.cFileName[8]);
				}
				continue;
			}

			FormatString(lpBuffer, _TEXT("%sWARNING: Extra file in user directory: %s\r\n"), tszMultilinePrefix,
				FindData.cFileName);
		} while (FindNextFile(hFind, &FindData));

		dwError = GetLastError();
		if (dwError != ERROR_NO_MORE_FILES)
		{
			FormatString(lpBuffer, _TEXT("%sFindNextFile returned error: %E\r\n"), tszMultilinePrefix, dwError);
		}
		else
		{
			dwError = 0;
		}
		FindClose(hFind);
		if (dwError) goto CLEANUP;
	}


	FormatString(lpBuffer, _TEXT("%sValidating group references in userfiles.\r\n"), tszMultilinePrefix);

	pUserCount = (PINT32) Allocate("Admin_Verify:UserCount", dwGroupFileArrayItems * sizeof(INT32));
	if (!pUserCount) goto CLEANUP;
	ZeroMemory(pUserCount, dwGroupFileArrayItems * sizeof(INT32));


	for ( n=0 ; n<dwUserFileArrayItems ; n++ )
	{
	    lpParentUserFile = lpUserFileArray[n];
		if (lpParentUserFile->dwErrorFlags & WAIT_UNREGISTER)
		{
			continue;
		}
		// need to hold this lock in case somebody modified and unlocked a userfile which might
		// update the lpUserFile pointer in the parent.
		while (InterlockedExchange(&lpParentUserFile->lSecondaryLock, TRUE)) SwitchToThread();
		lpUserFile = lpParentUserFile->lpUserFile;

		for ( g=0 ; g<MAX_GROUPS && lpUserFile->Groups[g] != -1 ; g++ )
		{
			Gid = lpUserFile->Groups[g];
			lpParentGroupFile = 0;
			// can't use GetParentGroupFile here since we want the index in the array for later
			for ( m=0 ; m<dwGroupFileArrayItems ; m++ )
			{
				if (lpGroupFileArray[m]->Gid == Gid)
				{
					lpParentGroupFile = lpGroupFileArray[m];
					break;
				}
			}

			for ( a=0 ; a<dbGroupId.dwIdArrayItems ; a++ )
			{
				if (dbGroupId.lpIdArray[a]->Id == Gid)
				{
					// found the group
					break;
				}
			}
			if (a >= dbGroupId.dwIdArrayItems)
			{
				// group not found
				FormatString(lpBuffer, _TEXT("%sUser '%s' (UID=%d) is a member of non-existant group GID=%d.\r\n"),
					tszMultilinePrefix, Uid2User(lpUserFile->Uid), lpUserFile->Uid, Gid);
				dwErrCount++;
				continue;
			}
			if (!lpParentGroupFile || lpParentGroupFile->dwErrorFlags & WAIT_UNREGISTER)
			{
				FormatString(lpBuffer, _TEXT("%sUser '%s' (UID=%d) is a member of a group being deleted GID=%d (%s).\r\n"),
					tszMultilinePrefix, Uid2User(lpUserFile->Uid), lpUserFile->Uid, Gid, Gid2Group(Gid));
				dwErrCount++;
				continue;
			}
			pUserCount[m]++;
		}
		if (g == 0)
		{
			FormatString(lpBuffer, _TEXT("%sUser '%s' (UID=%d) is not a member of any groups.\r\n"),
				tszMultilinePrefix, Uid2User(lpUserFile->Uid), lpUserFile->Uid);
		}
		for ( g=0 ; g<MAX_GROUPS && lpUserFile->AdminGroups[g] != -1 ; g++ )
		{
			Gid = lpUserFile->AdminGroups[g];
			lpParentGroupFile = GetParentGroupFile(Gid);
			for ( a=0 ; a<dbGroupId.dwIdArrayItems ; a++ )
			{
				if (dbGroupId.lpIdArray[a]->Id == Gid)
				{
					// found the group
					break;
				}
			}
			if (a >= dbGroupId.dwIdArrayItems)
			{
				// group not found
				FormatString(lpBuffer, _TEXT("%sUser '%s' (UID=%d) is an admin of a non-existant group GID=%d.\r\n"),
					tszMultilinePrefix, Uid2User(lpUserFile->Uid), lpUserFile->Uid, Gid);
				dwErrCount++;
				continue;
			}
			if (!lpParentGroupFile || lpParentGroupFile->dwErrorFlags & WAIT_UNREGISTER)
			{
				FormatString(lpBuffer, _TEXT("%sUser '%s' (UID=%d) is an admin of a group being deleted GID=%d (%s).\r\n"),
					tszMultilinePrefix, Uid2User(lpUserFile->Uid), lpUserFile->Uid, Gid, Gid2Group(Gid));
				dwErrCount++;
			}
		}

		InterlockedExchange(&lpParentUserFile->lSecondaryLock, FALSE);
	}

	// now look in the actual groupfiles and compare user count information
	for ( n=0 ; n<dwGroupFileArrayItems ; n++ )
	{
		lpParentGroupFile = lpGroupFileArray[n];
		if (lpParentGroupFile->dwErrorFlags & WAIT_UNREGISTER)
		{
			continue;
		}

		// need to hold this lock in case somebody modified and unlocked a groupfile which might
		// update the lpUserFile pointer in the parent.
		while (InterlockedExchange(&lpParentGroupFile->lSecondaryLock, TRUE)) SwitchToThread();
		lpGroupFile = lpParentGroupFile->lpGroupFile;
		Gid = lpGroupFile->Gid;
		m = lpGroupFile->Users;
		InterlockedExchange(&lpParentGroupFile->lSecondaryLock, FALSE);

		if (pUserCount[n] != m)
		{
			FormatString(lpBuffer, _TEXT("%sGroup '%s' (GID=%d) has %d members but groupfile shows %d.\r\n"),
				tszMultilinePrefix, Gid2Group(Gid), Gid, pUserCount[n], m);
			dwErrCount++;

			if (bFix)
			{
				if (! GroupFile_OpenPrimitive(Gid, &lpGroupFile, 0))
				{
					if (! GroupFile_Lock(&lpGroupFile, 0))
					{
						//	Alter counters
						lpGroupFile->Users = pUserCount[n];
						GroupFile_Unlock(&lpGroupFile, 0);
						FormatString(lpBuffer, _TEXT("%sFIXED! Group '%s' (GID=%d) user count modified\r\n"),
							tszMultilinePrefix, Gid2Group(Gid), Gid);
						dwErrCount--;
					}
				}
				GroupFile_Close(&lpGroupFile, 0);
			}
		}
	}


CLEANUP:
	if (tszGroupTableName)Free(tszGroupTableName);
	if (tszGroupFileName) Free(tszGroupFileName);
	if (pUserCount) Free(pUserCount);
	if (tszUserTableName) Free(tszUserTableName);
	if (tszUserFileName) Free(tszUserFileName);

	ReleaseSharedLock(&dbGroupId.loDataBase);
	ReleaseSharedLock(&dbUserId.loDataBase);
	ReleaseSharedLock(&loGroupFileArray);
	ReleaseSharedLock(&loUserFileArray);

	dbGroupFake.tszFileName = NULL;
	IdDataBase_Free(&dbGroupFake);

	dbUserFake.tszFileName = NULL;
	IdDataBase_Free(&dbUserFake);

	InterlockedExchange(&dwVerifyBufferLock, FALSE);

	FormatString(lpBuffer, _TEXT("%s\r\n"), tszMultilinePrefix);
	if (!dwError && !dwErrCount) return NULL;
	if (dwError)
	{
		ERROR_RETURN(dwError, GetStringIndexStatic(Args, 0));
	}
	if (!bFix)
	{
		FormatString(lpBuffer, _TEXT("%sNOTICE: There were errors discovered.  Try 'fix' option.\r\n"), tszMultilinePrefix);
	}
	else if (bFixed)
	{
		FormatString(lpBuffer, _TEXT("%sNOTICE: There were errors discovered, a fix was attempted, re-run the command.\r\n"), tszMultilinePrefix);
	}
	else
	{
		FormatString(lpBuffer, _TEXT("%sNOTICE: There were errors discovered.  Manual intervention required.\r\n"), tszMultilinePrefix);
	}
	ERROR_RETURN(0, GetStringIndexStatic(Args, 0));
}


LPTSTR Admin_SectionNums(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPSECTION_INFO lpSection, lpPrev;
	LPTSTR         tszCommand, tszPath;
	LPBUFFER       lpBuffer;
	DWORD          n, dwError;
	INT            i;
	BOOL           bIsVfsAdmin, bSame;
	VIRTUALPATH    vpPath;
	TCHAR          PathBuffer[_MAX_PWD + 1];
	MOUNT_DATA     MountData;
	CHAR          *ShowFlags;
	

	lpBuffer	= &lpUser->CommandChannel.Out;

	tszCommand  = GetStringIndexStatic(Args, 0);

	bIsVfsAdmin = !HasFlag(lpUser->UserFile, _T("MV"));

	Config_Lock(&IniConfigFile, FALSE);

	ShowFlags = Allocate(_T("Sections-flags"), dwSectionInfoArray);
	if (!ShowFlags)
	{
		dwError = GetLastError();
		Config_Unlock(&IniConfigFile, FALSE);
		ERROR_RETURN(dwError, tszCommand);
	}
	ZeroMemory(ShowFlags, dwSectionInfoArray);

	FormatString(lpBuffer, _T("%sSection summary:\r\n"), tszMultilinePrefix);

	for(i=0 ; i<MAX_SECTIONS ; i++)
	{
		lpPrev   = NULL;

		for(n=0 ; n<dwSectionInfoArray ; n++)
		{
			lpSection = lpSectionInfoArray[n];
			if (lpSection->iStat != i) continue;
			if (lpPrev && !_tcsicmp(lpPrev->tszSectionName, lpSection->tszSectionName))
			{
				bSame = TRUE;
			}
			else
			{
				bSame = FALSE;
			}
			if (lpSection->tszPath[0] != _T('/'))
			{
				ShowFlags[n] = 1;
				continue;
			}

			if (!bIsVfsAdmin)
			{
				// path matches, but double check full paths are visible to the user requesting info
				// so we don't give out hidden paths.
				_tcscpy(PathBuffer, lpSection->tszPath);
				tszPath = &PathBuffer[1];
				while (*tszPath)
				{
					if ((*tszPath == _T('*')) || (*tszPath == _T('?')) || (*tszPath == _T('[')) || (*tszPath == _T(']')))
					{
						// found a wildcard, now back to last /
						while ((tszPath != PathBuffer) && (*tszPath != _T('/'))) tszPath--;
						tszPath[1] = 0;
						break;
					}
					tszPath++;
				}

				ZeroMemory(&MountData, sizeof(MountData));
				PWD_Reset(&vpPath);
				PWD_Set(&vpPath, "/");
				if (!PWD_CWD2(lpUser->UserFile, &vpPath, PathBuffer, lpUser->hMountFile, &MountData, TYPE_LINK, NULL, NULL, NULL))
				{
					dwError = GetLastError();
					// TODO: test for virtual dirs here?  if (MountData.Initialised && MountData.lpVirtualDirEvent)?
					// no access, or wasn't found
					continue;
				}
				PWD_Free(&vpPath);
			}
			ShowFlags[n] = 1;

			if (!bSame || !lpPrev || lpSection->iStat != lpPrev->iStat || lpSection->iShare != lpPrev->iShare || lpSection->iCredit != lpPrev->iCredit)
			{
				bSame = FALSE;
				FormatString(lpBuffer, _T("%s#%-2d: %-30s"), tszMultilinePrefix, lpSection->iStat, lpSection->tszSectionName);
				if (lpSection->iStat != lpSection->iShare)
				{
					FormatString(lpBuffer, _T(" (Credits shared with #%2d)"), lpSection->iShare);
				}
				else
				{
					FormatString(lpBuffer, _T("                          "), lpSection->iShare);
				}
				if (lpSection->iStat != lpSection->iCredit)
				{
					FormatString(lpBuffer, _T(" (Ratio same as #%2d)\r\n"), lpSection->iShare);
				}
				else
				{
					FormatString(lpBuffer, _T("\r\n"));
				}
			}
		}
	}

	FormatString(lpBuffer, _T("%s\r\n%sFirst matching path defines section:\r\n"), tszMultilinePrefix, tszMultilinePrefix);

	for(n=0 ; n<dwSectionInfoArray ; n++)
	{
		if (!ShowFlags[n])
		{
			continue;
		}
		lpSection = lpSectionInfoArray[n];
		FormatString(lpBuffer, _T("%s#%-2d: %s\r\n"), tszMultilinePrefix, lpSection->iStat, lpSection->tszPath);
	}

	FormatString(lpBuffer, _T("%s\r\n"), tszMultilinePrefix);

	Config_Unlock(&IniConfigFile, FALSE);
	return NULL;
}

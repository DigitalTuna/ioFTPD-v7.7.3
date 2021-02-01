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

typedef LPTSTR (* ADMINPROC)(LPFTPUSER, LPTSTR, LPIO_STRING);


typedef struct _ADMINCOMMAND
{
	LPTSTR	   tszCommand;
	ADMINPROC  lpProc;

} ADMINCOMMAND, * LPADMINCOMMAND;


typedef struct _CMD_PROGRESS
{
	LPCOMMAND   lpCommand;           // command struct for socket
	LPCLIENT    lpClient;            // pointer to user's client structure or NULL
	DWORD       dwTicks;             // tickcount to execute at
	DWORD       dwDelay;             // time before next call
    LPTSTR      tszMultilinePrefix;  // Prefix for line.
	LPTSTR      tszFormatString;     // String to output.  gets 3 args
	DWORD       dwArg1;              // first arg to pass to string
	DWORD       dwArg2;              // second arg to pass to string
	DWORD       dwArg3;              // third arg to pass to string
	DWORD       dwArg4;              // forth arg to pass to string

} CMD_PROGRESS, *LPCMD_PROGRESS;


typedef struct _ADMIN_SIZE
{
	LPUSERFILE     lpUserFile;
	DWORD          dwFileCount;
	DWORD          dwDirCount;
	UINT64         u64Size;
	DWORD          dwNoAccess;
	CMD_PROGRESS   Progress;

} ADMIN_SIZE, *LPADMIN_SIZE;

typedef struct _ADMIN_UPDATE
{
	LPUSERFILE    lpUserFile;
	VFSUPDATE     UpdateData;
	LPTSTR        tszWildcard;
	BOOL          bDirOnly;
	BOOL          bFileOnly;
	DWORD         dwAddModes;
	DWORD         dwRemoveModes;
	CMD_PROGRESS  Progress;

} ADMIN_UPDATE, *LPADMIN_UPDATE;


typedef struct _ACTION_TEST
{
	LPTSTR  tszAction;
	LPTSTR  tszOwnerAction;
	BOOL    bFileActivity;
	BOOL    bDirectoryActivity;
	BOOL    bSymbolicActivity;
	BOOL    bNoMount;
	DWORD   dwParentRequiredPerms;
	DWORD   dwRequiredPerms;

} ACTION_TEST, *LPACTION_TEST;


typedef struct _CHANGE_VARIABLES
{
	LPUSERFILE	lpAdmin;
	LPUSERFILE	lpTarget;
	LPTSTR		tszGroupName;
	LPGROUPFILE	lpGroupFile;
	IO_STRING	*Args;
	INT			Section;
	INT			Service;
	BOOL        bLocalAdmin;
	LPTSTR      tszMultilinePrefix;
	LPBUFFER    lpBuffer;
	BOOL        bDefault;

} CHANGE_VARIABLES;


typedef struct _change_cmd_table
{
	LPSTR	Trigger;
	DWORD	l_Trigger;
	BOOL	bUserCommand;
	BOOL    bDefaultCommand;
	INT		(*Command)(CHANGE_VARIABLES *);

} CHANGE_COMMAND;

extern CHANGE_COMMAND ChangeCommand[];


ADMINPROC FindAdminCommand(LPTSTR tszCommand, LPTSTR *ptszName);
LPTSTR Admin_ChangeFileAttributes(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_ChangeFileMode(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_ChangeFileOwner(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_UsersOnline(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Who(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Users(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_UserStats(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_UserInfo(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Change(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Config(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Kick(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_AddUser(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_AddUserToGroup(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_DeleteUser(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_RenameUser(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_AddGroup(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_DeleteGroup(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_RenameGroup(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_ChangeUserGroups(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_AddIp(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_DeleteIp(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_FindIp(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_SetOwnPassword(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_SetOwnTagline(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Close(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Open(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Shutdown(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Kill(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Groups(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_GroupInfo(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Bans(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_DirSize(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_FreeSpace(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_CreateSymLink(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Version(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Purge(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_ReAdd(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Services(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Devices(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Ciphers(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);

VOID RecursiveAction(LPUSERFILE lpUserFile, MOUNTFILE hMountFile, LPTSTR lpPath, BOOL bIgnoreMountPoints,
					 BOOL bIgnoreFiles, DWORD dwMaxDepth,
					 BOOL (* lpModifyProc)(PVIRTUALPATH, LPFILEINFO, LPFILEINFO, LPVOID),
					 LPVOID lpContext);

VOID PrettyPrintSize(LPTSTR lpBuf, DWORD dwBufLen, UINT64 u64Size);

BOOL Admin_SizeAdd(PVIRTUALPATH lpVPath, LPFILEINFO lpFileInfo, LPFILEINFO lpParentInfo,
				   LPADMIN_SIZE lpAdminSize);

VOID Progress_Update(LPCMD_PROGRESS lpProgress);

LPTSTR LookupUserName(LPUSERFILE lpUserFile);

BOOL IsLocalAdmin(LPUSERFILE lpUserFile, PCONNECTION_INFO lpConnectionInfo);

DWORD CheckForMasterAccount(LPFTPUSER lpUser, LPUSERFILE lpUserFile);


// Admin_DirCache and Admin_Refresh defined in DirectoryCache.c but prototyped here (LPFTPUSER defined by now)
LPTSTR Admin_DirCache(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
LPTSTR Admin_Refresh(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);

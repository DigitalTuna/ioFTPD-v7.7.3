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


#define	_WIN32_WINNT	0x0501
// #define	_WIN32_WINNT	0x0403
#include <Tchar.h>
#include <Winsock2.h>
#include <MsWsock.h>
#include <Windows.h>
#include <errno.h>

#include "openssl/bio.h"
#include "openssl/err.h"
#include "openssl/ssl.h"

#include <time.h>
#include <conio.h>
#include <stdarg.h>
#include <locale.h>
#include <shlwapi.h>

#include <Lists.h>
#include <IoMemory.h>
#include <IoError.h>
#include <IoResource.h>
#include <StdDef.h>
#include <ServerLimits.h>
#include <UserFile.h>
#include <GroupFile.h>
#include <Time.h>
#include <StdIo.h>
#include <IoString.h>
#include <VirtualPath.h>
#include <LockObject.h>
#include <Timer.h>
#include <Select.h>
#include <IoTime.h>
#include <IoOverlapped.h>
#include <IoFile.h>
#include <IoService.h>
#include <IoSocket.h>
#include <NewClient.h>
#include <Buffer.h>
#include <Identify.h>
#include <ConnectionInfo.h>
#include <IdDataBase.h>
#include <User.h>
#include <Group.h>
#include <Log.h>
#include <Job.h>
#include <DirectoryCache.h>
#include <ConfigReader.h>
#include <ControlConnection.h>
#include <DataConnection.h>
#include <NewList.h>
#include <Threads.h>
#include <FTP.h>
#include <WinMessages.h>
#include <Who.h>
#include <Event.h>
#include <Stats.h>
#include <IoProc.h>
#include <DataOffset.h>
#include <TimerProcedure.h>
#include <Scheduler.h>
#include <RowParser.h>
#include <Client.h>
#include <DataCopy.h>
#include <Array.h>
#include <MessageFile.h>
#include <AdminCommands.h>
#include <IoTcl.h>
#include <SHA1.h>


#define	CopyString	strcpy
#define HEAD	0
#define TAIL	1


#define DAEMON_ACTIVE	0
#define DAEMON_GRACE    1
#define DAEMON_SHUTDOWN	2

// ?
VOID CrashGuard_Remove(VOID);
BOOL CrashGuard_Wait(LPTSTR *lpCommandLine);
BOOL CrashGuard_Create(LPTSTR tszConfigFile);

#ifndef _UNICODE
#define _tmemchr memchr
#else
#define _tmemchr wmemchr
#endif



// Change.c
LPTSTR Admin_Change(LPFTPUSER lpUser,
					LPTSTR tszMultilinePrefix,
					LPIO_STRING Args);

// command.c
INT  HavePermission(LPUSERFILE lpUserFile, LPSTR szAccessList);
INT  CheckPermissions(LPSTR szUserName, PINT32 lpGroups,
					  LPSTR szUserFlags, LPSTR szAccessList);
BOOL HasFlag(LPUSERFILE lpUserFile, LPSTR szFlagList);
BOOL Upload(LPFTPUSER lpUser,
			PVIRTUALPATH CurrentPath,
			DATA *Data,
			DWORD dwClientId,
			LPUSERFILE lpUserFile,
			MOUNTFILE  hMountFile,
			LPTSTR tszUploadFileName,
			LPVOID Event,
			LPVOID EventParam);
BOOL Upload_Resume(LPFTPUSER lpUser,
				   PVIRTUALPATH CurrentPath,
				   DATA *Data,
				   DWORD dwClientId,
				   LPUSERFILE lpUserFile,
				   MOUNTFILE hMountFile,
				   LPTSTR tszUploadFileName,
				   LPVOID Event,
				   LPVOID EventParam);
LPSTR Rename_Prepare_From(LPFTPUSER lpUser,
						  PVIRTUALPATH CurrentPath,
						  LPUSERFILE lpUserFile,
						  MOUNTFILE hMountFile,
						  LPSTR File,
						  PINT pError);

PCHAR Rename_Prepare_To(LPFTPUSER lpUser,
						PVIRTUALPATH CurrentPath,
						LPUSERFILE lpUserFile,
						MOUNTFILE hMountFile,
						LPSTR File,
						PINT pError);
BOOL Delete_File(LPFTPUSER lpUser,
				 PVIRTUALPATH CurrentPath,
				 LPUSERFILE lpUserFile,
				 MOUNTFILE hMountFile,
				 LPSTR File,
				 PINT pError);

BOOL Create_Directory(LPFTPUSER lpUser,
					  PVIRTUALPATH CurrentPath,
					  LPUSERFILE lpUserFile,
					  MOUNTFILE hMountFile,
					  LPSTR tszDirectoryName,
					  LPVOID Event,
					  LPVOID EventParam,
					  PINT pError);
BOOL Remove_Directory(LPFTPUSER lpUser,
					  PVIRTUALPATH CurrentPath,
					  LPUSERFILE lpUserFile,
					  MOUNTFILE hMountFile,
					  LPTSTR tszDirectoryName,
					  LPVOID Event,
					  LPVOID EventParam,
					  PINT pError);
BOOL Event_Run(PCHAR Array,
                      PCHAR Event,
                      PCHAR Arguments,
                      PVOID Context,
                      INT ContextType,
                      PCHAR Prefix);
BOOL Login_First(PUSERFILE *pUserFile,
                        PCHAR Login,
                        PCONNECTION_INFO Connection,
                        BOOL Secure,
                        PINT Error);
BOOL Login_Second(PUSERFILE *pUserFile,
                         PCHAR Password,
                         COMMAND *CommandChan,
                         PCONNECTION_INFO Connection,
                         MOUNTFILE *lpMountFile,
                         PINT Error,
						 LPFTPUSER lpFtpUser);
BOOL User_CheckPassword(LPSTR szPassword,
						PUCHAR pHashedPassword);
BOOL LogLoginErrorP(LPHOSTINFO lpHostInfo, DWORD dwType);



// Compare.c
INT spCompare(LPSTR String1, LPSTR String2);
INT iCompare(LPSTR String1, LPSTR String2);
INT PathCompare(LPSTR String1, LPSTR String2);

// Crc32.c
DWORD crc32_combine(DWORD crc1, DWORD crc2, UINT64 u64Len2);

// Help.c
BOOL Help_Init(BOOL bFirstInitialization);
VOID Help_DeInit(VOID);
BOOL FTP_Help(LPFTPUSER lpUser, LPIO_STRING Args);
LPTSTR Admin_Help(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);

// InternalMessageHandler.c
DWORD ProcessMessages(VOID);
VOID SetDaemonStatus(DWORD dwStatus);
extern HANDLE hRestartHeartbeat;


// ioDebug.c
BOOL GetFileVersion(LPTSTR tszFilePath, DWORD *pdwMajor, DWORD *pdwMinor,
					DWORD *pdwRev, DWORD *pdwBuild);
HANDLE LoadLatestLibrary(LPTSTR tszDllName, LPTSTR tszExtraDir);
VOID EnableSymSrvPrompt(VOID);
VOID InitializeExceptionHandler(LPTSTR tszStartDir);
BOOL Debug_Init(BOOL bFirstInitialization);
VOID Debug_DeInit(VOID);
VOID LogStackTrace(LPTSTR tszError, ...);

// main.c
extern TCHAR    *tszExeName;
extern TCHAR     tszExePath[MAX_PATH];
extern DWORD     dwIoVersion[3];
extern volatile DWORD dwDaemonStatus;
extern UINT64    u64WindowsStartTime;
extern UINT64    u64FtpStartTime;
BOOL InitializeDaemon(BOOL bFirstInitialization);
VOID NotifyServiceOfShutdown();


// NewList.c - here since LPFTPUSER not defined when newlist is included.
BOOL FTP_List(LPFTPUSER lpUser, IO_STRING *Args);
BOOL FTP_Nlist(LPFTPUSER lpUser, IO_STRING *Args);
BOOL FTP_Stat(LPFTPUSER lpUser, IO_STRING *Args);
BOOL FTP_MLSD(LPFTPUSER lpUser, IO_STRING *Args);
BOOL List_RunSearch(LPFTPUSER lpUser, LPTSTR tszVirtual, LPTSTR tszSearchFor);
VOID FTP_ContinueListing(LPFTPUSER lpUser);
VOID FTP_AbortListing(LPFTPUSER lpUser);

// WinErrors.c
LPWSTR FormatError(DWORD dwError, LPWSTR wszBuffer, LPDWORD lpBufferLenght);

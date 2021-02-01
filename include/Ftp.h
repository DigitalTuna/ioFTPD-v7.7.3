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


#define MOUNTFILE_USERFILE    1
#define MOUNTFILE_GROUPFILE   2
#define MOUNTFILE_DEFAULT     3


typedef struct _FTP_VARIABLES
{
	VIRTUALPATH vpRenameFrom;
	BOOL        bRenameDir;     // TRUE if renaming a directory, else a file
	BOOL        bNoPath;        // TRUE if rename is a simple rename and not a move
	BOOL        bLoginRetry;    // TRUE when retrying USER command because "OnUnknownLogin" may have created user
	BOOL        bHaveUserCmd;   // TRUE if successful USER command issued
	TCHAR       tszUserName[_MAX_NAME+1];  // Name of user at login time
	VIRTUALPATH vpLastUpload;
	DWORD       dwLastUploadUpTime;
	BOOL        bValidCRC;
	DWORD       dwLastCrc32;
	BOOL        bComputeCrc;
	LPTSTR      tszClientType;
	INT         iTheme;
	LPOUTPUT_THEME lpTheme;
	BOOL        bKeepLinksInPath;
	INT         iPos;
	INT         iMax;
	LPIOADDRESSLIST lpDenyPortAddressList; // my local copy from services so I don't have to lock it each time...
	LPTSTR      tszCurrentCommand;
	DWORD       dwConfigCounter;

	DWORD       dwUidList;     // size of array of all UIDs
	PINT32      lpUidList;     // list of all UIDs from last match operation
	PINT32      lpUidMatches;  // array of size dwUidList with non-matching entries set to -1

	BOOL        bSingleLineMode;      // "-" prefix to password supplied.
	DWORD       dwMountFileFrom;      // one of MOUNTFILE_* above

	// Must hold the CLIENT lock to access this!
	LPTSTR      tszMsgStringArray[MAX_MESSAGES];

} FTP_VARIABLES;


typedef struct _FTP_USER
{
  CONNECTION_INFO Connection;     /* Connection info */
  LPUSERFILE      UserFile;       /* Userfile */
  COMMAND         CommandChannel; /* CommandChannel */
  FTP_DATA        DataChannel;    /* FTP DataChannel */
  MOUNTFILE       hMountFile;     /* Handle to mount file */
  FTP_VARIABLES   FtpVariables;   /* Ftp Variables */
  LPLISTING       Listing;        /* For LIST -R command */

} FTP_USER, FTPUSER, *LPFTPUSER;


typedef struct _FTPCOMMAND
{
  LPTSTR tszName;
  DWORD  dwFlags;
  LONG   lArgMin;
  LONG   lArgMax;
  BOOL   (*lpProc)(LPFTPUSER, LPIO_STRING);
} FTPCOMMAND, *LPFTPCOMMAND;


struct _cmd_table
{
  PCHAR trigger;
  CHAR  trigger_len;
  CHAR  cmd_type;

  INT   min_argc;
  INT   max_argc;
  BOOL  (*cmd)(LPFTPUSER, LPIO_STRING);
};

#define SHUTDOWN_DEFAULT_GRACE  300
#define SHUTDOWN_TIMER_DELAY    5000

typedef struct _FTP_SETTINGS
{
  volatile DWORD  dwSocketBuffer[2];
  volatile DWORD  dwDataSocketBuffer[2];
  volatile DWORD  dwTransferBuffer;
  volatile DWORD  dwIdleTimeOut;
  volatile DWORD  dwLoginTimeOut;
  volatile DWORD  dwLoginAttempts;
  volatile BOOL   bTransmitFile;
  volatile BOOL   bNagle;
  volatile BOOL   bHideXferHost;
  volatile BOOL   bNoSubDirSizing;
  volatile BOOL   bComputeCRCs;
  volatile BOOL   bShowHostMaskError;

  volatile LONG   lStringLock; // must hold this lock to access the strings below!
  volatile LPTSTR tszSiteName;
  volatile LPTSTR tszSiteBoxTheme;
  volatile LPTSTR tszSiteBoxHeader;
  volatile DWORD   dwSiteBoxHeader;
  volatile LPTSTR tszSiteBoxFooter;
  volatile DWORD   dwSiteBoxFooter;
  volatile LPTSTR tszHelpBoxTheme;
  volatile LPTSTR tszHelpBoxHeader;
  volatile DWORD   dwHelpBoxHeader;
  volatile LPTSTR tszHelpBoxFooter;
  volatile DWORD   dwHelpBoxFooter;
  volatile LPTSTR tszAllowedRecursive;
  volatile LPTSTR tszIdleIgnore;
  volatile LPTSTR tszIdleExempt;
  volatile LPTSTR tszCloseExempt;
  volatile LPTSTR tszCloseMsg;

  volatile TCHAR  tBannedFlag;
  volatile TCHAR  tQuietLoginFlag;

  volatile BOOL     dwShutdownLock;
  volatile DWORD    dwShutdownTimeLeft;
  volatile DWORD    dwShutdownCID;
  volatile INT32    ShutdownUID;
  volatile LPTIMER  lpShutdownTimer;

  volatile BOOL     bServerStartingUp;
  volatile time_t   tmSiteClosedOn;
  volatile INT32    iSingleCloseUID;  // close to ALL (including exempt) except for UID
  volatile INT32    iServerSingleExemptUID;
  volatile BOOL     bKickNonExemptOnClose;

  volatile BOOL    bKeepLinksInPath;
  volatile BOOL    bOnlineDataExtraFields;
  volatile BOOL    bEnableTimeStampOnLastUpload;
  volatile DWORD   dwChmodCheck; // 0 = default, 1 = WriteOnly, 2 = None
  volatile BOOL    bWhoSortOutput;
  volatile DWORD   dwFtpDataTimeout;

} FTP_SETTINGS, * LPFTP_SETTINGS;


// FtpBaseCommands.c
DWORD FTP_TimerProc(LPFTPUSER lpUser, LPTIMER lpTimer);
BOOL  FTP_Command(LPFTPUSER lpUser);
VOID  MaybeDisplayStatus(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix);

// FtpDataChannel.c
BOOL  FTP_Data_Init_Transfer(LPFTPUSER lpUser, LPSTR szFileName);
BOOL  FTP_Data_Begin_Transfer(LPFTPUSER lpUser);
BOOL  FTP_Data_Close(LPFTPUSER lpUser);
BOOL  FTP_Data_Start(LPFTPUSER lpUser);


// FtpServer.c
extern FTP_SETTINGS  FtpSettings;
BOOL FTP_Init(BOOL bFirstInitialization);
VOID FTP_DeInit(VOID);
BOOL FTP_Close_Connection(LPFTPUSER lpUser);
VOID FTP_Cancel(LPFTPUSER lpUser);
VOID FTP_AcceptInput(LPFTPUSER lpUser);
VOID FTP_SSLAccept(LPFTPUSER lpUser, DWORD dwLastError, INT64 i64Total, ULONG ulSslError);
VOID FTP_ReceiveLine(LPFTPUSER lpUser, LPSTR szCommand, DWORD dwLastError, ULONG ulSslError);
VOID FTP_SendReply(LPFTPUSER lpUser, DWORD dwLastError, INT64 i64Total, ULONG ulSslError);
BOOL Download_Complete(PUSERFILE *pUserFile, INT64 Credits, INT64 Bytes,
					   DWORD Time, INT CreditSection, INT StatsSection, INT ShareSection);
BOOL Upload_Complete(PUSERFILE *pUserFile, INT64 Bytes, DWORD Time,
					 INT CreditSection, INT StatsSection, INT ShareSection);
BOOL FTP_New_Client(PCONNECTION_INFO lpConnection);

// FtpSiteCommands.c
BOOL FTP_AdminCommands(LPFTPUSER lpUser, IO_STRING *Args);


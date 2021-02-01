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

/*

  USHORT	Type

  TYPE 0 (String):

  USHORT	Length
  CHAR		String

  TYPE 1 (End of line):
  TYPE 2 (End of line - no prefix to next line):

  TYPE 5 - 155

  UCHAR		Prefix Length
  CHAR      *Prefix
  UCHAR		Parameters

  USHORT	Length
  CHAR		*Parameter

  ...

  USHORT	Length N
  CHAR		*Parameter N


  TYPE 200 - 254

  UCHAR		Prefix Length
  CHAR		*Prefix

  TYPE 255 (EOF)

  */

#define STRING			0
#define NEWLINE			1
#define PREFIX			2
#define NOPREFIX		3
#define ZERO			4
#define	OBJECT			5
#define	FILEPREFIX		6
#define	VARIABLE		200
#define THEEND			255

#define SERVICE_NAME				1
#define SERVICE_ACTIVE_STATE		2
#define SERVICE_HOST_IP             3
#define SERVICE_HOST_PORT           4
#define SERVICE_DEVICE_NAME			5
#define SERVICE_DEVICE_ID			6
#define SERVICE_USERS				7
#define SERVICE_MAX_CLIENTS			8
#define	SERVICE_TRANSFERS			9
#define SERVICE_ALLOWED_USERS		10
#define SERVICE_CERTIFICATE_IN_USE	11
#define SERVICE_CERTIFICATE_WHERE	12
#define SERVICE_ENCRYPTION_TYPE     13
#define SERVICE_REQUIRE_SECURE_AUTH	14
#define SERVICE_REQUIRE_SECURE_DATA	15
#define SERVICE_EXTERNAL_IDENTITY	16
#define SERVICE_MESSAGE_LOCATION	17
#define SERVICE_DATA_DEVICES		18
#define SERVICE_DATA_DEVICE_SELECTION 19
#define SERVICE_ACCEPTS_READY		20

#define DEVICE_NAME					1
#define DEVICE_ID					2
#define DEVICE_ACTIVE_STATE			3
#define DEVICE_BIND_IP				4
#define DEVICE_HOST_IP				5
#define DEVICE_PASV_PORTS			6
#define DEVICE_OUT_PORTS			7
#define DEVICE_OUT_RANDOMIZE		8
#define DEVICE_BW_GLOBAL_OUT_LIMIT	9
#define DEVICE_BW_GLOBAL_IN_LIMIT	10
#define DEVICE_BW_CLIENT_OUT_LIMIT	11
#define DEVICE_BW_CLIENT_IN_LIMIT	12


#define	GROUP_DESCRIPTION	1
#define	GROUP_USERSLOTS		2
#define	GROUP_LEECHSLOTS	3
#define	GROUP_USERS			4
#define GROUP_MOUNTFILE		5
#define GROUP_DEFAULTFILE   6


#define	CONVERT_INTEGER			0001L
#define CONVERT_SUFFIX			0002L
#define CONVERT_GROUPFILE		0004L
#define CONVERT_SERVICE			0400L
#define CONVERT_FILE			0100L
#define CONVERT_STATS			0200L
#define CONVERT_WHO				0010L

#define C_INTEGER				0
#define C_STRING				1
#define C_INTEGER_VARIABLE		2
#define C_STRING_VARIABLE		3
#define C_FLOAT_VARIABLE		4
#define C_UNKNOWN_VARIABLE		5
#define C_ARGS					0100000L


#define B_COMMAND			   0x1
#define B_DATA				   0x2
#define B_USERFILE			   0x8
#define B_GROUPFILE			  0x10
#define B_USERFILE_PLUS       0x20
#define B_CONNECTION		  0x40
#define B_MOUNTTABLE		     0     // =0
#define B_FILE				 0x200
#define B_WHO				 0x400
#define B_STATS				 0x800
#define B_ANY				0x1000
#define B_FTPUSER           0x2000


#define WHO_SPEED_CURRENT		2
#define WHO_SPEED_UPLOAD		3
#define WHO_SPEED_DOWNLOAD		4
#define WHO_SPEED_TOTAL			5
#define WHO_ACTION				6
#define WHO_FILESIZE			7
#define WHO_VIRTUALPATH			8
#define WHO_VIRTUALDATAPATH		9
#define WHO_PATH				10
#define WHO_DATAPATH			11
#define WHO_TRANSFERS_UPLOAD	12
#define WHO_TRANSFERS_DOWNLOAD	13
#define WHO_TRANSFERS_TOTAL		14
#define WHO_USERS_TOTAL			15
#define WHO_IDLERS_TOTAL		16
#define WHO_LOGIN_TIME          17
#define WHO_LOGIN_HOURS			18
#define WHO_LOGIN_MINUTES		19
#define WHO_LOGIN_SECONDS		20
#define WHO_IDLE_TIME			21
#define WHO_IDLE_HOURS			22
#define WHO_IDLE_MINUTES		23
#define WHO_IDLE_SECONDS		24
#define WHO_HOSTNAME			25
#define WHO_IP					26
#define WHO_CONNECTION_ID		27
#define	WHO_IDENT				28
#define WHO_SERVICENAME			29
#define WHO_HIDDEN              30
#define WHO_VIRTUALDATAFILE     31
#define WHO_MY_CONNECTION_ID	32

#define WHO_LEFT_ALIGN			1
#define WHO_RIGHT_ALIGN			2

typedef struct _USERFILE_PLUS
{
	LPUSERFILE lpUserFile;
	LPFTPUSER  lpFtpUserCaller;
	LPCOMMAND  lpCommandChannel;
	INT32      iGID;

} USERFILE_PLUS, *LPUSERFILE_PLUS;


// NOTE: These are used in DataOffset.c to pick out the right fields for InitDataOffsets
#define DT_FTPUSER			1
#define DT_USERFILE			2
#define DT_GROUPFILE		3
#define DT_STATS			4
#define DT_WHO				5
#define DT_USERFILE_PLUS    6
#define DT_CONNECTION       7


typedef struct _ARGUMENT_LIST
{

	BYTE					pBuffer[8];
	PCHAR					pArgument;
	DWORD					dwArgument;
	struct _ARGUMENT_LIST	*lpNext;

} ARGUMENT_LIST, * LPARGUMENT_LIST;





typedef struct _MESSAGEDATA
{
	LPSTR			szFormat;
	DWORD			dwFormat;

	LPVOID			lpData;
	DWORD			dwData;
	DATA_OFFSETS	DataOffsets;

	THEME_FIELD     CurrentThemes;
	THEME_FIELD     SavedThemes;
	LPOUTPUT_THEME  lpSavedTheme;
	DWORD           dwMarkPosition;
	DWORD           dwTempValue;

	LPSTR			szPrefix[2];
	DWORD			dwPrefix[2];

	LPBUFFER		lpOutBuffer;

} MESSAGEDATA, * LPMESSAGEDATA;





typedef struct _MESSAGE_CACHE
{

	LPSTR			szFileName;
	DWORD			dwFileName;
	FILETIME		ftCacheTime;
	DWORD           dwBufLen;
	CHAR           *pBuffer;

	struct _MESSAGE_CACHE	*lpNext;
	struct _MESSAGE_CACHE	*lpPrev;

} MESSAGE_CACHE, * LPMESSAGE_CACHE;


typedef struct _ARG_PROC
{
	LPVOID				lpProc;
	struct _ARG_PROC	*lpNext;

} ARG_PROC, * LPARG_PROC;


typedef struct _MESSAGE_VARIABLE
{
	LPTSTR		tszName;
	DWORD		dwName;
	BOOL		bArgs;
	LPARG_PROC	lpArgProc;
	LPVOID		AllocProc;
	LPVOID		FreeProc;
	DWORD		dwRequiredData;
	DWORD		dwType;

} MESSAGE_VARIABLE, * LPMESSAGE_VARIABLE;



typedef struct _OBJV
{
	LPVOID	lpMemory;
	LPVOID	FreeProc;
	PBYTE	pValue;

} OBJV, * LPOBJV;



typedef struct _VARIABLE_MODULE
{
	LPSTR					szName;
	HMODULE					hModule;
	LPVOID					(* GetProc)(LPSTR);
	BOOL					(* InstallMessageVariable)(LPTSTR, LPVOID, LPVOID, DWORD, DWORD, ...);

	struct _VARIABLE_MODULE	*lpNext;

} VARIABLE_MODULE, *LPVARIABLE_MODULE;




BOOL Object_Get_Int(LPMESSAGEDATA lpData, LPOBJV lpObject, LPINT lpValue);
LPSTR Object_Get_String(LPMESSAGEDATA lpData, LPOBJV lpObject);
BOOL TextFile_Show(LPSTR szFileName, LPBUFFER lpOutBuffer, LPSTR szPrefix);
BOOL MessageFile_Show(LPSTR szFileName, LPBUFFER lpOutBuffer, LPVOID lpData, DWORD dwData, LPSTR szPrefix, LPSTR szLastPrefix);
BOOL Message_Compile(LPBYTE pBuffer, LPBUFFER lpOutBuffer, BOOL bSkipFirstPrefix, LPVOID lpData, DWORD dwData, LPSTR szPrefix, LPSTR szLastPrefix);
VOID Compile_Message(LPMESSAGEDATA lpData, LPBYTE lpBuffer, BOOL bInline, BOOL bSkipFirstPrefix);
LPBYTE Message_PreCompile(PCHAR lpBuffer, LPDWORD lpOutSize);
LPBYTE Message_Load(LPSTR szFileName);
WORD FindMessageVariable(LPSTR szName, DWORD dwName, BOOL bArgs, LPBYTE lpType);
BOOL InstallMessageVariable(LPSTR szName, LPVOID lpAllocProc, LPVOID lpFreeProc, DWORD dwRequiredData, DWORD dwType, ...);
LPVOID GetVariable(WORD wIndex);

BOOL Message_Init(BOOL bFirstInitialization);
VOID Message_DeInit(VOID);
BOOL MessageObjects_Init(VOID);
BOOL MessageObject_Precompile(LPBUFFER lpOutBuffer, PCHAR pData, DWORD dwData, PCHAR pPrefix, BYTE bPrefix);
DWORD MessageObject_Compile(LPMESSAGEDATA lpData, LPBYTE lpBuffer);
BOOL MessageObject_SetSubTheme(LPTSTR tszSubTheme);

BOOL MessageVariables_Init(VOID);
VOID MessageVariables_DeInit(VOID);
BOOL MessageVariable_Precompile(LPBUFFER lpBuffer, LPSTR szName,
								DWORD dwName, LPSTR szFormat, BYTE bFormat);

USHORT MessageVariable_Compile(LPMESSAGEDATA lpData, LPBYTE lpBuffer);

LPOUTPUT_THEME LookupTheme(DWORD n);
INT Parse_ThemeField(LPTHEME_FIELD lpField, LPTSTR tszString);
LPOUTPUT_THEME Parse_Theme(LPOUTPUT_THEME lpInTheme, INT iTheme, LPTSTR tszSubTheme);
LPTSTR Admin_Color(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);

extern volatile DWORD dwNumThemes;
extern LPTSTR  tszLeechName;
extern volatile DWORD lThemeLock;

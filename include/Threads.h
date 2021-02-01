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

typedef struct _JOB
{
	LPVOID		lpProc;	//	Pointer for function to call
	LPVOID		lpContext;	//
	DWORD		dwFlags;

	struct _JOB	*lpNext;

} JOB, * LPJOB;


typedef struct _RESOURCE_DTOR
{
	VOID	(*lpProc)(VOID);
	struct _RESOURCE_DTOR	*lpNext;	

} RESOURCE_DTOR, *LPRESOURCE_DTOR; 


typedef union {
	struct {
		unsigned doFormat      : 1; // a formatter field
		unsigned doReset       : 1;
		unsigned doInverse     : 1;
		unsigned hasUnderLine  : 1;
		unsigned Underline     : 1;
		unsigned Italic        : 1;
		unsigned padding       : 10; // make last 2 fields be char aligned and fill int
		unsigned Foreground    : 8;
		unsigned Background    : 8;
	} Settings;
	struct {
		unsigned doFormat      : 1;
		unsigned Format        : 31;
	} Formatter;
	int i; // for quickly setting it to 0
} THEME_FIELD, *LPTHEME_FIELD;


typedef struct _OUTPUT_THEME
{
	INT32         iTheme;
	TCHAR         tszName[_MAX_NAME+1];
	THEME_FIELD   ThemeFieldsArray[MAX_COLORS+1]; // index 0 is SubThemeDefault
} OUTPUT_THEME, *LPOUTPUT_THEME;


typedef struct _THREADDATA
{
	BOOL				bReuse;
	HANDLE				hEvent;
	LONG volatile       lBlockingCount;
	LPOUTPUT_THEME      lpTheme;
	INT32               iAutoTheme;
	struct _FTP_USER   *lpFtpUser;
	struct _THREADDATA *lpNext, *lpPrev;
	LPRESOURCE_DTOR		lpDestructor;

} THREADDATA, *LPTHREADDATA;





#define THREAD_STATUS_DELAYED	0x00000001



#define JOB_PRIORITY_HIGH	0
#define JOB_PRIORITY_NORMAL	1
#define JOB_PRIORITY_LOW	2
#define JOB_PRIORITY_DELAYED	3
#define JOB_FLAG_NOFIBER	(0x0001 << 16)


#define SSPI_ENCRYPT	0
#define SSPI_DECRYPT	1
#define CRC32			2



BOOL SetBlockingThreadFlag(VOID);
VOID SetNonBlockingThreadFlag(VOID);
BOOL InstallResourceDestructor(VOID (* lpProc)(VOID));
HANDLE GetThreadEvent(VOID);
BOOL QueueJob(LPVOID lpProc, LPVOID lpContext, DWORD dwPriority);
BOOL Thread_Init(BOOL bFirstInitialization);
DWORD CalculateCrc32(PCHAR pOffset, DWORD dwBytes, PUINT32 pCrc32);
VOID Thread_DeInit(VOID);

extern HANDLE	hCompletionPort;

extern unsigned int crc32_table[256];


void SetTheme(LPOUTPUT_THEME lpTheme);
LPOUTPUT_THEME GetTheme(VOID);

void SetAutoTheme(INT32 iTheme);
INT32 GetAutoTheme();

VOID SetFtpUser(struct _FTP_USER *lpFtpUser);
struct _FTP_USER *GetFtpUser(VOID);
VOID AcquireHandleLock(VOID);
VOID ReleaseHandleLock(VOID);

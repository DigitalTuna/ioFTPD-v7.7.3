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

typedef struct _LOG_VECTOR
{
	DWORD				dwLogCode;
	FILETIME			FileTime;
	TCHAR				lpMessage[LOG_LENGTH];
	DWORD				dwMessageLength;
	struct _LOG_VECTOR	*lpNext;
	struct _LOG_VECTOR	*lpPrevious;

} LOG_VECTOR, * LPLOG_VECTOR;

#define LOG_SYSOP			0
#define LOG_ERROR			1
#define LOG_TRANSFER		2
#define LOG_GENERAL			3
#define LOG_DEBUG			4
#define LOG_SYSTEM			5
#define LOG_TOTAL_TYPES     (LOG_SYSTEM+1)

BOOL LogSystem_Init(BOOL bFirstInitialization);
VOID LogSystem_DeInit(VOID);
BOOL LogSystem_Queue(BOOL bFirstInitialization);
VOID LogSystem_NoQueue(VOID);
VOID LogSystem_Flush(VOID);

BOOL Putlog(DWORD dwLogCode, LPCSTR szFormatString, ...);
BOOL PutlogVA(DWORD dwLogCode, LPCTSTR tszFormatString, va_list Arguments);

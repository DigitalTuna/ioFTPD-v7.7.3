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

#define W_ANY       -1
#define	W_IDLE		0
#define W_LIST		1
#define W_UPLOAD	2
#define W_DOWNLOAD	3
#define W_LOGIN		4
#define W_NONE      5




typedef struct _IO_WHO
{
	//
	// Per connection information
	//
	DWORD			dwConnectionId;     // Index into lpClientSlot array

	ONLINEDATA      OnlineData;         // Copy of Client's static fields

	DWORD           dwMyCID;            // Callers connection ID
	DWORD           dwMyUID;            // Callers user ID

	LPUSERFILE		lpUserFile;

	INT64			i64FileSize;
	DOUBLE			fTransferSpeed;

	DWORD			dwLoginHours;
	DWORD			dwLoginMinutes;
	DWORD			dwLoginSeconds;

	DWORD			dwIdleHours;
	DWORD			dwIdleMinutes;
	DWORD			dwIdleSeconds;



	//
	// Total information across all users
	//
	DOUBLE			fTotalUpSpeed;
	DOUBLE			fTotalDnSpeed;

	DWORD			dwUploads;
	DWORD			dwDownloads;
	DWORD			dwUsers;


} IO_WHO, * LPIO_WHO;


typedef struct _SORT_WHO
{
	DWORD   dwNameIndex;
	DWORD   dwLineIndex;
	DWORD   dwLineLen;

} SORT_WHO, *LPSORT_WHO;

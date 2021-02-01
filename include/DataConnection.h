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

typedef struct _DATA
{
	IOFILE				IoFile;
	IOSOCKET			ioSocket;
	struct sockaddr_in	Address;				//	Address of client in datasocket
	DWORD				dwLastError;			//	Last error
	ULONG               ulSslError;             //  OpenSSL error if dwLastError = IO_SSL_FAIL

	VIRTUALPATH			File;
	INT64				Charged;				// Kilobytes charged from transfer
	INT64				Size;					// Size of last transfer
	INT64               FileSize;               // Size of file
	DWORD				dwResumeOffset[2];

	UCHAR				bActive				: 1;
	UCHAR				bInitialized		: 1;
	UCHAR				bProtected			: 1;
	UCHAR				bProtectedConnect	: 1;

	UCHAR				bEncoding		: 1;
	UCHAR				bTransferMode	: 1;
	UCHAR				bDirection		: 1;
	UCHAR				bSpecial		: 1;
	UCHAR				bFree			: 1;

	BOOL volatile		bAbort;

	TIME_STRUCT			Start;
	TIME_STRUCT			Stop;
	DWORD               dwDuration;

	BUFFER				Buffer;

} DATA, FTP_DATA, * PDATACHANNEL, DATACHANNEL, * LPDATACHANNEL;


#define	ASCII	TRUE
#define	BINARY	FALSE
#define	ACTIVE	TRUE
#define PASSIVE	FALSE
#define SEND	TRUE
#define RECEIVE	FALSE
#define LIST	TRUE


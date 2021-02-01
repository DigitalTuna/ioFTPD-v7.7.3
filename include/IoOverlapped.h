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

#define OVERLAPPED_TYPE_GENERIC		  0
#define OVERLAPPED_TYPE_FILE		  1
#define OVERLAPPED_TYPE_SOCKET_READ	  2
#define OVERLAPPED_TYPE_SOCKET_WRITE  3

//	Used with file routines
typedef struct _FILEOVERLAPPED
{
	ULONG_PTR	Internal;
	ULONG_PTR	InternalHigh;
	DWORD		Offset;
	DWORD		OffsetHigh;
	HANDLE		hEvent;

	VOID	    (* lpProc)(LPVOID, DWORD, DWORD);
	LPVOID		lpNext;
	LPVOID	    lpContext;
	volatile LONG lIdentifier;

	//	File specific information
	DWORD		dwCommand; // operation requested FILE_READ or FILE_WRITE 
	LPVOID		lpBuffer;  // buffer to read/write from, used when request had to be queued when too many requests to device
	DWORD		dwBuffer;  // size of buffer, used when request had to be queued when too many requests to device
	BOOL        bDoCrc;    // compute CRC?
	DWORD		Crc32;     // CRC value so far
	struct _IOFILE *hFile;  // file pointer...

} FILEOVERLAPPED, *LPFILEOVERLAPPED;

typedef struct _SOCKETOVERLAPPED
{
	ULONG_PTR	Internal;
	ULONG_PTR	InternalHigh;
	DWORD		Offset;
	DWORD		OffsetHigh;
	HANDLE		hEvent;

	VOID	    (* lpProc)(LPVOID, DWORD, DWORD);
	LPVOID	    lpNext;
	LPVOID	    lpContext;
	volatile LONG lIdentifier;

	//	Socket specific information
	WSABUF		Buffer;
	struct _IOSOCKET *hSocket;

} SOCKETOVERLAPPED, *LPSOCKETOVERLAPPED;


typedef struct _IOOVERLAPPED
{
	ULONG_PTR	Internal;
	ULONG_PTR	InternalHigh;
	DWORD		Offset;
	DWORD		OffsetHigh;
	HANDLE		hEvent;

	VOID	    (* lpProc)(LPVOID, DWORD, DWORD);
	LPVOID	    lpNext;
	LPVOID	    lpContext;
	volatile LONG ldentifier;

} IOOVERLAPPED, *LPIOOVERLAPPED;

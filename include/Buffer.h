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

typedef struct _BUFFER
{
	DWORD				len;	// Bytes in buffer
	DWORD				size;	// Size of buffer
	CHAR				*buf;	// Pointer to buffer
	DWORD				dwType;

} BUFFER, * LPBUFFER;

#define TYPE_CHAR		0
#define TYPE_MULTICHAR	1
#define TYPE_WIDE		2


VOID Put_Buffer_Format(LPBUFFER lpBuffer, LPCSTR szFormat, ...);
VOID Put_Buffer(LPBUFFER lpBuffer, LPVOID lpMemory, DWORD dwSize);
VOID Insert_Buffer(LPBUFFER lpBuffer, LPVOID lpMemory, DWORD dwSize);

BOOL FormatStringWVA(LPBUFFER lpBuffer, LPWSTR wszFormat, va_list Arguments);
BOOL FormatStringW(LPBUFFER lpBuffer, LPWSTR wszFormat, ...);
BOOL FormatStringAVA(LPBUFFER lpBuffer, LPCSTR szFormat, va_list Arguments);
BOOL FormatStringA(LPBUFFER lpBuffer, LPCSTR szFormat, ...);
DWORD aswprintf(LPTSTR *lpBuffer, LPCSTR tszFormat, ...);
#define AppendStringA	Put_Buffer
#define Put_Buffer_Format	FormatStringA

BOOL AllocateBuffer(LPBUFFER lpBuffer, DWORD dwMinimum);

#ifdef _UNICODE
#define FormatString FormatStringW
#else
#define FormatString FormatStringA
#endif

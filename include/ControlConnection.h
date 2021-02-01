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

typedef struct _COMMAND
{
	IOSOCKET			Socket;
	CHAR				Action[64];

	INT					ErrorCode;
	BOOL				TooLong;
	INT					Prefix;
	VIRTUALPATH			Path;

	LPSTR				Command;
	DWORD				Length;

	DWORD				Idle;
	BUFFER				In;
	BUFFER				Out;

} COMMAND, * LPCOMMAND;


#define	U_LOGIN			0000L
#define U_IDENTIFIED	0001L
#define U_LOGOFF		0002L
#define U_MAKESSL		0010L
#define	U_KILL			0020L
#define U_IDENT			0100L
#define U_FXPSSLCLIENT  0200L

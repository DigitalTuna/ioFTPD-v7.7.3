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

typedef struct _GROUPFILE
{
	INT		Gid;					// Group Id
	INT		Slots[2];				// # of slots left for this group
	INT		Users;					// # of users in this group
	CHAR	szDescription[128 + 1];	// Long description
	CHAR	szVfsFile[MAX_PATH + 1];	// Default VFS file

	LPVOID	lpInternal;
	LPVOID	lpParent;

} GROUPFILE, * LPGROUPFILE;

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

#define MAX_SECTIONS		25
#define MAX_GROUPS			128
#define MAX_IPS				25

#define	MAX_GID				262144
#define	MAX_UID				1048576

#define MAX_LOGS	100
#define LOG_LENGTH	512

#define	MEMORY_BUCKETS			128
#define MAX_LARGE_ALLOCATIONS	10000
#define MAX_MEMORY_IN_BUCKETS	4194304


#define NOGROUP_ID			1
#define DEFAULT_BUF_SIZE	2048

#define _MAX_NAME			64
#define _INI_LINE_LENGTH	1024
#define _IP_LINE_LENGTH		96
#define MAX_HOSTNAME		96
#define MAX_IDENT			64

#define _MAX_PWD		512
// directory listing code uses a bitmask so MAX_SUBMOUNTS <= sizeof(DWORD)-1
#define	MAX_SUBMOUNTS	31
#define MAX_CLIENTS     16384

#define SSL_TIMEOUT					60000
#define SSL_CLOSE_TIMEOUT			1000
#define FTP_LOGOUT_TIMEOUT			1000
#define FTP_DATA_CONNECT_TIMEOUT	20000
#define FTP_DATA_ACCEPT_TIMEOUT		20000
#define FTP_DATA_TIMEOUT			120000
#define MAX_FTP_COMMAND_LINE_LENGTH	1024
#define	SEND_TIMEOUT				120000

// number of milliseconds before transfer speed should be considered zero.
#define ZERO_SPEED_DELAY            10000

#define MAX_THEMES					20
#define MAX_COLORS					150
#define MAX_MESSAGES				5

#define MAX_VIRTUAL_DIR_MOUNTPOINTS 20

#define MAX_HELP_FILES				10

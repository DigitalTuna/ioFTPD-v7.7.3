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

#define MAX_DELETE_USER_MSG	64

typedef struct _USERFILE
{
  INT   Uid;                                /* User id */
  INT   Gid;                                /* User group id */

  CHAR  Tagline[128 + 1];                   /* Info line */
  CHAR  MountFile[_MAX_PATH + 1];           /* Root directory */
  CHAR  Home[_MAX_PATH + 1];                /* Home directory */
  CHAR  Flags[32 + 1];                      /* Flags */
  INT   Limits[5];                          /*  Up max speed, dn max speed,
                                                ftp logins, telnet, http */

  UCHAR  Password[20];                      /* Password */

  INT    Ratio[MAX_SECTIONS];               /* Ratio */
  INT64  Credits[MAX_SECTIONS];             /* Credits */

  INT64  DayUp[MAX_SECTIONS * 3];           /* Daily uploads */
  INT64  DayDn[MAX_SECTIONS * 3];           /* Daily downloads */
  INT64  WkUp[MAX_SECTIONS * 3];            /* Weekly uploads */
  INT64  WkDn[MAX_SECTIONS * 3];            /* Weekly downloads */
  INT64  MonthUp[MAX_SECTIONS * 3];         /* Monthly uploads */
  INT64  MonthDn[MAX_SECTIONS * 3];         /* Monthly downloads */
  INT64  AllUp[MAX_SECTIONS * 3];           /* Alltime uploads */
  INT64  AllDn[MAX_SECTIONS * 3];           /* Alltime downloads */

  INT    AdminGroups[MAX_GROUPS];           /* Admin for these groups */
  INT    Groups[MAX_GROUPS];                /* List of groups */
  CHAR   Ip[MAX_IPS][_IP_LINE_LENGTH + 1];  /* List of ips */

  INT    CreatorUid;                        /* uid of user who created account */
  CHAR   CreatorName[_MAX_NAME + 1];        /* name of creating user, in case deleted */
  INT64  CreatedOn;                         /* time account created */
  INT    LogonCount;                        /* number of successful logins */
  INT64  LogonLast;                         /* time of last successful login */
  CHAR   LogonHost[MAX_HOSTNAME + 1];       /* name/IP of host of last successful login */
  INT    MaxUploads;                        /* number of simultaneous uploads
											   -1 = unlimited, 0 = no uploads */
  INT    MaxDownloads;                      /* number of simultaneous downloads
											   -1 = unlimited, 0 = no downloads */
  INT    LimitPerIP;                        /* number of logs to account via same-ip */
  INT64  ExpiresAt;                         /* time after which account invalid */
  INT64  DeletedOn;                         /* When user was deleted, 0 means not deleted */
  INT    DeletedBy;                         /* UID who deleted this user if DeletedOn not 0 */
  CHAR   DeletedMsg[MAX_DELETE_USER_MSG + 1];/* <msg> argument to site deluser */
  INT    Theme;                             /* site color theme preference, 0 = off */
  CHAR   Opaque[257];                       /* for scripts, bots, etc */

  LPVOID lpInternal;                        /* handle reference */
  LPVOID lpParent;                          /* pointer to parent userfile */
} USERFILE, *PUSERFILE, *LPUSERFILE;


#define MAX_OLD_SECTIONS 10

typedef struct _USERFILE_OLD
{
	INT   Uid;                                /* User id */
	INT   Gid;                                /* User group id */

	CHAR  Tagline[128 + 1];                   /* Info line */
	CHAR  MountFile[_MAX_PATH + 1];           /* Root directory */
	CHAR  Home[_MAX_PATH + 1];                /* Home directory */
	CHAR  Flags[32 + 1];                      /* Flags */
	INT   Limits[5];                          /*  Up max speed, dn max speed,
											  ftp logins, telnet, http */

	UCHAR  Password[20];                      /* Password */

	INT    Ratio[MAX_OLD_SECTIONS];               /* Ratio */
	INT64  Credits[MAX_OLD_SECTIONS];             /* Credits */

	INT64  DayUp[MAX_OLD_SECTIONS * 3];           /* Daily uploads */
	INT64  DayDn[MAX_OLD_SECTIONS * 3];           /* Daily downloads */
	INT64  WkUp[MAX_OLD_SECTIONS * 3];            /* Weekly uploads */
	INT64  WkDn[MAX_OLD_SECTIONS * 3];            /* Weekly downloads */
	INT64  MonthUp[MAX_OLD_SECTIONS * 3];         /* Monthly uploads */
	INT64  MonthDn[MAX_OLD_SECTIONS * 3];         /* Monthly downloads */
	INT64  AllUp[MAX_OLD_SECTIONS * 3];           /* Alltime uploads */
	INT64  AllDn[MAX_OLD_SECTIONS * 3];           /* Alltime downloads */

	INT    AdminGroups[MAX_GROUPS];           /* Admin for these groups */
	INT    Groups[MAX_GROUPS];                /* List of groups */
	CHAR   Ip[MAX_IPS][_IP_LINE_LENGTH + 1];  /* List of ips */

	LPVOID lpInternal;
	LPVOID lpParent;
} USERFILE_OLD, *LPUSERFILE_OLD;

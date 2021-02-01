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

#define IO_BASE                   20000


/*        New Errors        */

#define IOERRORADMINGROUPSFULL    (IO_BASE + 20)
#define IOERRORGROUPDOESNOTEXIST  (IO_BASE + 6)


/*        Old Errors        */




#define IO_INVALID_ARGUMENTS      (IO_BASE + 1)
#define IO_NO_ACCESS              (IO_BASE + 2)
#define IO_GADMIN_EMPTY           (IO_BASE + 10)
#define IO_NOT_GADMIN             (IO_BASE + 11)
#define IO_NO_SLOTS               (IO_BASE + 15)

#define IO_INVALID_ARRAY          (IO_BASE + 50)
#define IO_INVALID_LINE           (IO_BASE + 51)

#define IO_ENCRYPTION_REQUIRED    (IO_BASE + 80)

#define IO_APPEND_SCRIPT          (IO_BASE + 100)
#define IO_STORE_SCRIPT           (IO_BASE + 101)
#define IO_NEWDIR_SCRIPT          (IO_BASE + 102)
#define IO_LOGIN_SCRIPT           (IO_BASE + 103)
#define IO_LOGOUT_SCRIPT          (IO_BASE + 104)

#define IO_NO_ACCESS_PATHCHECK    (IO_BASE + 200)
#define IO_NO_ACCESS_PRIVATE      (IO_BASE + 201)
#define IO_INVALID_FILENAME       (IO_BASE + 202)
#define IO_NO_ACCESS_FXP_IN       (IO_BASE + 203)
#define IO_NO_ACCESS_FXP_OUT      (IO_BASE + 204)
#define IO_NO_CREDITS             (IO_BASE + 205)
#define IO_SSL_FAIL               (IO_BASE + 206)
#define IO_NO_ACCESS_VFS          (IO_BASE + 207)
#define IO_MAX_DOWNLOADS          (IO_BASE + 208)
#define IO_MAX_UPLOADS            (IO_BASE + 209)
#define IO_BAD_FXP_ADDR           (IO_BASE + 210)
#define IO_NO_ACCESS_INI          (IO_BASE + 211)
#define IO_VIRTUAL_DIR            (IO_BASE + 212)
#define IO_SCRIPT_FAILURE         (IO_BASE + 213)
#define IO_SSL_FAIL2              (IO_BASE + 214)



#define IO_TRANSFER_ABORTED       (IO_BASE + 500)
#define IO_TRANSFER_CLOSING       (IO_BASE + 501)
#define IO_TRANSFER_TIMEOUT       (IO_BASE + 502)


#undef ERROR_GROUP_NOT_FOUND
#undef ERROR_USER_EXISTS
#undef ERROR_GROUP_EXISTS

#define ERROR_ID_NOT_FOUND        (IO_BASE + 600)
#define ERROR_MODULE_CONFLICT     (IO_BASE + 601)
#define ERROR_INVALID_IDNAME      (IO_BASE + 602)
#define ERROR_ID_EXISTS           (IO_BASE + 603)
#define ERROR_USER_NOT_FOUND      (IO_BASE + 605)
#define ERROR_GROUP_NOT_FOUND     (IO_BASE + 606)
#define ERROR_MODULE_NOT_FOUND    (IO_BASE + 607)
#define ERROR_USER_EXISTS         (IO_BASE + 608)
#define ERROR_GROUP_EXISTS        (IO_BASE + 609)
#define ERROR_INVALID_ARGUMENTS   (IO_BASE + 610)
#define ERROR_MISSING_ARGUMENT    (IO_BASE + 611)
#define ERROR_GROUP_LOCK_FAILED   (IO_BASE + 612)
#define ERROR_USER_LOCK_FAILED    (IO_BASE + 613)
#define ERROR_USERNAME            (IO_BASE + 614)
#define ERROR_GROUPNAME           (IO_BASE + 615)
#define ERROR_NO_ACTIVE_DEVICE    (IO_BASE + 616)
#define ERROR_USER_NOT_ONLINE     (IO_BASE + 617)
#define ERROR_SERVICE_LOGINS      (IO_BASE + 618)
#define ERROR_CLASS_LOGINS        (IO_BASE + 619)
#define ERROR_IP_LOGINS           (IO_BASE + 620)
#define ERROR_USER_LOGINS         (IO_BASE + 621)
#define ERROR_PASSWORD            (IO_BASE + 622)
#define ERROR_CLIENT_HOST         (IO_BASE + 623)
#define ERROR_MASTER              (IO_BASE + 624)
#define ERROR_GROUP_NOT_EMPTY     (IO_BASE + 625)
#define ERROR_FILESEEK            (IO_BASE + 626)
#define ERROR_INVALID_MOUNT_ENTRY (IO_BASE + 630)
#define ERROR_INVALID_FILEMODE    (IO_BASE + 631)
#define ERROR_USER_EXPIRED        (IO_BASE + 632)
#define ERROR_USER_DELETED        (IO_BASE + 633)
#define ERROR_USER_NOT_DELETED    (IO_BASE + 634)
#define ERROR_SITEOP              (IO_BASE + 635)
#define ERROR_SERVER_CLOSED       (IO_BASE + 636)
#define ERROR_ALREADY_CLOSED      (IO_BASE + 637)
#define ERROR_ALREADY_OPEN        (IO_BASE + 638)
#define ERROR_USER_IP_LOGINS      (IO_BASE + 639)
#define ERROR_COMMAND_FAILED      (IO_BASE + 640)
#define ERROR_SHUTTING_DOWN       (IO_BASE + 641)
#define ERROR_BAD_THEME           (IO_BASE + 642)
#define ERROR_NOT_MODIFIED        (IO_BASE + 643)
#define ERROR_DIRECTORY_LOCKED    (IO_BASE + 644)
#define ERROR_IDENT_FAILURE       (IO_BASE + 645)
#define ERROR_SERVICE_NAME_NOT_FOUND (IO_BASE + 646)
#define ERROR_DEVICE_NAME_NOT_FOUND  (IO_BASE + 647)
#define ERROR_FILE_MISSING        (IO_BASE + 648)
#define ERROR_NO_HELP             (IO_BASE + 649)
#define ERROR_NOGROUP             (IO_BASE + 650)
#define ERROR_SCRIPT_MISSING      (IO_BASE + 651)
#define ERROR_CLOSED_SOCKET       (IO_BASE + 652)
#define ERROR_MATCH_LIST          (IO_BASE + 653)
#define ERROR_VFS_FILE            (IO_BASE + 654)
#define ERROR_HOME_DIR            (IO_BASE + 655)
#define ERROR_USER_BANNED         (IO_BASE + 656)
#define ERROR_STARTING_UP         (IO_BASE + 657)
#define ERROR_SERVER_SINGLE       (IO_BASE + 658)
#define ERROR_TCL_VERSION         (IO_BASE + 659)
#define ERROR_TRANSFER_TIMEOUT    (IO_BASE + 660)
#define ERROR_INVALID_FS_TARGET   (IO_BASE + 661)


#define ERROR_RETURN(ERROR, RETURN) { SetLastError(ERROR); return RETURN; }

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

#include <windows.h>
#include <stdio.h>
#include <IoError.h>


#define WERROR(wszError, dwLength, wszErrorString) \
	dwLength	= sizeof(wszErrorString) / sizeof(WCHAR) - sizeof(CHAR); \
	wszError	= wszErrorString;



LPWSTR FormatError(DWORD dwError, LPWSTR wszBuffer, LPDWORD lpBufferSize)
{
	INT     iValue;
	DWORD	dwReturn;

	switch (dwError)
	{
	case ERROR_DEV_NOT_EXIST:
	case ERROR_PATH_NOT_FOUND:
	case ERROR_FILE_NOT_FOUND:
	case ERROR_INVALID_DRIVE:
	case IO_NO_ACCESS_PRIVATE:
		WERROR(wszBuffer, dwReturn, L"No such file or directory");
		break;
	case IO_VIRTUAL_DIR:
		WERROR(wszBuffer, dwReturn, L"Modifying virtual directories is not allowed");
		break;
	case ERROR_DIRECTORY:
		WERROR(wszBuffer, dwReturn, L"Not a directory");
		break;
	case ERROR_NOT_EMPTY:
		WERROR(wszBuffer, dwReturn, L"Directory not empty");
		break;
	case ERROR_WRITE_PROTECT:
		WERROR(wszBuffer, dwReturn, L"Permission denied");
		break;
	case ERROR_ALREADY_EXISTS:
		WERROR(wszBuffer, dwReturn, L"Directory/File already exists");
		break;
	case ERROR_HANDLE_DISK_FULL:
		WERROR(wszBuffer, dwReturn, L"Device full, contact system administrator");
		break;
	case ERROR_INVALID_NAME:
		WERROR(wszBuffer, dwReturn, L"Invalid filename");
		break;
	case IO_APPEND_SCRIPT:
	case IO_STORE_SCRIPT:
	case IO_NEWDIR_SCRIPT:
		WERROR(wszBuffer, dwReturn, L"Action blocked by external script");
		break;
	case WSAEWOULDBLOCK:
	case WSAEINPROGRESS:
	case WSAETIMEDOUT:
		WERROR(wszBuffer, dwReturn, L"Connection timed out");
		break;
	case WSAENOTSOCK:
		WERROR(wszBuffer, dwReturn, L"Data socket not connected");
		break;

	case IO_NO_ACCESS:
		WERROR(wszBuffer, dwReturn, L"Permission denied");
		break;
	case IO_NO_ACCESS_VFS:
		WERROR(wszBuffer, dwReturn, L"Permission denied (directory mode)");
		break;
	case IO_NO_ACCESS_INI:
		WERROR(wszBuffer, dwReturn, L"Permission denied (config file)");
		break;
	case IO_INVALID_FILENAME:
		WERROR(wszBuffer, dwReturn, L"Invalid filename");
		break;
	case IO_NO_ACCESS_FXP_IN:
		WERROR(wszBuffer, dwReturn, L"FXP to path is not allowed");
		break;
	case IO_NO_ACCESS_FXP_OUT:
		WERROR(wszBuffer, dwReturn, L"FXP from path is not allowed");
		break;
	case IO_BAD_FXP_ADDR:
		WERROR(wszBuffer, dwReturn, L"Transfer to specified network address is not allowed");
		break;
	case IO_NO_CREDITS:
		WERROR(wszBuffer, dwReturn, L"Insufficient credits left to perform requested action");
		break;
	case IO_SSL_FAIL:
		WERROR(wszBuffer, dwReturn, L"SSL library returned a failure code");
		break;
	case IO_SSL_FAIL2:
		WERROR(wszBuffer, dwReturn, L"SSL failure");
		break;
	case IO_MAX_DOWNLOADS:
		WERROR(wszBuffer, dwReturn, L"Maximum concurrent downloads for account reached");
		break;
	case IO_MAX_UPLOADS:
		WERROR(wszBuffer, dwReturn, L"Maximum concurrent uploads for account reached");
		break;


	case IO_ENCRYPTION_REQUIRED:
		WERROR(wszBuffer, dwReturn, L"Your user class requires you to use secure connections");
		break;
	case IO_TRANSFER_ABORTED:
		WERROR(wszBuffer, dwReturn, L"Transfer aborted");
		break;
	case IO_INVALID_ARGUMENTS:
		WERROR(wszBuffer, dwReturn, L"Invalid arguments");
		break;
	case IO_GADMIN_EMPTY:
		WERROR(wszBuffer, dwReturn, L"No group admin rights found");
		break;
	case IO_NOT_GADMIN:
		WERROR(wszBuffer, dwReturn, L"User is not in your admin groups");
		break;
	case IO_NO_SLOTS:
		WERROR(wszBuffer, dwReturn, L"Group's limit has been reached");
		break;

	case ERROR_GROUP_NOT_EMPTY:
		WERROR(wszBuffer, dwReturn, L"Group is not empty");
		break;
	case ERROR_NO_MATCH:
		WERROR(wszBuffer, dwReturn, L"No results for search term");
		break;
	case ERROR_GROUPNAME:
		WERROR(wszBuffer, dwReturn, L"Invalid name for group");
		break;
	case ERROR_USERNAME:
		WERROR(wszBuffer, dwReturn, L"Invalid name for user");
		break;
	case ERROR_MODULE_CONFLICT:
		WERROR(wszBuffer, dwReturn, L"Module ownership conflict");
		break;
	case ERROR_SERVICE_NAME_NOT_FOUND:
		WERROR(wszBuffer, dwReturn, L"Service name does not exist");
		break;
	case ERROR_DEVICE_NAME_NOT_FOUND:
		WERROR(wszBuffer, dwReturn, L"Device name does not exist");
		break;
	case ERROR_MODULE_NOT_FOUND:
		WERROR(wszBuffer, dwReturn, L"Required module is not loaded");
		break;
	case ERROR_USER_NOT_FOUND:
		WERROR(wszBuffer, dwReturn, L"User does not exist");
		break;
	case ERROR_GROUP_NOT_FOUND:
		WERROR(wszBuffer, dwReturn, L"Group does not exist");
		break;
	case ERROR_GROUP_EXISTS:
		WERROR(wszBuffer, dwReturn, L"Group already exists");
		break;
	case ERROR_USER_EXISTS:
		WERROR(wszBuffer, dwReturn, L"User already exists");
		break;
	case ERROR_USER_LOCK_FAILED:
		WERROR(wszBuffer, dwReturn, L"User locking failed");
		break;
	case ERROR_GROUP_LOCK_FAILED:
		WERROR(wszBuffer, dwReturn, L"Group locking failed");
		break;
	case ERROR_LOCK_FAILED:
		WERROR(wszBuffer, dwReturn, L"File locking failed");
		break;
	case ERROR_INVALID_ARGUMENTS:
		WERROR(wszBuffer, dwReturn, L"Syntax error in parameters or arguments");
		break;
	case ERROR_MISSING_ARGUMENT:
		WERROR(wszBuffer, dwReturn, L"Not enough parameters");
		break;
	case ERROR_NO_ACTIVE_DEVICE:
		WERROR(wszBuffer, dwReturn, L"Service has no active devices");
		break;
	case ERROR_USER_NOT_ONLINE:
		WERROR(wszBuffer, dwReturn, L"User is not online");
		break;
	case ERROR_PASSWORD:
		WERROR(wszBuffer, dwReturn, L"Invalid password");
		break;
	case ERROR_SERVICE_LOGINS:
		WERROR(wszBuffer, dwReturn, L"Service is full, try again later");
		break;
	case ERROR_CLASS_LOGINS:
		WERROR(wszBuffer, dwReturn, L"Maximum concurrent connections for ip-class reached, try again later");
		break;
	case ERROR_IP_LOGINS:
		WERROR(wszBuffer, dwReturn, L"Maximum concurrent connections for single host reached");
		break;
	case ERROR_USER_IP_LOGINS:
		WERROR(wszBuffer, dwReturn, L"Maximum concurrent connections for account from a single host reached");
		break;
	case ERROR_USER_LOGINS:
		WERROR(wszBuffer, dwReturn, L"Maximum concurrent connections for account reached, try again later");
		break;
	case ERROR_MASTER:
		WERROR(wszBuffer, dwReturn, L"Cannot target Master account");
		break;
	case ERROR_SITEOP:
		WERROR(wszBuffer, dwReturn, L"Cannot target Master or SiteOp account");
		break;
	case ERROR_FILESEEK:
		WERROR(wszBuffer, dwReturn, L"Seek offset exceeds end of file");
		break;
	case ERROR_INVALID_FILEMODE:
		WERROR(wszBuffer, dwReturn, L"Invalid filemode");
		break;
	case ERROR_USER_EXPIRED:
		WERROR(wszBuffer, dwReturn, L"Account has expired");
		break;
	case ERROR_USER_BANNED:
		WERROR(wszBuffer, dwReturn, L"Account has been temporarily banned");
		break;
	case ERROR_USER_DELETED:
		WERROR(wszBuffer, dwReturn, L"Account has been deleted");
		break;
	case ERROR_USER_NOT_DELETED:
		WERROR(wszBuffer, dwReturn, L"User not marked for deletion or account hasn't expired");
		break;
	case ERROR_HOME_DIR:
		WERROR(wszBuffer, dwReturn, L"Error with home directory");
		break;
	case ERROR_VFS_FILE:
		WERROR(wszBuffer, dwReturn, L"Error with VFS file");
		break;
	case ERROR_ALREADY_CLOSED:
		WERROR(wszBuffer, dwReturn, L"Server already closed");
		break;
	case ERROR_ALREADY_OPEN:
		WERROR(wszBuffer, dwReturn, L"Server already open");
		break;
	case ERROR_SERVER_CLOSED:
	case ERROR_SERVER_SINGLE:
		WERROR(wszBuffer, dwReturn, L"Server is closed");
		break;
	case ERROR_SHUTTING_DOWN:
		WERROR(wszBuffer, dwReturn, L"Server is shutting down");
		break;
	case ERROR_STARTING_UP:
		WERROR(wszBuffer, dwReturn, L"Server is starting up");
		break;
	case ERROR_COMMAND_FAILED:
		WERROR(wszBuffer, dwReturn, L"Command failed");
		break;
	case ERROR_CLIENT_HOST:
		WERROR(wszBuffer, dwReturn, L"Your IP/hostname is not authorized");
		break;
	case ERROR_IDENT_FAILURE:
		WERROR(wszBuffer, dwReturn, L"Your user ident response did not match");
		break;
	case ERROR_BAD_THEME:
		WERROR(wszBuffer, dwReturn, L"Invalid/Missing theme");
		break;
	case ERROR_NOT_MODIFIED:
		WERROR(wszBuffer, dwReturn, L"Not modified");
		break;
	case ERROR_DIRECTORY_LOCKED:
		WERROR(wszBuffer, dwReturn, L"Directory being moved or deleted");
		break;
	case IO_SCRIPT_FAILURE:
		WERROR(wszBuffer, dwReturn, L"Fatal script error");
		break;
	case ERROR_FILE_MISSING:
		WERROR(wszBuffer, dwReturn, L"File missing");
		break;
	case ERROR_NO_HELP:
		WERROR(wszBuffer, dwReturn, L"No help found");
		break;
	case ERROR_NOGROUP:
		WERROR(wszBuffer, dwReturn, L"Cannot delete default group (GID #1)");
		break;
	case ERROR_SCRIPT_MISSING:
		WERROR(wszBuffer, dwReturn, L"Cannot find script");
		break;
	case ERROR_CLOSED_SOCKET:
		WERROR(wszBuffer, dwReturn, L"Failed send/recv on already closed socket");
		break;
	case ERROR_MATCH_LIST:
		WERROR(wszBuffer, dwReturn, L"No previously saved user matches");
		break;
	case ERROR_TCL_VERSION:
		WERROR(wszBuffer, dwReturn, L"Error with TCL, .dll or /lib version mismatch");
		break;
	case ERROR_TRANSFER_TIMEOUT:
		WERROR(wszBuffer, dwReturn, L"Data transfer timeout");
		break;
	case ERROR_INVALID_FS_TARGET:
		WERROR(wszBuffer, dwReturn, L"Invalid filesystem target");
		break;

	default:
		//	Default error handler
		dwReturn	= FormatMessageW(FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, dwError, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), wszBuffer, lpBufferSize[0]-1, NULL);
		//	Check result
		if (dwReturn)
		{
			//	Remove carriage feed
			if (wszBuffer[dwReturn - 1] == L'\n') dwReturn--;
			if (wszBuffer[dwReturn - 1] == L'\r') dwReturn--;
			if (wszBuffer[dwReturn - 1] == L'.') dwReturn--;
			//	Add zero padding
			wszBuffer[dwReturn]	= L'\0';
			break;
		}

		// ok, no english for the above error, try local language but append (#) to end...
		dwReturn	= FormatMessageW(FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, dwError, 0, wszBuffer, lpBufferSize[0]-1, NULL);
		if (dwReturn)
		{
			//	Remove carriage feed
			if (wszBuffer[dwReturn - 1] == L'\n') dwReturn--;
			if (wszBuffer[dwReturn - 1] == L'\r') dwReturn--;
			if (wszBuffer[dwReturn - 1] == L'.') dwReturn--;
			//	Add zero padding
			wszBuffer[dwReturn]	= L'\0';
			iValue = _snwprintf(&wszBuffer[dwReturn], lpBufferSize[0]-dwReturn-1, L" (#%u)", dwError);
			if (iValue > 0)
			{
				dwReturn += iValue;
			}
			wszBuffer[dwReturn]	= L'\0';
			break;
		}

		dwReturn = 0;
		iValue = _snwprintf(wszBuffer, lpBufferSize[0], L"Unknown error (%u)", dwError);
		if (iValue > 0)
		{
			dwReturn = iValue;
		}
		wszBuffer[dwReturn]	= L'\0';
	}
	lpBufferSize[0]	= dwReturn;
	return wszBuffer;
}

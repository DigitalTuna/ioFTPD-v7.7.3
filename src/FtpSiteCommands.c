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

#include <ioFTPD.h>






BOOL FTP_AdminCommands2(LPFTPUSER lpUser, LPIO_STRING Args, BOOL bSubCmd)
{
	EVENT_COMMAND	Event;
	LPTSTR			tszCommand, tszTemp;
	IO_STRING		Alias;
	ADMINPROC		lpProc;
	LPBUFFER		lpBuffer;
	TCHAR			*pLine, *pCheck;
	TCHAR			pBuffer[_INI_LINE_LENGTH + 1], tszPrePostCmd[_INI_LINE_LENGTH + 1];
	LPTSTR			tszResult, tszEvent, tszTrueEvent, tszPrettyName;
	BOOL			bReturn, bExternal, bInternal, bOverride;
	INT				iPos;
	DWORD			dwLastError, n;
	TCHAR           tszArgs[_INI_LINE_LENGTH + 1];

	tszCommand	  = GetStringIndexStatic(Args, 0);
	lpBuffer	  = &lpUser->CommandChannel.Out;
	bExternal	  = FALSE;

	// this shouldn't be possible as SITE commands can't be called before login, but just in case...
	if (!lpUser->UserFile)
	{
		FormatString(lpBuffer, _TEXT("550 '%5TSITE %s%0T': %2TAccess denied.%0T\r\n"), tszCommand);
		return TRUE;
	}

	if (Config_Get_Permission2(&IniConfigFile, _TEXT("FTP_SITE_Permissions"), tszCommand, lpUser->UserFile) && _tcsicmp(tszCommand, _T("change")))
	{
		iPos = 0;
		if (Config_Get(&IniConfigFile, _TEXT("FTP_Custom_Commands"), tszCommand, pBuffer, &iPos) || FindAdminCommand(tszCommand, NULL))
		{
			FormatString(lpBuffer, _TEXT("550 '%5TSITE %s%0T': %2TAccess denied.%0T\r\n"), tszCommand);
			return TRUE;
		}
		FormatString(lpBuffer, _TEXT("500 '%5TSITE %s%0T': %2TCommand not understood.%0T\r\n"), tszCommand);
		return TRUE;
	}
	bReturn = FALSE;

	_sntprintf_s(tszPrePostCmd, sizeof(tszPrePostCmd)/sizeof(*tszPrePostCmd), _TRUNCATE, "@%s", tszCommand);

	// because some internal site commands can trash Args, it's necessary to make a copy here...
	if (tszTemp = GetStringRange(Args, 1, STR_END))
	{
		Event.tszParameters		= tszArgs;
		_tcsncpy_s(tszArgs, sizeof(tszArgs)/sizeof(*tszArgs), tszTemp, _TRUNCATE);
	}
	else
	{
		Event.tszParameters		= NULL;
	}
	Event.tszCommand		= pBuffer;
	Event.lpOutputBuffer	= &lpUser->CommandChannel.Out;
	Event.lpOutputSocket	= &lpUser->CommandChannel.Socket;
	Event.lpDataSource		= lpUser;
	Event.dwDataSource		= DT_FTPUSER;
	Event.tszOutputPrefix	= _TEXT("200-");

	iPos	= 0;
	while (Config_Get(&IniConfigFile, _TEXT("FTP_Pre-Command_Events"), tszPrePostCmd, pBuffer, &iPos))
	{
		bReturn	= RunEvent(&Event);

		if (bReturn)
		{
			FormatString(lpBuffer, _TEXT("500 %2TCommand failed.%0T\r\n"));
			return TRUE;
		}
	};

	//	External command/alias
	bOverride = FALSE;
	iPos	= 0;
	dwLastError = NO_ERROR;
	tszEvent	= Config_Get(&IniConfigFile, _TEXT("FTP_Custom_Commands"), tszCommand, pBuffer, &iPos);
	if (tszEvent)
	{
		bExternal	= TRUE;
		PushString(Args, 1);
		do
		{
			tszTrueEvent = tszEvent;
			if (tszTrueEvent[0] == _T('^'))
			{
				bOverride = TRUE;
				tszTrueEvent++;
			}

			switch (tszTrueEvent[0])
			{
			case _TEXT('!'):
				//	Show text file
				bReturn	= MessageFile_Show(&tszTrueEvent[1], lpBuffer, lpUser, DT_FTPUSER, _TEXT("200-"), NULL);
				break;
			case _TEXT('@'):
				bReturn	= TRUE;
				//	Translate alias
				if (! SplitString(&tszTrueEvent[1], &Alias))
				{
					if (! ConcatString(&Alias, Args))
					{
						//	Get result
						bReturn	= FTP_AdminCommands2(lpUser, &Alias, TRUE);
					}
					FreeString(&Alias);
				}
				break;

			default:
				//	Script
				Event.tszCommand = tszTrueEvent;
				bReturn	= RunEvent(&Event);
				if (bReturn)
				{
					dwLastError = GetLastError();
					// the problem here is the command may have printed a line with a success prefix
					// and then failed... it really shouldn't count as successful if the script died!
				}
			}

		} while (! bReturn && Config_Get(&IniConfigFile, _TEXT("FTP_Custom_Commands"), tszCommand, pBuffer, &iPos));
		PullString(Args, 1);
	}

	//	Find admin command
	tszPrettyName = NULL;
	lpProc	= (bReturn || bOverride ? NULL : FindAdminCommand(tszCommand, &tszPrettyName));
	bInternal = FALSE;

	if (lpProc)
	{
		lpUser->FtpVariables.tszCurrentCommand = tszPrettyName;
		SetFtpUser(lpUser);

		//	Execute admin command
		tszResult	= (lpProc)(lpUser, _TEXT("200-"), Args);

		//	Handle result
		if (tszResult)
		{
			switch (dwLastError = GetLastError())
			{
			case ERROR_INVALID_ARGUMENTS:
				FormatString(lpBuffer, _TEXT("500-%2TInvalid argument:%0T '%5T%s%0T'.\r\n"), tszResult);
				break;
			case ERROR_MISSING_ARGUMENT:
				FormatString(lpBuffer, _TEXT("500-%2TNot enough arguments for command:%0T '%5T%s%0T'.\r\n"), tszResult);
				break;
			default:
				FormatString(lpBuffer, _TEXT("500 %5T%s%0T: %2T%E.%0T\r\n"), tszResult, dwLastError);
				return TRUE;
			}
			//	Show help for command
//			FormatString(lpBuffer, "500-\r\n");

			FormatString(lpBuffer, _TEXT("500 %2TCommand failed.%0T\r\n"));
			return TRUE;
		}

		if (bSubCmd) return FALSE;
		FormatString(lpBuffer, _TEXT("200 '%5T%s%0T' %1TCommand successful.%0T\r\n"), tszCommand);
		bInternal = TRUE;
	}
	else if (! bExternal)
	{
		FormatString(lpBuffer, _TEXT("500 '%5TSITE %s%0T': %2TCommand not understood.%0T\r\n"), tszCommand);
		return TRUE;
	}


	//	Check validity of response
	if (!bInternal && (n = lpBuffer->len) >= 5)
	{
		for (pLine = lpBuffer->buf;--n > 0 && pLine[n - 1] != '\n';);
		strtoul(&pLine[n], &pCheck, 10);
		if (pCheck - &pLine[n] == 3)
		{
			switch (pCheck[0])
			{
			case ' ':
				//	"xxx response"
				break;
			case '-':
				//	"xxx- response"
				if (dwLastError)
				{
					FormatString(lpBuffer, _TEXT("550 %2TCommand failed (script): %E.%0T\r\n"), dwLastError);
				}
				else if (pLine[n] <= '3')
				{
					if (!bSubCmd) FormatString(lpBuffer, _TEXT("%.3s %1TCommand successful.%0T\r\n"), pLine);
				}
				else
				{
					FormatString(lpBuffer, _TEXT("%.3s %2TCommand failed.%0T\r\n"), pLine);
				}
				break;
			default:
				//	"unknown response"
				pCheck	= NULL;
			}
		}
		else pCheck	= NULL;

		if (! pCheck)
		{
			for (n = 0;n < 3 && isdigit(pLine[n]);n++);
			if (n == 3)
			{
				//	Make sure first line does not contain closing reply "xxx response"
				if (pLine[n] != '-') pLine[n]	= '-';
				//	Add closing reply
				if (dwLastError)
				{
					FormatString(lpBuffer, _TEXT("550 %2TCommand failed (script): %E.%0T\r\n"), dwLastError);
				}
				else if (pLine[n] <= '3')
				{
					if (!bSubCmd) FormatString(lpBuffer, _TEXT("%.3s %1TCommand successful.%0T\r\n"), pLine);
				}
				else
				{
					FormatString(lpBuffer, _TEXT("%.3s %2TCommand failed.%0T\r\n"), pLine);
				}
				if (pLine[n] > '3') bReturn = TRUE;
			}
			else if (bReturn)
			{
				Insert_Buffer(lpBuffer, "550-", 4);
				if (dwLastError)
				{
					FormatString(lpBuffer, _TEXT("550 %2TCommand failed (script): %E.%0T\r\n"), dwLastError);
				}
				else
				{
					FormatString(lpBuffer, _TEXT("550 %2TCommand failed.%0T\r\n"));
				}
			}
			else if (!bSubCmd)
			{
				Insert_Buffer(lpBuffer, "200-", 4);
				FormatString(lpBuffer, _TEXT("200 %1TCommand successful.%0T\r\n"));
			}
		}
	}
	else if (!bInternal)
	{
		// no text in buffer or it's too short to be a status code, just zero it
		lpBuffer->len	= 0;
		if (dwLastError)
		{
			FormatString(lpBuffer, _TEXT("550 %2TCommand failed (script): %E.%0T\r\n"), dwLastError);
		}
		else if (!bReturn)
		{
			if (!bSubCmd) FormatString(lpBuffer, _TEXT("200 %1TCommand successful.%0T\r\n"));
		}
		else
		{
			FormatString(lpBuffer, _TEXT("550 %2TCommand failed.%0T\r\n"));
		}
	}

	if (bReturn) return TRUE;

	iPos	= 0;
	while (Config_Get(&IniConfigFile, _TEXT("FTP_Post-Command_Events"), tszPrePostCmd, pBuffer, &iPos))
	{
		RunEvent(&Event);
	}

	return FALSE;
}

BOOL FTP_AdminCommands(LPFTPUSER lpUser, LPIO_STRING Args)
{
	return FTP_AdminCommands2(lpUser, Args, FALSE);
}

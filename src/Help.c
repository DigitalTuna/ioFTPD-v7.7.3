/*
* Copyright(c) 2006 Yil@Wondernet.nu
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

// NOTE: Config File access to loaded help files only occurs in this
//       file.  Therefore there is no need to lock/unlock the help
//       files individually, we'll just use one big lock around them
//       all and use that to protect the list of loaded help files
//       and reloading as well...

static LOCKOBJECT loHelpLock;
static LPCONFIG_FILE  lpCmdHelpFile;
static LPCONFIG_FILE  lpSiteHelpFileArray[MAX_HELP_FILES];
static LPTSTR         tszSiteHelpFlagArray[MAX_HELP_FILES];
static DWORD volatile dwSiteHelpFiles;


BOOL Help_Init(BOOL bFirstInitialization)
{
	TCHAR	          pBuffer[_INI_LINE_LENGTH + 1], tszName[_MAX_NAME + 1];
	LPVOID	          lpOffset;
	DWORD             n, dwName;
	LPCONFIG_FILE     lpSiteFile;

	if (bFirstInitialization)
	{
		lpCmdHelpFile = NULL;
		if (!InitializeLockObject(&loHelpLock)) return FALSE;
		dwSiteHelpFiles = 0;
		ZeroMemory(lpSiteHelpFileArray, sizeof(lpSiteHelpFileArray));
		ZeroMemory(tszSiteHelpFlagArray, sizeof(tszSiteHelpFlagArray));
	}

	AcquireExclusiveLock(&loHelpLock);

	for (n=0 ; n < dwSiteHelpFiles ; n++)
	{
		Config_Free(lpSiteHelpFileArray[n]);
		lpSiteHelpFileArray[n] = NULL;
		Free(tszSiteHelpFlagArray[n]);
		tszSiteHelpFlagArray[n] = NULL;
	}

	// load/reload ini file
	Config_Load("Help.ini", &lpCmdHelpFile);

	lpOffset = NULL;
	dwSiteHelpFiles = 0;
	while (Config_Get_Linear(&IniConfigFile, _T("Help"), tszName, pBuffer, &lpOffset))
	{
		if (dwSiteHelpFiles >= MAX_HELP_FILES) 
		{
			Config_Get_Linear_End(&IniConfigFile);
			break;
		}
		lpSiteFile = lpSiteHelpFileArray[dwSiteHelpFiles];

		// free old site file structure if name different
		if (lpSiteFile && _tcsicmp(lpSiteFile->tszConfigFile, pBuffer))
		{
			Config_Free(lpSiteFile);
			lpSiteHelpFileArray[dwSiteHelpFiles] = NULL;
		}

		if (!Config_Load(pBuffer, &lpSiteHelpFileArray[dwSiteHelpFiles]))
		{
			if (bFirstInitialization)
			{
				Putlog(LOG_ERROR, _T("Missing help file '%s'.\r\n"), pBuffer);
			}
			continue;
		}
		dwName = _tcslen(tszName);
		if (!dwName || (*tszName == _T('*')))
		{
			tszSiteHelpFlagArray[dwSiteHelpFiles] = NULL;
		}
		else
		{
			if (!(tszSiteHelpFlagArray[dwSiteHelpFiles] = Allocate(_T("Help:SitePerms"), dwName+1)))
			{
				Config_Get_Linear_End(&IniConfigFile);
				ReleaseExclusiveLock(&loHelpLock);
				return FALSE;
			}
			CopyMemory(tszSiteHelpFlagArray[dwSiteHelpFiles], tszName, (dwName+1)*sizeof(TCHAR));
		}
		dwSiteHelpFiles++;
	}

	ReleaseExclusiveLock(&loHelpLock);

	return TRUE;
}



VOID Help_DeInit(VOID)
{
	DWORD n;

	if (lpCmdHelpFile)
	{
		Config_Free(lpCmdHelpFile);
		lpCmdHelpFile = NULL;
	}

	for (n=0 ; n < dwSiteHelpFiles ; n++)
	{
		Config_Free(lpSiteHelpFileArray[n]);
		lpSiteHelpFileArray[n] = NULL;
		if (tszSiteHelpFlagArray[n]) Free(tszSiteHelpFlagArray[n]);
		tszSiteHelpFlagArray[n] = NULL;
	}
	dwSiteHelpFiles = 0;
	DeleteLockObject(&loHelpLock);
}



// Config file must be locked
BOOL Help_Do_Topic(LPCONFIG_FILE lpConfigFile, LPFTPUSER lpUser, LPTSTR tszTopic, BOOL bSummary, PBOOL pbFinal)
{
	LPCONFIG_LINE_ARRAY	lpArray;
	LPCONFIG_LINE		lpLine;
	MESSAGEDATA		    MessageData;
	BUFFER   Buffer;
	TCHAR    tszCmdBuffer[_INI_LINE_LENGTH + 1], tTemp, pBuffer[_INI_LINE_LENGTH + 1];
	INT      iPos;
	DWORD    dwTopic, dwLast, dwFirst, n, m, dwMark;
	LPBYTE   pMsgBuf;
	LPTSTR   tszTemp, tszTemp2, tszCommand;
	TCHAR    tszHeader[] = _T("%[THEME(Help)]");
	TCHAR    tszTest[] = _T(":TESTUSER");
	TCHAR    tszFinal[] = _T(":FINAL");
	TCHAR    tszCmds[] = _T(":TESTCMDS");
	TCHAR    tszChange[] = _T(":TESTCHANGE");
	TCHAR    tszNoTest[] = _T(":NOTEST");
	TCHAR    tszMark[] = _T(":MARK");
	BOOL     bSuppress, bFinal, bTestCmds, bTestChange, bMarked, bMatched, bBad;
	
	lpLine = NULL;
	dwTopic = _tcslen(tszTopic);

	//	Find array
	for (lpArray = lpConfigFile->lpLineArray ; lpArray ; lpArray = lpArray->Next)
	{
		//	Compare parameters
		if (lpArray->Name_Len == dwTopic &&	!_tcsicmp(lpArray->Name, tszTopic))
		{
			//	Store first line
			lpLine	= lpArray->First_Line;
			break;
		}
	}
	if (!lpLine)
	{
		SetLastError(ERROR_NOT_FOUND);
		return FALSE;
	}

	ZeroMemory(&Buffer, sizeof(Buffer));
	bSuppress = FALSE;
	bFinal = FALSE;
	bTestCmds = FALSE;
	bTestChange = FALSE;
	bMarked = FALSE;
	bMatched = FALSE;

	// we don't care about active lines, we process everything here
	Put_Buffer(&Buffer, tszHeader, sizeof(tszHeader)-sizeof(TCHAR));
	dwFirst = dwLast = Buffer.len;
	for ( ; lpLine ; lpLine = lpLine->Next)
	{
		bBad = FALSE;
		// However lines that start with ":TESTUSER" are treated special and
		// can suppress lines from being copied to the buffer.
		if ((lpLine->Text_l >= (sizeof(tszFinal)/sizeof(*tszFinal) -1)) && !_tcsnicmp(lpLine->Text, tszFinal, (sizeof(tszFinal)/sizeof(*tszFinal) -1)))
		{
			bFinal = TRUE;
			continue;
		}
		if ((lpLine->Text_l >= (sizeof(tszCmds)/sizeof(*tszCmds) -1)) && !_tcsnicmp(lpLine->Text, tszCmds, (sizeof(tszCmds)/sizeof(*tszCmds) -1)))
		{
			bTestCmds   = TRUE;
			bTestChange = FALSE;
			continue;
		}
		if ((lpLine->Text_l >= (sizeof(tszChange)/sizeof(*tszChange) -1)) && !_tcsnicmp(lpLine->Text, tszChange, (sizeof(tszChange)/sizeof(*tszChange) -1)))
		{
			bTestChange = TRUE;
			bTestCmds   = FALSE;
			continue;
		}
		if ((lpLine->Text_l >= (sizeof(tszNoTest)/sizeof(*tszNoTest) -1)) && !_tcsnicmp(lpLine->Text, tszNoTest, (sizeof(tszNoTest)/sizeof(*tszNoTest) -1)))
		{
			bTestChange = FALSE;
			bTestCmds   = FALSE;
			continue;
		}
		if ((lpLine->Text_l >= (sizeof(tszMark)/sizeof(*tszMark) -1)) && !_tcsnicmp(lpLine->Text, tszMark, (sizeof(tszMark)/sizeof(*tszMark) -1)))
		{
			if (bMarked && !bMatched)
			{
				Buffer.len = dwMark;
			}
			bMarked = TRUE;
			bMatched = FALSE;
			dwMark = Buffer.len;
			continue;
		}
		if ((lpLine->Text_l >= (sizeof(tszTest)/sizeof(*tszTest) -1)) && !_tcsnicmp(lpLine->Text, tszTest, (sizeof(tszTest)/sizeof(*tszTest) -1)))
		{
			tszTemp = lpLine->Text + (sizeof(tszTest)/sizeof(*tszTest))-1;
			while (*tszTemp == _T(' ')) tszTemp++;
			if (!*tszTemp || (*tszTemp == _T('*')) || !HavePermission(lpUser->UserFile, tszTemp))
			{
				bSuppress = FALSE;
			}
			else
			{
				bSuppress = TRUE;
			}
			continue;
		}
		if (!bSuppress && (bTestCmds || bTestChange))
		{
			_tcscpy(tszCmdBuffer, lpLine->Text);
			tszTemp = tszCmdBuffer;
			// skip over until first bounding box, color control codes, etc
			while (*tszTemp)
			{
				if ((*tszTemp == _T('%')) && tszTemp[1] == _T('['))
				{
					// it's a control code, skip to ]
					while (*tszTemp && *tszTemp++ != _T(']'));
					continue;
				}
				if (_istalnum(*tszTemp)) break;
				tszTemp++;
			}
			tszCommand = tszTemp;
			// find end of cmd
			while (*tszTemp && (*tszTemp != _T(' ') && *tszTemp != _T('%'))) tszTemp++;
			tszTemp2 = tszTemp;
			// consume spaces and control codes
			while (*tszTemp)
			{
				if ((*tszTemp == _T('%')) && tszTemp[1] == _T('['))
				{
					// it's a control code, skip to ]
					while (*tszTemp && *tszTemp++ != _T(']'));
					continue;
				}
				if (*tszTemp != _T(' ')) break;
				tszTemp++;
			}

			if (*tszCommand && *tszTemp == _T('-') && (*tszTemp2 == _T(' ') || *tszTemp2 == _T('%')))
			{
				// it looks like a command we should test
				tTemp = *tszTemp2;
				*tszTemp2 = 0;
				iPos = 0;
				if (!_tcsicmp(tszCommand, _T("change")))
				{
					// see if user has access to ANY of the change commands
					for (n = 0 ; ChangeCommand[n].Trigger ; n++)
					{
						//	Search for command
						if (Config_Get_Permission2(&IniConfigFile, _TEXT("Change_Permissions"), ChangeCommand[n].Trigger, lpUser->UserFile))
						{
							// not authorized to use command
							continue;
						}
						// ok
						break;
					}
					if (!ChangeCommand[n].l_Trigger) continue; // no rights to any change command!
				}
				else if (bTestCmds)
				{
					if (Config_Get(&IniConfigFile, _TEXT("FTP_Custom_Commands"), tszCommand, pBuffer, &iPos))
					{
						// it's an alias, we'll try to look it up
						if (Config_Get_Permission2(&IniConfigFile, _TEXT("FTP_SITE_Permissions"), tszCommand, lpUser->UserFile))
						{
							// no access
							continue;
						}
					}
					else if (FindAdminCommand(tszCommand, NULL))
					{
						// it's a built in site command
						if (Config_Get_Permission2(&IniConfigFile, _TEXT("FTP_SITE_Permissions"), tszCommand, lpUser->UserFile))
						{
							// no access
							continue;
						}
					}
					else if (!HasFlag(lpUser->UserFile, _T("M")))
					{
						// unknown command, but Master listing... mark it bad
						bBad = TRUE;
					}
					else continue; // unknown command
				}
				else // it's a change command
				{
					m = _tcslen(tszCommand);
					for (n = 0 ; ChangeCommand[n].Trigger ; n++)
					{
						//	Search for command
						if (ChangeCommand[n].l_Trigger == m && !_tcsnicmp(ChangeCommand[n].Trigger, tszCommand, m))
						{
							if (Config_Get_Permission2(&IniConfigFile, _TEXT("Change_Permissions"), tszCommand, lpUser->UserFile))
							{
								// not authorized to use command
								continue;
							}
							// ok
							break;
						}
					}
					if (!ChangeCommand[n].l_Trigger)
					{
						if (!HasFlag(lpUser->UserFile, _T("M")))
						{
							// unknown command, but Master listing... mark it bad
							bBad = TRUE;
						}
						else continue;
					}
				}
				*tszTemp2 = tTemp;
				bMatched = TRUE;
			}
		}
		
		if (!bSuppress)
		{
			Put_Buffer(&Buffer, lpLine->Text, lpLine->Text_l);
			if (bBad) Put_Buffer(&Buffer, _T(" (*BAD*)"), 8*sizeof(TCHAR));
			Put_Buffer(&Buffer, _T("\r\n"), 2*sizeof(TCHAR));
			tszTemp = lpLine->Text;
			for ( ; *tszTemp ; tszTemp++)
			{
				if ((*tszTemp == _T(' ')) || (*tszTemp == _T('\t'))) continue;
				// there is something visible on the line 
				dwLast = Buffer.len;
				break;
			}
		}
	}

	if (bMarked && !bMatched)
	{
		Buffer.len = dwMark;
		if (dwLast > Buffer.len ) dwLast = Buffer.len;
	}

	// now set the length to the stored pos after the last non-blank line
	Buffer.len = dwLast;

	// record FINAL state which means stop showing further summary lines in HELP_# section.
	if (pbFinal) *pbFinal = bFinal;

	if (dwLast == dwFirst)
	{
		// we have nothing to print so just return
		Free(Buffer.buf);
		return TRUE;
	}


	// the buffer isn't null terminated and might need truncation anyway...
	// and we'll add a single blank line if not doing the summary
	if (!bSummary)
	{
		Put_Buffer(&Buffer, _T("\r\n\0"), 3*sizeof(TCHAR));
	}
	else
	{
		Put_Buffer(&Buffer, _T("\0"), 1*sizeof(TCHAR));
	}

	// setup the buffer for cookie/formatting processing
	pMsgBuf = Message_PreCompile(Buffer.buf, NULL);

	if (pMsgBuf == NULL)
	{
		// the cookie argument didn't parse at all...
		Free(Buffer.buf);
		SetLastError(ERROR_COMMAND_FAILED);
		return FALSE;
	}

	//	Reset unified variable storage
	MessageData.dwFormat = 0;
	MessageData.szFormat = 0;
	MessageData.dwPrefix[0] = 4;
	MessageData.dwPrefix[1] = 4;
	MessageData.szPrefix[0] = "200-";
	MessageData.szPrefix[1] = "200-";
	MessageData.dwData   = DT_FTPUSER;
	MessageData.lpData   = lpUser;
	MessageData.lpOutBuffer = &lpUser->CommandChannel.Out;
	MessageData.CurrentThemes.i = 0;
	MessageData.SavedThemes.i = 0;
	MessageData.lpSavedTheme = NULL;
	MessageData.dwMarkPosition = 0;
	InitDataOffsets(&MessageData.DataOffsets, MessageData.lpData, MessageData.dwData);

	// process any cookies including theme colors and formatting directives
	Compile_Message(&MessageData, pMsgBuf, FALSE, FALSE);

	Free(Buffer.buf);
	Free(pMsgBuf);
	return TRUE;
}



BOOL FTP_Help(LPFTPUSER lpUser, LPIO_STRING Args)
{
	LPBUFFER lpBuffer = &lpUser->CommandChannel.Out;
	LPTSTR   tszTopic;
	DWORD    dwError;
	BOOL     bDefault, bReturn;
	LPOUTPUT_THEME  lpTheme;

	if (!lpCmdHelpFile)
	{
		FormatString(lpBuffer, _T("500 %2THelp file missing.%0T\r\n"));
		return TRUE;
	}

	if (GetStringItems(Args) == 0)
	{
		bDefault = TRUE;
		tszTopic = _T("HELP_0");
	}
	else
	{
		// no matter how many args, just display help for first word without complaining
		bDefault = FALSE;
		tszTopic = GetStringIndexStatic(Args, 0);
	}

	if (*tszTopic == _T('-'))
	{
		tszTopic++;
		lpTheme = GetTheme();
		if (lpTheme) SetTheme(0);
	}
	else
	{
		lpTheme = NULL;
	}
	
	AcquireSharedLock(&loHelpLock);
	if (Help_Do_Topic(lpCmdHelpFile, lpUser, tszTopic, FALSE, NULL))
	{
		ReleaseSharedLock(&loHelpLock);
		FormatString(lpBuffer, _T("214 '%5THELP%0T' %1TCommand successful.%0T\r\n"));
		bReturn = FALSE;
		goto done;
	}
	ReleaseSharedLock(&loHelpLock);
	dwError = GetLastError();
	// leave theme response off for command response
	bReturn = TRUE;
	if (dwError == ERROR_NOT_FOUND)
	{
		if (bDefault)
		{
			FormatString(lpBuffer, _T("501 %5TUse '%2THELP <COMMAND>%5T'.%0T\r\n"));
		}
		else
		{
			FormatString(lpBuffer, _T("502 %5THELP%0T: %2TNo help for '%s'.%0T\r\n"), tszTopic);
		}
	}
	else
	{
		FormatString(lpBuffer, _T("500 %5THELP%0T: %2%E%0T\r\n"), dwError);
	}
done:
	// restore active theme if needed here
	if (lpTheme) SetTheme(lpTheme);
	return bReturn;
}



// site help is similar to command help, except we allow for user extensions that override
// entries in the default help file.  We also do a quick permission check on the file to see
// if it's even worth checking...
LPTSTR Admin_Help(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPBUFFER lpBuffer = &lpUser->CommandChannel.Out;
	LPTSTR   tszTopic, tszErr;
	TCHAR    tszSummary[10], pBuffer[_INI_LINE_LENGTH + 1];
	INT	     iPos;
	DWORD    n, m, dwArgs, dwError;
	BOOL     bAlias, bFinal;
	LPOUTPUT_THEME  lpTheme;
	


	if (!dwSiteHelpFiles)
	{
		ERROR_RETURN(ERROR_FILE_MISSING, GetStringIndexStatic(Args, 0));
	}

	lpTheme = NULL;
	dwArgs = GetStringItems(Args);
	tszErr = NULL;
	if (dwArgs > 1)
	{
		tszTopic = GetStringIndexStatic(Args, 1);
		if (*tszTopic == _T('-'))
		{
			tszTopic++;
			lpTheme = GetTheme();
			if (lpTheme) SetTheme(0);
		}
	}

	// no args, or a single argument that is just a dash
	if ((dwArgs <= 1) || !*tszTopic)
	{
		AcquireSharedLock(&loHelpLock);
		tszTopic = tszSummary;
		strcpy(tszTopic, "HELP_");
		dwError = ERROR_NO_HELP;
		bFinal = FALSE;
		for (m=0 ; !bFinal && (m <= 99) ; m++)
		{
			itoa(m, tszSummary+5, 10);
			for (n=0 ; n < dwSiteHelpFiles ; n++)
			{
				if (tszSiteHelpFlagArray[n] && HasFlag(lpUser->UserFile, tszSiteHelpFlagArray[n])) continue;
				if (Help_Do_Topic(lpSiteHelpFileArray[n], lpUser, tszTopic, TRUE, &bFinal))
				{
					dwError = NO_ERROR;
					if (bFinal) break;
				}
			}
		}
		tszErr  = GetStringIndexStatic(Args, 0);
		goto lockdone;
	}

	bAlias   = FALSE;
	iPos     = 0;
	dwError  = NO_ERROR;

	if (!_tcsicmp(tszTopic, _T("change")))
	{
		if (GetStringItems(Args) != 2)
		{
			// handle change subcommands, if just plain "change" don't need to do anything...
			tszTopic = GetStringIndexStatic(Args, 2);
			m = _tcslen(tszTopic);
			for (n = 0 ; ChangeCommand[n].Trigger ; n++)
			{
				//	Search for command
				if (ChangeCommand[n].l_Trigger == m && !_tcsnicmp(ChangeCommand[n].Trigger, tszTopic, m))
				{
					if (Config_Get_Permission2(&IniConfigFile, _TEXT("Change_Permissions"), tszTopic, lpUser->UserFile))
					{
						// not authorized to use command
						dwError = IO_NO_ACCESS;
						tszErr  = GetStringRange(Args, 1, 2);
						goto done;
					}
					// ok
					break;
				}
			}
			if (!ChangeCommand[n].Trigger)
			{
				// not a valid subcmd, don't bother with generic NOT_FOUND, just return failure...
				dwError = ERROR_NO_HELP;
				tszErr = GetStringRange(Args, 1, STR_END);
				goto done;
			}
			tszTopic = GetStringRange(Args, 1, 2);
		}
	}
	else if (Config_Get(&IniConfigFile, _TEXT("FTP_Custom_Commands"), tszTopic, pBuffer, &iPos))
	{
		// it's a user added script, we'll try to look it up, but may need result for alias -> cmd output later
		bAlias = TRUE;
		if (Config_Get_Permission2(&IniConfigFile, _TEXT("FTP_SITE_Permissions"), tszTopic, lpUser->UserFile))
		{
			dwError = IO_NO_ACCESS;
			tszErr  = GetStringIndexStatic(Args, 1);
			goto done;
		}
	}
	else if (FindAdminCommand(tszTopic, NULL))
	{
		// it's a built in site command
		if (Config_Get_Permission2(&IniConfigFile, _TEXT("FTP_SITE_Permissions"), tszTopic, lpUser->UserFile))
		{
			dwError = IO_NO_ACCESS;
			tszErr  = GetStringIndexStatic(Args, 1);
			goto done;
		}
	}
	// if all 3 tests failed it might be a "topic" which isn't a real command but we'll look it up anyway

	AcquireSharedLock(&loHelpLock);
	for (n=0 ; n < dwSiteHelpFiles ; n++)
	{
		if (tszSiteHelpFlagArray[n] && HasFlag(lpUser->UserFile, tszSiteHelpFlagArray[n])) continue;
		if (Help_Do_Topic(lpSiteHelpFileArray[n], lpUser, tszTopic, FALSE, NULL))
		{
			goto lockdone;
		}
	}

	// no help found on topic, was it was a simple command substitution alias so print that
	if (bAlias && (pBuffer[0] == _T('@')))
	{
		FormatString(lpBuffer, _T("%s\r\n%s ALIAS 'site %s' => 'site %s'\r\n%s\r\n"),
			tszMultilinePrefix, tszMultilinePrefix, tszTopic, &pBuffer[1], tszMultilinePrefix);
		goto lockdone;
	}
	
	// try NOT_FOUND generic reply
	tszTopic = _T("NOT_FOUND");

	for (n=0 ; n < dwSiteHelpFiles ; n++)
	{
		if (tszSiteHelpFlagArray[n] && HasFlag(lpUser->UserFile, tszSiteHelpFlagArray[n])) continue;
		if (Help_Do_Topic(lpSiteHelpFileArray[n], lpUser, tszTopic, FALSE, NULL))
		{
			// we still want to return an error below, so just fall through
			break;
		}
	}

	dwError = ERROR_NO_HELP;
	tszErr  = GetStringIndexStatic(Args, 1);

lockdone:
	ReleaseSharedLock(&loHelpLock);
done:
	if (lpTheme) SetTheme(lpTheme);
	if ((dwError == NO_ERROR) || !tszErr)
	{
		return NULL;
	}
	if (*tszErr == _T('-')) tszErr++;
	ERROR_RETURN(dwError, tszErr);
}

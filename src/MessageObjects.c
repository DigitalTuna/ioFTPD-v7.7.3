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

#define MSG_FORMAT_PLAIN   0
#define MSG_FORMAT_AUTO    2
#define MSG_FORMAT_3DIGIT  3

typedef union {
	struct {
		unsigned type    : 2;
		unsigned lookup  : 8;
		unsigned size    : 5;
		unsigned show    : 1;
		unsigned theme   : 16;
	} d;
	int i;
} MsgSuffix;


BOOL Object_Get_Int(LPMESSAGEDATA lpData, LPOBJV lpObject, LPINT lpValue)
{
	LPMESSAGE_VARIABLE	lpVariable;
	LPVOID				lpResult;

	switch (lpObject->pValue[0])
	{
	case C_INTEGER:
		lpValue[0]	= ((LPINT)&lpObject->pValue[1])[0];
		return FALSE;

	case C_INTEGER_VARIABLE:
		lpVariable	= (LPMESSAGE_VARIABLE)GetVariable(((LPWORD)&lpObject->pValue[1])[0]);
		//	Make sure we have required pointers set for integer variable
		if (! HASDATAEX(lpData->DataOffsets, lpVariable->dwRequiredData)) break;
		//	Call alloc proc
		lpResult	= ((LPVOID (__cdecl *)(const LPMESSAGEDATA ))lpVariable->AllocProc)(lpData);
		//	Sanity check
		if (! lpResult) break;
		//	Update free proc
		lpObject->lpMemory		= lpResult;
		lpObject->FreeProc	= lpVariable->FreeProc;
		lpValue[0]	= ((LPINT)lpResult)[0];
		return FALSE;
	}
	return TRUE;
}



LPSTR Object_Get_String(LPMESSAGEDATA lpData, LPOBJV lpObject)
{
	LPMESSAGE_VARIABLE	lpVariable;
	LPVOID				lpResult;

	switch (lpObject->pValue[0])
	{
	case C_STRING:
		return (LPSTR)&lpObject->pValue[1];

	case C_STRING_VARIABLE:
		lpVariable	= (LPMESSAGE_VARIABLE)GetVariable(((LPWORD)&lpObject->pValue[1])[0]);
		//	Make sure we have required pointers set for string variable
		if (! HASDATAEX(lpData->DataOffsets, lpVariable->dwRequiredData)) break;
		//	Call alloc proc
		lpResult	= ((LPVOID (__cdecl *)(const LPMESSAGEDATA ))lpVariable->AllocProc)(lpData);
		//	Update structure
		lpObject->lpMemory		= lpResult;
		lpObject->FreeProc	= lpVariable->FreeProc;

		return (LPSTR)lpResult;
	}
	return NULL;
}


VOID Insert_Field_Code(LPMESSAGEDATA lpData, LPTHEME_FIELD lpField)
{
	LPBUFFER lpBuffer;

	lpBuffer = lpData->lpOutBuffer;

	if (!lpField->i) return;

	if (lpField->Settings.doReset)
	{
		// issue ANSI color reset
		Put_Buffer_Format(lpBuffer, "%c[%dm", 27, 39);
		lpData->CurrentThemes.i = 0;
	}

	if (lpField->Settings.doInverse)
	{
		Put_Buffer_Format(lpBuffer, "%c[%dm", 27, 7);
		lpData->CurrentThemes.Settings.doInverse = !lpData->CurrentThemes.Settings.doInverse;
	}
	if (lpField->Settings.hasUnderLine)
	{
		lpData->CurrentThemes.Settings.hasUnderLine = 1;
		if (lpField->Settings.Underline)
		{
			Put_Buffer_Format(lpBuffer, "%c[%dm", 27, 4);
			lpData->CurrentThemes.Settings.Underline = 1;
		}
		else
		{
			Put_Buffer_Format(lpBuffer, "%c[%dm", 27, 24);
			lpData->CurrentThemes.Settings.Underline = 0;
		}
	}

	if (lpField->Settings.Background)
	{
		Put_Buffer_Format(lpBuffer, "%c[%dm", 27, 39 + lpField->Settings.Background);
		lpData->CurrentThemes.Settings.Background = lpField->Settings.Background;
	}
	if (lpField->Settings.Foreground)
	{
		Put_Buffer_Format(lpBuffer, "%c[%dm", 27, 29 + lpField->Settings.Foreground);
		lpData->CurrentThemes.Settings.Foreground = lpField->Settings.Foreground;
	}
}


VOID Restore_Theme(LPMESSAGEDATA lpData)
{
	LPBUFFER lpBuffer;

	lpBuffer = lpData->lpOutBuffer;

	if (lpData->SavedThemes.i == lpData->CurrentThemes.i)
	{
		return;
	}

	if (!lpData->SavedThemes.i && lpData->CurrentThemes.i)
	{
		Put_Buffer_Format(lpBuffer, "%c[%dm", 27, 39);
		lpData->CurrentThemes.i = 0;
		return;
	}

	if (lpData->SavedThemes.Settings.doInverse != lpData->CurrentThemes.Settings.doInverse)
	{
		Put_Buffer_Format(lpBuffer, "%c[%dm", 27, 7);
	}
	if ((lpData->SavedThemes.Settings.hasUnderLine != lpData->CurrentThemes.Settings.hasUnderLine) &&
		(lpData->SavedThemes.Settings.Underline != lpData->CurrentThemes.Settings.Underline))
	{
		if (lpData->SavedThemes.Settings.Underline)
		{
			Put_Buffer_Format(lpBuffer, "%c[%dm", 27, 4);
		}
		else
		{
			Put_Buffer_Format(lpBuffer, "%c[%dm", 27, 24);
		}
	}

	if (lpData->SavedThemes.Settings.Background != lpData->CurrentThemes.Settings.Background)
	{
		Put_Buffer_Format(lpBuffer, "%c[%dm", 27, 39 + lpData->SavedThemes.Settings.Background);
	}
	if (lpData->SavedThemes.Settings.Foreground != lpData->CurrentThemes.Settings.Foreground)
	{
		Put_Buffer_Format(lpBuffer, "%c[%dm", 27, 29 + lpData->SavedThemes.Settings.Foreground);
	}
	lpData->CurrentThemes.i = lpData->SavedThemes.i;
}


BOOL Cookie_Convert_Units(LPMESSAGEDATA lpData, DOUBLE value, INT iStart, INT iThemeIndex)
{
	static LPTSTR lptszUnitsList[] = { _T("KB"), _T("MB"), _T("GB"), _T("TB") };
	LPOUTPUT_THEME lpTheme;
	int i;

	for ( i = 0; i+1 < sizeof(lptszUnitsList)/sizeof(*lptszUnitsList) ; i++ )
	{
		if ( value < 1024.0 && i >= iStart)
		{
			break;
		}
		value /= 1024.0;
	}

	if ((iThemeIndex > 0) && (iThemeIndex+i <= MAX_COLORS) && (lpTheme = GetTheme()))
	{
		Insert_Field_Code(lpData, &lpTheme->ThemeFieldsArray[iThemeIndex+i]);
	}

	FormatString(lpData->lpOutBuffer, lpData->szFormat, value);
	FormatString(lpData->lpOutBuffer, " %s", lptszUnitsList[i]);
	return FALSE;
}



BOOL Cookie_Convert_3Digit(LPMESSAGEDATA lpData, DOUBLE value, INT iStart, INT iThemeIndex)
{
	static LPTSTR lptszUnitsList[] = { _T("KB"), _T("MB"), _T("GB"), _T("TB") };
	TCHAR tszSize[64];
	LPOUTPUT_THEME lpTheme;
	int i, n;

	for ( i=0 ; i+1 < sizeof(lptszUnitsList)/sizeof(*lptszUnitsList) ; i++ )
	{
		if ( value < 1024.0 && i >= iStart)
		{
			break;
		}
		value /= 1024.0;
	}

	if (value < .01)
	{
		sprintf_s(tszSize, sizeof(tszSize), "0 %s", lptszUnitsList[i]);
	}
	else if (value < 10.0)
	{
		sprintf_s(tszSize, sizeof(tszSize), "%.2f %s", value, lptszUnitsList[i]);
	}
	else if ( value < 100.0)
	{
		sprintf_s(tszSize, sizeof(tszSize), "%.1f %s", value, lptszUnitsList[i]);
	}
	else
	{
		sprintf_s(tszSize, sizeof(tszSize), "%.0f %s", value, lptszUnitsList[i]);
	}

	if ((iThemeIndex > 0) && (iThemeIndex+i <= MAX_COLORS) && (lpTheme = GetTheme()))
	{
		Insert_Field_Code(lpData, &lpTheme->ThemeFieldsArray[iThemeIndex+i]);
	}

	// squash a dot and everything after it since we are using a string not a float...
	for ( n = lpData->dwFormat ; n > 0 && lpData->szFormat[n] != '.' ; n--);
	if (n == 0)
	{
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
	}
	else
	{
		lpData->szFormat[n] = 's';
		lpData->szFormat[n+1] = 0;
	}
	FormatString(lpData->lpOutBuffer, lpData->szFormat, tszSize);
	return FALSE;
}



// see MessageObject_ConvertSuffix below for range -> meaning
BOOL Cookie_Convert_Special(LPMESSAGEDATA lpData, INT iDivider, DOUBLE value)
{
	static LPTSTR lptszUnitsList[] = { _T("KB"), _T("MB"), _T("GB"), _T("TB") };
	MsgSuffix m;
	int i, iThemeIndex;
	LPOUTPUT_THEME lpTheme;
	LPTHEME_FIELD  lpField;
	if (iDivider == 1)
	{
		return FormatString(lpData->lpOutBuffer, lpData->szFormat, value);
	}

	m.i = iDivider;
	if (m.d.lookup != 0)
	{
		lpTheme = GetTheme();
		if (lpTheme && m.d.lookup <= MAX_COLORS && (lpField = &lpTheme->ThemeFieldsArray[m.d.lookup]) && lpField->i)
		{
			if (lpField->Formatter.doFormat)
			{
				i = lpField->Formatter.Format;
				iThemeIndex = i % 1000;
				i /= 1000;

				if (i >= 1 && i < 5)
				{
					return Cookie_Convert_Units(lpData, value, i-1, iThemeIndex);
				}
				if (i >= 5 && i < 9)
				{
					return Cookie_Convert_3Digit(lpData, value, i-5, iThemeIndex);
				}
				if (i == 0 && iThemeIndex >= 200 && iThemeIndex < 1000)
				{
					i = (iThemeIndex/200)-1;
					iDivider = 1<<(i*10);
					iThemeIndex %= 200;
					if (iThemeIndex > 0 && iThemeIndex <= MAX_COLORS)
					{
						Insert_Field_Code(lpData, &lpTheme->ThemeFieldsArray[iThemeIndex]);
					}

					FormatString(lpData->lpOutBuffer, lpData->szFormat, value / iDivider);
					FormatString(lpData->lpOutBuffer, " %s", lptszUnitsList[i]);
					return FALSE;
				}
			}
			// bad formatter
			m.d.type = MSG_FORMAT_PLAIN;
			m.d.size = 0;
		}
	}

	switch (m.d.type)
	{
	case MSG_FORMAT_PLAIN:
		iDivider = 1 <<(m.d.size);
		if (!m.d.show)
		{
			return FormatString(lpData->lpOutBuffer, lpData->szFormat, value / iDivider);
		}
		else
		{
			FormatString(lpData->lpOutBuffer, lpData->szFormat, value / iDivider);
			return FormatString(lpData->lpOutBuffer, " %s", lptszUnitsList[m.d.size/10]);
		}

	case MSG_FORMAT_AUTO:
		return Cookie_Convert_Units(lpData, value, m.d.size/10, m.d.theme);

	case MSG_FORMAT_3DIGIT:
		return Cookie_Convert_3Digit(lpData, value, m.d.size/10, m.d.theme);
	};
	return TRUE;
}


INT MessageObject_ConvertSuffix(LPSTR szArg)
{
	int iDigit;
	MsgSuffix m;

	m.i = 0;

	// check for a theme preference
	if (szArg[0] == '*')
	{
		m.d.show = 1;
		iDigit = GetAutoTheme();
		if (iDigit > 0 && iDigit <= MAX_COLORS)
		{
			// clear autotheme
			m.d.lookup = iDigit;
		}
		szArg++;
	}

	if (szArg[0] == '+')
	{
		m.d.show = 1;
		szArg++;
	}
	else
	{
		m.d.show = 0;
	}

	if (! memicmp("kilo", szArg, 4))
	{
		m.d.type = MSG_FORMAT_PLAIN;
		m.d.size = 0;
		return m.i;
	}
	if (! memicmp("mega", szArg, 4))
	{
		m.d.type = MSG_FORMAT_PLAIN;
		m.d.size = 10;
		return m.i;
	}

	if (! memicmp("giga", szArg, 4))
	{
		m.d.type = MSG_FORMAT_PLAIN;
		m.d.size = 20;
		return m.i;
	}

	if (! memicmp("tera", szArg, 4))
	{
		m.d.type = MSG_FORMAT_PLAIN;
		m.d.size = 30;
		return m.i;
	}

	m.d.show = 1;
	if (! memicmp("automb", szArg, 6))
	{
		if (1 != sscanf(&szArg[6], "%d", &iDigit)) iDigit = 0;
		m.d.theme = iDigit;
		m.d.type = MSG_FORMAT_AUTO;
		m.d.size = 10;
		return m.i;
	}
	if (! memicmp("autogb", szArg, 6))
	{
		if (1 != sscanf(&szArg[6], "%d", &iDigit)) iDigit = 0;
		m.d.theme = iDigit;
		m.d.type = MSG_FORMAT_AUTO;
		m.d.size = 20;
		return m.i;
	}
	if (! memicmp("autotb", szArg, 6))
	{
		if (1 != sscanf(&szArg[6], "%d", &iDigit)) iDigit = 0;
		m.d.theme = iDigit;
		m.d.type = MSG_FORMAT_AUTO;
		m.d.size = 30;
		return m.i;
	}
	if (! memicmp("auto", szArg, 4))
	{
		if (1 != sscanf(&szArg[6], "%d", &iDigit)) iDigit = 0;
		m.d.theme = iDigit;
		m.d.type = MSG_FORMAT_AUTO;
		m.d.size = 0;
		return m.i;
	}
	if (! memicmp("3digitmb", szArg, 8))
	{
		if (1 != sscanf(&szArg[8], "%d", &iDigit)) iDigit = 0;
		m.d.theme = iDigit;
		m.d.type = MSG_FORMAT_3DIGIT;
		m.d.size = 10;
		return m.i;
	}
	if (! memicmp("3digitgb", szArg, 8))
	{
		if (1 != sscanf(&szArg[8], "%d", &iDigit)) iDigit = 0;
		m.d.theme = iDigit;
		m.d.type = MSG_FORMAT_3DIGIT;
		m.d.size = 20;
		return m.i;
	}
	if (! memicmp("3digittb", szArg, 8))
	{
		if (1 != sscanf(&szArg[8], "%d", &iDigit)) iDigit = 0;
		m.d.theme = iDigit;
		m.d.type = MSG_FORMAT_3DIGIT;
		m.d.size = 30;
		return m.i;
	}
	if (! memicmp("3digit", szArg, 6))
	{
		if (1 != sscanf(&szArg[8], "%d", &iDigit)) iDigit = 0;
		m.d.theme = iDigit;
		m.d.type = MSG_FORMAT_3DIGIT;
		m.d.size = 0;
		return m.i;
	}
	return (ULONG)-1;
}


BOOL Cookie_Function_Stats(LPMESSAGEDATA lpData, INT64 *Stats, INT Argc, LPOBJV Argv)
{
	INT		Chosen, Section, Divider;
	INT64   i64[3];
	DOUBLE  value;

	Chosen	= 1;
	Divider	= 1;
	Section	= GETOFFSET(lpData->DataOffsets, DATA_SSECTION);

	switch (Argc)
	{
	case 3:
		//	Get stats section
		if (! Object_Get_Int(lpData, &Argv[2], &Section) &&
			Section < -2 || Section >= MAX_SECTIONS) Section	= 0;
		if (Section == -2) Section = -1;

	case 2:
		//	Suffix: Kilo, Mega, Giga
		if (! Object_Get_Int(lpData, &Argv[1], &Divider) &&
			! Divider)	Divider	= 1;

	case 1:
		//	Stats type: Files, Bytes, Average speed
		if (! Object_Get_Int(lpData, &Argv[0], &Chosen) &&
			(Chosen < 0 || Chosen > 2))	Chosen	= 1;
		break;
	}

	if (Section == -1)
	{
		i64[0] = i64[1] = i64[2] = 0;
		for (Section = 0 ; Section < MAX_SECTIONS ; Section++)
		{
			i64[0] += Stats[Section * 3];
			i64[1] += Stats[(Section * 3) + 1];
			i64[2] += Stats[(Section * 3) + 2];
		}
	}
	else
	{
		i64[0] = Stats[Section * 3];
		i64[1] = Stats[(Section * 3) + 1];
		i64[2] = Stats[(Section * 3) + 2];
	}

	// get result into i64[0];
	if (Chosen != 2)
	{
		value = i64[Chosen] * 1.0;
	}
	else if (!i64[2])
	{
		value = 0.0;
	}
	else 
	{
		value = i64[1] * 1.0 / i64[2];
	}

	return Cookie_Convert_Special(lpData, Divider, value);
}




/*

  GroupInfo()

  Description
  UserSlots
  LeechSlots
  Users


  */
BOOL MessageObject_GroupInfo(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPGROUPFILE	lpGroupFile;
	INT			Int;
	TCHAR       tszTemp[_MAX_NAME + 10];
	LPTSTR      tszFileName;
	LPPARENT_GROUPFILE lpParent;

	//	Get integer object
	if (! Argc || Object_Get_Int(lpData, &Argv[0], &Int)) return TRUE;
	//	Get groupfile
	lpGroupFile	= GETOFFSET(lpData->DataOffsets, DATA_GROUPFILE);

	switch (Int)
	{
	case GROUP_DESCRIPTION:
		//	Group description
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, lpGroupFile->szDescription);
		break;
	case GROUP_USERSLOTS:
		//	Group's user slots
		lpData->szFormat[lpData->dwFormat - 2]	= 'i';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, lpGroupFile->Slots[0]);
		break;
	case GROUP_LEECHSLOTS:
		//	Group's leech slots
		lpData->szFormat[lpData->dwFormat - 2]	= 'i';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, lpGroupFile->Slots[1]);
		break;
	case GROUP_USERS:
		//	Users within group
		lpData->szFormat[lpData->dwFormat - 2]	= 'i';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, lpGroupFile->Users);
		break;
	case GROUP_MOUNTFILE:
		//	Mount file
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, lpGroupFile->szVfsFile);
		break;
	case GROUP_DEFAULTFILE:
		//	Mount file
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		lpParent = (LPPARENT_GROUPFILE) lpGroupFile->lpParent;
		if (lpParent && lpParent->tszDefaultName)
		{
			if (tszFileName = Config_Get_Path(&IniConfigFile, _TEXT("Locations"), _TEXT("User_Files"), lpParent->tszDefaultName, NULL))
			{
				if (GetFileAttributes(tszFileName) != INVALID_FILE_ATTRIBUTES)
				{
					sprintf_s(tszTemp, sizeof(tszTemp)/sizeof(*tszTemp), "/%s", lpParent->tszDefaultName);
					FormatString(lpData->lpOutBuffer, lpData->szFormat, tszTemp);
					Free(tszFileName);
					break;
				}
				Free(tszFileName);
			}
		}

		FormatString(lpData->lpOutBuffer, lpData->szFormat, _T("/Default.User"));
		break;
	}
	return FALSE;
}



/*


  %[who()]

  Action
  TransferSpeed
  BytesTransfered
  IdleHours, IdleMinutes, IdleSeconds
  LoginHours, LoginMinutes, LoginSeconds
  Ip
  Ident
  Hostname
  Path
  VirtualPath
  DataPath
  VirtualDataPath
  
  */
BOOL MessageObject_Who(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	IO_WHO	*pWhoData;
	IN_ADDR	InetAddress;
	INT		Int, iDivider;
	LPTSTR  tszSlash;
	TCHAR   tszTime[12];

	if (! Argc || Object_Get_Int(lpData, &Argv[0], &Int)) return TRUE;
	//	Get who data
	pWhoData	= (IO_WHO *)GETOFFSET(lpData->DataOffsets, DATA_WHO);

	if (Argc == 1 || Object_Get_Int(lpData, &Argv[1], &iDivider) || ! iDivider) iDivider	= 1;

	switch (Int)
	{
	case WHO_SPEED_CURRENT:
		//	Speed of current user
		lpData->szFormat[lpData->dwFormat - 2]	= 'f';
		Cookie_Convert_Special(lpData, iDivider, pWhoData->fTransferSpeed);
		break;
	case WHO_CONNECTION_ID:
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwConnectionId);
		break;
	case WHO_MY_CONNECTION_ID:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		if (pWhoData->dwMyCID == pWhoData->dwConnectionId)
		{
			// I can't be a zombie if I'm doing this
			FormatString(lpData->lpOutBuffer, lpData->szFormat, "*");
		}
		else if (pWhoData->OnlineData.dwFlags & S_DEAD)
		{
			FormatString(lpData->lpOutBuffer, lpData->szFormat, "?");
		}
		else if (pWhoData->lpUserFile && (pWhoData->dwMyUID == pWhoData->lpUserFile->Uid))
		{
			FormatString(lpData->lpOutBuffer, lpData->szFormat, "+");
		}
		else
		{
			FormatString(lpData->lpOutBuffer, lpData->szFormat, "");
		}
		break;
	case WHO_SERVICENAME:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->OnlineData.tszServiceName);
		break;
	case WHO_HOSTNAME:
		//	Hostname
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		if (pWhoData->OnlineData.szHostName[0] != '\0')
		{
			FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->OnlineData.szHostName);
			break;
		}
	case WHO_IP:
		//	Ip
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		InetAddress.s_addr	= pWhoData->OnlineData.ulClientIp;
		FormatString(lpData->lpOutBuffer, lpData->szFormat, inet_ntoa(InetAddress));
		break;
	case WHO_IDENT:
		//	Ident
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->OnlineData.szIdent);
		break;
	case WHO_SPEED_UPLOAD:
		//	Total upload speed
		lpData->szFormat[lpData->dwFormat - 2]	= 'f';
		Cookie_Convert_Special(lpData, iDivider, pWhoData->fTotalUpSpeed);
		break;
	case WHO_SPEED_DOWNLOAD:
		//	Total download speed
		lpData->szFormat[lpData->dwFormat - 2]	= 'f';
		Cookie_Convert_Special(lpData, iDivider, pWhoData->fTotalDnSpeed);
		break;
	case WHO_SPEED_TOTAL:
		//	Total upload and download of all users summarized
		lpData->szFormat[lpData->dwFormat - 2]	= 'f';
		Cookie_Convert_Special(lpData, iDivider, pWhoData->fTotalUpSpeed + pWhoData->fTotalDnSpeed);
		break;
	case WHO_ACTION:
		//	Last action 
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->OnlineData.tszAction);
		break;
	case WHO_FILESIZE:
		//	Current size of file user is transferring
		lpData->szFormat[lpData->dwFormat - 2]	= 'f';
		if (Argc == 1)
		{
			FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->i64FileSize / 1024.);
		}
		else
		{
			Cookie_Convert_Special(lpData, iDivider, pWhoData->i64FileSize * 1.0);
		}
		break;
	case WHO_VIRTUALDATAPATH:
		//	File user is transferring
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->OnlineData.tszVirtualDataPath);
		break;
	case WHO_VIRTUALDATAFILE:
		//	File user is transferring
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		tszSlash = _tcsrchr(pWhoData->OnlineData.tszVirtualDataPath, _T('/'));
		if (!tszSlash)
		{
			tszSlash = pWhoData->OnlineData.tszVirtualDataPath;
		}
		else if (tszSlash[1] != 0)
		{
			tszSlash++;
		}
		FormatString(lpData->lpOutBuffer, lpData->szFormat, tszSlash);
		break;
	case WHO_DATAPATH:
		//	Path user is in
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, (pWhoData->OnlineData.tszRealDataPath ? pWhoData->OnlineData.tszRealDataPath : _TEXT("")));
		break;
	case WHO_PATH:
		//	Path user is in
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, (pWhoData->OnlineData.tszRealPath ? pWhoData->OnlineData.tszRealPath : _TEXT("")));
		break;
	case WHO_VIRTUALPATH:
		//	Path user is in
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->OnlineData.tszVirtualPath);
		break;
	case WHO_TRANSFERS_UPLOAD:
		//	Total amount of uploads
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwUploads);
		break;
	case WHO_TRANSFERS_DOWNLOAD:
		//	Total amount of downloads
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwDownloads);
		break;
	case WHO_TRANSFERS_TOTAL:
		//	Total amount of transfers
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwUploads + pWhoData->dwDownloads);
		break;
	case WHO_USERS_TOTAL:
		//	Total amount of users
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwUsers);
		break;
	case WHO_IDLERS_TOTAL:
		//	Total amount of users not transferring
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwUsers - pWhoData->dwDownloads - pWhoData->dwUploads);
		break;
	case WHO_HIDDEN:
		//	Total hidden users... only valid in "site who" and overloading dwLoginHours field
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwLoginHours);
		break;
	case WHO_LOGIN_TIME:
		//	Time user was logged in (hours):(minutes):(seconds)
		if (pWhoData->dwLoginHours)
		{
			_stprintf_s(tszTime, sizeof(tszTime), _T("%4u:%02u:%02u"), pWhoData->dwLoginHours, pWhoData->dwLoginMinutes, pWhoData->dwLoginSeconds);
		}
		else if (pWhoData->dwLoginMinutes)
		{
			_stprintf_s(tszTime, sizeof(tszTime), _T("%u:%02u"), pWhoData->dwLoginMinutes, pWhoData->dwLoginSeconds);
		}
		else
		{
			_stprintf_s(tszTime, sizeof(tszTime), _T("%u"), pWhoData->dwLoginSeconds);
		}
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, tszTime);
		break;
	case WHO_LOGIN_HOURS:
		//	Time user was logged in (hours)
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwLoginHours);
		break;
	case WHO_LOGIN_MINUTES:
		//	Time user was logged in (minutes)
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwLoginMinutes);
		break;
	case WHO_LOGIN_SECONDS:
		//	Time user was logged in (seconds)
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwLoginSeconds);
		break;
	case WHO_IDLE_TIME:
		//	Time user has been idle (hours):(minutes):(seconds)
		if (pWhoData->dwIdleHours)
		{
			_stprintf_s(tszTime, sizeof(tszTime)/sizeof(*tszTime), _T("%4u:%02u:%02u"), pWhoData->dwIdleHours, pWhoData->dwIdleMinutes, pWhoData->dwIdleSeconds);
		}
		else if (pWhoData->dwIdleMinutes)
		{
			_stprintf_s(tszTime, sizeof(tszTime)/sizeof(*tszTime), _T("%u:%02u"), pWhoData->dwIdleMinutes, pWhoData->dwIdleSeconds);
		}
		else
		{
			_stprintf_s(tszTime, sizeof(tszTime)/sizeof(*tszTime), _T("%u"), pWhoData->dwIdleSeconds);
		}
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, tszTime);
		break;
	case WHO_IDLE_HOURS:
		//	Time user has been idle (hours)
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwIdleHours);
		break;
	case WHO_IDLE_MINUTES:
		//	Time user has been idle (minutes)
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwIdleMinutes);
		break;
	case WHO_IDLE_SECONDS:
		//	Time user has been idle (seconds)
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		FormatString(lpData->lpOutBuffer, lpData->szFormat, pWhoData->dwIdleSeconds);
		break;
	}

	return FALSE;
}





/*

  %[environment(envvariable)]

  */
BOOL MessageObject_Environment(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPSTR	Result, EnvironmentVariable;
	DWORD	dwResultSize;
	CHAR	StackBuffer[512];

	//	Get name of variable to get
	if (! Argc || ! (EnvironmentVariable = Object_Get_String(lpData, &Argv[0]))) return TRUE;
	//	Get result to stack
	Result	= StackBuffer;
	//	Try to get variable from environment
	if ((dwResultSize = GetEnvironmentVariable(EnvironmentVariable, Result, 512)) > 512)
	{
		//	Allocate memory
		Result	= (LPSTR)Allocate("Cookie:Environment", dwResultSize);
		//	Check allocation
		if (! Result) return TRUE;
		//	Get variable from environment
		GetEnvironmentVariable(EnvironmentVariable, Result, dwResultSize);
	}
	else if (! dwResultSize) return TRUE;

	//	Append result to output buffer
	Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, Result);
	//	Free memory
	if (dwResultSize > 512) Free(Result);
	return FALSE;
}







/*

  %[pos]

  */
BOOL MessageObject_Position(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	INT	iPosition;
	//	Set position
	iPosition	= (INT)GETOFFSET(lpData->DataOffsets, DATA_POSITION);

	Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, iPosition);
	return FALSE;
}


/*

  %[max]

  */
BOOL MessageObject_Max(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	INT	iMax;
	//	Set position
	iMax = ((LPFTPUSER)GETOFFSET(lpData->DataOffsets, DATA_FTPUSER))->FtpVariables.iMax;

	Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, iMax);
	return FALSE;
}


/*

%[ShutdownGrace]

*/
BOOL MessageObject_ShutdownGrace(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, FtpSettings.dwShutdownTimeLeft/1000);
	return FALSE;
}



/*

%[C(color#)]

*/
BOOL MessageObject_Color(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPTSTR tszString;
	THEME_FIELD Field;

	//	Get color
	if (! Argc || ! (tszString = Object_Get_String(lpData, &Argv[0]))) return TRUE;

	Field.i = 0;
	if (!Parse_ThemeField(&Field, tszString))
	{
		return TRUE;
	}
	Insert_Field_Code(lpData, &Field);
	return FALSE;
}


/*
Theme
%[T(index#)]

*/
BOOL MessageObject_Theme(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPOUTPUT_THEME lpTheme;
	INT  iThemeColor;
	THEME_FIELD Field;

	lpTheme = GetTheme();
	if (!lpTheme) return TRUE;

	//	Get theme
	if (! Argc || Object_Get_Int(lpData, &Argv[0], &iThemeColor) || iThemeColor < 0 || iThemeColor > MAX_COLORS) return TRUE;

	if (!iThemeColor)
	{
		// reset case
		Field.i = 0;
		Field.Settings.doReset = 1;
		Insert_Field_Code(lpData, &Field);
	}
	else
	{
		Insert_Field_Code(lpData, &lpTheme->ThemeFieldsArray[iThemeColor]);
	}

	return FALSE;
}


/*
Formatter
%[F(index#)]

*/
BOOL MessageObject_Formatter(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	INT  iThemeColor;

	//	Get theme
	if (! Argc || Object_Get_Int(lpData, &Argv[0], &iThemeColor) || iThemeColor < 0 || iThemeColor > MAX_COLORS) return TRUE;

	SetAutoTheme(iThemeColor);
	return FALSE;
}


/*
RTheme
%[R(index#)]

*/
BOOL MessageObject_RTheme(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPBUFFER lpBuffer;
	DWORD    dwPos, dwLen;
	CHAR     c;
	LPOUTPUT_THEME lpTheme;
	INT  iThemeColor;

	lpTheme = GetTheme();
	if (!lpTheme) return FALSE;

	//	Get theme
	if (! Argc || Object_Get_Int(lpData, &Argv[0], &iThemeColor) || iThemeColor < 0 || iThemeColor > 10000) return TRUE;

	if (iThemeColor >= 1000)
	{
		SetAutoTheme(iThemeColor);
		return FALSE;
	}

	if (iThemeColor > MAX_COLORS) return TRUE;

	lpBuffer = lpData->lpOutBuffer;
	dwPos    = lpBuffer->len;
	dwLen    = 0;

	while (dwPos)
	{
		c = lpBuffer->buf[dwPos-1];

		if (c != ' ')
		{
			// we only reverse over spaces...
			break;
		}

		dwLen++;
		dwPos--;
	}

	lpBuffer->len -= dwLen;
	Insert_Field_Code(lpData, &lpTheme->ThemeFieldsArray[iThemeColor]);

	c = ' ';
	while ( dwLen-- )
	{
		Put_Buffer(lpBuffer, &c, 1);
	}
	return FALSE;
}


/*

%[Theme(Name)]

*/
BOOL MessageObject_SetTheme(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
    LPTSTR tszSubTheme;

	//	Get theme
	if (! Argc  )
	{
		tszSubTheme = NULL;
	}
	else
	{
		tszSubTheme = Object_Get_String(lpData, &Argv[0]);
	}

	return MessageObject_SetSubTheme(tszSubTheme);
}


// also used from Tcl.c to set subthemes.
BOOL MessageObject_SetSubTheme(LPTSTR tszSubTheme)
{
	LPOUTPUT_THEME lpTheme, lpNewTheme;

	lpTheme = GetTheme();
	if (!lpTheme) return FALSE;

	if (!tszSubTheme)
	{
		lpNewTheme = LookupTheme(lpTheme->iTheme);
	}
	else
	{
		while (InterlockedExchange(&lThemeLock, TRUE)) SwitchToThread();
		lpNewTheme = Parse_Theme(lpTheme, lpTheme->iTheme, tszSubTheme);
		InterlockedExchange(&lThemeLock, FALSE);
	}

	if (lpNewTheme)
	{
		SetTheme(lpNewTheme);
		// remove the extra reference
		FreeShared(lpNewTheme);
		return FALSE;
	}

	return TRUE;
}


/*

%[Mark]

*/
BOOL MessageObject_Mark(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	lpData->dwMarkPosition = lpData->lpOutBuffer->len;

	return FALSE;
}


/*

%[Fill(width,string)]

*/
BOOL MessageObject_Fill(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPBUFFER lpBuffer;
	DWORD  dwLen, dwCount, dwFill;
	LPSTR  szString, szFill;
	INT    iWidth;

	dwCount = 0;

	if (! Argc || Object_Get_Int(lpData, &Argv[0], &iWidth) || iWidth <= 0) return TRUE;

	if (Argc == 1)
	{
		// no fill string specified;
		szFill = " ";
		dwFill = 1;
	}
	else
	{
		szFill = Object_Get_String(lpData, &Argv[1]);
		if (!szFill)
		{
			szFill = " ";
			dwFill = 1;
		}
		else
		{
			dwFill = strlen(szFill);
		}
	}

	lpBuffer = lpData->lpOutBuffer;

	if (lpData->dwMarkPosition > lpBuffer->len)
	{
		// rut ro...
		return TRUE;
	}

	dwLen    = lpBuffer->len - lpData->dwMarkPosition;
	szString = &lpBuffer->buf[lpData->dwMarkPosition];

	while (dwLen)
	{
		if (!*szString)
		{
			// there shouldn't be embedded 0's...
			return TRUE;
		}
		if (*szString == 27 && dwLen > 3 && szString[1] == '[')
		{
			// skip over and don't count this ANSI control string
			szString += 2;
			for (dwLen -= 2; dwLen-- && *szString++ != 'm' ; );
			continue;
		}
		dwCount++;
		dwLen--;
		szString++;
	}

	iWidth -= dwCount;
	if (iWidth <= 0)
	{
		// nothing to do
		return FALSE;
	}

	dwLen = 0;
	for ( ; iWidth ; iWidth--)
	{
		if (dwLen >= dwFill) dwLen = 0;
		Put_Buffer(lpBuffer, &szFill[dwLen++], 1);
	}

	return FALSE;
}


/*

%[Pad(width,string)]

*/
BOOL MessageObject_Pad(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPBUFFER lpBuffer;
	DWORD  dwLen, dwCount, dwFill;
	LPSTR  szString, szFill;
	INT    iWidth;
	LPSTR  buf;

	dwCount = 0;

	if (! Argc || Object_Get_Int(lpData, &Argv[0], &iWidth) || iWidth <= 0) return TRUE;

	if (Argc == 1)
	{
		// no fill string specified;
		szFill = " ";
		dwFill = 1;
	}
	else
	{
		szFill = Object_Get_String(lpData, &Argv[1]);
		if (!szFill)
		{
			szFill = " ";
			dwFill = 1;
		}
		else
		{
			dwFill = strlen(szFill);
		}
	}

	lpBuffer = lpData->lpOutBuffer;

	if (lpData->dwMarkPosition > lpBuffer->len)
	{
		// rut ro...
		return TRUE;
	}

	dwLen    = lpBuffer->len - lpData->dwMarkPosition;
	szString = &lpBuffer->buf[lpData->dwMarkPosition];

	while (dwLen)
	{
		if (!*szString)
		{
			// there shouldn't be embedded 0's...
			return TRUE;
		}
		if (*szString == 27 && dwLen > 3 && szString[1] == '[')
		{
			// skip over and don't count this ANSI control string
			szString += 2;
			for (dwLen -= 2; dwLen-- && *szString++ != 'm' ; );
			continue;
		}
		dwCount++;
		dwLen--;
		szString++;
	}

	iWidth -= dwCount;
	if (iWidth <= 0)
	{
		// nothing to do
		return FALSE;
	}

	dwLen = 0;
	// make sure buffer large enough
	if (lpBuffer->size < lpBuffer->len + iWidth)
	{
		if (AllocateBuffer(lpBuffer, lpBuffer->len + iWidth))
		{
			return TRUE;
		}
	}

	buf = &lpBuffer->buf[lpData->dwMarkPosition];
	MoveMemory(&buf[iWidth], buf, lpBuffer->len - lpData->dwMarkPosition);

	lpBuffer->len += iWidth;

	for ( ; iWidth ; iWidth--)
	{
		if (dwLen >= dwFill) dwLen = 0;
		*buf++ = szFill[dwLen++];
	}

	return FALSE;
}

BOOL MessageObject_Save(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	if (lpData->lpSavedTheme)
	{
		FreeShared(lpData->lpSavedTheme);
	}

	lpData->lpSavedTheme = GetTheme();
	if (lpData->lpSavedTheme)
	{
		AllocateShared(lpData->lpSavedTheme, NULL, 0);
	}

	lpData->SavedThemes.i = lpData->CurrentThemes.i;
	return FALSE;
}


BOOL MessageObject_Restore(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	if (lpData->lpSavedTheme)
	{
		SetTheme(lpData->lpSavedTheme);
		FreeShared(lpData->lpSavedTheme);
		lpData->lpSavedTheme = NULL;
	}

	Restore_Theme(lpData);
	return FALSE;
}


/*

%[Msg(number)]

*/
BOOL MessageObject_Msg(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	PCONNECTION_INFO lpConnection;
	LPCLIENT lpClient;
	INT  iMsg;
	LPTSTR tszMsg;

	//	Get theme
	if (! Argc || Object_Get_Int(lpData, &Argv[0], &iMsg) || iMsg <= 0 || iMsg > MAX_MESSAGES) return TRUE;

	// connection info (dwUniqueId) -> client info -> lpUser -> ftpvariables
	// kind of round about but we need the client lock anyway...
	lpConnection	= GETOFFSET(lpData->DataOffsets, DATA_CONNECTION);
	if (!lpConnection) return TRUE;

	if ( !(lpClient = LockClient(lpConnection->dwUniqueId)) )
	{
		return TRUE;
	}

	if (!lpClient->lpUser)
	{
		UnlockClient(lpConnection->dwUniqueId);
		return TRUE;
	}

	tszMsg = lpClient->lpUser->FtpVariables.tszMsgStringArray[iMsg-1];
	if (!tszMsg) tszMsg = "";

	Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, tszMsg);

	UnlockClient(lpConnection->dwUniqueId);
	return FALSE;
}


/*

  %[free(path)(size)]

  */
BOOL MessageObject_Free(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	UINT64	FBC, TB, FB;
	DOUBLE	dblFreeSpace;
	LPSTR	Path;
	INT		iDivider = 1;

	if (Argc &&
		(Path = Object_Get_String(lpData, &Argv[0])) &&
		GetDiskFreeSpaceEx(Path, (PULARGE_INTEGER)&FBC, (PULARGE_INTEGER)&TB, (PULARGE_INTEGER)&FB) )
	{
		//	Get divider
		if (Argc == 1 ||
			Object_Get_Int(lpData, &Argv[1], &iDivider) || ! iDivider) iDivider	= 1;
		//	Calculate amount of free space on chosen accuracy
		dblFreeSpace	= ((INT64)(FBC / 1024)) / 1.0;
	}
	else
	{
		if (Argc < 2 ||
			Object_Get_Int(lpData, &Argv[1], &iDivider) || ! iDivider) iDivider	= 1;
		dblFreeSpace	= 0.;
	}
	//	Append result to buffer
	return Cookie_Convert_Special(lpData, iDivider, dblFreeSpace);
}




/*

  %[credits(suffix)(section)]

  */
BOOL MessageObject_Credits(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	PUSERFILE	pUserFile;
	INT			iDivider, iSection;

	//	Get userfile
	pUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);
	//	Get parameters
	if (! Argc)
	{
		iDivider	= 1;
		iSection	= GETOFFSET(lpData->DataOffsets, DATA_CSECTION);
	}
	else
	{
		//	Get divider
		if (Object_Get_Int(lpData, &Argv[0], &iDivider) || ! iDivider) iDivider	= 1;
		//	Get section
		if (Argc < 2 ||
			Object_Get_Int(lpData, &Argv[1], &iSection) ||
			iSection >= MAX_SECTIONS || iSection < 0)
		{
			iSection	= GETOFFSET(lpData->DataOffsets, DATA_CSECTION);
		}
	}
	//	Append result to output buffer
	return Cookie_Convert_Special(lpData, iDivider, pUserFile->Credits[iSection] / 1.);
}


/*

%[sharedcredits(suffix)(section)]

*/
BOOL MessageObject_SharedCredits(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	PUSERFILE	pUserFile;
	INT			iDivider, iSection;
	TCHAR		pBuffer[_MAX_NAME+1];
	CHAR        szTemp[14], szFormat[20], *pDot;
	INT         iCredit, iStat, iShare;

	//	Get userfile
	pUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);
	//	Get parameters
	if (! Argc)
	{
		iDivider	= 1;
		iSection	= GETOFFSET(lpData->DataOffsets, DATA_CSECTION);
	}
	else
	{
		//	Get divider
		if (Object_Get_Int(lpData, &Argv[0], &iDivider) || ! iDivider) iDivider	= 1;
		//	Get section
		if (Argc < 2 ||
			Object_Get_Int(lpData, &Argv[1], &iSection) ||
			iSection >= MAX_SECTIONS || iSection < 0)
		{
			iSection	= GETOFFSET(lpData->DataOffsets, DATA_CSECTION);
		}
	}

	ZeroMemory(pBuffer, sizeof(pBuffer));
	if (!Config_Get_SectionNum(iSection, pBuffer, &iCredit, &iStat, &iShare))
	{
		// section not defined
		lpData->szFormat[lpData->dwFormat - 2]	= 'f';
		Cookie_Convert_Special(lpData, iDivider, pUserFile->Credits[iSection] / 1.);
		return TRUE;
	}

	if (iCredit == iShare && iCredit == iStat)
	{
		// not shareing
		lpData->szFormat[lpData->dwFormat - 2]	= 'f';
		Cookie_Convert_Special(lpData, iDivider, pUserFile->Credits[iSection] / 1.);
		return FALSE;
	}

	// we're sharing credits...
	sprintf(szTemp, "Shared (#%d)", iShare);

	// now, find any "." in the format specification which was used for floats but means something different
	// for strings.
	strncpy_s(szFormat, sizeof(szFormat), lpData->szFormat, _TRUNCATE);
	pDot = strchr(szFormat, '.');
	if (pDot)
	{
		*pDot++ = 's';
		*pDot = 0;
	}
	else
	{
		szFormat[lpData->dwFormat - 2]	= 's';
	}
	Put_Buffer_Format(lpData->lpOutBuffer, szFormat, szTemp);
	return FALSE;
}



/*

  %[limit(limit)]

  */
BOOL MessageObject_Limit(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPUSERFILE	lpUserFile;
	INT			iLimit;

	//	Get userfile
	lpUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	if (Argc &&
		! Object_Get_Int(lpData, &Argv[0], &iLimit) &&
		iLimit >= 0 && iLimit < 5)
	{
		//	Append result to output buffer
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, lpUserFile->Limits[iLimit]);
	}
	return FALSE;
}



/*

  %[ratio(section)]

  */
BOOL MessageObject_Ratio(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	PUSERFILE	pUserFile;
	CHAR		Temp[24];
	INT			iSection;

	//	Get userfile
	pUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);
	//	Get credit section
	if ((Argc < 1) ||
		Object_Get_Int(lpData, &Argv[0], &iSection) ||
		((iSection >= MAX_SECTIONS) && (iSection != 999)) || (iSection < -1))
	{
		iSection	= GETOFFSET(lpData->DataOffsets, DATA_CSECTION);
	}

	if ((iSection == 999) || (! pUserFile->Ratio[iSection]))
	{
		//	Ratio 0 = Unlimited
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, tszLeechName);
	}
	else
	{
		//	Copy result to temporary buffer
		sprintf(Temp, "1:%i", pUserFile->Ratio[iSection]);
		//	Append temporary buffer to output buffer
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, Temp);
	}
	return FALSE;
}


/*

%[rationum(section)]

*/
BOOL MessageObject_RatioNum(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	PUSERFILE	pUserFile;
	INT			iSection;

	//	Get userfile
	pUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);
	//	Get credit section
	if (Argc < 1 ||
		Object_Get_Int(lpData, &Argv[0], &iSection) ||
		iSection >= MAX_SECTIONS || iSection < 0)
	{
		iSection	= GETOFFSET(lpData->DataOffsets, DATA_CSECTION);
	}

	Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, pUserFile->Ratio[iSection]);
	return FALSE;
}




/*

  %[group(group #)]

  */
BOOL MessageObject_Group(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	PUSERFILE	pUserFile;
	LPSTR		GroupName;
	PCHAR		Offset;
	CHAR		GroupList[MAX_GROUPS * (_MAX_NAME + 1)];
	INT			Gid, i;

	//	Get Userfile
	pUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	if (! Argc)
	{
		Offset		= GroupList;
		Offset[0]	= '\0';
		//	Add all groups to list
		for (i = 0 ; i < MAX_GROUPS && (Gid = pUserFile->Groups[i]) >= 0 ; i++)
		{
			//	Convert group id to name
			GroupName	= Gid2Group(Gid);
			if (! GroupName) continue;
			//	Append group name to buffer
			Offset	+= sprintf(Offset, (Offset != GroupList ? " %s" : "%s"), GroupName);
		}
		//	Append result to buffer
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, GroupList);
		return FALSE;
	}
	else
	{
		//	Get entry #
		if (Object_Get_Int(lpData, &Argv[0], &i) ||
			i < 0 || i >= MAX_GROUPS ) i = 0;
		//	Get group id
		Gid	= pUserFile->Groups[i];
		//	Resolve group id to name
		GroupName	= (Gid < 0 ? NULL : Gid2Group(Gid));
	}
	//	Make sure we got something to print
	if (! GroupName) GroupName	= "";
	//	Append result to output buffer
	Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, GroupName);
	return FALSE;
}

/*

  %[admingroups(admingroup #)]

  */
BOOL MessageObject_AdminGroups(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	TCHAR   tszGroupList[MAX_GROUPS * (_MAX_NAME + 1)];
	TCHAR  *lpPos;
	LPTSTR  tszGroupName;
	int     iLeft, i, Gid;
	size_t  stLen;
	LPUSERFILE lpUserFile;

	lpUserFile = GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	if (Argc)
	{
		//	Get entry #
		if (Object_Get_Int(lpData, &Argv[0], &i) ||	i < 0 || i >= MAX_GROUPS )
		{
			i = 0;
		}
		//	Get group id
		Gid = lpUserFile->AdminGroups[i];
		//	Resolve group id to name
		tszGroupName = (Gid < 0 ? NULL : Gid2Group(Gid));
		if (! tszGroupName) tszGroupName	= _T("");
		//	Append result to output buffer
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, tszGroupName);
		return FALSE;
	}

	// display all admin groups
	lpPos = tszGroupList;
	*lpPos = 0;
	iLeft = sizeof(tszGroupList) / sizeof(TCHAR);

	//	Add all groups to list
	for (i = 0 ; i < MAX_GROUPS && (Gid = lpUserFile->AdminGroups[i]) >= 0 ; i++)
	{
		if (!(tszGroupName = Gid2Group(lpUserFile->AdminGroups[i])))
		{
			continue;
		}
		if (iLeft > 0 && lpPos != tszGroupList)
		{
			*lpPos++ = _T(' ');
			iLeft--;
		}
		stLen = _tcslen(tszGroupName);
		if (iLeft < (INT) stLen)
		{
			break;
		}
		CopyMemory(lpPos, tszGroupName, (stLen+1)*sizeof(TCHAR));
		iLeft -= stLen;
		lpPos += stLen*sizeof(TCHAR);
	}
	Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, tszGroupList);
	return FALSE;
}



/*

  %[ip(ip #)]

  */
BOOL MessageObject_Ip(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	PUSERFILE	pUserFile;
	LPSTR		IpString;
	INT			i;

	//	Get userfile
	pUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);
	//	Get entry #
	if (! Argc ||
		Object_Get_Int(lpData, &Argv[0], &i) ||
		i < 1 || i > MAX_IPS)
	{
		i	= 1;
	}
	//	Get ip string
	IpString	= pUserFile->Ip[i - 1];
	//	Append result to 
	Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, IpString);

	if (IpString[0]) return FALSE;
	return TRUE;
}



/*

  %[execute(filename)(parameters)(prefix)]

  */
BOOL MessageObject_Execute(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	EVENT_COMMAND	Event;

	ZeroMemory(&Event, sizeof(EVENT_COMMAND));
	//	Get parameters
	switch (Argc)
	{
	case 3:
		//	Get prefix
		Event.tszOutputPrefix	= Object_Get_String(lpData, &Argv[2]);
	case 2:
		//	Get arguments
		Event.tszParameters	= Object_Get_String(lpData, &Argv[1]);
	case 1:
		//	Get command
		Event.tszCommand	= Object_Get_String(lpData, &Argv[0]);
		break;
	}

	if (Event.tszCommand)
	{
		Event.lpDataSource	= lpData->lpData;
		Event.dwDataSource	= lpData->dwData;
		Event.lpOutputBuffer	= lpData->lpOutBuffer;
		//	Execute command with chosen arguments and prefix
		RunEvent(&Event);
	}
	return FALSE;
}


/*

%[RemoveBlankLine]

*/
BOOL MessageObject_RemoveBlankLine(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPBUFFER lpBuffer;
	DWORD    dwPrefix;

	lpBuffer = lpData->lpOutBuffer;
	dwPrefix = lpData->dwPrefix[0];

	if (lpBuffer->len <= 2+lpData->dwPrefix[0]*2) 
	{
		if (!strnicmp(lpData->szPrefix[0], &lpBuffer->buf[lpBuffer->len - dwPrefix], dwPrefix))
		{
			lpBuffer->len -= dwPrefix;
		}
		return TRUE;
	}

	if (!strnicmp(lpData->szPrefix[0], &lpBuffer->buf[lpBuffer->len - dwPrefix], dwPrefix) &&
		!strnicmp("\r\n", &lpBuffer->buf[lpBuffer->len - dwPrefix -2], 2))
	{
		lpBuffer->len -= 2 + dwPrefix;
	}

	return FALSE;
}



/*

  %[include(filename)(permissions)]

  */
BOOL MessageObject_Include(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	PUSERFILE	pUserFile;
	LPSTR		FileName, AccessList;
	LPBYTE      pBuffer;

	//	Get userfile
	pUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);
	//	Get Filename
	if (! Argc ||
		! (FileName = Object_Get_String(lpData, &Argv[0]))) return TRUE;
	//	Get access list
	if (Argc == 1 ||
		(AccessList = Object_Get_String(lpData, &Argv[1])) &&
		! HavePermission(pUserFile, AccessList))
	{
		//	Load Message File
		if (pBuffer = Message_Load(FileName))
		{
			//	Show MessageFile
			Message_Compile(pBuffer, lpData->lpOutBuffer, TRUE, lpData->lpData, lpData->dwData, lpData->szPrefix[0], lpData->szPrefix[1]);
			//	Free memory
			Free(pBuffer);
			return FALSE;
		}
	}
	return FALSE;
}


/*

%[HasFlag(flags)]
  outputs 1 for yes, nothing for no
*/
BOOL MessageObject_HasFlag(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	PUSERFILE	pUserFile;
	LPSTR		AccessList;

	//	Get userfile
	pUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);
	//	Get Filename
	if (!Argc ||
		!(AccessList = Object_Get_String(lpData, &Argv[0])))
	{
		return FALSE;
	}

	if (HasFlag(pUserFile, AccessList))
	{
		// failed
		return FALSE;
	}
	// ok
	Put_Buffer_Format(lpData->lpOutBuffer, "%d", 1);

	return TRUE;
}


/*

%[CallerHasFlag(flags)]
outputs 1 for yes, nothing for no
*/
BOOL MessageObject_CallerHasFlag(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPFTPUSER	lpFtpUserCaller;
	LPSTR		AccessList;

	//	Get userfile
	lpFtpUserCaller	= GETOFFSET(lpData->DataOffsets, DATA_USER_CALLER);
	//	Get Filename
	if (!lpFtpUserCaller || !lpFtpUserCaller->UserFile || !Argc ||
		!(AccessList = Object_Get_String(lpData, &Argv[0])))
	{
		return FALSE;
	}

	if (HasFlag(lpFtpUserCaller->UserFile, AccessList))
	{
		// failed
		return FALSE;
	}
	// ok
	Put_Buffer_Format(lpData->lpOutBuffer, "%d", 1);

	return TRUE;
}


/*

%[IsGAdmin]
outputs * for yes, nothing for no
*/
BOOL MessageObject_IsGAdmin(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	PUSERFILE	pUserFile;
	INT32       GID, i;

	//	Get userfile
	pUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);
	GID         = GETOFFSET(lpData->DataOffsets, DATA_SSECTION);

	if (GID < 0)
	{
		FormatString(lpData->lpOutBuffer, lpData->szFormat, "");
		return FALSE;
	}

	//  Loop through groups checking for admin rights
	for (i = 0;i < MAX_GROUPS && pUserFile->AdminGroups[i] != -1;i++)
	{
		if (pUserFile->AdminGroups[i] == GID)
		{
			FormatString(lpData->lpOutBuffer, lpData->szFormat, "*");
			return TRUE;
		}
	}
	FormatString(lpData->lpOutBuffer, lpData->szFormat, "");
	return FALSE;
}




VOID MessageConvertBrackets(LPSTR szFrom, LPSTR szTo)
{
	LPSTR szStart = szFrom;
	CHAR  szPrev;
	BOOL  bDouble;

	szPrev = 0;
	bDouble = FALSE;
	for( ; *szFrom ; szFrom++ , bDouble = FALSE)
	{
		switch(*szFrom)
		{
		case '{':
			if (szPrev == '{')
			{
				szTo[-1] = '{';
				bDouble = TRUE;
			}
			else
			{
				*szTo++ = '[';
			}
			break;
		case '}':
			if (szPrev == '}')
			{
				szTo[-1] = '}';
				bDouble = TRUE;
			}
			else
			{
				*szTo++ = ']';
			}
			break;
		case '<':
			// not the first character and previous was also a <, then replace previous ( with <
			if (szPrev == '<')
			{
				szTo[-1] = '<';
				bDouble = TRUE;
			}
			else
			{
				*szTo++ = '(';
			}
			break;
		case '>':
			// not the first character and previous was also a <, then replace previous ) with >
			if (szPrev == '>')
			{
				szTo[-1] = '>';
				bDouble = TRUE;
			}
			else
			{
				*szTo++ = ')';
			}
			break;
		default:
			*szTo++ = *szFrom;
		}
		if (bDouble)
		{
			szPrev = 0;
		}
		else
		{
			szPrev = *szFrom;
		}
	}
	*szTo++ = '\n';
	*szTo = 0;
}



/*

%[IF(cookie)(THEN)(true-action)(false-action)]
%[IF(cookie)(EQUAL)(string)(true-action)(false-action)]

*/
BOOL MessageObject_IF(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	CHAR  *pBuf;
	LPSTR szCookie, szTemp, szCompare, szTrue, szFalse, szString;
	DWORD dwLen, dwLen2;
	INT   args;
	MESSAGEDATA		MessageData;
	BOOL  bCookie;

	if (Argc < 3 || !(szCookie = Object_Get_String(lpData, &Argv[0])) ||
		!(szTemp = Object_Get_String(lpData, &Argv[1])) )
	{
		return FALSE;
	}

	args = 2;
	if (!stricmp("THEN", szTemp))
	{
		szCompare = NULL;
	}
	else if (!stricmp("EQUAL", szTemp))
	{
		szCompare = Object_Get_String(lpData, &Argv[args++]);
	}
	else
	{
		return FALSE;
	}

	if ((args >= Argc) ||
		!(szTrue = Object_Get_String(lpData, &Argv[args++])))
	{
		return FALSE;
	}


	szFalse = NULL;
	if (args < Argc)
	{
		szFalse = Object_Get_String(lpData, &Argv[args]);
	}

	// just want to allocate 1 string to pick max of cookie,true,false
	dwLen  = strlen(szCookie);
	dwLen2 = strlen(szTrue);
	if (dwLen2 > dwLen) dwLen = dwLen2;
	dwLen2 = (szFalse ? strlen(szFalse) : 0);
	if (dwLen2 > dwLen) dwLen = dwLen2;
	dwLen += 2;

	szString = Allocate("IF:string", dwLen);
	if (!szString) return FALSE;

	MessageConvertBrackets(szCookie, szString);

	pBuf = Message_PreCompile(szString, NULL);

	if (pBuf == NULL)
	{
		// the cookie argument didn't parse at all...
		Free(szString);
		return FALSE;
	}

	//	Reset unified variable storage
	CopyMemory(&MessageData, lpData, sizeof(MESSAGEDATA));

	MessageData.szFormat = 0;
	MessageData.dwFormat = 0;

	dwLen = lpData->lpOutBuffer->len;
	Compile_Message(&MessageData, pBuf, TRUE, FALSE);
	dwLen = lpData->lpOutBuffer->len - dwLen;
	lpData->lpOutBuffer->len -= dwLen;

	if (szCompare)
	{
		if ((dwLen == strlen(szCompare)) && !strnicmp(&lpData->lpOutBuffer->buf[lpData->lpOutBuffer->len], szCompare, dwLen))
		{
			szTemp = szTrue;
		}
		else
		{
			szTemp = szFalse;
		}
	}
	else
	{
		if (dwLen)
		{
			szTemp = szTrue;
		}
		else
		{
			szTemp  = szFalse;
		}
	}

	Free(pBuf);

	if (!szTemp)
	{
		Free(szString);
		return TRUE;
	}

	bCookie = FALSE;

	MessageConvertBrackets(szTemp, szString);

	pBuf = Message_PreCompile(szString, NULL);

	if (pBuf == NULL)
	{
		// the cookie argument didn't parse at all...
		Free(szString);
		return FALSE;
	}

	Compile_Message(&MessageData, pBuf, TRUE, FALSE);
	Free(pBuf);
	Free(szString);
	return TRUE;
}



BOOL MessageObject_SiteName(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();

	if (FtpSettings.tszSiteName)
	{
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, FtpSettings.tszSiteName);
	}
	else
	{
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, "");
	}

	InterlockedExchange(&FtpSettings.lStringLock, FALSE);
	return TRUE;
}


BOOL MessageObject_FtpSettingsCookie(LPMESSAGEDATA lpData, LPTSTR tszString, DWORD dwString, LONG volatile *plLock)
{
	LPTSTR         tszPreCompiled;
	PCHAR          pBuf;

	if (!tszString)
	{
		if (plLock) InterlockedExchange(plLock, FALSE);
		return TRUE;
	}

	// need to make a copy of the string since cookie processing will trash it, and we need a \n on end
	tszPreCompiled = Allocate(_T("FormattedCookieString"), (dwString+2)*sizeof(TCHAR));
	if (!tszPreCompiled)
	{
		if (plLock) InterlockedExchange(plLock, FALSE);
		return FALSE;
	}
	CopyMemory(tszPreCompiled, tszString, dwString*sizeof(TCHAR));
	tszPreCompiled[dwString] = _T('\n');
	tszPreCompiled[dwString+1] = 0;

	// release the string lock
	if (plLock) InterlockedExchange(plLock, FALSE);

	if (pBuf = Message_PreCompile(tszPreCompiled, NULL))
	{
		Compile_Message(lpData, pBuf, TRUE, FALSE);
		Free(pBuf);
	}

	Free(tszPreCompiled);
	return TRUE;
}


BOOL MessageObject_SiteBoxHeader(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();

	return MessageObject_FtpSettingsCookie(lpData, FtpSettings.tszSiteBoxHeader, FtpSettings.dwSiteBoxHeader, &FtpSettings.lStringLock);
}


BOOL MessageObject_SiteBoxFooter(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();

	return MessageObject_FtpSettingsCookie(lpData, FtpSettings.tszSiteBoxFooter, FtpSettings.dwSiteBoxFooter, &FtpSettings.lStringLock);
}


BOOL MessageObject_HelpBoxHeader(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();

	return MessageObject_FtpSettingsCookie(lpData, FtpSettings.tszHelpBoxHeader, FtpSettings.dwHelpBoxHeader, &FtpSettings.lStringLock);
}


BOOL MessageObject_HelpBoxFooter(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();

	return MessageObject_FtpSettingsCookie(lpData, FtpSettings.tszHelpBoxFooter, FtpSettings.dwHelpBoxFooter, &FtpSettings.lStringLock);
}


BOOL MessageObject_SiteCmd(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPFTPUSER lpFtpUser;

	lpFtpUser = GetFtpUser();
	if (!lpFtpUser || !lpFtpUser->FtpVariables.tszCurrentCommand)
	{
		return FALSE;
	}

	FormatString(lpData->lpOutBuffer, lpData->szFormat, lpFtpUser->FtpVariables.tszCurrentCommand);

	return TRUE;

}


BOOL MessageObject_Service(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPIOSERVICE	lpService;
	LPSTR		szServiceName;
	LPTSTR      tszTemp;
	DWORD       n;
	TCHAR       pBuffer[16*1024];  // insanely huge temp buffer for device names just to be safe...
	INT			iType/*, iDivider*/;

	//	Find service
	if (Argc < 2 ||
		! (szServiceName = Object_Get_String(lpData, &Argv[0])) ||
		Object_Get_Int(lpData, &Argv[1], &iType) ||
		! (lpService = Service_FindByName(szServiceName)))
	{
		return TRUE;
	}

	switch (iType)
	{
	case SERVICE_NAME:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, lpService->tszName);
		break;
	case SERVICE_ACTIVE_STATE:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, (lpService->bActive ? _T("Active") : _T("Inactive")));
		break;
	case SERVICE_HOST_IP:
		if (lpService->lAddress == INADDR_NONE)
		{
			tszTemp = _T("<invalid>");
		}
		else if (lpService->lAddress == INADDR_ANY)
		{
			tszTemp = _T("<any>");
		}
		else
		{
			tszTemp = inet_ntoa(*((struct in_addr *) &lpService->lAddress));
		}
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, tszTemp);
		break;
	case SERVICE_HOST_PORT:
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, lpService->sPort);
		break;
	case SERVICE_DEVICE_NAME:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, (lpService->lpDevice ? lpService->lpDevice->tszName : ""));
		break;
	case SERVICE_DEVICE_ID:
		lpData->szFormat[lpData->dwFormat - 2]	= 'd'; // %ld = %d here
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, lpService->lpDevice->lDeviceNum);
		break;
	case SERVICE_USERS:
		//	Show # of users using service
		lpData->szFormat[lpData->dwFormat - 2]	= 'd';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, lpService->lClients);
		break;
	case SERVICE_MAX_CLIENTS:
		lpData->szFormat[lpData->dwFormat - 2]	= 'd';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, lpService->lMaxClients);
		break;
	case SERVICE_TRANSFERS:
		//	Show # of active transfers
		lpData->szFormat[lpData->dwFormat - 2]	= 'i';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, lpService->lTransfers);
		break;
	case SERVICE_ALLOWED_USERS:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, (lpService->tszAllowedUsers ? lpService->tszAllowedUsers : _T("<all>")));
		break;
	case SERVICE_CERTIFICATE_IN_USE:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		switch (lpService->dwFoundCredentials)
		{
		case 1:
			tszTemp = lpService->tszServiceValue;
			break;
		case 2:
			tszTemp = lpService->tszHostValue;
			break;
		case 3:
			tszTemp = _T("ioFTPD");
			break;
		default:
			tszTemp = _T("<disabled/invalid>");
		}
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, tszTemp);
		break;
	case SERVICE_CERTIFICATE_WHERE:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		switch (lpService->dwFoundCredentials)
		{
		case 1:
			tszTemp = _T("Certificate_Name");
			break;
		case 2:
			tszTemp = _T("Device HOST=");
			break;
		case 3:
			tszTemp = _T("default");
			break;
		default:
			tszTemp = _T("");
		}
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, tszTemp);
		break;
	case SERVICE_ENCRYPTION_TYPE:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, (lpService->bExplicitEncryption ? "Explicit" : "Implicit"));
		break;
	case SERVICE_REQUIRE_SECURE_AUTH:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, (lpService->tszRequireSecureAuth ? lpService->tszRequireSecureAuth : _T("<disabled>")));
		break;
	case SERVICE_REQUIRE_SECURE_DATA:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, (lpService->tszRequireSecureData ? lpService->tszRequireSecureData : _T("<disabled>")));
		break;
	case SERVICE_EXTERNAL_IDENTITY:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, (lpService->tszBncAddressArray[0] ? _T("True") : _T("False")));
		break;
	case SERVICE_MESSAGE_LOCATION:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		// message location must exist for it to be in service list
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, (lpService->tszMessageLocation ? lpService->tszMessageLocation : "<disabled>"));
		break;
	case SERVICE_DATA_DEVICES:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		if (!lpService->dwDataDevices)
		{
			Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, _T("<default>"));
		}
		else
		{
			tszTemp = pBuffer;
			pBuffer[0] = 0;
			for(n=0;n<lpService->dwDataDevices;n++)
			{
				if (n)
				{
					*tszTemp++ = _T(',');
					*tszTemp++ = _T(' ');
				}
				_tcscpy(tszTemp, lpService->lpDataDevices[n]->tszName);
				tszTemp += _tcslen(lpService->lpDataDevices[n]->tszName);
			}
			Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, pBuffer);
		}
		break;
	case SERVICE_DATA_DEVICE_SELECTION:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, (lpService->bRandomDataDevices ? _T("Random") : _T("FIFO")));
		break;
	case SERVICE_ACCEPTS_READY:
		lpData->szFormat[lpData->dwFormat - 2]	= 'u';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, lpService->lAcceptClients);
		break;
	}

	Service_ReleaseLock(lpService);
	return FALSE;
}



BOOL MessageObject_Device(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPIODEVICE	lpDevice;
	LPTSTR      tszTemp;
	LPSTR       szDeviceName;
	TCHAR       pBuffer[16*1024];  // insanely huge temp buffer just to be safe...
	INT			iType, iDivider;
	LPIOPORT    lpPort;

	//	Find device
	if (Argc < 2 ||
		! (szDeviceName = Object_Get_String(lpData, &Argv[0])) ||
		Object_Get_Int(lpData, &Argv[1], &iType) ||
		! (lpDevice = Device_FindByName(szDeviceName)))
	{
		return TRUE;
	}

	if (Argc <= 2 || Object_Get_Int(lpData, &Argv[2], &iDivider) || ! iDivider) iDivider	= 1;

	switch (iType)
	{
	case DEVICE_NAME:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, lpDevice->tszName);
		break;
	case DEVICE_ID:
		lpData->szFormat[lpData->dwFormat - 2]	= 'd'; // %ld = %d here
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, lpDevice->lDeviceNum);
		break;
	case DEVICE_ACTIVE_STATE:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, (lpDevice->bActive ? _T("Active") : _T("Inactive")));
		break;
	case DEVICE_BIND_IP:
		if (lpDevice->lBindAddress == INADDR_NONE)
		{
			tszTemp = _T("<invalid>");
		}
		else if (lpDevice->lBindAddress == INADDR_ANY)
		{
			tszTemp = _T("<any>");
		}
		else
		{
			tszTemp = inet_ntoa(*((struct in_addr *) &lpDevice->lBindAddress));
		}
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, tszTemp);
		break;
	case DEVICE_HOST_IP:
		if (lpDevice->lHostAddress == INADDR_NONE)
		{
			tszTemp = _T("<invalid>");
		}
		else if (lpDevice->lHostAddress == INADDR_ANY)
		{
			tszTemp = _T("<any>");
		}
		else
		{
			tszTemp = inet_ntoa(*((struct in_addr *) &lpDevice->lHostAddress));
		}
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, tszTemp);
		break;
	case DEVICE_PASV_PORTS:
		tszTemp = pBuffer;
		pBuffer[0] = 0;
		for (lpPort = lpDevice->lpPort ; lpPort ; lpPort = lpPort->lpNext)
		{
			if (tszTemp != pBuffer)
			{
				*tszTemp++ = _T(',');
				*tszTemp++ = _T(' ');
			}

			if (lpPort->sLowPort == lpPort->sHighPort)
			{
				_stprintf(tszTemp, "%d", lpPort->sLowPort);
			}
			else
			{
				_stprintf(tszTemp, "%d-%d", lpPort->sLowPort, lpPort->sHighPort);
			}
			tszTemp += _tcslen(tszTemp);
		}
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, pBuffer);
		break;
	case DEVICE_OUT_PORTS:
		if (!lpDevice->lpOutPorts)
		{
			tszTemp = _T("<Service port - 1>");
		}
		else if (lpDevice->lpOutPorts->sLowPort == 0)
		{
			// use any old port
			tszTemp = _T("<any>");
		}
		else
		{
			tszTemp = pBuffer;
			pBuffer[0] = 0;
			for (lpPort = lpDevice->lpOutPorts ; lpPort ; lpPort = lpPort->lpNext)
			{
				if (tszTemp != pBuffer)
				{
					*tszTemp++ = _T(',');
					*tszTemp++ = _T(' ');
				}
				if (lpPort->sLowPort == lpPort->sHighPort)
				{
					_stprintf(tszTemp, "%d", lpPort->sLowPort);
				}
				else
				{
					_stprintf(tszTemp, "%d-%d", lpPort->sLowPort, lpPort->sHighPort);
				}
				tszTemp += _tcslen(tszTemp);
			}
			tszTemp = pBuffer;
		}
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, tszTemp);
		break;
	case DEVICE_OUT_RANDOMIZE:
		lpData->szFormat[lpData->dwFormat - 2]	= 's';
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, (lpDevice->bRandomizePorts ? _T("True") : _T("False")));
		break;
	case DEVICE_BW_GLOBAL_OUT_LIMIT:
		lpData->szFormat[lpData->dwFormat - 2]	= 'f';
		Cookie_Convert_Special(lpData, iDivider, (DOUBLE) (lpDevice->Outbound.bGlobalBandwidthLimit ? lpDevice->Outbound.dwGlobalBandwidthLimit : 0.));
		break;
	case DEVICE_BW_GLOBAL_IN_LIMIT:
		lpData->szFormat[lpData->dwFormat - 2]	= 'f';
		Cookie_Convert_Special(lpData, iDivider, (DOUBLE) (lpDevice->Inbound.bGlobalBandwidthLimit ? lpDevice->Inbound.dwGlobalBandwidthLimit : 0.));
		break;
	case DEVICE_BW_CLIENT_OUT_LIMIT:
		lpData->szFormat[lpData->dwFormat - 2]	= 'f';
		Cookie_Convert_Special(lpData, iDivider, (DOUBLE) lpDevice->Outbound.dwClientBandwidthLimit);
		break;
	case DEVICE_BW_CLIENT_IN_LIMIT:
		lpData->szFormat[lpData->dwFormat - 2]	= 'f';
		Cookie_Convert_Special(lpData, iDivider, (DOUBLE) lpDevice->Inbound.dwClientBandwidthLimit);
		break;
   }

   Device_ReleaseLock(lpDevice);
   return FALSE;
}



/*

  %[speed]

  */
BOOL MessageObject_Speed(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	PDATACHANNEL	pDataChannel;
	int iDivider;
	DOUBLE dVal;

	//	Get datachannel
	pDataChannel	= GETOFFSET(lpData->DataOffsets, DATA_DCHANNEL);

	//	Get divider
	if (!Argc || Object_Get_Int(lpData, &Argv[0], &iDivider) || ! iDivider) iDivider = 1;

	if (pDataChannel->Size > 0 &&
		Time_Compare(&pDataChannel->Stop, &pDataChannel->Start) > 0)
	{
		dVal = pDataChannel->Size / 1024. / Time_Difference(&pDataChannel->Start, &pDataChannel->Stop);
	}
	else
	{
		dVal = 0.;
	}

	return Cookie_Convert_Special(lpData, iDivider, dVal);
}



/*

%[ClosedMsg]

*/
BOOL MessageObject_ClosedMsg(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	if (FtpSettings.tmSiteClosedOn && FtpSettings.tszCloseMsg)
	{
		while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();
		if (FtpSettings.tmSiteClosedOn && FtpSettings.tszCloseMsg)
		{
			Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, FtpSettings.tszCloseMsg);
		}
		InterlockedExchange(&FtpSettings.lStringLock, FALSE);
		return FALSE;
	}
	return TRUE;
}



/*

%[Expired]
  Prints 1 if account has an expiration date and has expired
  Prints 0 if account has an expiration date but hasn't expired yet
  Prints nothing if account does not have an expiration date
*/
BOOL MessageObject_Expired(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	PUSERFILE	pUserFile;

	//	Get userfile
	pUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	if (pUserFile->ExpiresAt == 0)
	{
		return FALSE;
	}

	if (pUserFile->ExpiresAt < time((time_t) NULL))
	{
		Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, _T("1"));
		return FALSE;
	}

	Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, _T("0"));
	return TRUE;
}



/*

  %[time(format)(time)]

  */
BOOL MessageObject_Time(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPTSTR		tszTimeFormat;
	TCHAR		pBuffer[64];
	time_t		tTime;
	INT			iTime;
	struct tm	*lpTime;

	//	Get local time
	if (Argc < 2 || Object_Get_Int(lpData, &Argv[1], &iTime))
	{
		tTime	= time((time_t*)NULL);
	}
	else tTime	= iTime;

	if (tTime == 0)
	{
		FormatString(lpData->lpOutBuffer, lpData->szFormat, "<never>");
		return FALSE;
	}

	//	Build time structure
	lpTime	= localtime(&tTime);
	//	Get time format
	if (! Argc ||
		! (tszTimeFormat = Object_Get_String(lpData, &Argv[0])))
	{
		tszTimeFormat	= _TEXT("%a %b %d %H:%M:%S %Y");
	}
	//	szFormat time to static buffer
	_tcsftime(pBuffer, sizeof(pBuffer), tszTimeFormat, lpTime);
	//	Append result to output buffer
	FormatString(lpData->lpOutBuffer, lpData->szFormat, pBuffer);
	return FALSE;
}


/*

%[duration(time1)(time2)(extra text)(last)(first)(suffix-type)(dozeros)]
  Display y, w, d, h, m, s between time1 and time2. if time2 not specified it means now.

*/
BOOL MessageObject_Duration(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	CHAR		pBuffer[64], cLast, cFirst;
	INT         i;
	time_t		tTime, tTime2, tTime3;
	LPSTR       szFirst, szLast, szExtra, szField;
	DWORD       dwSuffixType, dwShowZeros, dwMinWidth;

	tTime2  = time((time_t *) NULL);
	tTime3  = tTime2;
	szExtra = NULL;
	szField = NULL;
	cLast   = 0;
	cFirst  = 0;
	dwSuffixType = 0;
	dwShowZeros  = 0;
	dwMinWidth   = 0;

	switch (Argc)
	{
	case 9:
		if (!Object_Get_Int(lpData, &Argv[8], &i) && i)
		{
			dwMinWidth = i;
		}

	case 8:
		if (!Object_Get_Int(lpData, &Argv[7], &i) && i)
		{
			dwShowZeros = 1;
		}

	case 7:
		szField = Object_Get_String(lpData, &Argv[6]);

	case 6:
		if (!Object_Get_Int(lpData, &Argv[5], &i) && i)
		{
			dwSuffixType = i;
		}

	case 5:
		szFirst = Object_Get_String(lpData, &Argv[4]);
		if (szFirst && szFirst[0])
		{
			cFirst = szFirst[0];
		}

	case 4:
		szLast = Object_Get_String(lpData, &Argv[3]);
		if (szLast && szLast[0])
		{
			cLast = szLast[0];
		}

	case 3:
		szExtra = Object_Get_String(lpData, &Argv[2]);

	case 2:
		if (!Object_Get_Int(lpData, &Argv[1], &i) && i != 0)
		{
			tTime2 = i;
		}

	case 1:
		if (!Object_Get_Int(lpData, &Argv[0], &i))
		{
			tTime = i;
		}
		else
		{
			tTime = tTime2;
		}
		break;
	default:
		return TRUE;
	}

	if (tTime == 0 && tTime2 == tTime3)
	{
		tTime = 0;
	}
	else if (tTime > tTime2)
	{
		tTime -= tTime2;
	}
	else
	{
		tTime = tTime2 - tTime;
	}

	Time_Duration(pBuffer, sizeof(pBuffer), tTime, cLast, cFirst, dwSuffixType, dwShowZeros, dwMinWidth, szField);
	if (szExtra && szExtra[0])
	{
		strncat_s(pBuffer, sizeof(pBuffer), szExtra, _TRUNCATE);
	}

	Put_Buffer_Format(lpData->lpOutBuffer, lpData->szFormat, pBuffer);
	return FALSE;
}



/*

%[sectionname(section)]

*/
BOOL MessageObject_SectionName(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	TCHAR		pBuffer[_MAX_NAME+1];
	INT         iSection, iCredit, iStat, iShare;

	if (Argc < 1 || Object_Get_Int(lpData, &Argv[0], &iSection) || iSection > MAX_SECTIONS || iSection < -1)
	{
		// use current stats section
		iSection = GETOFFSET(lpData->DataOffsets, DATA_SSECTION);
	}

	if (iSection == -1)
	{
		FormatString(lpData->lpOutBuffer, lpData->szFormat, _T("[TOTAL]"));
		return FALSE;
	}

	ZeroMemory(pBuffer, sizeof(pBuffer));
	if (!Config_Get_SectionNum(iSection, pBuffer, &iCredit, &iStat, &iShare))
	{
		return TRUE;
	}

	FormatString(lpData->lpOutBuffer, lpData->szFormat, pBuffer);
	return FALSE;
}




/*

  %[stats()()()()...]

  */
BOOL MessageObject_Stats(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	STATS		Stats;
	IO_STATS    IoStatsTotals;
	LPTSTR		tszOrder, tszType, tszFileName, tszHeaderName, tszFooterName;
	USERFILE    UserFile;
	LPBYTE	    pBuffer;
	BOOL        bFirst;


	if (Argc < 2) return TRUE;

	//	Filename      [Location of stats body file]
	//	Sort Method 1 [...]
	//	Sort Method 2 [Files/Bytes]                  (Optional)
	//	Section       [Section # to use for sorting] (Optional)
	//	Limit         [Limit amount users in list]   (Optional)
	//	Search        [Search parameters]            (Optional)
	//  Footer        [Location of stats footer file]
	//  Header        [Location of stats header file]

	ZeroMemory(&Stats, sizeof(STATS));

	Stats.dwType	= STATS_TYPE_WEEKUP|STATS_ORDER_BYTES|STATS_NO_ZEROS_FLAG;
	Stats.lpBuffer	= lpData->lpOutBuffer;
	Stats.tszMessagePrefix	= lpData->szPrefix[0];
	Stats.iSection	= -1;

	tszHeaderName = NULL;
	tszFooterName = NULL;
	bFirst = TRUE;

	switch (Argc)
	{
	case 8:
		tszHeaderName = Object_Get_String(lpData, &Argv[7]);
	case 7:
		tszFooterName = Object_Get_String(lpData, &Argv[6]);
	case 6:
		//	Get search parameters
		Stats.tszSearchParameters	= Object_Get_String(lpData, &Argv[5]);
	case 5:
		//	Get maximum output limit
		Object_Get_Int(lpData, &Argv[4], (PINT)&Stats.dwUserMax);
	case 4:
		//	Get section #
		Object_Get_Int(lpData, &Argv[3], (PINT)&Stats.iSection);
	case 3:
		//	Get sort method
		tszOrder = Object_Get_String(lpData, &Argv[2]);
		if (tszOrder && ! _tcsicmp(tszOrder, _TEXT("files")))
		{
			Stats.dwType	= STATS_TYPE_WEEKUP|STATS_ORDER_FILES;
		}
	case 2:
		//	Get stats type
		tszType	= Object_Get_String(lpData, &Argv[1]);
		if (tszType) SetStatsType(&Stats, tszType);

		if (!Stats.tszSearchParameters)	Stats.tszSearchParameters = _T("*");
		Stats.lpSearch = FindParse(Stats.tszSearchParameters, NULL, NULL, FALSE);

		if ((tszFileName = Object_Get_String(lpData, &Argv[0])) &&
			(Stats.lpMessage = Message_Load(tszFileName)))
		{
			IoStatsTotals.iPosition  = Stats.iSection;
			IoStatsTotals.iSection   = Stats.iSection;
			IoStatsTotals.lpUserFile = &UserFile;
			CopyMemory(IoStatsTotals.lpUserFile, User_GetFake(), sizeof(USERFILE));

			if (tszHeaderName && (pBuffer = Message_Load(tszHeaderName)))
			{
				//	Show MessageFile
				Message_Compile(pBuffer, Stats.lpBuffer, bFirst, &IoStatsTotals, DT_STATS, Stats.tszMessagePrefix, NULL);
				//	Free memory
				Free(pBuffer);
				bFirst = FALSE;
			}

			CompileStats(&Stats, &IoStatsTotals, bFirst);
			if (Stats.dwUserMax) bFirst = FALSE;

			if (tszFooterName && (pBuffer = Message_Load(tszFooterName)))
			{
				//	Show MessageFile
				Message_Compile(pBuffer, Stats.lpBuffer, bFirst, &IoStatsTotals, DT_STATS, Stats.tszMessagePrefix, NULL);
				//	Free memory
				Free(pBuffer);
			}

			//	Free message buffer
			Free(Stats.lpMessage);
		}
	}

	return FALSE;
}


/*

%[stats2()()()()...]

*/
BOOL MessageObject_Stats2(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	STATS		Stats;
	IO_STATS    IoStatsTotals;
	LPTSTR		tszOrder, tszType, tszFileName, tszBasePath;
	USERFILE    UserFile;
	LPBYTE	    pBuffer;
	BOOL        bFirst;
	DWORD       dwFileName;
	PCONNECTION_INFO lpConnectionInfo;


	if (Argc < 1) return TRUE;

	//	Sort Method 1 [...]
	//	Sort Method 2 [Files/Bytes]                  (Optional)
	//	Section       [Section # to use for sorting] (Optional)
	//	Limit         [Limit amount users in list]   (Optional)
	//	Search        [Search parameters]            (Optional)

	ZeroMemory(&Stats, sizeof(STATS));

	Stats.dwType	= STATS_TYPE_WEEKUP|STATS_ORDER_BYTES|STATS_NO_ZEROS_FLAG;
	Stats.lpBuffer	= lpData->lpOutBuffer;
	Stats.tszMessagePrefix	= lpData->szPrefix[0];
	Stats.iSection	= -1;

	bFirst = TRUE;

	switch (Argc)
	{
	case 5:
		//	Get search parameters
		Stats.tszSearchParameters	= Object_Get_String(lpData, &Argv[4]);
	case 4:
		//	Get maximum output limit
		Object_Get_Int(lpData, &Argv[3], (PINT)&Stats.dwUserMax);
	case 3:
		//	Get section #
		Object_Get_Int(lpData, &Argv[2], (PINT)&Stats.iSection);
	case 2:
		//	Get sort method
		tszOrder = Object_Get_String(lpData, &Argv[1]);
		if (tszOrder && ! _tcsicmp(tszOrder, _TEXT("files")))
		{
			Stats.dwType	= STATS_TYPE_WEEKUP|STATS_ORDER_FILES;
		}
	case 1:
		//	Get stats type
		tszType	= Object_Get_String(lpData, &Argv[0]);
		if (tszType) SetStatsType(&Stats, tszType);

		tszType	= GetStatsTypeName(&Stats);

		lpConnectionInfo = GETOFFSET(lpData->DataOffsets, DATA_CONNECTION);
		if (!lpConnectionInfo)
		{
			break;
		}

		//	Get messagefile path
		tszBasePath	= Service_MessageLocation(lpConnectionInfo->lpService);

		if (!tszBasePath)
		{
			break;
		}

		if (!Stats.tszSearchParameters)	Stats.tszSearchParameters = _T("*");
		Stats.lpSearch = FindParse(Stats.tszSearchParameters, NULL, NULL, FALSE);

		IoStatsTotals.iPosition  = Stats.iSection;
		IoStatsTotals.iSection   = Stats.iSection;
		IoStatsTotals.lpUserFile = &UserFile;
		CopyMemory(IoStatsTotals.lpUserFile, User_GetFake(), sizeof(USERFILE));

		dwFileName	= aswprintf(&tszFileName, _TEXT("%s\\%s.Header"), tszBasePath, tszType);
		if (dwFileName)
		{
			//	Show message header
			if (pBuffer = Message_Load(tszFileName))
			{
				//	Show MessageFile
				Message_Compile(pBuffer, Stats.lpBuffer, bFirst, &IoStatsTotals, DT_STATS, Stats.tszMessagePrefix, NULL);
				//	Free memory
				Free(pBuffer);
				bFirst = FALSE;
			}

			//	Show message body
			_tcscpy(&tszFileName[dwFileName - 6], _TEXT("Body"));
			if (Stats.lpMessage = Message_Load(tszFileName))
			{
				CompileStats(&Stats, &IoStatsTotals, FALSE);
				Free(Stats.lpMessage);
			}

			//	Show message footer
			_tcscpy(&tszFileName[dwFileName - 6], _TEXT("Footer"));
			if (pBuffer = Message_Load(tszFileName))
			{
				//	Show MessageFile
				Message_Compile(pBuffer, Stats.lpBuffer, bFirst, &IoStatsTotals, DT_STATS, Stats.tszMessagePrefix, NULL);
				//	Free memory
				Free(pBuffer);
			}
			Free(tszFileName);
		}
		FreeShared(tszBasePath);
	}
	return FALSE;
}


/*

  %[alldn()()]

  */
BOOL MessageObject_AllDn(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPUSERFILE	lpUserFile;

	//	Get userfile
	lpUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);
	
	Cookie_Function_Stats(lpData, lpUserFile->AllDn, Argc, Argv);
	return FALSE;
}




/*

  %[allup()()]

  */
BOOL MessageObject_AllUp(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPUSERFILE	lpUserFile;

	//	Get userfile
	lpUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	return Cookie_Function_Stats(lpData, lpUserFile->AllUp, Argc, Argv);
}





/*

  %[daydn()()]

  */
BOOL MessageObject_DayDn(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPUSERFILE	lpUserFile;

	//	Get userfile
	lpUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	return Cookie_Function_Stats(lpData, lpUserFile->DayDn, Argc, Argv);
}




/*

  %[dayup()()]

  */
BOOL MessageObject_DayUp(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPUSERFILE	lpUserFile;

	//	Get userfile
	lpUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	return Cookie_Function_Stats(lpData, lpUserFile->DayUp, Argc, Argv);
}



/*

  %[monthdn()()]

  */
BOOL MessageObject_MonthDn(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPUSERFILE	lpUserFile;

	//	Get userfile
	lpUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	return Cookie_Function_Stats(lpData, lpUserFile->MonthDn, Argc, Argv);
}



/*

  %[monthup()()]

  */
BOOL MessageObject_MonthUp(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPUSERFILE	lpUserFile;

	//	Get userfile
	lpUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	return Cookie_Function_Stats(lpData, lpUserFile->MonthUp, Argc, Argv);
}



/*

  %[weekdn()()]

  */
BOOL MessageObject_WeekDn(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPUSERFILE	lpUserFile;

	//	Get userfile
	lpUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	return Cookie_Function_Stats(lpData, lpUserFile->WkDn, Argc, Argv);
}




/*

  %[weekup()()]

  */
BOOL MessageObject_WeekUp(LPMESSAGEDATA lpData, INT Argc, LPOBJV Argv)
{
	LPUSERFILE	lpUserFile;

	//	Get userfile
	lpUserFile	= GETOFFSET(lpData->DataOffsets, DATA_USERFILE);

	return Cookie_Function_Stats(lpData, lpUserFile->WkUp, Argc, Argv);
}






DWORD MessageObject_Compile(LPMESSAGEDATA lpData, LPBYTE lpBuffer)
{
	LPMESSAGE_VARIABLE	lpVariable;
	LPBYTE				lpBufferBegin;
	WORD				ObjectID;
	LPOBJV				Argv;
	INT					Argc, i;

	//	Get variables
	lpBufferBegin		= lpBuffer;
	ObjectID			= ((LPWORD)lpBuffer)[0];
	lpBuffer			+= sizeof(WORD);
	lpData->dwFormat	= (lpBuffer++)[0];
	lpData->szFormat	= (LPSTR)lpBuffer;
	lpBuffer			+= lpData->dwFormat;
	Argc				= (lpBuffer++)[0];
	//	Allocate arguments
	Argv	= (LPOBJV)Allocate("Cookie:Compile:Argv", sizeof(OBJV) * Argc);

	for (i = 0 ; i < Argc ; i++)
	{
		//	Initialize argument context
		if (Argv)
		{
			Argv[i].pValue		= &lpBuffer[sizeof(USHORT)];
			Argv[i].lpMemory	= NULL;
			Argv[i].FreeProc	= NULL;
		}
		//	Shift buffer
		lpBuffer	+= (((PUSHORT)lpBuffer)[0] + sizeof(USHORT));
	}

	//	Check memory allocation
	if ((! Argc || Argv) && (lpVariable = (LPMESSAGE_VARIABLE)GetVariable(ObjectID)))
	{
		//	Condition check
		if (HASDATAEX(lpData->DataOffsets, lpVariable->dwRequiredData))
		{
			//	Compile cookie to output buffer
			((BOOL (__cdecl *)(LPMESSAGEDATA , INT, LPOBJV))lpVariable->AllocProc)(lpData, Argc, Argv);
			//	Free memory
			for (i = 0;i < Argc;i++)
			{
				if (Argv[i].FreeProc) ((VOID (__cdecl *)(LPVOID))Argv[i].FreeProc)(Argv[i].lpMemory);
			}
		}
		else
		{
			switch (lpVariable->dwType)
			{
			case C_INTEGER_VARIABLE:
				FormatString(lpData->lpOutBuffer, lpData->szFormat, 0);
				break;
			case C_FLOAT_VARIABLE:
				FormatString(lpData->lpOutBuffer, lpData->szFormat, 0.);
				break;
			case C_UNKNOWN_VARIABLE:
				lpData->szFormat[lpData->dwFormat - 2]	= 's';
			case C_STRING_VARIABLE:
				FormatString(lpData->lpOutBuffer, lpData->szFormat, "");
				break;
			}
		}
		Free(Argv);
	}
	//	Return amount of data parsed
	return lpBuffer - lpBufferBegin;
}




INT MessageObject_ConvertFileInfo(LPSTR szArg)
{
	//	Filename
	if (! memicmp("name", szArg, 4)) return 1;
	//	Filesize
	if (! memicmp("size", szArg, 4)) return 2;
	//	File owner (user)
	if (! memicmp("user", szArg, 4)) return 3;
	//	File owner (group)
	if (! memicmp("group", szArg, 5)) return 4;
	//	File modification date
	if (! memicmp("date", szArg, 4)) return 5;
	//	File permissions
	if (! memicmp("perm", szArg, 4)) return 6;
	return (ULONG)-1;
}



INT MessageObject_ConvertLimit(LPSTR szArg)
{
	if (! stricmp("dn_speed", szArg)) return 0;
	if (! stricmp("up_speed", szArg)) return 1;
	// 2 was telnet...
	if (! stricmp("ftp_logins", szArg)) return 2 + C_FTP;
	return (ULONG)-1;
}


INT MessageObject_ConvertService(LPSTR szArg)
{
	if (! stricmp("Name", szArg)) return SERVICE_NAME;
	if (! stricmp("Active", szArg)) return SERVICE_ACTIVE_STATE;
	if (! stricmp("HostIP", szArg)) return SERVICE_HOST_IP;
	if (! stricmp("HostPort", szArg)) return SERVICE_HOST_PORT;
	if (! stricmp("DeviceName", szArg)) return SERVICE_DEVICE_NAME;
	if (! stricmp("DeviceID", szArg)) return SERVICE_DEVICE_ID;
	if (! stricmp("Users", szArg)) return SERVICE_USERS;
	if (! stricmp("MaxClients", szArg)) return SERVICE_MAX_CLIENTS;
	if (! stricmp("Transfers", szArg)) return SERVICE_TRANSFERS;
	if (! stricmp("AllowedUsers", szArg)) return SERVICE_ALLOWED_USERS;
	if (! stricmp("CertificateInUse", szArg)) return SERVICE_CERTIFICATE_IN_USE;
	if (! stricmp("CertificateWhere", szArg)) return SERVICE_CERTIFICATE_WHERE;
	if (! stricmp("EncryptionType", szArg)) return SERVICE_ENCRYPTION_TYPE;
	if (! stricmp("RequireSecureAuth", szArg)) return SERVICE_REQUIRE_SECURE_AUTH;
	if (! stricmp("RequireSecureData", szArg)) return SERVICE_REQUIRE_SECURE_DATA;
	if (! stricmp("ExternalIdentity", szArg)) return SERVICE_EXTERNAL_IDENTITY;
	if (! stricmp("MessageLocation", szArg)) return SERVICE_MESSAGE_LOCATION;
	if (! stricmp("DataDevices", szArg)) return SERVICE_DATA_DEVICES;
	if (! stricmp("DataDeviceSelection", szArg)) return SERVICE_DATA_DEVICE_SELECTION;
	if (! stricmp("AcceptsReady", szArg)) return SERVICE_ACCEPTS_READY;

	return (ULONG)-1;
}



INT MessageObject_ConvertDevice(LPSTR szArg)
{
	if (! stricmp("Name", szArg)) return DEVICE_NAME;
	if (! stricmp("ID", szArg)) return DEVICE_ID;
	if (! stricmp("Active", szArg)) return DEVICE_ACTIVE_STATE;
	if (! stricmp("BindIP", szArg)) return DEVICE_BIND_IP;
	if (! stricmp("HostIP", szArg)) return DEVICE_HOST_IP;
	if (! stricmp("PasvPorts", szArg)) return DEVICE_PASV_PORTS;
	if (! stricmp("OutPorts", szArg)) return DEVICE_OUT_PORTS;
	if (! stricmp("OutRandomize", szArg)) return DEVICE_OUT_RANDOMIZE;
	if (! stricmp("GlobalOutLimit", szArg)) return DEVICE_BW_GLOBAL_OUT_LIMIT;
	if (! stricmp("GlobalInLimit", szArg)) return DEVICE_BW_GLOBAL_IN_LIMIT;
	if (! stricmp("ClientOutLimit", szArg)) return DEVICE_BW_CLIENT_OUT_LIMIT;
	if (! stricmp("ClientInLimit", szArg)) return DEVICE_BW_CLIENT_IN_LIMIT;

	return (ULONG)-1;
}


INT MessageObject_ConvertWho(LPSTR szArg)
{
	if (! stricmp("TransferSpeed", szArg)) return WHO_SPEED_CURRENT;
	if (! stricmp("UpSpeed", szArg)) return WHO_SPEED_UPLOAD;
	if (! stricmp("DnSpeed", szArg)) return WHO_SPEED_DOWNLOAD;
	if (! stricmp("TotalSpeed", szArg)) return WHO_SPEED_TOTAL;
	if (! stricmp("Action", szArg)) return WHO_ACTION;
	if (! stricmp("FileSize", szArg)) return WHO_FILESIZE;
	if (! stricmp("VirtualDataPath", szArg)) return WHO_VIRTUALDATAPATH;
	if (! stricmp("VirtualDataFile", szArg)) return WHO_VIRTUALDATAFILE;
	if (! stricmp("VirtualPath", szArg)) return WHO_VIRTUALPATH;
	if (! stricmp("DataPath", szArg)) return WHO_DATAPATH;
	if (! stricmp("Path", szArg)) return WHO_PATH;
	if (! stricmp("Uploads", szArg)) return WHO_TRANSFERS_UPLOAD;
	if (! stricmp("Downloads", szArg)) return WHO_TRANSFERS_DOWNLOAD;
	if (! stricmp("Transfers", szArg)) return WHO_TRANSFERS_TOTAL;
	if (! stricmp("Users", szArg)) return WHO_USERS_TOTAL;
	if (! stricmp("Idlers", szArg)) return WHO_IDLERS_TOTAL;
	if (! stricmp("OnlineTime", szArg)) return WHO_LOGIN_TIME;
	if (! stricmp("OnlineHours", szArg)) return WHO_LOGIN_HOURS;
	if (! stricmp("OnlineMinutes", szArg)) return WHO_LOGIN_MINUTES;
	if (! stricmp("OnlineSeconds", szArg)) return WHO_LOGIN_SECONDS;
	if (! stricmp("IdleTime", szArg)) return WHO_IDLE_TIME;
	if (! stricmp("IdleHours", szArg)) return WHO_IDLE_HOURS;
	if (! stricmp("IdleMinutes", szArg)) return WHO_IDLE_MINUTES;
	if (! stricmp("IdleSeconds", szArg)) return WHO_IDLE_SECONDS;
	if (! stricmp("ConnectionId", szArg)) return WHO_CONNECTION_ID;
	if (! stricmp("MyCID", szArg)) return WHO_MY_CONNECTION_ID;
	if (! stricmp("HostName", szArg)) return WHO_HOSTNAME;
	if (! stricmp("ServiceName", szArg)) return WHO_SERVICENAME;
	if (! stricmp("Ip", szArg)) return WHO_IP;
	if (! stricmp("Ident", szArg)) return WHO_IDENT;
	if (! stricmp("Hidden", szArg)) return WHO_HIDDEN;
	return (ULONG)-1;
}


INT MessageObject_ConvertGroupFile(LPSTR szArg)
{
	if (! stricmp("Description", szArg)) return GROUP_DESCRIPTION;
	if (! stricmp("Users", szArg)) return GROUP_USERS;
	if (! stricmp("UserSlots", szArg)) return GROUP_USERSLOTS;
	if (! stricmp("LeechSlots", szArg)) return GROUP_LEECHSLOTS;
	if (! stricmp("MountFile", szArg)) return GROUP_MOUNTFILE;
	if (! stricmp("DefaultFile", szArg)) return GROUP_DEFAULTFILE;
	return (ULONG)-1;
}

INT MessageObject_ConvertStats(LPSTR szArg)
{
	if (! stricmp("files", szArg)) return 0;
	if (! stricmp("bytes", szArg)) return 1;
	if (! stricmp("average", szArg)) return 2;
	return (ULONG)-1;
}

INT MessageObject_ConvertInteger(LPSTR szArg)
{
	PCHAR	pCheck;
	INT		Int;

	Int	= strtol(szArg, &pCheck, 10);
	if (pCheck[0] == '\0' && pCheck != szArg) return Int;
	return (ULONG)-1;
}


INT MessageObject_ConvertFormatter(LPSTR szArg)
{
	PCHAR	pCheck;
	INT		Int;

	Int	= strtol(szArg, &pCheck, 10);
	if (pCheck[0] == '\0' && pCheck != szArg)
	{
		SetAutoTheme(Int);
		return Int;
	}
	return (ULONG)-1;
}









BOOL MessageObject_Precompile(LPBUFFER lpOutBuffer, PCHAR pData, DWORD dwData, PCHAR pPrefix, BYTE bPrefix)
{
	LPMESSAGE_VARIABLE	lpVariable;
	LPARG_PROC			lpArgProc;
	LPARGUMENT_LIST		lpFirstArgument, lpArgument;
	DWORD				dwNameLength;
	WORD				wResult, wVariable;
	BYTE				bArguments, pBuffer[8];
	PCHAR				pParenthesis, pEnd, pCurrent; 
	BOOL				bError;
	INT					Int;

	lpFirstArgument	= NULL;
	lpArgument		= NULL;
	bArguments		= 0;

	switch (dwData)
	{
	case 0:
	case 1:
		//	Not long enough
		return TRUE;
	default:
		//	Find parenthesis
		if ((pParenthesis = (PCHAR)memchr(pData, '(', dwData)))
		{
			pCurrent		= pParenthesis;
			pEnd			= &pData[dwData];
			dwNameLength	= pCurrent - pData;

			do
			{
				//	Find closing parenthesis
				if (! (pParenthesis = (PCHAR)memchr(pCurrent, ')', pEnd - pCurrent))) break;

				//	Allocate memory for argument
				if (! lpFirstArgument)
				{
					//	First argument
					lpFirstArgument	= (LPARGUMENT_LIST)Allocate("Cookie:PreCompile:Args", sizeof(ARGUMENT_LIST));
					lpArgument		= lpFirstArgument;
				}
				else
				{
					//	Append item to list
					lpArgument->lpNext	= (LPARGUMENT_LIST)Allocate("Cookie:PreCompile:Args", sizeof(ARGUMENT_LIST));
					lpArgument			= lpArgument->lpNext;
				}

				//	Verify allocation
				if (! lpArgument) break;

				//	Initialize variable
				lpArgument->dwArgument	= &pParenthesis[1] - pCurrent;
				lpArgument->pArgument	= pCurrent;
				//	Set some variables
				pCurrent[0]		= C_STRING;
				pParenthesis[0]	= '\0';
				bArguments++;
				//	Shift buffer
				pCurrent	= &pParenthesis[1];

			} while (pCurrent < pEnd && pCurrent[0] == '(');

			//	Nill list's last pointer
			if (lpArgument) lpArgument->lpNext	= NULL;
			//	Check for errors
			bError	= (pCurrent == pEnd ? FALSE : TRUE);

			break;
		}
	case 2:
		dwNameLength	= dwData;
		bError			= FALSE;
		break;
	}

	if (! bError)
	{
		//	Find variable by name
		if ((wVariable = FindMessageVariable(pData, dwNameLength, TRUE, NULL)) != (WORD)-1)
		{
			lpVariable	= (LPMESSAGE_VARIABLE)GetVariable(wVariable);
			//	Convert known arguments
			for (lpArgument = lpFirstArgument;lpArgument;lpArgument = lpArgument->lpNext)
			{
				//	Skip zero length variable
				if (! lpArgument->dwArgument) continue;

				pCurrent	= &lpArgument->pArgument[1];
				//	Convert variables
				if (pCurrent[0] == '$' &&
					(wResult = FindMessageVariable(&pCurrent[1], lpArgument->dwArgument - 3, FALSE, &lpArgument->pBuffer[0])) != (WORD)-1)
				{
					CopyMemory(&lpArgument->pBuffer[1], &wResult, sizeof(WORD));
					lpArgument->dwArgument	= sizeof(WORD) + 1;
					lpArgument->pArgument	= (PCHAR)lpArgument->pBuffer;
					//	Process next argument
					continue;
				}

				for (lpArgProc = lpVariable->lpArgProc;lpArgProc;lpArgProc = (LPARG_PROC)lpArgProc->lpNext)
				{
					//	Execute prog
					Int	= ((INT (__cdecl *)(LPSTR))lpArgProc->lpProc)(pCurrent);
					//	Check result
					if ((ULONG)Int != (ULONG)-1)
					{
						//	Copy integer to buffer
						lpArgument->pBuffer[0]	= C_INTEGER;
						CopyMemory(&lpArgument->pBuffer[1], &Int, sizeof(INT));
						//	Reset argument variables
						lpArgument->pArgument	= (PCHAR)lpArgument->pBuffer;
						lpArgument->dwArgument	= sizeof(INT) + 1;
						break;
					}
				}
			}

			bPrefix	+= (2 * sizeof(CHAR));
			//	szFormat object info in temporary buffer
			pBuffer[0]	= OBJECT;
			pBuffer[4]	= '\0';
			pBuffer[5]	= bArguments;
			switch (lpVariable->dwType)
			{
			case C_INTEGER_VARIABLE:
				pBuffer[3]	= 'i';
				break;
			case C_STRING_VARIABLE:
				pBuffer[3]	= 's';
				break;
			case C_FLOAT_VARIABLE:
				pBuffer[3]	= 'f';
				break;
			case C_UNKNOWN_VARIABLE:
				pBuffer[3]	= '?';
				break;
			}
			CopyMemory(&pBuffer[1], &wVariable, sizeof(WORD));
			//	Copy to buffer
			Put_Buffer(lpOutBuffer, pBuffer, 3);
			Put_Buffer(lpOutBuffer, &bPrefix, sizeof(BYTE));
			Put_Buffer(lpOutBuffer, pPrefix, bPrefix - 2);
			Put_Buffer(lpOutBuffer, &pBuffer[3], 3);
			//	Store arguments to buffer
			for (lpArgument = lpFirstArgument;lpArgument;lpArgument = lpArgument->lpNext)
			{
				Put_Buffer(lpOutBuffer, &lpArgument->dwArgument, sizeof(USHORT));
				Put_Buffer(lpOutBuffer, lpArgument->pArgument, lpArgument->dwArgument);
			}
		}
	}

	//	Free memory
	for (;(lpArgument = lpFirstArgument);)
	{
		lpFirstArgument	= lpFirstArgument->lpNext;
		Free(lpArgument);
	}

	return bError;
}



BOOL MessageObjects_Init(VOID)
{
	// converter functions are tried from last to first and first one to return something other than -1 works...

	//	Float type functions
	InstallMessageVariable("ALLDN", MessageObject_AllDn, NULL, B_USERFILE, C_FLOAT_VARIABLE|C_ARGS,
		MessageObject_ConvertStats, MessageObject_ConvertInteger, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("ALLUP", MessageObject_AllUp, NULL, B_USERFILE, C_FLOAT_VARIABLE|C_ARGS,
		MessageObject_ConvertStats, MessageObject_ConvertInteger, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("CREDITS", MessageObject_Credits, NULL, B_USERFILE, C_FLOAT_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, MessageObject_ConvertSuffix, NULL);
	// technically this prints out an unknown, but using float and fixing in routine...
	InstallMessageVariable("SHAREDCREDITS", MessageObject_SharedCredits, NULL, B_USERFILE, C_FLOAT_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("DAYDN", MessageObject_DayDn, NULL, B_USERFILE, C_FLOAT_VARIABLE|C_ARGS,
		MessageObject_ConvertStats, MessageObject_ConvertInteger, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("DAYUP", MessageObject_DayUp, NULL, B_USERFILE, C_FLOAT_VARIABLE|C_ARGS,
		MessageObject_ConvertStats, MessageObject_ConvertInteger, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("MONTHDN", MessageObject_MonthDn, NULL, B_USERFILE, C_FLOAT_VARIABLE|C_ARGS,
		MessageObject_ConvertStats, MessageObject_ConvertInteger, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("MONTHUP", MessageObject_MonthUp, NULL, B_USERFILE, C_FLOAT_VARIABLE|C_ARGS,
		MessageObject_ConvertStats, MessageObject_ConvertInteger, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("WKDN", MessageObject_WeekDn, NULL, B_USERFILE, C_FLOAT_VARIABLE|C_ARGS,
		MessageObject_ConvertStats, MessageObject_ConvertInteger, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("WKUP", MessageObject_WeekUp, NULL, B_USERFILE, C_FLOAT_VARIABLE|C_ARGS,
		MessageObject_ConvertStats, MessageObject_ConvertInteger, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("FREE", MessageObject_Free, NULL, B_ANY, C_FLOAT_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("SPEED", MessageObject_Speed, NULL, B_DATA, C_FLOAT_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, MessageObject_ConvertSuffix, NULL);

	//	Integer type functions
	InstallMessageVariable("POS", MessageObject_Position, NULL, B_ANY, C_INTEGER_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("MAX", MessageObject_Max, NULL, B_FTPUSER, C_INTEGER_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("LIMIT", MessageObject_Limit, NULL, B_USERFILE, C_INTEGER_VARIABLE|C_ARGS,
		MessageObject_ConvertLimit, NULL);
	InstallMessageVariable("SHUTDOWNGRACE", MessageObject_ShutdownGrace, NULL, B_ANY, C_INTEGER_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("RATIONUM", MessageObject_RatioNum, NULL, B_USERFILE, C_INTEGER_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);

	//	String type functions
	InstallMessageVariable("IP", MessageObject_Ip, NULL, B_USERFILE, C_STRING_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("GROUP", MessageObject_Group, NULL, B_USERFILE, C_STRING_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("ADMINGROUPS", MessageObject_AdminGroups, NULL, B_USERFILE, C_STRING_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("ISGADMIN", MessageObject_IsGAdmin, NULL, B_USERFILE, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("RATIO", MessageObject_Ratio, NULL, B_USERFILE, C_STRING_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("ENVIRONMENT", MessageObject_Environment, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("TIME", MessageObject_Time, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("DURATION", MessageObject_Duration, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("CLOSEDMSG", MessageObject_ClosedMsg, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("WHO", MessageObject_Who, NULL, B_WHO, C_STRING_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, MessageObject_ConvertWho, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("SECTIONNAME", MessageObject_SectionName, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("IF", MessageObject_IF, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("RemoveBlankLine", MessageObject_RemoveBlankLine, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("Expired", MessageObject_Expired, NULL, B_USERFILE, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("C", MessageObject_Color, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("T", MessageObject_Theme, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("F", MessageObject_Formatter, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		MessageObject_ConvertFormatter, NULL);
	InstallMessageVariable("R", MessageObject_RTheme, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("THEME", MessageObject_SetTheme, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("MSG", MessageObject_Msg, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("SITENAME", MessageObject_SiteName, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("SITEBOXHEADER", MessageObject_SiteBoxHeader, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("SITEBOXFOOTER", MessageObject_SiteBoxFooter, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("HELPBOXHEADER", MessageObject_HelpBoxHeader, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("HELPBOXFOOTER", MessageObject_HelpBoxFooter, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("SITECMD", MessageObject_SiteCmd, NULL, B_ANY, C_STRING_VARIABLE|C_ARGS,
		NULL);

	//	Unknown type functions
	InstallMessageVariable("GROUPINFO", MessageObject_GroupInfo, NULL, B_GROUPFILE, C_UNKNOWN_VARIABLE|C_ARGS,
		MessageObject_ConvertGroupFile, NULL);
	InstallMessageVariable("EXECUTE", MessageObject_Execute, NULL, B_ANY, C_UNKNOWN_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("INCLUDE", MessageObject_Include, NULL, B_ANY, C_UNKNOWN_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("HASFLAG", MessageObject_HasFlag, NULL, B_USERFILE, C_UNKNOWN_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("CALLERHASFLAG", MessageObject_CallerHasFlag, NULL, B_USERFILE, C_UNKNOWN_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("SERVICE", MessageObject_Service, NULL, B_ANY, C_UNKNOWN_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, MessageObject_ConvertService, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("DEVICE", MessageObject_Device, NULL, B_ANY, C_UNKNOWN_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, MessageObject_ConvertDevice, MessageObject_ConvertSuffix, NULL);
	InstallMessageVariable("STATS", MessageObject_Stats, NULL, B_ANY, C_UNKNOWN_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("STATS2", MessageObject_Stats2, NULL, B_ANY, C_UNKNOWN_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("MARK", MessageObject_Mark, NULL, B_ANY, C_UNKNOWN_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("FILL", MessageObject_Fill, NULL, B_ANY, C_UNKNOWN_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("PAD", MessageObject_Pad, NULL, B_ANY, C_UNKNOWN_VARIABLE|C_ARGS,
		MessageObject_ConvertInteger, NULL);
	InstallMessageVariable("SAVE", MessageObject_Save, NULL, B_ANY, C_UNKNOWN_VARIABLE|C_ARGS,
		NULL);
	InstallMessageVariable("RESTORE", MessageObject_Restore, NULL, B_ANY, C_UNKNOWN_VARIABLE|C_ARGS,
		NULL);

	return FALSE;
}

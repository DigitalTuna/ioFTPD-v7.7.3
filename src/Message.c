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

static LPMESSAGE_CACHE		*lpMessageCache, lpMessageCacheList[2];
static CRITICAL_SECTION	lIndex;
static INT					CacheMax, Cached;

volatile DWORD        lThemeLock;
volatile DWORD        dwNumThemes;
static LPOUTPUT_THEME lpThemeArray[MAX_THEMES];
static LPCONFIG_FILE  lpIniThemeFile;

LPTSTR  tszLeechName;


INT Parse_ThemeField(LPTHEME_FIELD lpField, LPTSTR tszString)
{
	INT   iColor;
	TCHAR tChar;
	BOOL  bIsUpper, bValid, bBackground;
	INT   n;

	n = 0;
	while (*tszString == _T(' ') || *tszString == _T('\t'))
	{
		n++;
		tszString++;
	}

	bValid      = FALSE;
	bBackground = FALSE;
	iColor      = 0;


	for ( ; *tszString ; tszString++ )
	{
		tChar    = *tszString;
		bIsUpper = isupper(tChar);
		if (!bIsUpper) tChar = toupper(tChar);
		switch (tChar)
		{
		case _T(' '):
		case _T('\t'):
			if (!bValid)
			{
				return 0;
			}
			return n;
		case _T('*'):
			break;
		case _T('0'):
			lpField->Settings.doReset = 1;
			break;
		case _T('_'):
			lpField->Settings.hasUnderLine = 1;
			lpField->Settings.Underline    = 1;
			break;
		case _T('|'):
			lpField->Settings.hasUnderLine = 1;
			lpField->Settings.Underline    = 0;
			break;
		case _T('%'):
			lpField->Settings.doInverse = 1;
			break;
		case _T('&'):
			bBackground = TRUE;
			break;
		case _T('K'): // also 'k', Black
			iColor = 1;
			break;
		case _T('R'): // also 'r', Red
			iColor = 2;
			break;
		case _T('G'): // also 'g', Green
			iColor = 3;
			break;
		case _T('Y'): // also 'y', Yellow
			iColor = 4;
			break;
		case _T('B'): // also 'b', Blue
			iColor = 5;
			break;
		case _T('M'): // also 'm', Magenta
			iColor = 6;
			break;
		case _T('C'): // also 'c', Cyan
			iColor = 7;
			break;
		case _T('W'): // also 'w', White
			iColor = 8;
			break;
		case _T('D'): // also 'd', use default color
			iColor = 10;
			break;
		default:
			return 0;
		}

		bValid = TRUE;

		if (iColor)
		{
			if (bIsUpper)
			{
				iColor += 60;
			}

			if (bBackground)
			{
				lpField->Settings.Background = iColor;
			}
			else
			{
				lpField->Settings.Foreground = iColor;
			}
		}
		n++;
	}
	if (bValid)
	{
		return n;
	}
	return 0;
}


// Must hold lThemeLock when calling this!
LPOUTPUT_THEME Parse_Theme(LPOUTPUT_THEME lpInTheme, INT iTheme, LPTSTR tszSubTheme)
{
	TCHAR           tszName[_MAX_NAME], tszTemp[_INI_LINE_LENGTH+1];
	DWORD           n;
	LPTSTR          tszLine, tszStart, tszEnd;
	INT32           iColor, iSubDefault, iPos;
	LPOUTPUT_THEME  lpTheme;
	LPTHEME_FIELD   lpField;

	if (!tszSubTheme)
	{
		_sntprintf_s(tszName, sizeof(tszName)/sizeof(*tszName), _TRUNCATE, "%d", iTheme);
		if ( !(tszLine = Config_Get(lpIniThemeFile, _T("Themes"), tszName, tszTemp, NULL)) )
		{
			// this is valid situation when reading config file and indicates theme not defined
			return NULL;
		}

		if ((2 != _stscanf_s(tszLine, _T("%d %64s%n"), &iSubDefault, tszName, sizeof(tszName)/sizeof(*tszName), &iPos)) ||
			(iSubDefault < 0) || (iSubDefault > MAX_THEMES))
		{
			// it's an invalid main entry
			Putlog(LOG_ERROR, _T("Theme #%d is invalid: %s\r\n"), iTheme, tszLine);
			return NULL;
		}
		tszStart = &tszLine[iPos];
	}
	else
	{
		_sntprintf_s(tszName, sizeof(tszName)/sizeof(*tszName), _TRUNCATE, "%d_%s", iTheme, tszSubTheme);

		if ( !(tszLine = Config_Get(lpIniThemeFile, _T("Themes"), tszName, tszTemp, NULL)) )
		{
			// subtheme not found
			if (lpInTheme && lpInTheme->ThemeFieldsArray[0].i)
			{
				// there is a SubThemeDefault, so try looking that theme up in the subtheme
				_sntprintf_s(tszName, sizeof(tszName)/sizeof(*tszName), _TRUNCATE, "%d_%s", lpInTheme->ThemeFieldsArray[0].i, tszSubTheme);

				if ( !(tszLine = Config_Get(lpIniThemeFile, _T("Themes"), tszName, tszTemp, NULL)) )
				{
					Putlog(LOG_ERROR, _T("Theme #%d missing sub-theme and default (%d): %s\r\n"), iTheme, lpInTheme->ThemeFieldsArray[0].i, tszSubTheme);
					tszLine = _T("*");
				}
			}
			else
			{
				// missing settings
				_sntprintf_s(tszName, sizeof(tszName)/sizeof(*tszName), _TRUNCATE, "%d_%s [missing]", lpInTheme->ThemeFieldsArray[0].i, tszSubTheme);
				Putlog(LOG_ERROR, _T("Theme #%d missing sub-theme: %s\r\n"), iTheme, tszSubTheme);
				tszLine = _T("*");
			}
		}
		tszStart = tszLine;
		iSubDefault = 0;
	}

	if (! (lpTheme = AllocateShared(0, _T("THEME"), sizeof(*lpTheme))) )
	{
		return NULL;
	}

	lpTheme->iTheme = iTheme;
	ZeroMemory(lpTheme->tszName, sizeof(lpTheme->tszName));
	_tcscpy_s(lpTheme->tszName, sizeof(lpTheme->tszName), tszName);
	lpTheme->ThemeFieldsArray[0].i = iSubDefault;

	// skip over initial whitespace
	while (*tszStart == _T(' ') || *tszStart == _T('\r')) tszStart++;
	ZeroMemory(lpTheme->ThemeFieldsArray, sizeof(lpTheme->ThemeFieldsArray));

	// process each field
	for (n=1 ; *tszStart && (n <= MAX_COLORS) ; )
	{
		lpField = &lpTheme->ThemeFieldsArray[n];

		// we'll handle the plain '0' case down below if this fails
		iColor = _tcstol(tszStart, &tszEnd, 10);
		if (iColor > 0 && (tszStart != tszEnd))
		{
			if (!*tszEnd || *tszEnd == _T(' ') || *tszEnd == _T('\t'))
			{
				// it was a valid number
				lpField->Formatter.doFormat = 1;
				lpField->Formatter.Format   = iColor;
				tszStart = tszEnd;
				n++;
				continue;
			}
		}

		iPos = Parse_ThemeField(lpField, tszStart);
		if (!iPos)
		{
			Putlog(LOG_ERROR, _T("Theme field #%d invalid: %s\r\n"), n, tszName);
			break;
		}
		tszStart += iPos;
		n++;
	}

	// undefined theme indexes get -1 which means don't do anything
	for( ; n <= MAX_COLORS ; n++)
	{
		lpTheme->ThemeFieldsArray[n].i = 0;
	}

	return lpTheme;
}



BOOL TextFile_Show(LPSTR szFileName, LPBUFFER lpOutBuffer, LPSTR szPrefix)
{
	HANDLE	hFileHandle;
	CHAR	pReadBuffer[1024];
	PCHAR	pNewline, pLine, pEnd;
	DWORD	dwBytesRead, dwLine, dwPrefix;
	BOOL	bPrefix, bNewline;

	if ((hFileHandle = CreateFile(szFileName, GENERIC_READ, 
		FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE)
	{
		dwPrefix	= (szPrefix ? strlen(szPrefix) : 0);
		bPrefix		= TRUE;
		bNewline	= FALSE;

		while (ReadFile(hFileHandle, pReadBuffer, sizeof(pReadBuffer), &dwBytesRead, NULL) &&
			dwBytesRead > 0)
		{
			pLine	= pReadBuffer;
			pEnd	= &pReadBuffer[dwBytesRead];

			for (pLine = pReadBuffer;(pNewline = (PCHAR)memchr(pLine, '\n', pEnd - pLine));pLine = &pNewline[1])
			{
				dwLine	= pNewline - pLine;
				//	Add prefix
				if (bPrefix) Put_Buffer(lpOutBuffer, szPrefix, dwPrefix);

				if (dwLine && pNewline[-1] == '\r')
				{
					//	Has "\r\n"
					Put_Buffer(lpOutBuffer, pLine, dwLine + 1);
				}
				else
				{
					//	Has "\n"
					Put_Buffer(lpOutBuffer, pLine, dwLine);
					Put_Buffer(lpOutBuffer, "\r\n", 2);
				}
				bPrefix		= TRUE;
				bNewline	= FALSE;
			}

			if (pLine != pEnd)
			{
				//	Add prefix
				if (bPrefix) Put_Buffer(lpOutBuffer, szPrefix, dwPrefix);
				//	Remove carriage feed
				if (pEnd[-1] == '\r') pEnd--;
				//	Put buffer
				Put_Buffer(lpOutBuffer, pLine, pEnd - pLine);
				bNewline	= TRUE;
				bPrefix		= FALSE;
			}
		}
		//	Add newline
		if (bNewline) Put_Buffer(lpOutBuffer, "\r\n", 2);
		//	Close file
		CloseHandle(hFileHandle);
		return FALSE;
	}
	return TRUE;
}


LPBYTE Message_PreCompile2(PCHAR lpBuffer, LPDWORD lpOutSize, LPTSTR tszFilePath)
{
	LPTSTR      tszFileName;
	LPBUFFER	lpOutBuffer;
	BUFFER		OutBuffer;
	PUCHAR		pStringEnd, pString, pFormat, pFormatEnd, pCookie, pCookieEnd;
	DWORD		dwLines, dwObjects, dwVariables, dwPrefixOffset, dwLine, dwFormat, dwCookie;
	USHORT		sString;
	BYTE		pTempBuffer[4];

	dwLines		= 0;
	dwLine		= 0;
	dwObjects	= 0;
	dwVariables	= 0;
	lpOutBuffer	= &OutBuffer;
	//	Allocate work buffer
	OutBuffer.len	= 0;
	OutBuffer.dwType	= TYPE_CHAR;
	OutBuffer.size	= 4096;
	OutBuffer.buf	= (PCHAR)Allocate("Cookie:Precompile", OutBuffer.size);

	if (! OutBuffer.buf) return NULL;

	Put_Buffer(lpOutBuffer, &dwLines, sizeof(DWORD));
	if (tszFilePath)
	{
		if (! (tszFileName = _tcsrchr(tszFilePath, _T('\\'))) )
		{
			tszFileName = tszFilePath;
		}
		else
		{
			tszFileName++;
		}
		// technically this can screw up with a huge filename, but it will just fail to load the right colors...
		// reserved space for %d%d_ at front and \0 at end when computing size
		FormatString(lpOutBuffer, "%c%c%s%c", FILEPREFIX, strlen(tszFileName)+2, tszFileName, 0);
	}
	pTempBuffer[0]	= PREFIX;
	//	Insert data to work buffer
	Put_Buffer(lpOutBuffer, pTempBuffer, 1);

	pString			= lpBuffer;
	pStringEnd		= lpBuffer;
	dwPrefixOffset	= OutBuffer.len;

	for ( ; ; pStringEnd++ )
	{
		switch (pStringEnd[0])
		{
		case '%':
			pFormat		= pStringEnd;
			pFormatEnd	= &pStringEnd[1];
			//	Accept minus
			if (pFormatEnd[0] == '-') pFormatEnd++;
			//	Accept all numeric values
			while (isdigit(pFormatEnd[0])) pFormatEnd++;
			//	Accept dot
			if (pFormatEnd[0] == '.')
			{
				//	Accept minus
				if ((++pFormatEnd)[0] == '-') pFormatEnd++;
				//	Accept all numeric values
				while (isdigit(pFormatEnd[0])) pFormatEnd++;
			}
			//	Accept custom format characters
			switch (pFormatEnd[0])
			{
			case 'U':
			case 'H':
				pFormatEnd++;
				break;
			}

			pCookie	= &pFormatEnd[1];

			if (pFormatEnd[0] == '[' &&
				(pCookieEnd = strpbrk(pCookie, "]\n")) && pCookieEnd[0] == ']')
			{
				//	Calculate preceeding string's length
				sString		= pStringEnd - pString;
				dwFormat	= pFormatEnd - pFormat;
				dwCookie	= pCookieEnd - pCookie;

				if (sString)
				{
					//	Increase length of current line
					dwLine	+= sString;
					//	Append string to work buffer
					pTempBuffer[0]	= STRING;

					Put_Buffer(lpOutBuffer, pTempBuffer, 1);
					Put_Buffer(lpOutBuffer, &sString, sizeof(USHORT));
					Put_Buffer(lpOutBuffer, pString, sString);
				}

				if (dwFormat < 20 &&
					dwCookie > 0)
				{
					if (pCookie[0] == '$')
					{
						MessageVariable_Precompile(lpOutBuffer, &pCookie[1], dwCookie - 1, pFormat, (BYTE)dwFormat);
						dwVariables++;
					}
					else
					{
						MessageObject_Precompile(lpOutBuffer, pCookie, dwCookie, pFormat, (BYTE)dwFormat);
						dwObjects++;
					}
				}

				pStringEnd	= pCookieEnd;
				pString		= &pCookieEnd[1];
			}
			break;
		case '\n':
			//	Increase line count
			dwLines++;
			//	Get string length
			sString	= (pStringEnd > pString && pStringEnd[-1] == '\r' ?
				&pStringEnd[-1] : pStringEnd) - pString;

			if (sString > 0)
			{
				//	Increase length of current line
				dwLine		+= sString;
				//	Append string to work buffer
				pTempBuffer[0]	= STRING;
				Put_Buffer(lpOutBuffer, pTempBuffer, 1);
				Put_Buffer(lpOutBuffer, &sString, sizeof(USHORT));
				Put_Buffer(lpOutBuffer, pString, sString);
			}

#if 0 // this messes up object variable if the only thing on the line
			//	Update prefix
			if (dwObjects && ! dwLine && ! dwVariables)
			{				
				lpOutBuffer.buf[dwPrefixOffset - 1]	= NOPREFIX;
			}
			else
			{
				pTempBuffer[0]	= NEWLINE;
				Put_Buffer(lpOutBuffer, pTempBuffer, 1);
			}
#endif
			pTempBuffer[0] = NEWLINE;
			Put_Buffer(lpOutBuffer, pTempBuffer, 1);

			if (pStringEnd[1] != '\0')
			{
				pString		= &pStringEnd[1];
				dwObjects	= 0;
				dwVariables	= 0;
				dwLine		= 0;
				//	Append prefix to work buffer
				pTempBuffer[0]	= PREFIX;
				Put_Buffer(lpOutBuffer, pTempBuffer, 1);

				dwPrefixOffset	= OutBuffer.len;
				break;
			}

			//	Insert end of file mark
			pTempBuffer[0]	= THEEND;
			Put_Buffer(lpOutBuffer, pTempBuffer, 1);
			//	Update line count
			((LPDWORD)OutBuffer.buf)[0]	= dwLines;
			//	Set buffer size
			if (lpOutSize) ((LPDWORD)lpOutSize)[0]	= OutBuffer.len;

			return (PBYTE)OutBuffer.buf;
		}
	}
}


LPBYTE Message_PreCompile(PCHAR lpBuffer, LPDWORD lpOutSize)
{
	return Message_PreCompile2(lpBuffer, lpOutSize, 0);
}



VOID Compile_Message(LPMESSAGEDATA lpData, LPBYTE lpBuffer, BOOL bInline, BOOL bSkipFirstPrefix)
{
	LPTSTR          tszFileName;
	LPOUTPUT_THEME  lpOldTheme;
	DWORD	        dwLines, dwLastLen;
	BOOL	        bHaveEnd;

	dwLines		= ((LPDWORD)lpBuffer)[0];
	lpBuffer	+= sizeof(DWORD);
	bHaveEnd	= FALSE;
	lpOldTheme  = NULL;
	if (!bInline)
	{
		lpOldTheme  = GetTheme();
	}

	if ( lpOldTheme )
	{
		AllocateShared(lpOldTheme, "", 0);
	}

	dwLastLen = lpData->lpOutBuffer->len;
	for (;;)
	{
		switch ((lpBuffer++)[0])
		{
		case STRING:
			//	Text string
			Put_Buffer(lpData->lpOutBuffer, &lpBuffer[sizeof(USHORT)], ((PUSHORT)lpBuffer)[0]);
			lpBuffer	+= ((PUSHORT)lpBuffer)[0] + sizeof(USHORT);
			break;
		case FILEPREFIX:
			// not really used atm
			tszFileName = (LPTSTR) &lpBuffer[1];
			lpBuffer	+= lpBuffer[0];
			break;

		case PREFIX:
			//	Beginning of line
			if (bInline) continue;
			--dwLines;
			if (bSkipFirstPrefix)
			{
				bSkipFirstPrefix = FALSE;
				continue;
			}
			if (!dwLines)
			{
				bHaveEnd	= TRUE;
				Put_Buffer(lpData->lpOutBuffer, lpData->szPrefix[1], lpData->dwPrefix[1]);
			}
			else
			{
				Put_Buffer(lpData->lpOutBuffer, lpData->szPrefix[0], lpData->dwPrefix[0]);
			}
			break;
		case NEWLINE:
			//	End of line
			if (bInline)
			{
				return;
			}
			if ( ! lpData->dwPrefix[0] || 
				 ( lpData->lpOutBuffer->len && (lpData->lpOutBuffer->len != dwLastLen) && (lpData->lpOutBuffer->buf[lpData->lpOutBuffer->len-1] != '\n') ) )
			{
				// no prefix or something outputted and it didn't end with a return, so just include blank line
				Put_Buffer(lpData->lpOutBuffer, "\r\n", sizeof(CHAR) * 2);
			}
			dwLastLen = lpData->lpOutBuffer->len;
			// reset fill mark and saved theme... they don't span lines
			lpData->dwMarkPosition = lpData->lpOutBuffer->len;
			lpData->SavedThemes.i = 0;
			break;
		case THEEND:
			//	End of file
			if (bInline) return;

			SetTheme(lpOldTheme);
			FreeShared(lpOldTheme);

			if (lpData->lpSavedTheme)
			{
				FreeShared(lpData->lpSavedTheme);
				lpData->lpSavedTheme = NULL;
			}

			if (! bHaveEnd &&
				lpData->szPrefix[0] != lpData->szPrefix[1])
			{
				Put_Buffer(lpData->lpOutBuffer, lpData->szPrefix[1], lpData->dwPrefix[1]);
				Put_Buffer(lpData->lpOutBuffer, "\r\n", sizeof(CHAR) * 2);
			}

			return;
		case VARIABLE:
			//	Compile variable type cookie
			lpBuffer	+= MessageVariable_Compile(lpData, lpBuffer);
			break;
		case OBJECT:
			//	Compile object type cookie
			lpBuffer	+= MessageObject_Compile(lpData, lpBuffer);
			break;
		}
	}
}









INT __cdecl Cache_Compare(LPMESSAGE_CACHE *lpItem1, LPMESSAGE_CACHE *lpItem2)
{
	register INT	iResult;

	//	Compare length of filename
	iResult	= memcmp(&(lpItem1[0]->dwFileName), &(lpItem2[0]->dwFileName), sizeof(DWORD));
	//	Compare filename if length matches
	if (! iResult)
	{
		iResult	= memicmp(lpItem1[0]->szFileName, lpItem2[0]->szFileName, lpItem1[0]->dwFileName);
	}
	return iResult;
}






BOOL Message_Init(BOOL bFirstInitialization)
{
	DWORD           n;
	INT32           iOffset;
	LPOUTPUT_THEME  lpTheme, lpPrevTheme;

	if (bFirstInitialization)
	{
		Cached			= 0;
		lpMessageCacheList[HEAD]	= NULL;
		lpMessageCacheList[TAIL]	= NULL;
		//	Get amount of items to cache
		if (Config_Get_Int(&IniConfigFile, _TEXT("File"), _TEXT("MessageCache_Size"), &CacheMax) ||
			CacheMax < 75) CacheMax	= 100;
		//	Allocate memory for cache index
		if (! (lpMessageCache = (LPMESSAGE_CACHE *)Allocate(NULL, sizeof(LPMESSAGE_CACHE) * CacheMax))) return FALSE;
		//	Initialize critical section
		InitializeCriticalSection(&lIndex);
		//	Initialize message variables
		if (MessageVariables_Init()) return FALSE;
		if (MessageObjects_Init()) return FALSE;
		lThemeLock = 0;
		dwNumThemes = 0;
		for (n=0 ; n < MAX_THEMES ; n++)
		{
			lpThemeArray[n] = 0;
		}
		if ( !(tszLeechName = Config_Get(&IniConfigFile, _T("FTP"), _T("LeechName"), NULL, 0)) )
		{
			tszLeechName = _T("Leech");
		}
		lpIniThemeFile = NULL;
	}

	while (InterlockedExchange(&lThemeLock, TRUE)) SwitchToThread();

	// load/reload ini file
	Config_Load("Theme.ini", &lpIniThemeFile);

	iOffset = 0;
	for (n=0 ; n < MAX_THEMES ; n++)
	{
		if ( !(lpTheme = Parse_Theme(NULL, n+1, NULL)) )
		{
			break;
		}

		lpPrevTheme     = lpThemeArray[n];
		lpThemeArray[n] = lpTheme;
		FreeShared(lpPrevTheme);
	}
	dwNumThemes = n;
	for (; n < MAX_THEMES ; n++)
	{
		lpPrevTheme     = lpThemeArray[n];
		lpThemeArray[n] = 0;
		FreeShared(lpPrevTheme);
	}

	InterlockedExchange(&lThemeLock, FALSE);

	return TRUE;
}



VOID Message_DeInit(VOID)
{
	LPMESSAGE_CACHE	lpMessage;
	DWORD n;

	//	Deinit variables
	MessageVariables_DeInit();
	//	Free memory
	for (;lpMessage = lpMessageCacheList[HEAD];)
	{
		lpMessageCacheList[HEAD]	= lpMessageCacheList[HEAD]->lpNext;
		if (lpMessage->pBuffer) Free(lpMessage->pBuffer);
		Free(lpMessage);
	}
	Free(lpMessageCache);

	for (n=0 ; n < MAX_THEMES ; n++)
	{
		if (lpThemeArray[n])
		{
			FreeShared(lpThemeArray[n]);
		}
	}

	if (lpIniThemeFile)
	{
		Config_Free(lpIniThemeFile);
		lpIniThemeFile = NULL;
	}
	//	Delete critical section
	DeleteCriticalSection(&lIndex);
}



LPBYTE Message_Load(LPSTR szFileName)
{
	LPMESSAGE_CACHE	lpCacheItem, *lpSearchResult, lpItem;
	HANDLE			hMessageFile;
	DWORD			dwFileName, dwFileSize, dwBytesRead, dwBytesWritten;
	LPBYTE			lpReturn, lpTemp, lpReadBuffer;

	lpReturn	  = NULL;
	lpReadBuffer  = NULL;
	dwBytesWritten = 0;

	if (! szFileName) return NULL;
	//	Open file for reading
	hMessageFile = CreateFile(szFileName, GENERIC_READ,
		FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hMessageFile == INVALID_HANDLE_VALUE) return NULL;

	dwFileName	= strlen(szFileName) + 1;
	//	Allocate cache item
	lpCacheItem	= (LPMESSAGE_CACHE)Allocate("Cookie:Cache", sizeof(MESSAGE_CACHE) + dwFileName);
	if (! lpCacheItem)
	{
		CloseHandle(hMessageFile);
		return NULL;
	}

	//	Setup structure
	lpCacheItem->dwFileName	= dwFileName;
	lpCacheItem->szFileName	= szFileName;
	lpCacheItem->pBuffer    = NULL;
	lpCacheItem->dwBufLen   = 0;
	GetFileTime(hMessageFile, NULL, NULL, &lpCacheItem->ftCacheTime);

	EnterCriticalSection(&lIndex);
	//	Find item
	lpSearchResult	= (LPMESSAGE_CACHE *)bsearch(&lpCacheItem, lpMessageCache, Cached,
		                                         sizeof(LPMESSAGE_CACHE), (QUICKCOMPAREPROC) Cache_Compare);

	if (lpSearchResult &&
		! CompareFileTime(&lpSearchResult[0]->ftCacheTime, &lpCacheItem->ftCacheTime))
	{
		lpItem = lpSearchResult[0];
		lpReturn = lpItem->pBuffer;
		dwBytesWritten = lpItem->dwBufLen;
	}

	if (! lpReturn)
	{
		//	Get filesize
		dwFileSize	= GetFileSize(hMessageFile, NULL);
		//	Check filesize
		if (dwFileSize != INVALID_FILE_SIZE &&
			dwFileSize > 0)
		{
			//	Allocate read buffer
			lpReadBuffer	= (LPBYTE)Allocate("Cookie:Buffer", dwFileSize + 2);
			//	Check buffer
			if (lpReadBuffer)
			{
				//	Read file to buffer
				if (ReadFile(hMessageFile, lpReadBuffer, dwFileSize, &dwBytesRead, NULL) &&
					dwBytesRead > 0)
				{
					//	Read successful
					switch (lpReadBuffer[dwBytesRead - 1])
					{
					case '\0':
						lpReadBuffer[dwBytesRead - 1]	= '\n';
					case '\n':
						lpReadBuffer[dwBytesRead]		= '\0';
						break;
					default:
						lpReadBuffer[dwBytesRead]		= '\n';
						lpReadBuffer[dwBytesRead + 1]	= '\0';
						break;
					}
					//	Compile cookies to return buffer
					if (lpTemp = (LPBYTE)Message_PreCompile2((PCHAR)lpReadBuffer, &dwBytesWritten, szFileName))
					{
						if (lpReturn = Allocate("Cookie:Cache:Result", dwBytesWritten))
						{
							CopyMemory(lpReturn, lpTemp, dwBytesWritten);
						}
						Free(lpTemp);
					}
				}
				Free(lpReadBuffer);
			}
		}
		//	Check return value
		if (! lpReturn) goto END;
	}

	if (! lpSearchResult)
	{
		lpItem = lpCacheItem;
		//	Update item
		CopyMemory((LPSTR)&lpItem[1], szFileName, dwFileName);
		lpItem->szFileName = (LPSTR)&lpItem[1];

		if (Cached == CacheMax)
		{
			//	Cache full, remove last entry
			lpCacheItem = QuickDelete(lpMessageCache, Cached, lpMessageCacheList[TAIL],
				                      (QUICKCOMPAREPROC) Cache_Compare, NULL);
			if (lpCacheItem)
			{
				//	Get temp filename
				if ((lpMessageCacheList[TAIL] = lpMessageCacheList[TAIL]->lpPrev)) 
				{
					lpMessageCacheList[TAIL]->lpNext	= NULL;
				}
				else lpMessageCacheList[HEAD]	= NULL;
				Cached--;
				Free(lpCacheItem->pBuffer);
			}
		}
		lpCacheItem = NULL;

		//	Insert item into tables
		QuickInsert(lpMessageCache,	Cached++, lpItem, (QUICKCOMPAREPROC) Cache_Compare);
	}
	else
	{
		lpItem = lpSearchResult[0];
		//	Update cache time
		CopyMemory(&lpItem->ftCacheTime, &lpCacheItem->ftCacheTime, sizeof(FILETIME));

		//	Delete item from list
		DELETELIST(lpItem, lpMessageCacheList);
	}
	//	Insert item to list
	INSERTLIST(lpItem, lpMessageCacheList);

	lpItem->dwBufLen = dwBytesWritten;
	if (lpItem->pBuffer && (lpItem->pBuffer != lpReturn))
	{
		Free(lpItem->pBuffer);
	}
	lpItem->pBuffer = lpReturn;

	// duplicate item here...
	if (lpReturn = Allocate("MessageLoad", lpItem->dwBufLen))
	{
		CopyMemory(lpReturn, lpItem->pBuffer, lpItem->dwBufLen);
	}

END:
	LeaveCriticalSection(&lIndex);
	if (lpCacheItem) Free(lpCacheItem);
	CloseHandle(hMessageFile);
	return lpReturn;
}





BOOL Message_Compile(LPBYTE pBuffer, LPBUFFER lpOutBuffer, BOOL bSkipFirstPrefix, LPVOID lpData,
					 DWORD dwData, LPSTR szPrefix, LPSTR szLastPrefix)
{
	MESSAGEDATA		MessageData;

	//	Make sure input buffer exists
	if (! pBuffer) return TRUE;
	//	Reset unified variable storage
	ZeroMemory(&MessageData, sizeof(MESSAGEDATA));
	//	Initialize offsets
	InitDataOffsets(&MessageData.DataOffsets, lpData, dwData);

	MessageData.lpOutBuffer	= lpOutBuffer;

	MessageData.lpData		= lpData;
	MessageData.dwData		= dwData;

	if (szPrefix)
	{
		MessageData.szPrefix[0]	= szPrefix;
		MessageData.dwPrefix[0]	= strlen(szPrefix);

		if (! szLastPrefix)
		{
			MessageData.dwPrefix[1]	= MessageData.dwPrefix[0];
			MessageData.szPrefix[1]	= MessageData.szPrefix[0];
		}
		else
		{
			MessageData.dwPrefix[1]	= strlen(szLastPrefix);
			MessageData.szPrefix[1]	= szLastPrefix;
		}
	}

	//	Compile Cookies
	Compile_Message(&MessageData, pBuffer, FALSE, bSkipFirstPrefix);

	return FALSE;
}



BOOL MessageFile_Show(LPSTR szFileName, LPBUFFER lpOutBuffer, LPVOID lpData,
					  DWORD dwData, LPSTR szPrefix, LPSTR szLastPrefix)
{
	LPBYTE	pBuffer;

	//	Load Message File
	if (pBuffer = Message_Load(szFileName))
	{
		//	Show MessageFile
		Message_Compile(pBuffer, lpOutBuffer, FALSE, lpData, dwData, szPrefix, szLastPrefix);
		//	Free memory
		Free(pBuffer);
		return FALSE;
	}
	return TRUE;
}



LPOUTPUT_THEME LookupTheme(DWORD n)
{
	LPOUTPUT_THEME lpTheme;

	if (n >= MAX_THEMES) return NULL;

	while (InterlockedExchange(&lThemeLock, TRUE)) SwitchToThread();

	lpTheme = lpThemeArray[n-1];
	if (lpTheme)
	{
		AllocateShared(lpTheme, 0, 0);
	}

	InterlockedExchange(&lThemeLock, FALSE);
	return lpTheme;
}


LPTSTR Admin_Color(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR         tszCommand, tszAction, tszIndex, tszDesc, tszBasePath, tszFileName;
	LPBUFFER       lpBuffer;
	DWORD          n, dwError, dwFileName;
	INT32          iTheme;
	LPOUTPUT_THEME lpTheme;
	LPUSERFILE     lpUserFile;

	tszCommand  = GetStringIndexStatic(Args, 0);
	lpBuffer	= &lpUser->CommandChannel.Out;

	if (!(lpTheme = LookupTheme(1)))
	{
		// no themes defined...
		FormatString(lpBuffer, _T("%sColor feature not available since no themes are defined.\r\n"), tszMultilinePrefix);

		// make sure there isn't an active theme, just in case...
		SetTheme(0);
		SetLastError(ERROR_COMMAND_FAILED);
		return tszCommand;
	}

	if (GetStringItems(Args) == 1)
	{
		// show help and available themes
		// NOTE: lpTheme already points to valid 1st entry so we know we have at least one...
		tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
		if (!tszBasePath)
		{
			return GetStringIndexStatic(Args, 0);
		}
		dwFileName	= aswprintf(&tszFileName, _TEXT("%s\\Color"), tszBasePath);
		if (dwFileName)
		{
			MessageFile_Show(tszFileName, lpBuffer, lpUser, DT_FTPUSER, tszMultilinePrefix, NULL);
			Free(tszFileName);
		}
		FreeShared(tszBasePath);

		FormatString(lpBuffer, _T("%s\r\n%sList of possible themes:\r\n"), tszMultilinePrefix, tszMultilinePrefix);

		while (InterlockedExchange(&lThemeLock, TRUE)) SwitchToThread();
		for(n=1 ; n<= MAX_THEMES ; n++)
		{
			// skip first lookup since we already did it, bail on first not found
			if (n != 1 && !(lpTheme = lpThemeArray[n-1]))
			{
				break;
			}
			if (lpTheme->tszName && (tszDesc = Config_Get(lpIniThemeFile, _TEXT("Themes"), lpTheme->tszName, NULL, NULL)))
			{
				FormatString(lpBuffer, "%s #%u: %s - %s\r\n", tszMultilinePrefix, n, lpTheme->tszName, tszDesc);
				Free(tszDesc);
			}
			else
			{
				FormatString(lpBuffer, "%s #%u: %s - No description available.\r\n", tszMultilinePrefix, n, lpTheme->tszName);
			}
		}
		InterlockedExchange(&lThemeLock, FALSE);

		if (lpUser->FtpVariables.iTheme == 0)
		{
			FormatString(lpBuffer, _T("%s\r\n%sColor mode is currently disabled.\r\n"), 
				tszMultilinePrefix, tszMultilinePrefix);
		}
		else
		{
			FormatString(lpBuffer, _T("%s\r\n%sColor mode is currently set to theme #%d.\r\n"), 
				tszMultilinePrefix, tszMultilinePrefix, lpUser->FtpVariables.iTheme);
		}
		if (lpUser->UserFile->Theme == 0)
		{
			FormatString(lpBuffer, _T("%sOn new logins color mode will default to off.\r\n%s\r\n"), 
				tszMultilinePrefix, tszMultilinePrefix);
		}
		else
		{
			FormatString(lpBuffer, _T("%sOn new logins color mode will automatically enable theme #%d.\r\n%s\r\n"),
				tszMultilinePrefix, lpUser->UserFile->Theme, tszMultilinePrefix);
		}

		return NULL;
	}

	tszAction = GetStringIndexStatic(Args,1);

	if (!_tcsicmp(tszAction, _T("default")))
	{
		if (GetStringItems(Args) == 2)
		{
			FreeShared(lpTheme);
			ERROR_RETURN(ERROR_MISSING_ARGUMENT, tszCommand);
		}

		FreeShared(lpTheme);
		tszIndex = GetStringIndexStatic(Args,2);
		if ((1 != _stscanf_s(tszIndex, "%d", &iTheme)) || iTheme < 0 || iTheme > MAX_THEMES)
		{
			ERROR_RETURN(ERROR_BAD_THEME, tszIndex);
		}
		if ( iTheme != 0 )
		{
			if ( ! (lpTheme = LookupTheme(iTheme)) )
			{
				ERROR_RETURN(ERROR_BAD_THEME, tszIndex);
			}
			FreeShared(lpTheme);
		}

		// not changing the active status, just changing the userfile
		dwError = NO_ERROR;
		if (! UserFile_OpenPrimitive(lpUser->UserFile->Uid, &lpUserFile, 0))
		{
			if (! UserFile_Lock(&lpUserFile, 0))
			{
				lpUserFile->Theme = iTheme;
				UserFile_Unlock(&lpUserFile, 0);
			}
			else dwError	= GetLastError();
			UserFile_Close(&lpUserFile, 0);
			if (dwError == NO_ERROR)
			{
				FormatString(lpBuffer, "%sUserfile updated.\r\n", tszMultilinePrefix);
				return NULL;
			}
		}
		else dwError	= GetLastError();
		ERROR_RETURN(dwError, tszCommand);
	}
	else if (!_tcsicmp(tszAction, _T("on")))
	{
		if (GetStringItems(Args) == 2)
		{
			iTheme = 1;
		}
		else
		{
			tszIndex = GetStringIndexStatic(Args,2);
			if ((1 != _stscanf_s(tszIndex, "%d", &iTheme)) || iTheme <= 0 || iTheme > MAX_THEMES)
			{
				ERROR_RETURN(ERROR_BAD_THEME, tszIndex);
			}
			if (iTheme != 1)
			{
				FreeShared(lpTheme);
				if (! (lpTheme = LookupTheme(iTheme)) )
				{
					ERROR_RETURN(ERROR_BAD_THEME, tszIndex);
				}
			}
		}
		FreeShared(lpUser->FtpVariables.lpTheme);
		lpUser->FtpVariables.iTheme = iTheme;
		lpUser->FtpVariables.lpTheme = lpTheme;
		SetTheme(lpTheme);
		if (dwNumThemes == 1)
		{
			FormatString(lpBuffer, _T("%sColor feature enabled.\r\n"), tszMultilinePrefix);
		}
		else
		{
			FormatString(lpBuffer, _T("%sColor feature enabled using theme #%d (%s).\r\n"), tszMultilinePrefix, iTheme, lpTheme->tszName);
		}
		return NULL;
	}
	else if (!_tcsicmp(tszAction, _T("off")))
	{
		lpUser->FtpVariables.iTheme  = 0;
		SetTheme(0);
		FreeShared(lpTheme);
		lpUser->FtpVariables.lpTheme = 0;
		FormatString(lpBuffer, _T("%sColor feature disabled.\r\n"), tszMultilinePrefix);
		return NULL;
	}

	FreeShared(lpTheme);
	ERROR_RETURN(IO_INVALID_ARGUMENTS, tszAction);
}

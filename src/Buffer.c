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

VOID Insert_Buffer(LPBUFFER Target, LPVOID In, DWORD Size)
{
	LPVOID	Memory;
	//	Check buffer size
	if (Target->len + Size > Target->size)
	{
		//	Need to allocate more space
		Memory	= ReAllocate(Target->buf, NULL, Target->len + Size + 256);
		if (! Memory) return;
		Target->buf	= (PCHAR)Memory;
	}
	//	Shift existing data
	MoveMemory(&Target->buf[Size], Target->buf, Target->len);
	CopyMemory(Target->buf, In, Size);
	//	Increase size
	Target->len	+= Size;
}



VOID Put_Buffer(LPBUFFER Buffer, LPVOID In, DWORD Size)
{
	LPVOID	Memory;
	DWORD	bSize;

	//	Allocate more space if needed
	if (Buffer->len + Size > Buffer->size)
	{
		//	Zero sized buffer
		if (! (bSize = Buffer->size)) bSize	= 512;

		do
		{
			//	Double the size
			bSize	*= 2;

		} while (Buffer->len + Size > bSize);
		//	Try to reallocate memory
		if (! (Memory = ReAllocate(Buffer->buf, NULL, bSize)))
		{
			return;
		}
		//	Set New pointers
		Buffer->buf		= (PCHAR)Memory;
		Buffer->size	= bSize;
	}
	//	Append to buffer
	CopyMemory(&Buffer->buf[Buffer->len], In, Size);
	Buffer->len	+= Size;
}



BOOL AllocateBuffer(LPBUFFER lpBuffer, DWORD dwMinimum)
{
	LPVOID	lpMemory;
	DWORD	dwBufferSize;

	if (lpBuffer->size >= dwMinimum)
	{
		return FALSE;
	}

	dwBufferSize = (dwMinimum & 1023 ? dwMinimum + 1024 : dwMinimum) & (((DWORD)-1) - 1023);

	if (! (lpMemory = ReAllocate(lpBuffer->buf, NULL, dwBufferSize))) return TRUE;

	lpBuffer->buf	= (PCHAR)lpMemory;
	lpBuffer->size	= dwBufferSize;
	return FALSE;
}




BOOL AppendStringW(LPBUFFER lpBuffer, LPWSTR wsString, DWORD dwString)
{
	DWORD	dwBufferSpace;
	INT		iLength;

	if (! dwString) return FALSE;

	dwBufferSpace	= lpBuffer->size - lpBuffer->len;

	switch (lpBuffer->dwType)
	{
	case TYPE_WIDE:
		dwString	*= 2;
		break;
	case TYPE_MULTICHAR:
		//	Multichar string
		while (dwString--)
		{
			//	Check available space
			if (dwBufferSpace < (DWORD)MB_CUR_MAX)
			{
				if (AllocateBuffer(lpBuffer, lpBuffer->len + MB_CUR_MAX)) return TRUE;
				dwBufferSpace	= lpBuffer->size - lpBuffer->len;
			}
			iLength	= wctomb(&lpBuffer->buf[lpBuffer->len], (wsString++)[0]);

			if (iLength == -1)
			{
				lpBuffer->buf[lpBuffer->len]	= 'x';
				iLength	= 1;
			}
			dwBufferSpace	-= iLength;
			lpBuffer->len	+= iLength;
		}
		return FALSE;
	}
	
	//	Check available space
	if (dwBufferSpace < dwString &&
		AllocateBuffer(lpBuffer, lpBuffer->len + dwString)) return TRUE;

	//	Append to buffer
	switch (lpBuffer->dwType)
	{
	case TYPE_WIDE:
		CopyMemory(&lpBuffer->buf[lpBuffer->len], wsString, dwString);
		lpBuffer->len	+= dwString;
		break;
	case TYPE_CHAR:
		while (dwString--) lpBuffer->buf[lpBuffer->len++]	= ((wsString++)[0] <= 255 ? wsString[-1] : 'x');
		break;
	}
	return FALSE;
}








BOOL FormatStringWVA(LPBUFFER lpBuffer, LPWSTR wszFormat, va_list Arguments)
{
	INT		iFormatArgs, iVariableSize, iResult, iFormatArg[2], iLen;
	LPOUTPUT_THEME lpTheme;
	LPTHEME_FIELD  lpField;
	WCHAR	wszTempBuffer[512];
	BYTE	pFormatString[512];
	PBYTE	pOffset;
	CHAR	pBuffer[16];
	BOOL	bLoop, bReturn, bProcess;
	LPWSTR	wszString;
	LPVOID	lpMemory;
	PWCHAR	wpCopyFrom, wpFormat;
	DWORD	dwBufferSize, dwFormatString, dwBufferSpace, dwStackSize, dwLength, n;

	wpCopyFrom	= wszFormat;
	bReturn		= TRUE;
	bProcess	= TRUE;

	do
	{
		switch ((wszFormat++)[0])
		{
		case L'%':
			//	Format string
			if (AppendStringW(lpBuffer, wpCopyFrom, wszFormat - wpCopyFrom))
			{
				bProcess	= FALSE;
				break;
			}
			//	"%%"
			if (wszFormat[0] == L'%')
			{
				wpCopyFrom	= ++wszFormat;
				break;
			}

			iFormatArg[0]	= 0;
			iFormatArg[1]	= -1;
			wpFormat		= &wszFormat[0];
			lpBuffer->len	-= (lpBuffer->dwType == TYPE_WIDE ? sizeof(WCHAR) : sizeof(CHAR));
			iFormatArgs		= 0;
			dwStackSize		= 0;
			iVariableSize	= 0;

			//	Get format arguments
			if (! iswalpha(wszFormat[0]))
			{
				//	"*" / "xx" / "-xx"
				if (wpFormat[0] == L'*')
				{
					iFormatArgs++;
					wpFormat++;
				}
				else if (iswdigit(wpFormat[0]) || wpFormat[0] == L'-')
				{
					iFormatArg[0]	= wcstol(wpFormat, &wpFormat, 10);
				}

				//	".*" / ".xx" / ".-xx"
				if (wpFormat[0] == L'.')
				{
					if ((++wpFormat)[0] == L'*')
					{
						iFormatArgs++;
						wpFormat++;
					}
					else if (iswdigit(wpFormat[0]) || wpFormat[0] == L'-')
					{
						iFormatArg[1]	= wcstol(wpFormat, &wpFormat, 10);
					}
				}
			}

			//	Format modificators
			do
			{
				switch (wpFormat[0])
				{
				case L'h':
					iVariableSize--;
					wpFormat++;
					break;
				case L'l':
					iVariableSize++;
					wpFormat++;
					break;
				case L'I':
					if (! wcsncmp(&wpFormat[1], L"64", 2))
					{
						iVariableSize	= 100;
						wpFormat	+= 3;
						bLoop		= FALSE;
						break;
					}
				case L'U':
					wpFormat++;
					iVariableSize	= -1;
					break;
				case L'H':
					wpFormat++;
					iVariableSize	= 1;
					break;
				default:
					bLoop	= FALSE;
				}
			} while (bLoop);

			wpCopyFrom		= &wpFormat[1];
			//	Copy zero padded formatstring to stack buffer, in right format (uber neat)
			if (lpBuffer->dwType != TYPE_WIDE)
			{
				//	Convert format string to c-string
				wszString		= &wszFormat[-1];
				dwFormatString	= min(&wpFormat[1] - &wszFormat[-1], sizeof(pFormatString) - 2);
				pOffset			= pFormatString;

				if (wpFormat[0] == L'S')
				{
					pFormatString[dwFormatString - 1]	= 's';
					pFormatString[dwFormatString--]	= '\0';
				}
				else pFormatString[dwFormatString]	= '\0';			

				while (dwFormatString--) (pOffset++)[0]	= ((wszString++)[0] <= 255 ? wszString[-1] : 'x');
			}
			else
			{
				//	Wide format string
				dwFormatString	= min((ULONG)&wpFormat[1] - (ULONG)&wszFormat[-1], sizeof(pFormatString) - sizeof(WCHAR));
				CopyMemory(pFormatString, &wszFormat[-1], dwFormatString);
				pFormatString[dwFormatString]	= L'\0';
			}

			for (;;)
			{
				dwBufferSpace	= (lpBuffer->size - lpBuffer->len) / sizeof(WCHAR);

				switch (wpFormat[0])
				{
				case L'f':
				case L'e':
					//	Floating point
					switch (lpBuffer->dwType)
					{
					case TYPE_CHAR:
					case TYPE_MULTICHAR:
						iResult	= _vsnprintf((LPSTR)&lpBuffer->buf[lpBuffer->len], dwBufferSpace * sizeof(WCHAR), (LPCSTR)pFormatString, Arguments);
						break;
					case TYPE_WIDE:
						iResult	= _vsnwprintf((LPWSTR)&lpBuffer->buf[lpBuffer->len], dwBufferSpace, (LPCWSTR)pFormatString, Arguments) * sizeof(WCHAR);
						break;
					}
					dwStackSize	= (iVariableSize < 0 ? sizeof(FLOAT) : sizeof(DOUBLE));
					break;

				case L'i':
				case L'd':
				case L'u':
				case L'x':
				case L'X':
				case L'o':
					//	Integer
					switch (lpBuffer->dwType)
					{
					case TYPE_CHAR:
					case TYPE_MULTICHAR:
						iResult	= _vsnprintf((LPSTR)&lpBuffer->buf[lpBuffer->len], dwBufferSpace * sizeof(WCHAR), (LPCSTR)pFormatString, Arguments);
						break;
					case TYPE_WIDE:
						iResult	= _vsnwprintf((LPWSTR)&lpBuffer->buf[lpBuffer->len], dwBufferSpace, (LPCWSTR)pFormatString, Arguments) * sizeof(WCHAR);
						break;
					}

					switch (iVariableSize)
					{
					case 0:
						dwStackSize	= _INTSIZEOF(INT);
						break;
					case 100:
						dwStackSize	= _INTSIZEOF(INT64);
						break;
					case 1:
						dwStackSize	= _INTSIZEOF(LONG);
						break;
					case -1:
						dwStackSize	= _INTSIZEOF(SHORT);
						break;
					}
					break;

				case L'c':
					//	Character
					switch (lpBuffer->dwType)
					{
					case TYPE_CHAR:
					case TYPE_MULTICHAR:
						if (iswascii(((LPINT)(Arguments + _INTSIZEOF(INT) * iFormatArgs))[0]))
						{
							iResult	= _vsnprintf((LPSTR)&lpBuffer->buf[lpBuffer->len], dwBufferSpace * sizeof(WCHAR), (LPCSTR)pFormatString, Arguments);
						}
						else iResult	= 0;
						break;
					case TYPE_WIDE:
						iResult	= _vsnwprintf((LPWSTR)&lpBuffer->buf[lpBuffer->len], dwBufferSpace, (LPCWSTR)pFormatString, Arguments) * sizeof(WCHAR);
						break;
					}
					dwStackSize	= _INTSIZEOF(INT);
					break;
				case L's':
				case L'S':
					switch (iVariableSize)
					{
					default:
						switch (lpBuffer->dwType)
						{
						case TYPE_CHAR:
							iResult	= _vsnprintf((LPSTR)&lpBuffer->buf[lpBuffer->len], dwBufferSpace * sizeof(WCHAR), (LPCSTR)pFormatString, Arguments);
							dwStackSize	= _INTSIZEOF(LPSTR);
							break;
						case TYPE_MULTICHAR:
							//	Multibyte character conversion
							switch (iFormatArgs)
							{
							case 3:
								iFormatArg[0]	= va_arg(Arguments, INT);
							case 2:
								iFormatArg[1]	= va_arg(Arguments, INT);
								break;
							case 1:
								iFormatArg[0]	= va_arg(Arguments, INT);
							}

							wszString	= va_arg(Arguments, LPWSTR);
							dwLength	= wcslen(wszString);
							if (iFormatArg[1] > 0 && (DWORD)iFormatArg[1] < dwLength) dwLength	= iFormatArg[1];

							//	Heading spaces
							if (iFormatArg[0] < 0 &&
								(DWORD)(0 - iFormatArg[0]) > dwLength)								
							{
								n	= (0 - iFormatArg[0]) - dwLength;
								//	Check buffer space
								if (dwBufferSpace < n)
								{
									if (AllocateBuffer(lpBuffer, lpBuffer->len + n))
									{
										bProcess	= FALSE;
										break;
									}
								}
								FillMemory(&lpBuffer->buf[lpBuffer->len], n, ' ');
								lpBuffer->len	+= n;
							}
							dwBufferSpace	= lpBuffer->size - lpBuffer->len;
							//	Multibyte char string
							for (n = dwLength;n--;)
							{
								if (dwBufferSpace < (DWORD)MB_CUR_MAX)
								{
									if (AllocateBuffer(lpBuffer, lpBuffer->len + MB_CUR_MAX))
									{
										bProcess	= FALSE;
										break;
									}
									dwBufferSpace	= lpBuffer->size - lpBuffer->len;
								}
								if ((iResult = wctomb(&lpBuffer->buf[lpBuffer->len], (wszString++)[0])) < 0)
								{
									lpBuffer->buf[lpBuffer->len]	= 'x';
									iResult	= 1;
								}
								lpBuffer->len	+= iResult;
								dwBufferSpace	-= iResult;
							}
							//	Trailing spaces
							if (iFormatArg[0] > 0 &&
								(DWORD)iFormatArg[0] > dwLength)
							{
								n	= iFormatArg[0] - dwLength;
								if (dwBufferSpace < n)
								{
									if (AllocateBuffer(lpBuffer, lpBuffer->len + n))
									{
										bProcess	= FALSE;
										break;
									}
								}
								FillMemory(&lpBuffer->buf[lpBuffer->len], n, ' ');
								lpBuffer->len	+= n;
								dwBufferSpace	= lpBuffer->size - lpBuffer->len;
							}
							iFormatArgs	= 0;
							iResult	= 0;
							break;
						case TYPE_WIDE:
							iResult	= _vsnwprintf((LPWSTR)&lpBuffer->buf[lpBuffer->len], dwBufferSpace, (LPWSTR)pFormatString, Arguments) * sizeof(WCHAR);
							dwStackSize	= _INTSIZEOF(LPWSTR);
							break;
						}
						break;
					case -1:
						//	Url encode
						wszString	= va_arg(Arguments, LPWSTR);
						iResult		= 0;

						while (wszString[0])
						{
							//	Check buffer space
							if (dwBufferSpace < 8)
							{
								if (AllocateBuffer(lpBuffer, lpBuffer->len + 16))
								{
									bProcess	= TRUE;
									break;
								}
								dwBufferSpace	= (lpBuffer->size - lpBuffer->len) / sizeof(WCHAR);
							}

							if ((dwLength = wctomb(pBuffer, wszString[0])) > 1)
							{
								//	Wide character
								dwLength	= _snprintf((LPSTR)&lpBuffer->buf[lpBuffer->len], 16, "&#%u;", (DWORD)wszString[0]);
							}
							else if (wcschr(L"$&+,/:;=@ \"<>#%", wszString[0]))
							{
								//	Special character
								dwLength	= _snprintf((LPSTR)&lpBuffer->buf[lpBuffer->len], 16, "%%%02X", (BYTE)wszString[0]);
							}
							else
							{
								//	Wide character
								((PCHAR)&lpBuffer->buf[lpBuffer->len])[0]	= (CHAR)wszString[0];
								dwLength	= 1;
							}
							wszString++;
							dwBufferSpace	-= dwLength;
							lpBuffer->len	+= dwLength;// * sizeof(WCHAR));
						}
						break;
/*					case 1:
						//	Html encode
						wszString	= va_arg(Arguments, LPWSTR);
						iResult		= wcslen(wszString) * sizeof(WIDE);
						CopyMemory(&lpBuffer->buf[lpBuffer->len], wszString, iResult);
						break; */
					}
					break;
					
				case L'E':
					//	Error string
					n	= sizeof(wszTempBuffer) / sizeof(WCHAR);
					if (wszString = FormatError(va_arg(Arguments, DWORD), wszTempBuffer, &n))
					{
						AppendStringW(lpBuffer, wszString, n);
					}
					iResult	= 0;
					break;

				case L'Z':
					AppendStringW(lpBuffer, L"\0", 1);
					iResult	= 0;
					break;

				case L'T':
					iResult = 0;
					wszString = wszTempBuffer;
					dwLength = 0;

					if ((iFormatArg[0] < 0) || (iFormatArg[0] > MAX_COLORS) || !(lpTheme = GetTheme()))
					{
						break;
					}

					if (iFormatArg[0] == 0)
					{
						iLen = _snprintf((LPSTR)wszTempBuffer, sizeof(wszTempBuffer), "%c[%dm", 27, 39);
						if (iLen > 0)
						{
							wszString += iLen;
							dwLength += iLen;
						}
					}
					else
					{
						lpField = &lpTheme->ThemeFieldsArray[iFormatArg[0]];
						if (!lpField->i || lpField->Settings.doFormat) break;

						if (lpField->Settings.doReset)
						{
							// issue ANSI color reset
							iLen = _snprintf((LPSTR)wszTempBuffer, sizeof(wszTempBuffer), "%c[%dm", 27, 39);
							if (iLen > 0)
							{
								wszString += iLen;
								dwLength += iLen;
							}
						}

						if (lpField->Settings.doInverse)
						{
							iLen = _snprintf((LPSTR)wszTempBuffer, sizeof(wszTempBuffer), "%c[%dm", 27, 7);
							if (iLen > 0)
							{
								wszString += iLen;
								dwLength += iLen;
							}
						}
						if (lpField->Settings.hasUnderLine)
						{
							if (lpField->Settings.Underline)
							{
								iLen = _snprintf((LPSTR)wszTempBuffer, sizeof(wszTempBuffer), "%c[%dm", 27, 4);
							}
							else
							{
								iLen = _snprintf((LPSTR)wszTempBuffer, sizeof(wszTempBuffer), "%c[%dm", 27, 24);
							}
							if (iLen > 0)
							{
								wszString += iLen;
								dwLength += iLen;
							}
						}

						// it can only have foreground/background settings
						if (lpField->Settings.Background)
						{
							iLen = _snprintf((LPSTR)wszTempBuffer, sizeof(wszTempBuffer), "%c[%dm", 27, 39 + lpField->Settings.Background);
							if (iLen > 0)
							{
								wszString += iLen;
								dwLength += iLen;
							}
						}
						if (lpField->Settings.Foreground)
						{
							iLen = _snprintf((LPSTR)wszTempBuffer, sizeof(wszTempBuffer), "%c[%dm", 27, 29 + lpField->Settings.Foreground);
							if (iLen > 0)
							{
								wszString += iLen;
								dwLength += iLen;
							}
						}
					}

					//	Check buffer space
					if (dwBufferSpace < dwLength)
					{
						if (AllocateBuffer(lpBuffer, lpBuffer->len + 64))
						{
							bProcess	= FALSE;
							break;
						}
						dwBufferSpace	= (lpBuffer->size - lpBuffer->len) / sizeof(WCHAR);
					}
					CopyMemory(&lpBuffer->buf[lpBuffer->len], wszTempBuffer, dwLength);
					dwBufferSpace	-= dwLength;
					lpBuffer->len	+= dwLength;
					iResult = 0;
					break;

				default:
					iResult	= 0;
					break;
				}

				//	Check format result
				if (iResult >= 0)
				{
					lpBuffer->len	+= iResult;
					Arguments		= (va_list)((ULONG)Arguments + (iFormatArgs * sizeof(INT)) + dwStackSize);
					break;
				}

				//	Allocate more buffer space
				dwBufferSize	= lpBuffer->size;
				if (! (dwBufferSize *= 2)) dwBufferSize	= 1024;

				if (! (lpMemory = ReAllocate(lpBuffer->buf, NULL, dwBufferSize)))
				{
					//	Could not allocate buffer, no mem?
					bProcess	= FALSE;
					break;
				}

				lpBuffer->buf	= (PCHAR)lpMemory;
				lpBuffer->size	= dwBufferSize;
			}


			break;
		case L'\0':
			//	String end
			bReturn		= AppendStringW(lpBuffer, wpCopyFrom, &wszFormat[-1] - wpCopyFrom);
			bProcess	= FALSE;
			break;
		}

	} while (bProcess);

	return TRUE;
}




BOOL FormatStringW(LPBUFFER lpBuffer, LPWSTR wszFormat, ...)
{
	register va_list	Arguments;
	register DWORD		dwReturn;

	va_start(Arguments, wszFormat);
	dwReturn	= FormatStringWVA(lpBuffer, wszFormat, Arguments);
	va_end(Arguments);
	return dwReturn;
}





BOOL FormatStringAVA(LPBUFFER lpBuffer, LPCSTR szFormat, va_list Arguments)
{
	LPWSTR			wszFormat;
	register PWCHAR	wpOffset;
	register DWORD	dwFormat;
	BOOL			bSwap;

	bSwap		= 0;
	dwFormat	= strlen(szFormat);
	wszFormat	= _alloca((dwFormat + 1) * sizeof(WCHAR));
	if (! wszFormat) return FALSE;
	//	Format string
	for (wpOffset = wszFormat;dwFormat--;)
	{
		switch (wpOffset++[0] = szFormat++[0])
		{
		case L'S':
			if (bSwap) wpOffset[-1]	= L's';
			bSwap	= FALSE;
			break;
		case L's':
			if (bSwap) wpOffset[-1]	= L'S';
			bSwap	= FALSE;
			break;
		case L'%':
			bSwap	= (bSwap ? FALSE : TRUE);
			break;
		default:
			if (szFormat[-1] == '.' ||
				szFormat[-1] == '-' ||
				szFormat[-1] == '*' ||
				(szFormat[-1] >= '0' &&
				szFormat[-1] <='9')) break;
			bSwap	= FALSE;
		}
	}
	wpOffset[0]	= L'\0';

	return FormatStringWVA(lpBuffer, wszFormat, Arguments);
}




BOOL FormatStringA(LPBUFFER lpBuffer, LPCSTR szFormat, ...)
{
	va_list	Arguments;
	BOOL	bReturn;

	va_start(Arguments, szFormat);
	bReturn	= FormatStringAVA(lpBuffer, szFormat, Arguments);
	va_end(Arguments);

	return bReturn;
}








DWORD aswprintf(LPTSTR *lpBuffer, LPCTSTR tszFormat, ...)
{
	BUFFER	Buffer;
	va_list	Arguments;
	BOOL	bResult;

	ZeroMemory(&Buffer, sizeof(BUFFER));
	va_start(Arguments, tszFormat);
	//	Format string
#ifdef _UNICODE
	bResult	= FormatStringWVA(&Buffer, tszFormat, Arguments);
	if (bResult) AppendStringW(&Buffer, L"", 1);
#else
	bResult	= FormatStringAVA(&Buffer, tszFormat, Arguments);
	if (bResult) AppendStringA(&Buffer, "", 1);
#endif
	va_end(Arguments);

	//	Check result
	if (! bResult || ! Buffer.buf)
	{
		Free(Buffer.buf);
		return 0;
	}
	lpBuffer[0]	= (LPTSTR)Buffer.buf;
	return Buffer.len / sizeof(TCHAR) - 1;
}














/*
VOID Put_Buffer_FormatEx(BUFFER *Target, LPCSTR Format, ...)
{
	va_list		Arguments, bArguments;
	CHAR		szErrorBuffer[512];
	CHAR		FormatString[32], *nFormat, *pLast, Sets, vSize, *Buffer, *Data;
	BOOL		Loop;
	DWORD		bLength, bSize;
	INT			Length, Set[2];

	pLast	= (LPSTR)Format;
	Buffer	= &Target->buf[Target->len];
	bLength	= Target->len;
	bSize	= Target->size;

	va_start(Arguments, Format);

	for (;;)
	{
		switch ((Format++)[0])
		{
		case '%':
			Length	= Format - pLast;

			if ((DWORD)Length > bSize - bLength)
			{
				//	Zero sized buffer
				if (! bSize) bSize	= 512;

				do
				{
					bSize	*= 2;

				} while (Length + bLength > bSize);

				if (! (Buffer = (PCHAR)ReAllocate(Target->buf, NULL, bSize)))
				{
					//	Could not allocate buffer, no mem?
					va_end(Arguments);
					return;
				}

				Target->buf		= Buffer;
				Target->size	= bSize;
				Buffer			+= bLength;
			}

			CopyMemory(Buffer, pLast, Length);
			bLength	+= Length;
			Buffer	+= Length;

			if (Format[0] == '%')
			{
				pLast	= (PCHAR)(++Format);
				break;
			}

			//	Remove '%'
			Buffer--;
			bLength--;

			Sets		= 0;
			vSize		= 0;
			nFormat		= (CHAR *)&Format[0];

			if (! isalpha(nFormat[0]))
			{
				//	Check: digit.digit
				switch (nFormat[0])
				{
				case '*':
					Set[Sets++] = va_arg(Arguments, INT);
					nFormat++;
					break;

				case '-':
					//	Minus
					nFormat++;
				default:
					while (isdigit(nFormat[0]))
					{
						//	Digits
						nFormat++;
					}
				}

				if (nFormat[0] == '.')
				{
					//	Dot
					nFormat++;

					switch (nFormat[0])
					{
					case '*':
						Set[Sets++] = va_arg(Arguments, INT);
						nFormat++;
						break;

					case '-':
						//	Minus
						nFormat++;

					default:
						while (isdigit(nFormat[0]))
						{
							//	Digits
							nFormat++;
						}
					}
				}
			}

			Loop	= TRUE;

			while (Loop)
			{
				switch (nFormat[0])
				{
				case 'h':
					vSize--;
					nFormat++;
					break;
				case 'l':
					vSize++;
					nFormat++;
					break;

				case 'I':
					if (! memcmp(&nFormat[1], "64", 2))
					{
						vSize	= 100;
						nFormat	+= 3;

						break;
					}
				case 'U':
					nFormat++;
					vSize	= -1;
					break;
				case 'H':
					nFormat++;
					vSize	= 1;
					break;
				default:
					Loop	= FALSE;
				}
			}

			Length	= nFormat - &Format[-1] + 1;

			CopyMemory(FormatString, &Format[-1], Length);
			FormatString[Length]	= '\0';
			bArguments		= Arguments;

			for (;;)
			{
				switch (nFormat[0])
				{
				case 'i':
				case 'd':
					Length	= _vsnprintf(Buffer, bSize - bLength, FormatString, Arguments);
					switch (vSize)
					{
					case -2:
						//  Character (8bit)
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, CHAR));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, CHAR));
							break;
						default:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, CHAR));
							break;
						}
						break;
					case -1:
						//	Short int (16bit)
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, SHORT));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, SHORT));
							break;
						default:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, SHORT));
							break;
						}
						break;
					case 0:
						//	Int (32bit)
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, INT));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, INT));
							break;
						default:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, INT));
							break;
						}
						break;
					case 1:
						//	Long Int (varies)
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, LONG));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, LONG));
							break;
						default:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, LONG));
							break;
						}
						break;
					case 2:
						//	Long Long Int (64bit)
					case 100:
						//	64bit Int
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, INT64));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, INT64));
							break;
						default:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, INT64));
							break;
						}
						break;
					}
					break;
					
				case 'u':
				case 'x':
				case 'X':
				case 'o':
					switch (vSize)
					{
					case -2:
						//	Character
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, UCHAR));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, UCHAR));
							break;
						default:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, UCHAR));
							break;
						}
						break;

					case -1:
						//	Short int
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, USHORT));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, USHORT));
							break;
						default:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, USHORT));
							break;
						}
						break;

					case 0:
						//	Int
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, UINT));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, UINT));
							break;
						default:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, UINT));
							break;
						}
						break;

					case 1:
						//	Long Int
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, ULONG));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, ULONG));
							break;
						default:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, ULONG));
							break;
						}
						break;

					case 2:
						//	Long Long Int (64bit)
					case 100:
						//	64bit Int
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, UINT64));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, UINT64));
							break;
						default:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, UINT64));
							break;
						}
						break;
					}
					break;
				
				case 's':
					//	String
					switch (vSize)
					{
					case 0:
						//	Normal string
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, LPSTR));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, LPSTR));
							break;
						case 2:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, LPSTR));
							break; 
						}
					break;

					case -1:
						//	Url Encode string (we don't care about formating)
						Data	= va_arg(Arguments, PCHAR);
						Length	= 0;

						while (Data[0])
						{
							if (memchr("$&+,/:;=@ \"<>#%", Data[0], 15))
							{
								if (bSize - bLength - Length < 3)
								{
									Buffer	= (CHAR *)ReAllocate(Target->buf, NULL, bSize *= 2);

									if (! Buffer) return;

									Target->buf		= Buffer;
									Target->size	= bSize;
									Buffer			+= bLength;
								}

								_snprintf(&Buffer[Length], 3, "%%%02X", (UCHAR)(Data++)[0]);
								Length	+= 3;
							}
							else
							{
								if (bSize - bLength - Length < 1)
								{
									Buffer	= (CHAR *)ReAllocate(Target->buf, NULL, bSize *= 2);

									Target->buf		= Buffer;
									Target->size	= bSize;
									Buffer			+= bLength;
								}

								Buffer[Length++]	= (Data++)[0];
							}
						}						
						break;

					case 1:
						//	Html encode string (format doesn't matter either)
						Data	= va_arg(Arguments, CHAR *);
						Length	= strlen(Data);
						CopyMemory(Buffer, Data, Length);
						break;
					}
					break;

					//	Error string
				case 'E':
//					Length = _snprintf(Buffer, bSize - bLength, "%s",
//						FormatError(va_arg(Arguments, DWORD), szErrorBuffer, sizeof(szErrorBuffer)));
					break;
					
					//	Character
				case 'c':
					switch (Sets)
					{
					case 0:
						Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, CHAR));
						break;
					case 1:
						Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, CHAR));
						break;
					default:
						Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, CHAR));
						break;
					}
					break;
					
				case 'f':
				case 'e':
					switch (vSize)
					{
					case 0:
						//	Double
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, DOUBLE));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, DOUBLE));
							break;
						default:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, DOUBLE));
							break;
						}
						break;
					default:
						//	Float							
						switch (Sets)
						{
						case 0:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, va_arg(Arguments, FLOAT));
							break;
						case 1:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], va_arg(Arguments, FLOAT));
							break;
						default:
							Length = _snprintf(Buffer, bSize - bLength, FormatString, Set[0], Set[1], va_arg(Arguments, FLOAT));
							break;
						}
					}
					break;

				case 'Z':
					if (bLength == bSize)
					{
						//	Not enough buffer space available
						Length	= -1;
					}
					else
					{
						Buffer[0]	= '\0';
						Length		= 1;
					}
					break;
						
				default:
					//	Unknown conversion
					Length	= 0;							
					break;
				}

				if (Length >= 0)
				{
					Buffer	+= Length;
					bLength	+= Length;
					break;
				}

				//	Did not fit to buffer
				if (! (bSize *= 2)) bSize = 1024;

				if (! (Buffer = (PCHAR)ReAllocate(Target->buf, NULL, bSize)))
				{
					//	Could not allocate buffer, no mem?

					va_end(Arguments);
					return;
				}

				Target->buf		= Buffer;
				Target->size	= bSize;
				Buffer			+= bLength;
				Arguments		= bArguments;
			}

			Format	= ++nFormat;
			pLast	= nFormat;
			break;

		case '\0':
			if ((Length = Format - pLast - 1) > 0)
			{
				if ((DWORD)Length > bSize - bLength)
				{
					//	Zero sized buffer
					if (! bSize) bSize	= 512;

					do
					{
						bSize	*= 2;

					} while (Length + bLength > bSize);

					if (! (Buffer = (CHAR *)ReAllocate(Target->buf, NULL, bSize)))
					{
						//	Could not allocate buffer, no mem?

						va_end(Arguments);
						return;
					}

					Target->buf		= Buffer;
					Target->size	= bSize;
					Buffer			+= bLength;
				}

				CopyMemory(Buffer, pLast, Length);
				bLength	+= Length;
			}

			va_end(Arguments);

			Target->len	= bLength;
			return;
		}
	}
}*/

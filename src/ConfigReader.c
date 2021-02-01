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

CONFIG_FILE IniConfigFile;
LPSECTION_INFO *lpSectionInfoArray;
DWORD          dwSectionInfoArray;
DWORD          dwMaxSectionInfoArray;



LPCONFIG_FILE Config_GetIniFile(VOID)
{
	return &IniConfigFile;
}



BOOL FindString(LPTSTR tszIn, DWORD dwIn, LPTSTR *lpOut, PINT iOut)
{
	//	Remove heading spaces
	while (dwIn && _istspace(tszIn[0]))
	{
		dwIn--;
		tszIn++;
	}
	//	Remove padding spaces
	do
	{
		//	Check string length
		if (! dwIn--) return FALSE;

	} while (_istspace(tszIn[dwIn]));
	//	Set return pointers
	lpOut[0]	= tszIn;
	iOut[0]		= dwIn + 1;
	return TRUE;
}



INT Config_Sort(LPCONFIG_LINE *a, LPCONFIG_LINE *b)
{
	register INT	iResult;

	//	Compare variable name length
	if (a[0]->Variable_l > b[0]->Variable_l) return 1;
	if (a[0]->Variable_l < b[0]->Variable_l) return -1;
	//	Compare variable name
	iResult	= _tcsnicmp(a[0]->Variable, b[0]->Variable, a[0]->Variable_l);
	if (iResult != 0) return iResult;

	//	Compare row variable is on
	if (a[0]->Row == b[0]->Row) return 0;
	if (a[0]->Row < b[0]->Row) return -1;

	return 1;
}



INT __cdecl Config_Search(LPCONFIG_LINE *a, LPCONFIG_LINE *b)
{
	//	Compare variable name length
	if (a[0]->Variable_l > b[0]->Variable_l) return 1;
	if (a[0]->Variable_l < b[0]->Variable_l) return -1;
	//	Compare variable name
	return _tcsnicmp(a[0]->Variable, b[0]->Variable, a[0]->Variable_l);
}



BOOL Config_Sort_Array(LPCONFIG_LINE_ARRAY lpLineArray)
{
	LPCONFIG_LINE	lpLine;
	DWORD			dwLines;

	dwLines	= 0;
	//	Check total number of active lines
	if (! lpLineArray->SSize) return FALSE;
	//	Free previous sorted array
	Free(lpLineArray->Sorted);
	//	Allocate memory for new array
	lpLineArray->Sorted	= (LPCONFIG_LINE *)Allocate("Config:SortedArray", sizeof(LPCONFIG_LINE) * lpLineArray->SSize);
	//	Check allocation
	if (! lpLineArray->Sorted) return TRUE;
	//	Move active lines
	for (lpLine = lpLineArray->First_Line;lpLine;lpLine = lpLine->Next)
	{
		//	Insert active line into array
		if (lpLine->Active) lpLineArray->Sorted[dwLines++]	= lpLine;
	}
	//	Quick sort array
	qsort(lpLineArray->Sorted, dwLines, sizeof(LPCONFIG_LINE),
		(INT (__cdecl *)(LPCVOID, LPCVOID))Config_Sort);
	return FALSE;
}



VOID Config_Lock(LPCONFIG_FILE lpConfigFile, BOOL bExclusive)
{
	//	Check lock type
	if (! bExclusive)
	{
		//	Acquire shared lock
		AcquireSharedLock(&lpConfigFile->loConfig);
	}
	else
	{
		//	Acquire exclusive lock
		AcquireExclusiveLock(&lpConfigFile->loConfig);
	}
}



VOID Config_Unlock(LPCONFIG_FILE lpConfigFile, BOOL bExclusive)
{
	//	Check lock type
	if (! bExclusive)
	{
		//	Release shared lock
		ReleaseSharedLock(&lpConfigFile->loConfig);
	}
	else
	{
		//	Release exclusive lock
		ReleaseExclusiveLock(&lpConfigFile->loConfig);
	}
}


BOOL Config_Load(LPTSTR tszFileName, LPCONFIG_FILE *plpConfigFile)
{
	DWORD dwLen, dwError;
	LPCONFIG_FILE lpConfigFile;

	lpConfigFile = *plpConfigFile;

	if (!lpConfigFile)
	{
		dwLen = _tcslen(tszFileName);
		lpConfigFile = (LPCONFIG_FILE) AllocateShared(NULL, _T("CONFIG_FILE"), sizeof(*lpConfigFile)+(dwLen+1)*sizeof(TCHAR));
		if (!lpConfigFile) return FALSE;

		lpConfigFile->tszConfigFile = (LPTSTR) &lpConfigFile[1];
		CopyMemory(lpConfigFile->tszConfigFile, tszFileName, (dwLen+1)*sizeof(TCHAR));

		if (! InitializeLockObject(&lpConfigFile->loConfig))
		{
			dwError = GetLastError();
			FreeShared(lpConfigFile);
			ERROR_RETURN(dwError, FALSE);
		}
		lpConfigFile->lpLineArray = NULL;
		*plpConfigFile = lpConfigFile;
	}

	//	Read config
	if (Config_Read(lpConfigFile))
	{
		dwError = GetLastError();
		ERROR_RETURN(dwError, FALSE);
	}
	return TRUE;
}



VOID Config_Free_Line_Array(LPCONFIG_LINE_ARRAY lpLineArray)
{
	LPCONFIG_LINE_ARRAY	lpNextArray;
	LPCONFIG_LINE		lpLine, lpNextLine;

	//	Loop through array
	for ( ; lpLineArray  ;lpLineArray = lpNextArray)
	{
		//	Free all lines
		for (lpLine = lpLineArray->First_Line;lpLine;lpLine = lpNextLine)
		{
			//	Get pointer to next line
			lpNextLine	= lpLine->Next;
			//	Free previous line
			Free(lpLine->Text);
			Free(lpLine);
		}
		//	Get pointer to next array
		lpNextArray	= lpLineArray->Next;
		//	Free memory
		Free(lpLineArray->Sorted);
		Free(lpLineArray);
	}
}



VOID Config_Free(LPCONFIG_FILE lpConfigFile)
{
	Config_Free_Line_Array(lpConfigFile->lpLineArray);
	DeleteLockObject(&lpConfigFile->loConfig);
	FreeShared(lpConfigFile);
}


// automatically uses IniConfigFile
VOID Config_Parse_Sections()
{
	LPSECTION_INFO      lpSection, *lpTempSectionArray;
	LPCONFIG_LINE_ARRAY	lpArray;
	LPCONFIG_LINE		lpLine;
	DWORD				dwPath, n;
	TCHAR				*tpCheck, *tpLineOffset;
	INT					iCreditSection, iStatsSection, iShareSection;
	BOOL                bOK;

	// free existing information.
	for (n=0 ; n < dwSectionInfoArray ; n++)
	{
		Free(lpSectionInfoArray[n]);
		lpSectionInfoArray[n] = NULL;
	}
	dwSectionInfoArray = 0;

	bOK = TRUE;

	//	Find sections array from config
	for (lpArray = IniConfigFile.lpLineArray ; bOK && lpArray ; lpArray = lpArray->Next)
	{
		if (lpArray->Name_Len != 8 || _tcsicmp(lpArray->Name, _TEXT("Sections")))
		{
			continue;
		}

		//	process lines
		for (lpLine = lpArray->First_Line ; lpLine  ;lpLine = lpLine->Next)
		{
			if (!lpLine->Active) continue;

			//	Get credit section
			tpLineOffset	= lpLine->Value;
			iCreditSection	= strtol(tpLineOffset, &tpCheck, 10);
			//	Check validity
			if (tpCheck == tpLineOffset || (tpCheck[0] != _TEXT(' ') && tpCheck[0] != _TEXT('\t'))) continue;
			//	Get stats section
			tpLineOffset	= &tpCheck[1];
			iStatsSection	= _tcstol(tpLineOffset, &tpCheck, 10);
			//	Check validity
			if (tpCheck == tpLineOffset || (tpCheck[0] != _TEXT(' ') && tpCheck[0] != _TEXT('\t')))
			{
				//	Invalid
				iStatsSection	= iCreditSection;
			}
			else
			{
				//	Valid
				tpLineOffset	= &tpCheck[1];
			}
			//	Get share section
			tpLineOffset	= &tpCheck[1];
			iShareSection	= _tcstol(tpLineOffset, &tpCheck, 10);
			//	Check validity
			if (tpCheck == tpLineOffset ||
				(tpCheck[0] != _TEXT(' ') && tpCheck[0] != _TEXT('\t')))
			{
				//	Invalid
				iShareSection	= iCreditSection;
			}
			else
			{
				//	Valid
				tpLineOffset	= &tpCheck[1];
			}
			//	Make sure section numbers are within specified bounds
			if (iCreditSection < 0 || iCreditSection >= MAX_SECTIONS) iCreditSection = 0;
			if (iStatsSection < 0 || iStatsSection >= MAX_SECTIONS) iStatsSection = iCreditSection;
			if (iShareSection < 0 || iShareSection >= MAX_SECTIONS) iShareSection = iCreditSection;
			//	Skip spaces
			for (;tpLineOffset[0] == _TEXT(' ') || tpLineOffset[0] == _TEXT('\t');tpLineOffset++);

			// no path found...
			if (!*tpLineOffset) continue;

			if (dwSectionInfoArray >= dwMaxSectionInfoArray)
			{
				dwMaxSectionInfoArray += 10;
				lpTempSectionArray = ReAllocate(lpSectionInfoArray, _T("SectionInfoArray"), dwMaxSectionInfoArray*sizeof(*lpSectionInfoArray));
				if (!lpTempSectionArray)
				{
					dwMaxSectionInfoArray -= 10;
					bOK = FALSE;
					break;
				}
				lpSectionInfoArray = lpTempSectionArray;
			}

			dwPath    = _tcslen(tpLineOffset);
			lpSection = lpSectionInfoArray[dwSectionInfoArray++] = Allocate("SectionName", sizeof(*lpSection) + dwPath * sizeof(TCHAR));
			if (!lpSection)
			{
				dwSectionInfoArray--;
				bOK = FALSE;
				break;
			}

			lpSection->dwSectionName = min(lpLine->Variable_l, _MAX_NAME);
			lpSection->iCredit = iCreditSection;
			lpSection->iStat   = iStatsSection;
			lpSection->iShare  = iShareSection;

			CopyMemory(lpSection->tszSectionName, lpLine->Variable, lpSection->dwSectionName);
			lpSection->tszSectionName[lpSection->dwSectionName] = 0;
			CopyMemory(lpSection->tszPath, tpLineOffset, (dwPath+1)*sizeof(TCHAR));
		}
		break;
	}
}


BOOL Config_Read(LPCONFIG_FILE lpConfigFile)
{
	LPCONFIG_LINE_ARRAY	Line_Array_Start, Line_Array;
	LPCONFIG_LINE		CurrentLine;
	LPSTR				Buffer, Line, NewLine, Seek, End;
	HANDLE				File;
	ULONG				Read;
	INT					Size, Row, l_Line, l_Name;
	BOOL				Active, Return;
	DWORD               dwError;

	//	Open config file
	if ((File = CreateFile(lpConfigFile->tszConfigFile, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
	{
		//	Could not open config file
		dwError = GetLastError();
		Putlog(LOG_ERROR, "Unable to open config file '%s': error=%d\r\n", lpConfigFile->tszConfigFile, dwError);
		return TRUE;
	}

	//	Get file size
	Size	= GetFileSize(File, NULL);
	//	Check file size and allocate memory
	if (Size == INVALID_FILE_SIZE ||
		! (Buffer = (LPSTR)Allocate("Config:ReadBuffer", Size + 1)))
	{
		//	Could not allocate buffer
		CloseHandle(File);
		return TRUE;
	}

	Return	= TRUE;
	//	Read file to buffer
	if (ReadFile(File, Buffer, Size, &Read, 0) && (Read > 0))
	{
		//	Reset local pointers
		CurrentLine			= NULL;
		Line_Array_Start	= NULL;
		Line_Array			= NULL;

		Row		= 0;
		Line	= Buffer;
		Size	= Read;
		Return	= FALSE;
		//	Increase size of buffer by one, if last character is not null
		if (Line[Size - 1] != '\0') Size++;
		//	Insert newline character
		Line[Size - 1]	= '\n';

		//	Find all lines from config
		for (End = &Buffer[Size];(NewLine = (LPSTR)memchr(Line, '\n', End - Line));Line = &NewLine[1])
		{
			//	Default status of line is active
			Active	= TRUE;
			//	Calculate line length
			l_Line	= NewLine - Line;
			//	Replace newline character with zero
			NewLine[0]	= '\0';

			switch (Line[0])
			{
			case '[':
				//
				if ((Seek = (LPSTR)memchr(&Line[1], ']', l_Line - 1)))
				{
					//	Calculate length of array name
					l_Name	= Seek - &Line[1];

					if (! Line_Array_Start)
					{
						//	First item of list
						Line_Array			=
							Line_Array_Start	= (CONFIG_LINE_ARRAY *)Allocate("Config:LineArray", sizeof(CONFIG_LINE_ARRAY) + l_Name + 1);
					}
					else
					{
						//	Append to list
						Line_Array->Next	= (CONFIG_LINE_ARRAY *)Allocate("Config:LineArray", sizeof(CONFIG_LINE_ARRAY) + l_Name + 1);
						Line_Array			= Line_Array->Next;
					}

					//	Check memory allocation
					if (! Line_Array) break;
					//	Reset memory
					ZeroMemory(Line_Array, sizeof(CONFIG_LINE_ARRAY));
					Line_Array->Name	= (LPSTR)&Line_Array[1];
					//	Copy name
					CopyMemory(Line_Array->Name, &Line[1], l_Name);
					Line_Array->Name[l_Name]	= '\0';
					Line_Array->Name_Len		= l_Name;

					CurrentLine	= NULL;
					Row			= 0;
					break;
				}

			case '#':
			case ';':
			case '=':
				Active	= FALSE;

			default:
				//	Make sure that there is linearray
				if (! Line_Array_Start) break;
				//	Remove carriage feed
				if (NewLine > Line && NewLine[-1] == '\r') l_Line--;

				if (! CurrentLine)
				{
					//	First line of array
					CurrentLine	=
						Line_Array->First_Line	= (CONFIG_LINE *)Allocate("Config:Line", sizeof(CONFIG_LINE));
				}
				else
				{
					//	Append line to array
					CurrentLine->Next	= (CONFIG_LINE *)Allocate("Config:Line", sizeof(CONFIG_LINE));
					CurrentLine			= CurrentLine->Next;
				}
				//	Check allocation
				if (! CurrentLine) break;
				//	Reset memory
				ZeroMemory(CurrentLine, sizeof(CONFIG_LINE));
				//	Allocate memory for line contents
				CurrentLine->Text	= (LPSTR)Allocate("Config:Line:Data", l_Line + 1);
				//	Copy Line data
				CopyMemory(CurrentLine->Text, Line, l_Line);
				CurrentLine->Text[l_Line]	= '\0';
				CurrentLine->Text_l = l_Line;

				if (Active &&
					l_Line > 0 &&
					(Seek = (LPSTR)memchr(&CurrentLine->Text[1], '=', l_Line - 1)) &&
					Seek > CurrentLine->Text &&
					Seek < &CurrentLine->Text[l_Line - 1] &&
					FindString(CurrentLine->Text, Seek - CurrentLine->Text, &CurrentLine->Variable, &CurrentLine->Variable_l) &&
					FindString(&Seek[1], CurrentLine->Text + l_Line - &Seek[1], &CurrentLine->Value, &CurrentLine->Value_l))
				{
					//	Line is active
					CurrentLine->Active	= TRUE;
					Line_Array->SSize++;
				}

				CurrentLine->Row	= Row++;
				Line_Array->Lines	= Row;
				break;
			}
		}

		if (Line_Array_Start)
		{
			//	Sort config
			for (Line_Array = Line_Array_Start; Line_Array ;Line_Array = Line_Array->Next) Config_Sort_Array(Line_Array);
			//	Lock config
			Config_Lock(lpConfigFile, TRUE);
			//	Check if config exists in memory
			if (lpConfigFile->lpLineArray)
			{
				//	Swap pointers
				Line_Array	= lpConfigFile->lpLineArray;
				lpConfigFile->lpLineArray = Line_Array_Start;
				//	Unlock config
				//	Free old config
				Config_Free_Line_Array(Line_Array);
			}
			else
			{
				lpConfigFile->lpLineArray = Line_Array_Start;
			}

			// perform some extra processing while we own the lock if ioFTPD.ini
			if (lpConfigFile == &IniConfigFile)
			{
				Config_Parse_Sections();
			}
			Config_Unlock(lpConfigFile, TRUE);
		}
	}
	//	Close file
	CloseHandle(File);
	//	Free read buffer
	Free(Buffer);

	return Return;
}






BOOL Config_Write(LPCONFIG_FILE lpConfigFile)
{
	LPCONFIG_LINE_ARRAY	lpArray;
	LPCONFIG_LINE		lpLine;
	HANDLE				hConfigFile;
	DWORD				dwBytesWritten, dwBytesToWrite;

	//	Open file for writing
	hConfigFile	= CreateFile(lpConfigFile->tszConfigFile, GENERIC_WRITE, 0,
		NULL, OPEN_EXISTING, 0, NULL);
	if (hConfigFile == INVALID_HANDLE_VALUE) return TRUE;

	Config_Lock(lpConfigFile, FALSE);
	//	Loop through line list
	for (lpArray = lpConfigFile->lpLineArray ; lpArray ; lpArray = lpArray->Next)
	{
		WriteFile(hConfigFile, _TEXT("["), sizeof(TCHAR), &dwBytesWritten, NULL);
		WriteFile(hConfigFile, lpArray->Name, lpArray->Name_Len * sizeof(TCHAR), &dwBytesWritten, NULL);
		WriteFile(hConfigFile, _TEXT("]\r\n"), 3 * sizeof(TCHAR), &dwBytesWritten, NULL);
		//	Loop through line array
		for (lpLine = lpArray->First_Line;lpLine;lpLine = lpLine->Next)
		{
			//	Calculate line length
			// TODO: use Text_l... but make sure it's always correct first...
			dwBytesToWrite	= _tcslen(lpLine->Text);
			//	Write line to file
			WriteFile(hConfigFile, lpLine->Text, dwBytesToWrite * sizeof(TCHAR), &dwBytesWritten, NULL);
			WriteFile(hConfigFile, _TEXT("\r\n"), 2 * sizeof(TCHAR), &dwBytesWritten, NULL);
		}
	}
	Config_Unlock(lpConfigFile, FALSE);
	//	Set end of file
	SetEndOfFile(hConfigFile);
	//	Close file
	CloseHandle(hConfigFile);

	return FALSE;
}




BOOL Config_Init(BOOL bFirstInitialization, LPTSTR tszFileName)
{
	if (! bFirstInitialization) return TRUE;

	IniConfigFile.lpLineArray      = NULL;
	IniConfigFile.tszConfigFile = tszFileName;
	if (! InitializeLockObject(&IniConfigFile.loConfig)) return FALSE;

	//	Read config
	if (Config_Read(&IniConfigFile)) return FALSE;
	return TRUE;
}



VOID Config_DeInit(VOID)
{
	//	Free config
	Config_Free_Line_Array(IniConfigFile.lpLineArray);

	//	Close Sempahore
	DeleteLockObject(&IniConfigFile.loConfig);
}



LPCONFIG_LINE Config_Get_Primitive(LPCONFIG_FILE lpConfigFile, LPTSTR tszArray, LPTSTR tszVariable, LPINT lpOffset)
{
	LPCONFIG_LINE_ARRAY	lpArray;
	LPCONFIG_LINE		lpSearchLine, *lpResultLine;
	CONFIG_LINE			SearchLine;
	DWORD				dwVariable, dwArray;
	INT					iOffset;

	dwArray	= _tcslen(tszArray);
	//	Find the named array
	for (lpArray = lpConfigFile->lpLineArray;lpArray;lpArray = lpArray->Next)
	{
		if (lpArray->Name_Len == dwArray &&
			lpArray->SSize > 0 &&
			! _tcsnicmp(lpArray->Name, tszArray, dwArray))
		{
			//	Get variable length
			dwVariable	= _tcslen(tszVariable);
			//	Get offset
			iOffset	= (lpOffset ? lpOffset[0] : 0);

			if (iOffset)
			{
				//	Make sure that there is next item
				if (iOffset >= lpArray->SSize) break;
				//	Store pointer to item
				lpResultLine	= &lpArray->Sorted[iOffset];
				//	Make sure that item is valid
				if (lpResultLine[0]->Variable_l != dwVariable ||
					_tcsnicmp(tszVariable, lpResultLine[0]->Variable, dwVariable)) break;
				//	Update our offset
				lpOffset[0]	= iOffset + 1;
			}
			else
			{
				//	Generate comparison item for binary search
				SearchLine.Variable		= tszVariable;
				SearchLine.Variable_l	= dwVariable;
				//	Get pointer to searchline item
				lpSearchLine	= &SearchLine;
				//	Binary search
				lpResultLine	= (LPCONFIG_LINE *)bsearch(&lpSearchLine, lpArray->Sorted,
					                                       lpArray->SSize, sizeof(LPCONFIG_LINE),
														   (QUICKCOMPAREPROC) Config_Search);
				//	Check search result
				if (! lpResultLine) break;
				//	Find first matching line using lame linear method
				while (lpResultLine-- > lpArray->Sorted)
				{
					if (lpResultLine[0]->Variable_l != dwVariable ||
						_tcsnicmp(lpResultLine[0]->Variable, tszVariable, dwVariable)) break;
				}
				//	Shift result by one
				lpResultLine++;
				//	Update our offset
				if (lpOffset) lpOffset[0]	= (lpResultLine - lpArray->Sorted) + 1;
			}
			return lpResultLine[0];
		}
	}
	return NULL;
}



LPSTR Config_Get_Linear(LPCONFIG_FILE lpConfigFile, LPTSTR tszArray, LPTSTR tszVariable, LPTSTR tszBuffer, LPVOID *lpOffset)
{
	LPCONFIG_LINE_ARRAY	lpArray;
	LPCONFIG_LINE		lpLine;
	LPTSTR				tszReturn;
	DWORD				dwArray, dwVariable, dwBuffer;

	tszReturn	= NULL;
	if (! tszArray ||
		! tszVariable) return NULL;
	//	Get current offset
	lpLine	= ((LPCONFIG_LINE *)lpOffset)[0];

	//	Find beginning of array
	if (! lpLine)
	{
		dwArray	= _tcslen(tszArray);

		Config_Lock(lpConfigFile, FALSE);
		//	Find array
		for (lpArray = lpConfigFile->lpLineArray;lpArray;lpArray = lpArray->Next)
		{
			//	Compare parameters
			if (lpArray->Name_Len == dwArray &&
				lpArray->SSize > 0 &&
				! _tcsnicmp(lpArray->Name, tszArray, dwArray))
			{
				//	Store first line
				lpLine	= lpArray->First_Line;
				break;
			}
		}
	}
	else lpLine	= lpLine->Next;

	//	Cycle until active line is found
	for (;lpLine && ! lpLine->Active;lpLine = lpLine->Next);
	//	Store current offset
	lpOffset[0]	= (LPVOID)lpLine;

	if (lpLine)
	{
		//	Get value length as string
		dwBuffer	= min(lpLine->Value_l, _INI_LINE_LENGTH);
		//	Get variable name length
		dwVariable	= min(lpLine->Variable_l, _MAX_NAME);
		//	Allocate memory for buffer
		if (! tszBuffer) tszBuffer	= (LPSTR)Allocate("Config:Get:Linear:Result", (dwBuffer + 1) * sizeof(TCHAR));
		if (tszBuffer)
		{
			//	Copy result
			CopyMemory(tszVariable, lpLine->Variable, dwVariable * sizeof(TCHAR));
			CopyMemory(tszBuffer, lpLine->Value, dwBuffer * sizeof(TCHAR));
			tszBuffer[dwBuffer]	= _TEXT('\0');
			tszVariable[dwVariable]	= _TEXT('\0');
			//	Set return value
			tszReturn	= tszBuffer;
		}
	}
	//	If no return value, unlock config
	if (! tszReturn) Config_Unlock(lpConfigFile, FALSE);

	return tszReturn;
}


VOID Config_Get_Linear_End(LPCONFIG_FILE lpConfigFile)
{
	Config_Unlock(lpConfigFile, FALSE);
}


BOOL Config_Get_Bool(LPCONFIG_FILE lpConfigFile, LPTSTR tszArray, LPTSTR tszVariable, LPBOOL lpValue)
{
	LPCONFIG_LINE	lpLine;
	BOOL			bReturn;

	bReturn	= TRUE;

	Config_Lock(lpConfigFile, FALSE);
	//	Search for line
	lpLine = Config_Get_Primitive(lpConfigFile, tszArray, tszVariable, NULL);
	//	Check search result
	if (lpLine)
	{
		if (! _tcsnicmp(lpLine->Value, _TEXT("True"), 4) &&
			(lpLine->Value[4] == _TEXT('\0') || isspace(lpLine->Value[4])))
		{
			//	True
			bReturn		= FALSE;
			lpValue[0]	= TRUE;
		}
		else if (! _tcsnicmp(lpLine->Value, _TEXT("False"), 5) &&
				(lpLine->Value[5] == _TEXT('\0') || isspace(lpLine->Value[5])))
		{
			//	False
			bReturn		= FALSE;
			lpValue[0]	= FALSE;
		}
	}
	Config_Unlock(lpConfigFile, FALSE);

	return bReturn;
}








BOOL Config_Get_Permission(LPCONFIG_FILE lpConfigFile, LPTSTR tszArray, LPTSTR tszVariable, LPUSERFILE lpUserFile)
{
	LPCONFIG_LINE	lpLine;
	BOOL			bReturn;

	if (! lpUserFile) return FALSE;
	Config_Lock(lpConfigFile, FALSE);
	//	Get return value
	bReturn	= ((lpLine = Config_Get_Primitive(lpConfigFile, tszArray, tszVariable, NULL)) ?
		HavePermission(lpUserFile, lpLine->Value) : FALSE);
	Config_Unlock(lpConfigFile, FALSE);

	return bReturn;
}



BOOL Config_Get_Permission2(LPCONFIG_FILE lpConfigFile, LPTSTR tszArray, LPTSTR tszVariable, LPUSERFILE lpUserFile)
{
	LPCONFIG_LINE	lpLine;
	BOOL			bReturn;

	if (! lpUserFile) return FALSE;
	if (! HasFlag(lpUserFile, "M")) return FALSE;
	Config_Lock(lpConfigFile, FALSE);
	//	Get return value
	bReturn	= ((lpLine = Config_Get_Primitive(lpConfigFile, tszArray, tszVariable, NULL)) ?
		HavePermission(lpUserFile, lpLine->Value) : TRUE);
	Config_Unlock(lpConfigFile, FALSE);

	if (bReturn)
	{
		SetLastError(IO_NO_ACCESS);
		return TRUE;
	}
	return FALSE;
}






BOOL Config_Get_Int(LPCONFIG_FILE lpConfigFile, LPTSTR tszArray, LPTSTR tszVariable, LPINT lpValue)
{
	LPCONFIG_LINE	lpLine;
	TCHAR			*tpCheck;
	BOOL			bReturn;
	INT				iValue;

	bReturn	= TRUE;

	Config_Lock(lpConfigFile, FALSE);
	//	Search variable from config
	lpLine = Config_Get_Primitive(lpConfigFile, tszArray, tszVariable, NULL);
	//	Check search result
	if (lpLine)
	{
		//	Convert string to integer
		iValue	= _tcstol(lpLine->Value, &tpCheck, 10);
		//	Check validity of integer		
		if (tpCheck != lpLine->Value &&
			(tpCheck[0] == _TEXT('\0') || _istspace(tpCheck[0])))
		{
			//	Conversion ok
			lpValue[0]	= iValue;
			bReturn		= FALSE;
		}
	}
	Config_Unlock(lpConfigFile, FALSE);

	return bReturn;
}






LPTSTR Config_Get(LPCONFIG_FILE lpConfigFile, LPTSTR tszArray, LPTSTR tszVariable, LPTSTR tszBuffer, LPINT iOffset)
{
	LPCONFIG_LINE	lpLine;
	DWORD			dwBuffer;

	Config_Lock(lpConfigFile, FALSE);
	//	Search variable from config
	lpLine = Config_Get_Primitive(lpConfigFile, tszArray, tszVariable, iOffset);

	if (lpLine)
	{
		if (tszBuffer)
		{
			//	Predefined buffer
			dwBuffer	= min(lpLine->Value_l, _INI_LINE_LENGTH);
		}
		else
		{
			//	Allocate memory
			dwBuffer	= lpLine->Value_l;
			tszBuffer	= (LPTSTR)Allocate("Config:Get:Result", (dwBuffer + 1) * sizeof(TCHAR));
		}
		if (tszBuffer)
		{
			//	Copy result to buffer
			CopyMemory(tszBuffer, lpLine->Value, dwBuffer * sizeof(TCHAR));
			tszBuffer[dwBuffer]	= '\0';
		}
	}
	else tszBuffer	= NULL;
	Config_Unlock(lpConfigFile, FALSE);

	return tszBuffer;
}






LPTSTR Config_Get_Path(LPCONFIG_FILE lpConfigFile, LPTSTR tszArray, LPTSTR tszVariable, LPTSTR tszSuffix, LPTSTR tszBuffer)
{
	LPCONFIG_LINE	lpLine;
	DWORD			dwSuffix, dwPath;

	Config_Lock(lpConfigFile, FALSE);
	//	Search variable from config
	lpLine	= Config_Get_Primitive(lpConfigFile, tszArray, tszVariable, NULL);

	if (lpLine)
	{
		dwSuffix	= _tcslen(tszSuffix) + 1;
		dwPath		= lpLine->Value_l;

		if (tszBuffer)
		{
			//	Predefined buffer (assume it's at least _INI_LINE_LENGTH + 1 long)
			if (dwPath + dwSuffix > _INI_LINE_LENGTH) tszBuffer	= NULL;
		}
		else
		{
			//	Allocate buffer
			tszBuffer	= (LPSTR)Allocate("Config:Get:Path", dwPath + dwSuffix + sizeof(TCHAR));
		}

		if (tszBuffer)
		{
			//	Copy value to buffer
			CopyMemory(tszBuffer, lpLine->Value, dwPath * sizeof(TCHAR));
			tszBuffer[dwPath]	= '\\';
			CopyMemory(&tszBuffer[dwPath + 1], tszSuffix, dwSuffix * sizeof(TCHAR));
		}
	}
	else tszBuffer	= NULL;
	//	Unlock config
	Config_Unlock(lpConfigFile, FALSE);

	return tszBuffer;
}


LPTSTR Config_Get_Path_Shared(LPCONFIG_FILE lpConfigFile, LPTSTR tszArray, LPTSTR tszVariable, LPTSTR tszSuffix)
{
	LPCONFIG_LINE	lpLine;
	DWORD			dwSuffix, dwPath;
	LPSTR           tszBuffer;

	Config_Lock(lpConfigFile, FALSE);
	//	Search variable from config
	lpLine	= Config_Get_Primitive(lpConfigFile, tszArray, tszVariable, NULL);

	if (lpLine)
	{
		dwSuffix	= _tcslen(tszSuffix) + 1;
		dwPath		= lpLine->Value_l;

		tszBuffer	= (LPSTR)AllocateShared(NULL, "Config:Get:Path", dwPath + dwSuffix + sizeof(TCHAR));

		if (tszBuffer)
		{
			//	Copy value to buffer
			CopyMemory(tszBuffer, lpLine->Value, dwPath * sizeof(TCHAR));
			tszBuffer[dwPath]	= '\\';
			CopyMemory(&tszBuffer[dwPath + 1], tszSuffix, dwSuffix * sizeof(TCHAR));
		}
	}
	else tszBuffer	= NULL;
	//	Unlock config
	Config_Unlock(lpConfigFile, FALSE);

	return tszBuffer;
}







BOOL Config_Set(LPCONFIG_FILE lpConfigFile, char *array, int line, char *value, int mode)
{
	CONFIG_LINE_ARRAY	*Array;
	CONFIG_LINE			*Last, *Line, *New;
	CHAR				*Seek;
	INT					Line_Len, i;

	i	= strlen(array);

	Config_Lock(lpConfigFile, TRUE);

	for ( Array = lpConfigFile->lpLineArray ; Array ; Array = Array->Next )
	{
		if ( ! strnicmp(Array->Name, array, i)) break;
	}

	if ( Array == NULL )
	{
		Config_Unlock(lpConfigFile, TRUE);
		SetLastError(IO_INVALID_ARRAY);
		return TRUE;
	}

	if ( line >= Array->Lines )
	{
		Config_Unlock(lpConfigFile, TRUE);
		SetLastError(IO_INVALID_LINE);
		return TRUE;
	}

	if ( mode == CONFIG_INSERT &&
		line == -1 ) line = Array->Lines + 1;

	if ( line < 0 )
	{
		Config_Unlock(lpConfigFile, TRUE);
		SetLastError(IO_INVALID_LINE);
		return TRUE;
	}

	if ( mode == CONFIG_DEL )
	{
		Last	= NULL;
		Line	= Array->First_Line;

		for ( i = 0 ; i < line ; i++ )
		{
			Last	= Line;
			Line	= Line->Next;
		}

		if ( Last == NULL )
		{
			Array->First_Line	= Line->Next;
		}
		else
		{
			Last->Next	= Line->Next;
		}

		Last	= Line;

		if ( Last->Active ) Array->SSize--;
		Array->Lines--;

		for ( ; i < Array->Lines ; i++ )
		{
			Line	= Line->Next;
			Line->Row--;
		}

		Free(Last->Text);
		Free(Last);

		Config_Sort_Array(Array);
		Config_Unlock(lpConfigFile, TRUE);
		
		return FALSE;
	}

	Line_Len	= strlen(value);

	New			= (CONFIG_LINE *)Allocate("Config:Line", sizeof(CONFIG_LINE));
	New->Text	= (CHAR *)Allocate("Config:Line:Data", Line_Len + 1);
	New->Text_l = Line_Len;

	memcpy(New->Text, value, Line_Len + 1);

	if ( Line_Len > 0 &&
		(Seek = (CHAR *)memchr(New->Text + 1, '=', Line_Len - 1)) != NULL &&
		Seek > New->Text &&
		Seek < New->Text + Line_Len - 1 &&
		FindString(New->Text, Seek - New->Text, &New->Variable, &New->Variable_l) &&
		FindString(Seek + 1, New->Text + Line_Len - 1 - Seek, &New->Value, &New->Value_l))
	{
		New->Active	= TRUE;
		Array->SSize++;
	}
	else
	{
		New->Active	= FALSE;
	}

	Last	= NULL;
	Line	= Array->First_Line;

	switch ( mode )
	{
	case CONFIG_INSERT:
		Array->Lines++;

		for ( i = 0 ; i < line ; i++ )
		{
			Last	= Line;
			Line	= Line->Next;

			if ( Line == NULL ) break;
		}

		if ( Last == NULL )
		{
			Array->First_Line	= New;
			New->Next			= Line;
		}
		else
		{
			New->Next	= Line;
			New->Row	= Last->Row + 1;
			Last->Next	= New;
		}

		for ( i = line + 1 ; i < Array->Lines ; i++ )
		{
			Line->Row++;
			Line		= Line->Next;
		}

		break;

	case CONFIG_REPLACE:

		for ( i = 0 ; i < line ; i++ )
		{
			Last	= Line;
			Line	= Line->Next;
		}

		if ( Line->Active ) Array->SSize--;

		New->Row	= Line->Row;
		New->Next	= Line->Next;

		if ( Last == NULL )
		{
			Array->First_Line	= New;
		}
		else
		{
			Last->Next	= New;
		}

		Free(Line->Text);
		Free(Line);

		break;
	}

	Config_Sort_Array(Array);
	Config_Unlock(lpConfigFile, TRUE);

	return FALSE;
}









VOID Config_Print(LPCONFIG_FILE lpConfigFile, LPBUFFER lpBuffer, LPTSTR tszArray, LPSTR tszLinePrefix)
{
	LPCONFIG_LINE_ARRAY	lpArray;
	LPCONFIG_LINE		lpLine;
	DWORD				dwArray;

	dwArray	= (tszArray ? _tcslen(tszArray) + 1 : 0);

	Config_Lock(lpConfigFile, FALSE);
	//	Loop through config
	for (lpArray = lpConfigFile->lpLineArray ; lpArray ; lpArray = lpArray->Next)
	{
		//	Select print method
		if (! tszArray)
		{
			//	Show array
			FormatString(lpBuffer, _TEXT("%s[%s]\r\n"), tszLinePrefix, lpArray->Name);
		}
		else if (! _tcsncmp(lpArray->Name, tszArray, dwArray))
		{
			//	Print to buffer
			FormatString(lpBuffer, _TEXT("%s### [%s]\r\n"), tszLinePrefix, lpArray->Name);
			//	Loop through array
			for (lpLine = lpArray->First_Line;lpLine;lpLine = lpLine->Next)
			{
				//	Show line
				FormatString(lpBuffer, _TEXT("%s%03d: %s\r\n"), tszLinePrefix, lpLine->Row, lpLine->Text);
			}
			break;
		}
	}
	Config_Unlock(lpConfigFile, FALSE);
}


VOID Config_Get_Section(LPTSTR tszVirtualPath, LPTSTR tszSection,
						LPINT lpCreditSection, LPINT lpStatsSection, LPINT lpShareSection)
{
	LPSECTION_INFO      lpSection;
	DWORD				n;

	Config_Lock(&IniConfigFile, FALSE);
	for (n=0 ; n < dwSectionInfoArray ; n++)
	{
		lpSection = lpSectionInfoArray[n];
		if (! PathCompare(lpSection->tszPath, tszVirtualPath))
		{
			//	Copy section name
			CopyMemory(tszSection, lpSection->tszSectionName, lpSection->dwSectionName * sizeof(TCHAR));
			tszSection[lpSection->dwSectionName]	= 0;

			*lpCreditSection = lpSection->iCredit;
			*lpStatsSection	 = lpSection->iStat;
			*lpShareSection	 = lpSection->iShare;
			Config_Unlock(&IniConfigFile, FALSE);
			return;
		}
	}

	Config_Unlock(&IniConfigFile, FALSE);
	*lpCreditSection = 0;
	*lpStatsSection	 = 0;
	*lpShareSection	 = 0;
	_tcscpy(tszSection, _TEXT("default"));
}



BOOL Config_Get_SectionNum(INT iSectionNum, LPTSTR tszSectionName,
						   LPINT lpCreditSection, LPINT lpStatsSection, LPINT lpShareSection)
{
	LPSECTION_INFO      lpSection;
	DWORD				n;

	Config_Lock(&IniConfigFile, FALSE);
	for (n=0 ; n < dwSectionInfoArray ; n++)
	{
		lpSection = lpSectionInfoArray[n];
		if (lpSection->iStat != iSectionNum)
		{
			// no match
			continue;
		}
		//	Copy section name
		CopyMemory(tszSectionName, lpSection->tszSectionName, lpSection->dwSectionName * sizeof(TCHAR));
		tszSectionName[lpSection->dwSectionName] = 0;

		*lpCreditSection = lpSection->iCredit;
		*lpStatsSection	 = lpSection->iStat;
		*lpShareSection	 = lpSection->iShare;
		Config_Unlock(&IniConfigFile, FALSE);
		return TRUE;
	}

	Config_Unlock(&IniConfigFile, FALSE);
	*lpCreditSection = 0;
	*lpStatsSection	 = 0;
	*lpShareSection	 = 0;
	_tcscpy(tszSectionName, _TEXT("default"));
	return FALSE;
}


// automatically uses IniConfigFile
BOOL PathCheck(LPUSERFILE lpUserFile, LPTSTR tszVirtualPath, LPTSTR tszAccessType)
{
	LPCONFIG_LINE	lpLine;
	LPTSTR			tszPath, tszAccessList;
	TCHAR			PathBuffer[_MAX_PWD + 1];
	DWORD			dwPath;
	BOOL			bReturn;
	INT				iOffset;

	//	Init local variables
	iOffset	= 0;
	bReturn	= TRUE;

	Config_Lock(&IniConfigFile, FALSE);
	while (lpLine = Config_Get_Primitive(&IniConfigFile, "VFS", tszAccessType, &iOffset))
	{
		tszPath	= lpLine->Value;
		if (tszPath[0] == _TEXT('"'))
		{
			//	Find second quote
			tszAccessList	= (LPTSTR)_tmemchr(++tszPath, _TEXT('"'), lpLine->Value_l - 1);
			dwPath	= &tszAccessList[-1] - tszPath;
		}
		else
		{
			//	Find first space from string
			if (! (tszAccessList = (LPSTR)_tmemchr(&tszPath[1], _TEXT(' '), lpLine->Value_l - 1)))
			{
				//	Find first '\t'
				tszAccessList	= (LPSTR)_tmemchr(&tszPath[1], _TEXT('\t'), lpLine->Value_l - 1);
			}
			dwPath	= tszAccessList - tszPath;
		}

		if (tszAccessList++)
		{
			//	Limit path length
			if (dwPath > _MAX_PWD) dwPath	= _MAX_PWD;
			//	Copy path to local variable
			CopyMemory(PathBuffer, tszPath, dwPath * sizeof(TCHAR));
			PathBuffer[dwPath]	= '\0';
			//	Compare current path with config path
			if (! PathCompare(PathBuffer, tszVirtualPath))
			{
				//	Get permission
				bReturn	= HavePermission(lpUserFile, tszAccessList);
				break;
			}
		}
	}
	Config_Unlock(&IniConfigFile, FALSE);
	//	Set error
	if (bReturn) SetLastError(IO_NO_ACCESS_INI);

	return bReturn;
}




// Shim functions to keep the old syntax because nxMyDB uses these and isn't updated yet...

LPSTR Config_Get_Old(LPSTR szArray, LPSTR szVariable, LPSTR szBuffer, LPINT lpOffset)
{
	return Config_Get(&IniConfigFile, szArray, szVariable, szBuffer, lpOffset);
}

BOOL Config_Get_Int_Old(LPSTR szArray, LPSTR szVariable, LPINT lpValue)
{
	return Config_Get_Int(&IniConfigFile, szArray, szVariable, lpValue);
}

BOOL Config_Get_Bool_Old(LPSTR szArray, LPSTR szVariable, LPBOOL lpValue)
{
	return Config_Get_Bool(&IniConfigFile, szArray, szVariable, lpValue);
}

LPSTR Config_Get_Path_Old(LPSTR szArray, LPSTR szVariable, LPSTR szSuffix, LPSTR szBuffer)
{
	return Config_Get_Path(&IniConfigFile, szArray, szVariable, szSuffix, szBuffer);
}

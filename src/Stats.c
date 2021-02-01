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

static DWORD dwStatsOffset[]	= {
	offsetof(USERFILE, DayUp), offsetof(USERFILE, DayDn),
	offsetof(USERFILE, WkUp), offsetof(USERFILE, WkDn),
	offsetof(USERFILE, MonthUp), offsetof(USERFILE, MonthDn),
	offsetof(USERFILE, AllUp), offsetof(USERFILE, AllDn) };


static LPTSTR tszStatTypes[]	= {
	_TEXT("DayUp"), _TEXT("DayDn"), _TEXT("WkUp"), _TEXT("WkDn"),
	_TEXT("MonthUp"), _TEXT("MonthDn"), _TEXT("AllUp"), _TEXT("AllDn") };


BOOL CompileStats(LPSTATS lpStats, IO_STATS *pIoStatsTotals, BOOL bSkipFirstPrefix)
{
	LPUSERFILE	 lpUserFile;
	LPCSTAT      lpCStats, lpOffset;
	CSTAT        CStat;
	INT64		 Compare, Compare2;
	PINT64		 pTotal, pInt64;
	IO_STATS	 IoStats;
	DWORD		 dwGathered, dwMatched, n, dwUserMax, dwOffset, dwOffset2, dwType;
	INT			 Left, Shift, iSection, iSection2;

	dwGathered	= dwMatched = 0;
	dwUserMax	= min(10000, lpStats->dwUserMax);
	if (! dwUserMax) dwUserMax	= 10;
	iSection = lpStats->iSection;
	if (iSection > MAX_SECTIONS || iSection < -1) iSection = -1;
	iSection2 = iSection;
	if (iSection == -1) iSection = 0;
	if (! lpStats->tszSearchParameters) lpStats->tszSearchParameters	= _TEXT("*");
	//	Calculate comparison offset
	n	= (lpStats->dwType & 0xFF);
	if (n >= sizeof(dwStatsOffset) / sizeof(DWORD)) n	= 0;
	dwType = (lpStats->dwType >> 8) & 0xFF;
	dwOffset	= dwStatsOffset[n] + (dwType + iSection * 3) * sizeof(INT64);
	dwOffset2 = dwStatsOffset[n] + (iSection * 3 * sizeof(INT64));
	if (pIoStatsTotals)
	{
		pTotal = (PINT64)((ULONG)pIoStatsTotals->lpUserFile + dwOffset2);
	}
	else
	{
		pTotal = 0;
	}

	//	Allocate memory
	lpCStats = (LPCSTAT) Allocate("Stats:Array", sizeof(CSTAT) * (dwUserMax + 1));
	if (! lpCStats) return TRUE;

	//	Begin search
	while (! FindNextUser(lpStats->lpSearch, &lpUserFile))
	{
		if (iSection2 != -1)
		{
			pInt64 = (PINT64)((ULONG)lpUserFile + dwOffset2);

			CStat.Stat[0] = pInt64[0];
			CStat.Stat[1] = pInt64[1];
			CStat.Stat[2] = pInt64[2];
		}
		else
		{
			CStat.Stat[0] = CStat.Stat[1] = CStat.Stat[2] = 0;
			// NOTE: iSection 2 == -1 implies section=0 so it's the start of the correct array
			for (n=0 ; n<MAX_SECTIONS ; n++)
			{
				pInt64 = (PINT64)((ULONG)lpUserFile + dwOffset2 + n*3*sizeof(INT64));

				CStat.Stat[0] += pInt64[0];
				CStat.Stat[1] += pInt64[1];
				CStat.Stat[2] += pInt64[2];
			}
		}

		Compare	= CStat.Stat[dwType];
		lpOffset	= lpCStats;

		if (Compare != 0 || !(lpStats->dwType & STATS_NO_ZEROS_FLAG))
		{
			dwMatched++;

			if (pTotal)
			{
				pTotal[0] += CStat.Stat[0];
				pTotal[1] += CStat.Stat[1];
				pTotal[2] += CStat.Stat[2];
			}

			if (!(lpStats->dwType & STATS_TOTAL_ONLY_FLAG))
			{
				for (Left = Shift = dwGathered;Shift;Left -= Shift)
				{
					Shift	= Left >> 1;
					Compare2 = lpOffset[Shift].Stat[dwType];

					if (Compare2 > Compare ||
						(Compare2 == Compare && lpOffset[Shift].lpUserFile->Uid < lpUserFile->Uid))
					{
						lpOffset	+= (Shift ? Shift : 1);
					}
				}

				if (lpOffset != &lpCStats[dwGathered])
				{
					MoveMemory(&lpOffset[1], lpOffset, (ULONG)&lpCStats[dwGathered] - (ULONG)lpOffset);
				}
				lpOffset[0].lpUserFile = lpUserFile;
				lpOffset[0].Stat[0] = CStat.Stat[0];
				lpOffset[0].Stat[1] = CStat.Stat[1];
				lpOffset[0].Stat[2] = CStat.Stat[2];

				if (dwGathered == dwUserMax)
				{
					//	Maximum amount of userfiles buffered
					UserFile_Close(&lpCStats[dwGathered].lpUserFile, 0);
				}
				else dwGathered++;
			}
			else
			{
				UserFile_Close(&lpUserFile,0);
			}

		}
		else
		{
			UserFile_Close(&lpUserFile, 0);
		}
	}
	//	Set section
	IoStats.iSection	= iSection2;

	if (pIoStatsTotals)
	{
		pIoStatsTotals->iPosition = dwMatched;
		pIoStatsTotals->iSection = iSection2;
	}

	for (n = 0;n < dwGathered;n++)
	{
		//	Prepare stats structure
		IoStats.lpUserFile	= lpCStats[n].lpUserFile;
		IoStats.iPosition	= n + 1;
		//	Compile message file
		Message_Compile(lpStats->lpMessage, lpStats->lpBuffer, bSkipFirstPrefix,
			&IoStats, DT_STATS, lpStats->tszMessagePrefix, NULL);
		bSkipFirstPrefix = FALSE;
		UserFile_Close(&lpCStats[n].lpUserFile, 0);
	}
	Free(lpCStats);

	return FALSE;
}









BOOL SetStatsType(LPSTATS lpStats, LPTSTR tszType)
{
	DWORD	n;

	for (n = sizeof(tszStatTypes) / sizeof(LPTSTR);n--;)
	{
		if (! _tcsicmp(tszStatTypes[n], tszType)) break;
	}
	if (n == (DWORD)-1) return FALSE;
	
	lpStats->dwType	= (lpStats->dwType & 0xFFFFFF00) + n;
	return TRUE;
}


LPTSTR GetStatsTypeName(LPSTATS lpStats)
{
	return tszStatTypes[lpStats->dwType & 0xFF];
}



LPTSTR Admin_UserStats(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszArgument, tszFileName, tszType, tszBasePath;
	DWORD			dwFileName;
	STATS			Stats;
	DWORD			n;
	IO_STATS        IoStatsTotals;
	USERFILE        UserFile;

	ZeroMemory(&Stats, sizeof(STATS));

	Stats.dwType	= STATS_TYPE_WEEKUP|STATS_ORDER_BYTES|STATS_NO_ZEROS_FLAG;
	Stats.lpBuffer	= &lpUser->CommandChannel.Out;
	Stats.tszMessagePrefix	= tszMultilinePrefix;
	Stats.iSection	= -1;

	//	Parse arguments
	for (n = 1;n < GetStringItems(Args);n++)
	{
		tszArgument	= GetStringIndexStatic(Args, n);

		if (! _tcsicmp(_TEXT("Section"), tszArgument))
		{
			if (++n == GetStringItems(Args)) ERROR_RETURN(ERROR_MISSING_ARGUMENT, tszArgument);
			tszArgument	= GetStringIndexStatic(Args, n);
			Stats.iSection	= _tcstol(tszArgument, NULL, 10);
		}
		else if (! _tcsicmp(_TEXT("Dir"), tszArgument))
		{
			Stats.iSection	= lpUser->CommandChannel.Path.StatsSection;
		}
		else if (! _tcsicmp(_TEXT("Count"), tszArgument))
		{
			if (++n == GetStringItems(Args)) ERROR_RETURN(ERROR_MISSING_ARGUMENT, tszArgument);
			tszArgument	= GetStringIndexStatic(Args, n);
			Stats.dwUserMax	= _tcstol(tszArgument, NULL, 10);
			if (Stats.dwUserMax == 0)
			{
				Stats.dwType |= STATS_TOTAL_ONLY_FLAG;
			}
		}
		else if (! _tcsicmp(_TEXT("NoZeros"), tszArgument))
		{
			Stats.dwType |= STATS_NO_ZEROS_FLAG;
		}
		else if (! _tcsicmp(_TEXT("Zeros"), tszArgument))
		{
			Stats.dwType &= ~STATS_NO_ZEROS_FLAG;
		}
		else if (! _tcsicmp(_TEXT("Files"), tszArgument))
		{
			Stats.dwType	= (Stats.dwType & 0xFFFF00FF)|STATS_ORDER_FILES;
		}
		else if (! _tcsicmp(_TEXT("Bytes"), tszArgument))
		{
			Stats.dwType	= (Stats.dwType & 0xFFFF00FF)|STATS_ORDER_BYTES;
		}
		else if (! _tcsicmp(_TEXT("Limit"), tszArgument))
		{
			if (++n == GetStringItems(Args)) ERROR_RETURN(ERROR_MISSING_ARGUMENT, tszArgument);
			Stats.tszSearchParameters	= GetStringIndexStatic(Args, n);
		}
		else if (! SetStatsType(&Stats, tszArgument)) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszArgument);
	}

	tszType	= tszStatTypes[Stats.dwType & 0xFF];

	if (! Stats.tszSearchParameters) Stats.tszSearchParameters = _TEXT("*");

	Stats.lpSearch = FindParse(Stats.tszSearchParameters, lpUser->UserFile, lpUser, FALSE);
	if (!Stats.lpSearch)
	{
		return Stats.tszSearchParameters;
	}

	//	Get messagefile path
	tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);

	if (!tszBasePath)
	{
		FindFinished(Stats.lpSearch);
		return GetStringIndexStatic(Args, 0);
	}

	IoStatsTotals.iPosition  = Stats.iSection;
	IoStatsTotals.iSection   = Stats.iSection;
	IoStatsTotals.lpUserFile = &UserFile;
	CopyMemory(IoStatsTotals.lpUserFile, User_GetFake(), sizeof(USERFILE));

	dwFileName	= aswprintf(&tszFileName, _TEXT("%s\\%s.Header"), tszBasePath, tszType);
	if (dwFileName)
	{
		//	Show message header
		MessageFile_Show(tszFileName, Stats.lpBuffer, &IoStatsTotals, DT_STATS, tszMultilinePrefix, NULL);

		//	Show message body
		_tcscpy(&tszFileName[dwFileName - 6], _TEXT("Body"));
		if ((Stats.lpMessage = Message_Load(tszFileName)))
		{
			CompileStats(&Stats, &IoStatsTotals, FALSE);
			Free(Stats.lpMessage);
		}

		//	Show message footer
		_tcscpy(&tszFileName[dwFileName - 6], _TEXT("Footer"));
		MessageFile_Show(tszFileName, Stats.lpBuffer, &IoStatsTotals, DT_STATS, tszMultilinePrefix, NULL);
		Free(tszFileName);
	}
	FreeShared(tszBasePath);

	return NULL;
}

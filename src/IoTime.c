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

static INT64	i64TicksPerSecond;
static TIME_ZONE_INFORMATION	TimeZoneInformation;


VOID SystemTimeToLocalTime(LPSYSTEMTIME lpUniversalTime, LPSYSTEMTIME lpLocalTime)
{
	if (! SystemTimeToTzSpecificLocalTime(&TimeZoneInformation, lpUniversalTime, lpLocalTime))
	{
		CopyMemory(lpLocalTime, lpUniversalTime, sizeof(SYSTEMTIME));
	}
}


INT Time_Compare(LPTIME_STRUCT lpStartTime, LPTIME_STRUCT lpStopTime)
{
	DWORD	dwTickCount;

	if (i64TicksPerSecond)
	{
		if (lpStartTime->i64TickCount > lpStopTime->i64TickCount) return 1;
		if (lpStartTime->i64TickCount < lpStopTime->i64TickCount) return -1;
	}
	else
	{
		dwTickCount	= GetTickCount();
		//	Check timer 1wrapping
		if (lpStartTime->i64TickCount > dwTickCount)
		{
			//	Check timer2 wrapping
			if (lpStopTime->i64TickCount < dwTickCount) return 1;
		}
		else if (lpStopTime->i64TickCount > dwTickCount) return -1;

		if (lpStartTime->i64TickCount > lpStopTime->i64TickCount) return 1;
		if (lpStartTime->i64TickCount < lpStopTime->i64TickCount) return -1;
	}
	return 0;
}



DOUBLE Time_Difference(LPTIME_STRUCT lpStartTime, LPTIME_STRUCT lpStopTime)
{
	if (i64TicksPerSecond)
	{
		return (lpStopTime->i64TickCount - lpStartTime->i64TickCount) / 1. / i64TicksPerSecond;
	}
	return (lpStartTime->i64TickCount > lpStopTime->i64TickCount ?
		(0xFFFFFFFFL - lpStartTime->i64TickCount) + 1 + lpStopTime->i64TickCount : lpStopTime->i64TickCount - lpStartTime->i64TickCount) / 1000.;
}



DWORD Time_DifferenceDW32(DWORD dwStart, DWORD dwStop)
{
	return (dwStart > dwStop ?
		(0xFFFFFFFFL - dwStart) + 1 + dwStop : dwStop - dwStart);
}


VOID Time_Read(LPTIME_STRUCT lpTime)
{
	if (i64TicksPerSecond)
	{
		QueryPerformanceCounter((PLARGE_INTEGER)&lpTime->i64TickCount);
	}
	else lpTime->i64TickCount	= GetTickCount();
}


VOID Time_Duration(LPTSTR tszBuf, DWORD dwSize, time_t tDiff, TCHAR cLast, TCHAR cFirst,
				   DWORD dwSuffixType, DWORD dwShowZeros, DWORD dwMinWidth, LPTSTR tszField)
{
	DWORD		n, tTemp, tTime;
	INT			i, iResult;
	DWORD       pdwList[]    = { 31536000, 604800,  86400,  3600,    60,     1,      0 };
	LPSTR       pszSuffix[]  = { "y",      "w",     "d",    "h",     "m",    "s",    0 };
	LPSTR       pszLongSuf[] = { " year",  " week", " day", " hour", " min", " sec", 0 };
	BOOL        bFirst;
	LPTSTR      tszSpace, *tszSuffixArray;

	tszBuf[0] = 0;
	i = 0;
	bFirst = TRUE;
	tTime = (DWORD) tDiff;
	if (!tszField) tszField = _T(" ");
	if ((dwSuffixType == 2) && dwMinWidth)
	{
		tszSpace = _T(" ");
	}
	else
	{
		tszSpace = _T("");
	}
	if (dwSuffixType)
	{
		tszSuffixArray = pszLongSuf;
	}
	else
	{
		tszSuffixArray = pszSuffix;
	}

	for ( n=0 ; pdwList[n] ; n++ )
	{
		// skip until start position
		if (cFirst)
		{
			if (cFirst != *pszSuffix[n]) continue;
			cFirst = 0;
		}

		tTemp = tTime / pdwList[n];

		if (tTemp || (dwShowZeros && !bFirst))
		{
			bFirst = FALSE;
			iResult = _snprintf_s(&tszBuf[i], dwSize - i, _TRUNCATE, _T("%s%*d%s%s"),
				(i == 0 ? _T("") : tszField), dwMinWidth, tTemp, tszSuffixArray[n],
				(dwSuffixType==2 && tTemp>1 ? _T("s") : tszSpace));
			if (iResult  < 0) break;
			i += iResult;
		}

		if (cLast == *pszSuffix[n]) break;

		tTime = tTime % pdwList[n];
	}

	if (!tszBuf[0])
	{
		if (!cLast)
		{
			n = 5;
		}
		else
		{
			for ( n=0 ; pdwList[n] ; n++ )
			{
				if (cLast == *pszSuffix[n]) break;
			}
			if (!pdwList[n]) n--;
		}
		_snprintf_s(tszBuf, dwSize, _TRUNCATE, "%*d%s", dwMinWidth, 0, tszSuffixArray[n]);
	}
}





BOOL Time_Init(BOOL bFirstInitialization)
{
	if (! bFirstInitialization) return TRUE;
	//	Get time zone
	if (GetTimeZoneInformation(&TimeZoneInformation) == TIME_ZONE_ID_INVALID)
	{
		ZeroMemory(&TimeZoneInformation, sizeof(TIME_ZONE_INFORMATION));
	}

	if (! QueryPerformanceFrequency((PLARGE_INTEGER)&i64TicksPerSecond) ||
		! i64TicksPerSecond)
	{
		//	Don't use Performance Counter
		i64TicksPerSecond	= 0;
	}
	return TRUE;
}

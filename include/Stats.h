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


typedef struct _IO_STATS
{
	LPUSERFILE		lpUserFile;
	INT				iPosition;
	INT				iSection;

} IO_STATS;


typedef struct _STATS
{
	LPBUFFER	 lpBuffer;

	LPTSTR		 tszSearchParameters;
	LPUSERSEARCH lpSearch;
	LPTSTR		 tszMessagePrefix;
	LPBYTE		 lpMessage;
	DWORD		 dwType;
	INT		     iSection;
	DWORD		 dwUserMax;

} STATS, * LPSTATS;


typedef struct _CSTAT
{
	LPUSERFILE lpUserFile;
	INT64      Stat[3];
} CSTAT, *LPCSTAT;


#define STATS_TYPE_DAYUP	0
#define STATS_TYPE_DAYDN	1
#define STATS_TYPE_WEEKUP	2
#define STATS_TYPE_WEEKDN	3
#define STATS_TYPE_MONTHUP	4
#define STATS_TYPE_MONTHDN	5
#define STATS_TYPE_ALLUP	6
#define STATS_TYPE_ALLDN	7
#define STATS_ORDER_FILES	(0 << 8)
#define STATS_ORDER_BYTES	(1 << 8)
#define STATS_ORDER_TIME	(2 << 8)
#define STATS_NO_ZEROS_FLAG		(1 << 16)
#define STATS_TOTAL_ONLY_FLAG	(1 << 24)

BOOL SetStatsType(LPSTATS lpStats, LPTSTR tszType);
LPTSTR GetStatsTypeName(LPSTATS lpStats);
BOOL CompileStats(LPSTATS lpStats, IO_STATS *pIoStatsTotals, BOOL bSkipFirstPrefix);

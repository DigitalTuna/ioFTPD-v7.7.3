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


//	Local Declarations
static BOOL	Change_User_Ratio(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_Credits(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_VfsFile(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_Logins(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_LimitPerIp(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_Flags(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_Slots(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_Password(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_Stats(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_HomeDir(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_Tagline(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_AdminGroup(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_SpeedLimit(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_MaxDownloads(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_MaxUploads(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_Opaque(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_Expiration(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_UpSpeed(CHANGE_VARIABLES *Vars);
static BOOL	Change_User_DnSpeed(CHANGE_VARIABLES *Vars);

static BOOL	Change_Group_VfsFile(CHANGE_VARIABLES *Vars);
static BOOL	Change_Group_Slots(CHANGE_VARIABLES *Vars);
static BOOL	Change_Group_Description(CHANGE_VARIABLES *Vars);


CHANGE_COMMAND ChangeCommand[] =
{
	//	User Commands
	"admingroup",	10,	TRUE, FALSE, Change_User_AdminGroup,
	"credits",		7,	TRUE, TRUE,  Change_User_Credits,
	"dnspeed",		7,	TRUE, TRUE,  Change_User_DnSpeed,
	"expires",      7,  TRUE, TRUE,  Change_User_Expiration,
	"flags",		5,	TRUE, TRUE,  Change_User_Flags,
	"homedir",		7,	TRUE, TRUE,  Change_User_HomeDir,
	"logins",		6,	TRUE, TRUE,  Change_User_Logins,
	"LimitPerIp",   10, TRUE, TRUE,  Change_User_LimitPerIp,
	"MaxDownloads", 12, TRUE, TRUE,  Change_User_MaxDownloads,
	"MaxUploads",   10, TRUE, TRUE,  Change_User_MaxUploads,
	"opaque",       6,  TRUE, TRUE,  Change_User_Opaque,
	"passwd",		6,	TRUE, FALSE, Change_User_Password,
	"ratio",		5,	TRUE, TRUE,  Change_User_Ratio,
	"speedlimit",	10, TRUE, TRUE,  Change_User_SpeedLimit,
	"stats",		5,	TRUE, FALSE, Change_User_Stats,
	"upspeed",		7,	TRUE, TRUE,  Change_User_UpSpeed,
	"tagline",		7,	TRUE, TRUE,  Change_User_Tagline,
	"vfsfile",		7,	TRUE, TRUE,  Change_User_VfsFile,
	//	Group Commands
	"groupslots",		10,	FALSE, FALSE, Change_Group_Slots,
	"groupvfsfile",		12,	FALSE, FALSE, Change_Group_VfsFile,
	"groupdescription",	16, FALSE, FALSE, Change_Group_Description,
	0, 0, 0, 0, 0
};











/*

  Change_User_Stats() - Change user's statistics

  */
static BOOL Change_User_Stats(CHANGE_VARIABLES *Vars)
{
	PINT64	lpTarget;
	INT64   i64Value, i64Prev;
	DWORD	Shift[3], n;
	LPTSTR	tszArgument;
	LPTSTR  tszAdmin, tszUser, tszField, tszType;

	if (Vars->lpAdmin)
	{
		tszAdmin = Uid2User(Vars->lpAdmin->Uid);
	}
	else
	{
		tszAdmin = NULL;
	}
	tszUser  = Uid2User(Vars->lpTarget->Uid),
	Shift[0]	= offsetof(USERFILE, Credits);
	tszField    = 0;

	Shift[1]	= 0;
	tszType     = _T("FILES");

	Shift[2]	= 0; // section #

	for (n = 3;n < GetStringItems(Vars->Args);n++)
	{
		tszArgument	= GetStringIndexStatic(Vars->Args, n);
		//	Non alphabetic characters
		if (! _istalpha(tszArgument[0]))
		{
			if (Shift[0] == offsetof(USERFILE, Credits))
			{
				//	Credits are treated in a different manner
				lpTarget = (PINT64)((DWORD)Vars->lpTarget + Shift[0] + sizeof(INT64) * Shift[2]);
			}
			else
			{
				//	Stats
				lpTarget = (PINT64)((DWORD)Vars->lpTarget + Shift[0] + sizeof(INT64) * (Shift[1] + Shift[2] * 3));
			}

			i64Prev = lpTarget[0];
			switch (tszArgument[0])
			{
			case _TEXT('+'):
				//	Add to target
				if (!tszArgument[1] || !_istdigit(tszArgument[1]))
				{
					SetLastError(ERROR_INVALID_ARGUMENTS);
					return FALSE;
				}
				i64Value = _ttoi64(&tszArgument[1]);
				lpTarget[0]	+= i64Value;
				break;
			case _TEXT('-'):
				//	Subtract from target
				if (!tszArgument[1] || !_istdigit(tszArgument[1]))
				{
					SetLastError(ERROR_INVALID_ARGUMENTS);
					return FALSE;
				}
				i64Value = _ttoi64(&tszArgument[1]);
				lpTarget[0]	-= i64Value;
				break;
			default:
				//	Set target
				if (!_istdigit(tszArgument[0]))
				{
					SetLastError(ERROR_INVALID_ARGUMENTS);
					return FALSE;
				}
				i64Value = _ttoi64(tszArgument);
				lpTarget[0]	= i64Value;
				break;
			}
			if (Vars->lpAdmin)
			{
				if (Shift[0] == offsetof(USERFILE, Credits))
				{
					Putlog(LOG_SYSOP, _TEXT("'%s' changed credits for user '%s' from '%I64i' to '%I64i' on section '%i'.\r\n"),
						tszAdmin, tszUser, i64Prev, lpTarget[0], Shift[2]);
					if (Vars->lpBuffer)
					{
						FormatString(Vars->lpBuffer,_TEXT("%sChanged credits for user '%s' from '%I64i' to '%I64i' on section '%i'.\r\n"),
							Vars->tszMultilinePrefix, tszUser, i64Prev, lpTarget[0], Shift[2]);
					}
				}
				else
				{
					Putlog(LOG_SYSOP, _TEXT("'%s' changed %s %s stat for user '%s' from '%I64i' to '%I64i' on section '%i'.\r\n"),
						tszAdmin, tszField, tszType, tszUser, i64Prev, lpTarget[0], Shift[2]);
					if (Vars->lpBuffer)
					{
						FormatString(Vars->lpBuffer,_TEXT("%sChanged %s %s stat user '%s' from '%I64i' to '%I64i' on section '%i'.\r\n"),
							Vars->tszMultilinePrefix, tszField, tszType, tszUser, i64Prev, lpTarget[0], Shift[2]);
					}
				}
			}
			continue;
		}

		if (! _tcsicmp(tszArgument, _TEXT("credits")))
		{
			//	Set credits
			Shift[0] = offsetof(USERFILE, Credits);
			tszField = 0;
		}
		else if (! _tcsicmp(tszArgument, _TEXT("dayup")))
		{
			//	Set dayup stats
			Shift[0] = offsetof(USERFILE, DayUp);
			tszField = _T("DayUp");
		}
		else if (! _tcsicmp(tszArgument, _TEXT("daydn")))
		{
			//	Set daydn stats
			Shift[0] = offsetof(USERFILE, DayDn);
			tszField = _T("DayDn");
		}
		else if (! _tcsicmp(tszArgument, _TEXT("wkup")))
		{
			//	Set wkup stats
			Shift[0] = offsetof(USERFILE, WkUp);
			tszField = _T("WkUp");
		}
		else if (! _tcsicmp(tszArgument, _TEXT("wkdn")))
		{
			//	Set wkdn stats
			Shift[0] = offsetof(USERFILE, WkDn);
			tszField = _T("WkDn");
		}
		else if (! _tcsicmp(tszArgument, _TEXT("monthup")))
		{
			//	Set monthup stats
			Shift[0] = offsetof(USERFILE, MonthUp);
			tszField = _T("MonthUp");
		}
		else if (! _tcsicmp(tszArgument, _TEXT("monthdn")))
		{
			//	Set monthdn stats
			Shift[0] = offsetof(USERFILE, MonthDn);
			tszField = _T("MonthDn");
		}
		else if (! _tcsicmp(tszArgument, _TEXT("allup")))
		{
			//	Set allup stats
			Shift[0] = offsetof(USERFILE, AllUp);
			tszField = _T("AllUp");
		}
		else if (! _tcsicmp(tszArgument, _TEXT("alldn")))
		{
			//	Set alldn stats
			Shift[0] = offsetof(USERFILE, AllDn);
			tszField = _T("AllDn");
		}
		else if (! _tcsicmp(tszArgument, _TEXT("files")))
		{
			//	Set file stats
			Shift[1]	= 0;
			tszType = _T("FILES");
		}
		else if (! _tcsicmp(tszArgument, _TEXT("bytes")))
		{
			//	Set byte stats
			Shift[1]	= 1;
			tszType = _T("BYTES");
		}
		else if (! _tcsicmp(tszArgument, _TEXT("time")))
		{
			//	Set time stats
			Shift[1]	= 2;
			tszType = _T("TIME");
		}
		else if (! _tcsicmp(tszArgument, _TEXT("section")))
		{
			//	Make sure that there is one more argument
			if (++n == GetStringItems(Vars->Args)) return TRUE;
			//	Get section
			Shift[2]	= _tcstoul(GetStringIndexStatic(Vars->Args, n), NULL, 10);
			if (Shift[2] >= MAX_SECTIONS) Shift[2]	= 0;
		}
	}
	return TRUE;
}






/*

  Change_User_AdminGroup() - Change user's administration groups

  */
static BOOL Change_User_AdminGroup(CHANGE_VARIABLES *Vars)
{
	LPTSTR	tszGroup;
	PINT32	AdminGroups;
	DWORD	n, i;
	INT		Gid;
	BOOL    bFirst, bSkip, bModified;

	bModified = FALSE;
	AdminGroups	= Vars->lpTarget->AdminGroups;
	for (n = 3;n < GetStringItems(Vars->Args);n++)
	{
		tszGroup	= GetStringIndexStatic(Vars->Args, n);
		if (tszGroup && tszGroup[0] == _T('.'))
		{
			// special case... we want this group to be first
			bFirst = TRUE;
			tszGroup++;
		}
		else
		{
			bFirst = FALSE;
		}
		//	Get group id
		Gid	= Group2Gid(tszGroup);
		if (Gid >= 0)
		{
			//	Search existing groups
			for (i = 0;i < MAX_GROUPS && AdminGroups[i] != -1;i++)
			{
				if (AdminGroups[i] == Gid) break;
			}

			if (bFirst && i == 0 && AdminGroups[i] == Gid)
			{
				// we are already first so nothing to do
				if (Vars->lpAdmin && Vars->lpBuffer)
				{
					FormatString(Vars->lpBuffer, _TEXT("%sGroup '%s' is already the DEFAULT admingroup for user '%s'.\r\n"),
						Vars->tszMultilinePrefix, tszGroup, Uid2User(Vars->lpTarget->Uid));
				}
				continue;
			}

			if (i >= MAX_GROUPS)
			{
				if (Vars->lpAdmin &&  Vars->lpBuffer)
				{
					//	Could not add user to group
					FormatString(Vars->lpBuffer, _TEXT("%sCould not add '%s' to group '%s': max admingroups limit reached!\r\n"),
						Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), tszGroup);
				}
				continue;
			}


			bSkip = FALSE;
			if (AdminGroups[i] == Gid || bFirst)
			{
				bModified = TRUE;
				//	Remove user's admin group
				if (Vars->lpAdmin && AdminGroups[i] == Gid)
				{
					Putlog(LOG_SYSOP, _TEXT("'%s' Changed admingroup rights for user '%s' by removing group '%s'.\r\n"),
						Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), tszGroup);
					if (Vars->lpBuffer)
					{
						FormatString(Vars->lpBuffer, _TEXT("%sChanged admingroup rights for user '%s' by removing group '%s'.\r\n"),
							Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), tszGroup);
					}
				}

				MoveMemory(&AdminGroups[i], &AdminGroups[i + 1], (MAX_GROUPS - (i + 1)) * sizeof(INT));
				AdminGroups[MAX_GROUPS - 1]	= -1;
				if (!bFirst)
				{
					bSkip = TRUE;
				}
			}
			
			if (!bSkip)
			{
				bModified = TRUE;
				//	Add user to group admin
				if (Vars->lpAdmin)
				{
					Putlog(LOG_SYSOP, _TEXT("'%s' Changed admingroup rights for user '%s' by adding group '%s'%s.\r\n"),
						Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), tszGroup,
						(bFirst ? _T(" as DEFAULT") : _T("")));
					if (Vars->lpBuffer)
					{
						FormatString(Vars->lpBuffer, _TEXT("%sChanged admingroup rights for user '%s' by adding group '%s'%s.\r\n"),
							Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), tszGroup,
							(bFirst ? _T(" as DEFAULT") : _T("")));
					}
				}

				if (bFirst && i != 0)
				{
					MoveMemory(&AdminGroups[1], &AdminGroups[0], i * sizeof(INT));
					AdminGroups[0]	= Gid;
					if (++i < MAX_GROUPS) AdminGroups[i]	= -1;
				}
				else
				{
					AdminGroups[i]	= Gid;
					if (++i < MAX_GROUPS) AdminGroups[i]	= -1;
				}
			}
		}
		else if (Vars->lpAdmin && Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sUnknown group '%s'\r\n"), Vars->tszMultilinePrefix, tszGroup);
		}
	}

	if (Vars->lpAdmin && Vars->lpBuffer)
	{
		FormatString(Vars->lpBuffer, _TEXT("%sAdminGroups: "), Vars->tszMultilinePrefix);
		for (i = 0;i < MAX_GROUPS && AdminGroups[i] != -1;i++)
		{
			tszGroup = Gid2Group(AdminGroups[i]);
			if (tszGroup)
			{
				FormatString(Vars->lpBuffer, _TEXT("%s "), tszGroup);
			}
			else
			{
				FormatString(Vars->lpBuffer, _TEXT("(gid=%d) "), AdminGroups[i]);
			}
		}
		FormatString(Vars->lpBuffer, _TEXT("\r\n"));
	}

	if (bModified)
	{
		return TRUE;
	}
	SetLastError(ERROR_NOT_MODIFIED);
	return FALSE;
}







/*

  Change_User_Ratio() - Change user's Ratio

  */
static BOOL Change_User_Ratio(CHANGE_VARIABLES *Vars)
{
	LPTSTR	tszRatio;
	BOOL	bError;
	DWORD	dwSection;
	INT		OldRatio, Ratio;

	//	Get new ratio
	tszRatio	= GetStringIndexStatic(Vars->Args, 3);
	Ratio	= _tcstol(tszRatio, NULL, 10);
	//	Get section to set Ratio on
	if (GetStringItems(Vars->Args) < 5 ||
		(dwSection = _tcstoul(GetStringIndexStatic(Vars->Args, 4), NULL, 10)) >= MAX_SECTIONS)
	{
		dwSection	= 0;
	}

	//	Reduce slots, if executed by administrator
	if (Vars->lpAdmin)
	{
		// but only if we aren't modifying Default.User
		if (!Vars->bDefault)
		{
			//	Store old Ratio
			OldRatio	= Vars->lpTarget->Ratio[dwSection];
			//	User is administrator of some group
			if (OldRatio != 0 &&
				Vars->lpGroupFile)
			{
				bError	= TRUE;
				//	Lock groupfile it
				if (GroupFile_Lock(&Vars->lpGroupFile, 0)) return FALSE;

				switch (Vars->lpGroupFile->Slots[1])
				{
				case 0:
					//	Group has no more slots
					break;
				default:
					//	Reduce one slot
					Vars->lpGroupFile->Slots[1]--;
				case -1:
					//	Group has unlimited slots
					bError	= FALSE;
					break;
				}
				GroupFile_Unlock(&Vars->lpGroupFile, 0);

				if (bError) ERROR_RETURN(IO_NO_SLOTS, FALSE);
			}
			//	Log event
			Putlog(LOG_SYSOP, _TEXT("'%s' changed Ratio for user '%s' from '%i' to '%i' on section '%i'.\r\n"),
				Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), OldRatio, Ratio, dwSection);
			if (Vars->lpBuffer)
			{
				FormatString(Vars->lpBuffer, _TEXT("%sChanged Ratio for user '%s' from '%i' to '%i' on section '%i'.\r\n"),
					Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), OldRatio, Ratio, dwSection);
			}
		}
	}
	//	Update Ratio
	Vars->lpTarget->Ratio[dwSection]	= Ratio;
	return TRUE;
}



/*

  Change_User_Credits() - Changes user's credits

  */
static BOOL Change_User_Credits(CHANGE_VARIABLES *Vars)
{
	INT64	OldCredits;
	LPTSTR	tszCredits;
	DWORD	dwSection;


	//	Get new credits as string
	tszCredits	= GetStringIndexStatic(Vars->Args, 3);
	//	Get section to set credits on
	if (GetStringItems(Vars->Args) < 5 ||
		(dwSection = _tcstoul(GetStringIndexStatic(Vars->Args, 4), NULL, 10)) >= MAX_SECTIONS)
	{
		dwSection	= Vars->Section;
	}

	//	Store old credits
	OldCredits	= Vars->lpTarget->Credits[dwSection];

	switch (tszCredits[0])
	{
	case _TEXT('+'):
		//	Add credits
		Vars->lpTarget->Credits[dwSection]	+= _ttoi64(&tszCredits[1]);
		break;
	case _TEXT('-'):
		//	Subtract credits
		Vars->lpTarget->Credits[dwSection]	-= _ttoi64(&tszCredits[1]);
		break;
	default:
		//	Set credits
		Vars->lpTarget->Credits[dwSection]	= _ttoi64(tszCredits);
		break;
	}
	//	Log event
	if (Vars->lpAdmin)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed credits for user '%s' from '%I64i' to '%I64i' on section '%i'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid),
			OldCredits, Vars->lpTarget->Credits[dwSection], dwSection);
		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged credits for user '%s' from '%I64i' to '%I64i' on section '%i'.\r\n"),
				Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), OldCredits, Vars->lpTarget->Credits[dwSection], dwSection);
		}
	}
	return TRUE;
}



/*

Change_User_Expiration() - Changes user's credits

*/
static BOOL Change_User_Expiration(CHANGE_VARIABLES *Vars)
{
	INT64	i64Time;
	LPTSTR  tszValue, tszTemp;
	TCHAR   tszOldTime[30], tszNewTime[30];
	struct tm  tmTime;


	//	Get new credits as string
	tszValue = GetStringIndexStatic(Vars->Args, 3);

	if (!tszValue)
	{
		ERROR_RETURN(ERROR_INVALID_ARGUMENTS, FALSE);
	}

	if (Vars->lpTarget->ExpiresAt)
	{
		_gmtime64_s(&tmTime, &Vars->lpTarget->ExpiresAt);

		//	szFormat time to static buffer
		_tcsftime(tszOldTime, sizeof(tszOldTime), _T("%a %b %d %H:%M:%S %Y UTC"), &tmTime);
	}
	else
	{
		_tcscpy(tszOldTime, _T("<never>"));
	}

	i64Time = -1;
	if (!_tcsicmp(tszValue, _T("never")) || !_tcsicmp(tszValue, _T("0")))
	{
		i64Time = 0;
	}
	else if (tszValue[0] == _T('+'))
	{
		i64Time = _tcstol(&tszValue[1], &tszTemp, 10);

		if (!tszTemp || *tszTemp)
		{
			i64Time = -1;
		}
		else
		{
			i64Time = i64Time * 60 * 60;
			i64Time += time((time_t *) NULL);
		}
	}
	else if (tszValue[0] && tszValue[1] && tszValue[2] && tszValue[3])
	{
		ZeroMemory(&tmTime, sizeof(tmTime));

		if (tszValue[4] == _T('-'))
		{
			if (_stscanf_s(tszValue, "%4d-%2d-%2d-%2d-%2d-%2d",
				&tmTime.tm_year, &tmTime.tm_mon, &tmTime.tm_mday,
				&tmTime.tm_hour, &tmTime.tm_min, &tmTime.tm_sec) > 2)
			{
				if (tmTime.tm_year > 2000)
				{
					tmTime.tm_year -= 1900;
					if (tmTime.tm_mon) tmTime.tm_mon--; // need 0-11 instead of 1-12
					i64Time = mktime(&tmTime);
				}
			}
		}
		else
		{
			if ( _stscanf_s(tszValue, "%4d%2d%2d%2d%2d%2d",
				&tmTime.tm_year, &tmTime.tm_mon, &tmTime.tm_mday,
				&tmTime.tm_hour, &tmTime.tm_min, &tmTime.tm_sec) > 2)
			{
				if (tmTime.tm_year > 2000)
				{
					tmTime.tm_year -= 1900;
					if (tmTime.tm_mon) tmTime.tm_mon--; // need 0-11 instead of 1-12
					i64Time = mktime(&tmTime);
				}
			}
		}
	}

	if (i64Time == -1)
	{
		ERROR_RETURN(ERROR_INVALID_ARGUMENTS, FALSE);
	}

	if (i64Time == 0)
	{
		_tcscpy(tszNewTime, _T("<never>"));
	}
	else
	{
		_gmtime64_s(&tmTime, &i64Time);
		_tcsftime(tszNewTime, sizeof(tszNewTime), _T("%a %b %d %H:%M:%S %Y UTC"), &tmTime);
	}

	//	Log event
	if (Vars->lpAdmin)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed expiration for user '%s' from '%s' to '%s'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), tszOldTime, tszNewTime);

		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged expiration for user '%s' from '%s' to '%s'.\r\n"),
				Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), tszOldTime, tszNewTime);
		}
	}

	Vars->lpTarget->ExpiresAt = i64Time;
	return TRUE;
}




/*

  Change_User_SpeedLimit() - Changes user's speedlimits

  */
static BOOL Change_User_SpeedLimit(CHANGE_VARIABLES *Vars)
{
	LPDWORD		dwLimit;
	DWORD		dwSpeedLimit[2];
	LPTSTR		tszSendLimit, tszReceiveLimit;

	dwLimit			= (LPDWORD)Vars->lpTarget->Limits;
	dwSpeedLimit[0]	= dwLimit[0];
	dwSpeedLimit[1]	= dwLimit[1];

	tszSendLimit	= GetStringIndexStatic(Vars->Args, 3);
	tszReceiveLimit	= (GetStringItems(Vars->Args) < 5 ? NULL : GetStringIndexStatic(Vars->Args, 4));
	//	Update userfile
	if (tszSendLimit && _istdigit(tszSendLimit[0])) dwLimit[0]	= _tcstoul(tszSendLimit, NULL, 10);
	if (tszReceiveLimit && _istdigit(tszReceiveLimit[0])) dwLimit[1]	= _tcstoul(tszReceiveLimit, NULL, 10);
	//	Log events
	if (Vars->lpAdmin)
	{
		if (dwLimit[0] != dwSpeedLimit[0])
		{
			Putlog(LOG_SYSOP, _TEXT("'%s' changed max download speed for user '%s' from '%ukb/s' to '%ukb/s'.\r\n"),
				Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), dwSpeedLimit[0], dwLimit[0]);
			if (Vars->lpBuffer)
			{
				FormatString(Vars->lpBuffer, _TEXT("%sChanged max download speed for user '%s' from '%ukb/s' to '%ukb/s'.\r\n"),
					Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), dwSpeedLimit[0], dwLimit[0]);
			}

		}
		if (dwLimit[1] != dwSpeedLimit[1])
		{
			Putlog(LOG_SYSOP, _TEXT("'%s' changed max upload speed for user '%s' from '%ukb/s' to '%ukb/s'.\r\n"),
				Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), dwSpeedLimit[1], dwLimit[1]);
			if (Vars->lpBuffer)
			{
				FormatString(Vars->lpBuffer, _TEXT("%sChanged max upload speed for user '%s' from '%ukb/s' to '%ukb/s'.\r\n"),
					Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), dwSpeedLimit[1], dwLimit[1]);
			}
		}
	}
	return TRUE;
}


/*

  Change_User_DnLimit() - Changes user's download speed limit

  */
static BOOL Change_User_DnSpeed(CHANGE_VARIABLES *Vars)
{
	INT         iLimit, iCurrent;
	LPTSTR		tszLimit;

	iCurrent = Vars->lpTarget->Limits[0]; // dnlimit

	tszLimit	= GetStringIndexStatic(Vars->Args, 3);

	if (!tszLimit || !_istdigit(tszLimit[0]))
	{
		SetLastError(ERROR_INVALID_ARGUMENTS);
		return FALSE;
	}
	
	iLimit	= _tcstoul(tszLimit, NULL, 10);

	//	Update userfile
	Vars->lpTarget->Limits[0] = iLimit;

	//	Log events
	if (Vars->lpAdmin && iLimit != iCurrent)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed max download speed for user '%s' from '%dkb/s' to '%dkb/s'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), iCurrent, iLimit);
		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged max download speed for user '%s' from '%dkb/s' to '%dkb/s'.\r\n"),
				Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), iCurrent, iLimit);
		}
	}
	return TRUE;
}


/*

  Change_User_UpLimit() - Changes user's upload speed limit

  */
static BOOL Change_User_UpSpeed(CHANGE_VARIABLES *Vars)
{
	INT         iLimit, iCurrent;
	LPTSTR		tszLimit;

	iCurrent = Vars->lpTarget->Limits[1]; // uplimit

	tszLimit	= GetStringIndexStatic(Vars->Args, 3);

	if (!tszLimit || !_istdigit(tszLimit[0]))
	{
		SetLastError(ERROR_INVALID_ARGUMENTS);
		return FALSE;
	}
	
	iLimit	= _tcstoul(tszLimit, NULL, 10);

	//	Update userfile
	Vars->lpTarget->Limits[1] = iLimit;

	//	Log events
	if (Vars->lpAdmin && iLimit != iCurrent)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed max upload speed for user '%s' from '%dkb/s' to '%dkb/s'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), iCurrent, iLimit);
		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged max upload speed for user '%s' from '%dkb/s' to '%dkb/s'.\r\n"),
				Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), iCurrent, iLimit);
		}
	}
	return TRUE;
}



/*

  Change_User_Flags() - Changes user's flags

  */
static BOOL Change_User_Flags(CHANGE_VARIABLES *Vars)
{
	TCHAR	tszFlags[32 + 1];
	LPTSTR	tszNewFlags;
	TCHAR	*tpOffset;
	DWORD	dwFlags;
	BOOL    bModified = FALSE;
	BOOL    bRejected = FALSE;

	tszNewFlags	= GetStringIndexStatic(Vars->Args, 3);
	//	Copy flags from user to local string
	_tcscpy(tszFlags, Vars->lpTarget->Flags);
	dwFlags	= _tcslen(tszFlags);

	switch (tszNewFlags[0])
	{
	case _TEXT('+'):
		//	Add new flags
		for (;(++tszNewFlags)[0] != _TEXT('\0') && dwFlags < 32;)
		{
			if (!_tmemchr(tszFlags, tszNewFlags[0], dwFlags))
			{
				// it's not a duplicate flag, now test for M/V
				if (Vars->lpAdmin &&  ( ( (tszNewFlags[0] == _TEXT('M')) && !Vars->bLocalAdmin ) ||
					                    ( (tszNewFlags[0] == _TEXT('V')) && HasFlag(Vars->lpAdmin, _TEXT("MV")) ) ) )
				{
					bRejected = TRUE;
				}
				else
				{
					//	Append flag to string
					tszFlags[dwFlags++]	= tszNewFlags[0];
					bModified = TRUE;
				}
			}
		}
		break;

	case _TEXT('-'):
		//	Remove old flags
		for (;(++tszNewFlags)[0] != _TEXT('\0') && dwFlags;)
		{
			if (tpOffset = (TCHAR *)_tmemchr(tszFlags, tszNewFlags[0], dwFlags))
			{
				// it's an existing flag
				if (tszNewFlags[0] != _TEXT('M') ||	Vars->bLocalAdmin)
				{
					//	Remove flag from string
					MoveMemory(tpOffset, &tpOffset[1], &tszFlags[dwFlags--] - &tpOffset[1]);
					bModified = TRUE;
				}
				else
				{
					bRejected = TRUE;
				}
			}
		}
		break;

	default:
		//	Preserve master flag
		if (_tmemchr(tszFlags, _TEXT('M'), dwFlags))
		{
			tszFlags[0]	= _TEXT('M');
			dwFlags		= 1;
		}
		else dwFlags	= 0;

		for (;tszNewFlags[0] != _TEXT('\0') && dwFlags < 32;tszNewFlags++)
		{
			//	Do not add duplicate flags / master flag
			if (!_tmemchr(tszFlags, tszNewFlags[0], dwFlags))
			{
				// it's not a duplicate flag, now test for M/V
				if (Vars->lpAdmin && ( ( (tszNewFlags[0] == _TEXT('M')) && !Vars->bLocalAdmin ) ||
					                   ( (tszNewFlags[0] == _TEXT('V')) && HasFlag(Vars->lpAdmin, _TEXT("MV")) ) ) )
				{
					bRejected = TRUE;
				}
				else
				{
					//	Append flag to string
					tszFlags[dwFlags++]	= tszNewFlags[0];
					bModified = TRUE;
				}
			}
		}
		break;
	}
	//	Pad with zero
	tszFlags[dwFlags]	= _TEXT('\0');

	//	Log event
	if (Vars->lpAdmin)
	{
		if (bModified)
		{
			Putlog(LOG_SYSOP, _TEXT("'%s' changed flags for user '%s' from '%s' to '%s'.\r\n"),
				Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->Flags, tszFlags);
		}
		if (bRejected)
		{
			Putlog(LOG_SYSOP, _TEXT("'%s' had at least one flag change denied for user '%s'.\r\n"),
				Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid));
		}
		if (Vars->lpBuffer)
		{
			if (bModified)
			{
				FormatString(Vars->lpBuffer, _TEXT("%sChanged flags for user '%s' from '%s' to '%s'.\r\n"),
					Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->Flags, tszFlags);
			}
			else
			{
				FormatString(Vars->lpBuffer, _TEXT("%sUser '%s' flags are unchanged from '%s'.\r\n"),
					Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->Flags);
			}
			if (bRejected)
			{
				FormatString(Vars->lpBuffer, _TEXT("%sAt least one requested flag was denied.\r\n"),
					Vars->tszMultilinePrefix);
			}
		}
	}

	//	Update user's flags
	_tcscpy(Vars->lpTarget->Flags, tszFlags);

	if (!bModified)
	{
		if (bRejected)
		{
			SetLastError(IO_NO_ACCESS);
			return FALSE;
		}
		SetLastError(ERROR_NOT_MODIFIED);
		return FALSE;
	}
	return TRUE;
}




/*

  Change_User_VfsFile() - Changes uses VfsFile

  */
static BOOL Change_User_VfsFile(CHANGE_VARIABLES *Vars)
{
	DWORD	dwAttributes;
	LPTSTR	tszFileName;

	tszFileName	= GetStringRange(Vars->Args, 3, STR_END);

	if (!_tcsicmp(tszFileName, _T("default")) || !_tcsicmp(tszFileName, _T("<none>")))
	{
		if (Vars->lpAdmin)
		{
			Putlog(LOG_SYSOP, _TEXT("'%s' changed vfsfile for user '%s' from '%s' to default.\r\n"),
				Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->MountFile);
			if (Vars->lpBuffer)
			{
				FormatString(Vars->lpBuffer, _TEXT("%sChanged vfsfile for user '%s' from '%s' to default.\r\n"),
					Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->MountFile);
			}
		}

		//	Update user's vfs file
		Vars->lpTarget->MountFile[0] = 0;
		return TRUE;
	}

	//	Validate filename
	if (_tcslen(tszFileName) <= MAX_PATH &&
		(dwAttributes = GetFileAttributes(tszFileName)) != INVALID_FILE_SIZE &&
		! (dwAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		//	Log event, if it was executed by administrator
		if (Vars->lpAdmin)
		{
			Putlog(LOG_SYSOP, _TEXT("'%s' changed vfsfile for user '%s' from '%s' to '%s'.\r\n"),
				Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->MountFile, tszFileName);
			if (Vars->lpBuffer)
			{
				FormatString(Vars->lpBuffer, _TEXT("%sChanged vfsfile for user '%s' from '%s' to '%s'.\r\n"),
					Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->MountFile, tszFileName);
			}
		}
		//	Update user's vfs file
		_tcscpy(Vars->lpTarget->MountFile, tszFileName);
		return TRUE;
	}
	ERROR_RETURN(ERROR_FILE_NOT_FOUND, FALSE);
}






/*

  Change_User_Logins() - Change Maximum concurrent logins for user

  */
static BOOL Change_User_Logins(CHANGE_VARIABLES *Vars)
{
	LPTSTR	tszServiceType;
	INT		Logins, ServiceType;

	//	Get default service type
	ServiceType	= C_FTP;

	tszServiceType	= _TEXT("FTP");

	//	Store old max login
	Logins	= Vars->lpTarget->Limits[ServiceType + 2];
	//	Update max logins
	Vars->lpTarget->Limits[ServiceType + 2]	= atoi(GetStringIndexStatic(Vars->Args, 3));
	//	Log event
	if (Vars->lpAdmin)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed max concurrent '%s' logins for user '%s' from '%i' to '%i'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), tszServiceType,
			Uid2User(Vars->lpTarget->Uid), Logins, Vars->lpTarget->Limits[ServiceType + 2]);
		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged max concurrent '%s' logins for user '%s' from '%i' to '%i'.\r\n"),
				Vars->tszMultilinePrefix, tszServiceType,
				Uid2User(Vars->lpTarget->Uid), Logins, Vars->lpTarget->Limits[ServiceType + 2]);
		}
	}
	return TRUE;
}



/*

Change_User_LimitPerIp() - Change Maximum concurrent logins for user per IP address

*/
static BOOL Change_User_LimitPerIp(CHANGE_VARIABLES *Vars)
{
	LPTSTR  tszArg;
	INT		iPerIp, iOld;

	tszArg = GetStringIndexStatic(Vars->Args, 3);

	if (tszArg && _istdigit(tszArg[0])) iPerIp = _tcstoul(tszArg, NULL, 10);

	iOld = Vars->lpTarget->LimitPerIP;
	// store new
	Vars->lpTarget->LimitPerIP = iPerIp;

	//	Log event
	if (Vars->lpAdmin)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed max concurrent logins per IP for user '%s' from '%i' to '%i'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), iOld, iPerIp);
		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged max concurrent logins per IP for user '%s' from '%i' to '%i'.\r\n"),
				Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), iOld, iPerIp);
		}
	}
	return TRUE;
}




/*

  Change_User_Password() - Change user's password

  */
static BOOL Change_User_Password(CHANGE_VARIABLES *Vars)
{
	LPTSTR	tszPassword;

	tszPassword	= GetStringRange(Vars->Args, 3, STR_END);
	//	Encrypt password
	HashString(tszPassword, Vars->lpTarget->Password);
	//	Log event, if it was executed by administrator
	if (Vars->lpAdmin)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed password for user '%s'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid));
	}
	return TRUE;
}





/*

  Change_User_HomeDir() - Change user's homedir

  */
static BOOL Change_User_HomeDir(CHANGE_VARIABLES *Vars)
{
	LPTSTR	tszHomeDirectory;

	tszHomeDirectory	= GetStringRange(Vars->Args, 3, STR_END);

	if (!_tcsicmp(tszHomeDirectory, _T("<none>")))
	{
		tszHomeDirectory = _T("");
	}

	//	Log event, if it was executed by administrator
	if (Vars->lpAdmin)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed homedir for user '%s' from '%s' to '%.*s'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid),
			Vars->lpTarget->Home, _MAX_PATH, tszHomeDirectory);
		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged homedir for user '%s' from '%s' to '%.*s'.\r\n"),
				Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid),
				Vars->lpTarget->Home, _MAX_PATH, tszHomeDirectory);
		}
	}
	//	Update homedir
	_stprintf(Vars->lpTarget->Home, _TEXT("%.*s"), _MAX_PATH, tszHomeDirectory);

	return TRUE;
}







/*

  Change_User_Tagline() - Change user's tagline

  */
static BOOL Change_User_Tagline(CHANGE_VARIABLES *Vars)
{
	LPTSTR	tszTagline;
	
	tszTagline	= GetStringRange(Vars->Args, 3, STR_END);

	if (!_tcsicmp(tszTagline, _T("<none>")))
	{
		tszTagline = _T("");
	}

	//	Log event, if it was executed by administrator
	if (Vars->lpAdmin)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed tagline for user '%s' from '%s' to '%.*s'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->Tagline, 128, tszTagline);
		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged tagline for user '%s' from '%s' to '%.*s'.\r\n"),
				Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->Tagline, 128, tszTagline);
		}
	}
	//	Update tagline
	_stprintf(Vars->lpTarget->Tagline, _TEXT("%.*s"), 128, tszTagline);

	return TRUE;
}


/*

Change_User_MaxDownloads() - Change user's maximum concurrent downloads

*/
static BOOL Change_User_MaxDownloads(CHANGE_VARIABLES *Vars)
{
	LPTSTR	tszLimit;
	INT32   iLimit;

	tszLimit = GetStringIndexStatic(Vars->Args, 3);
	iLimit = atoi(tszLimit);

	if (iLimit < -1) iLimit = -1;

	//	Log event, if it was executed by administrator
	if (Vars->lpAdmin)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed MaxDownloads for user '%s' from '%d' to '%d'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->MaxDownloads, iLimit);
		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged MaxDownloads for user '%s' from '%d' to '%d'.\r\n"),
				Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->MaxDownloads, iLimit);
		}
	}
	//	Update field
	Vars->lpTarget->MaxDownloads = iLimit;

	return TRUE;
}


/*

Change_User_MaxUploads() - Change user's maximum concurrent uploads

*/
static BOOL Change_User_MaxUploads(CHANGE_VARIABLES *Vars)
{
	LPTSTR	tszLimit;
	INT32   iLimit;

	tszLimit = GetStringIndexStatic(Vars->Args, 3);
	iLimit = atoi(tszLimit);

	if (iLimit < -1) iLimit = -1;

	//	Log event, if it was executed by administrator
	if (Vars->lpAdmin)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed MaxUploads for user '%s' from '%d' to '%d'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->MaxUploads, iLimit);
		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged MaxUploads for user '%s' from '%d' to '%d'.\r\n"),
				Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->MaxUploads, iLimit);
		}
	}
	//	Update field
	Vars->lpTarget->MaxUploads = iLimit;

	return TRUE;
}


/*

Change_User_Opaque() - Change user's opaque field

*/
static BOOL Change_User_Opaque(CHANGE_VARIABLES *Vars)
{
	LPSTR   szOpaque;

	szOpaque = GetStringRange(Vars->Args, 3, STR_END);

	if (!szOpaque || !strcmp(szOpaque, "<none>"))
	{
		szOpaque = "";
	}

#if 0
	DWORD   n, items, len;
	INT     iResult;

	len = 0;
	items = GetStringItems(Vars->Args);
	n = 3;
	szOpaque[0] = 0;

	while ((len < sizeof(szOpaque)) && (n < items))
	{
		iResult = _snprintf_s(&szOpaque[len], sizeof(szOpaque)-len, _TRUNCATE, "%s\"%s\"", 
			(len > 0 ? " " : ""), GetStringIndexStatic(Vars->Args, n));
		if (iResult < 0) break;
		len += iResult;
		n++;
	}
#endif


	//	Log event, if it was executed by administrator
	if (Vars->lpAdmin)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed opaque for user '%s' from '%s' to '%.*s'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->Opaque, 256, szOpaque);
		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged opaque for user '%s' from '%s' to '%.*s'.\r\n"),
				Vars->tszMultilinePrefix, Uid2User(Vars->lpTarget->Uid), Vars->lpTarget->Opaque, 256, szOpaque);
		}
	}

	strncpy_s(Vars->lpTarget->Opaque, sizeof(Vars->lpTarget->Opaque), szOpaque, 256);

	return TRUE;
}





/*

  Change_Group_VfsFile() - Changes group's vfs file

  */
static BOOL Change_Group_VfsFile(CHANGE_VARIABLES *Vars)
{
	DWORD	dwAttributes;
	LPTSTR	tszFileName;

	//	Get new vfs file
	tszFileName	= GetStringRange(Vars->Args, 3, STR_END);

	if (!_tcsicmp(tszFileName, _T("default")) || !_tcsicmp(tszFileName, _T("<none>")))
	{
		if (Vars->lpAdmin)
		{
			Putlog(LOG_SYSOP, _TEXT("'%s' changed vfsfile for group '%s' from '%s' to default.\r\n"),
				Uid2User(Vars->lpAdmin->Uid), Vars->tszGroupName, Vars->lpGroupFile->szVfsFile);
			if (Vars->lpBuffer)
			{
				FormatString(Vars->lpBuffer, _TEXT("%sChanged vfsfile for group '%s' from '%s' to default.\r\n"),
					Vars->tszMultilinePrefix, Vars->tszGroupName, Vars->lpGroupFile->szVfsFile);
			}
		}
		//	Update group's vfs file
		Vars->lpGroupFile->szVfsFile[0] = 0;
		return TRUE;
	}

	if (_tcslen(tszFileName) <= MAX_PATH &&
		(dwAttributes = GetFileAttributes(tszFileName)) != INVALID_FILE_SIZE &&
		! (dwAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		//	Log event, if it was executed by administrator
		if (Vars->lpAdmin)
		{
			//	Get group name
			Putlog(LOG_SYSOP, _TEXT("'%s' changed vfsfile for group '%s' from '%s' to '%s'.\r\n"),
				Uid2User(Vars->lpAdmin->Uid), Vars->tszGroupName, Vars->lpGroupFile->szVfsFile, tszFileName);
			if (Vars->lpBuffer)
			{
				FormatString(Vars->lpBuffer, _TEXT("%sChanged vfsfile for group '%s' from '%s' to '%s'.\r\n"),
					Vars->tszMultilinePrefix, Vars->tszGroupName, Vars->lpGroupFile->szVfsFile, tszFileName);
			}
		}
		//	Update group's vfs file
		_tcscpy(Vars->lpGroupFile->szVfsFile, tszFileName);
		return TRUE;
	}
	ERROR_RETURN(ERROR_FILE_NOT_FOUND, FALSE);
}




/*

  Change_Group_Slots() - Changes group's slots

  */
static BOOL Change_Group_Slots(CHANGE_VARIABLES *Vars)
{
	INT		Normal, Leech;

	//	Store old values
	Normal	= Vars->lpGroupFile->Slots[0];
	Leech	= Vars->lpGroupFile->Slots[1];
	//	Get new values
	Vars->lpGroupFile->Slots[0]	= atoi(GetStringIndexStatic(Vars->Args, 3));
	if (GetStringItems(Vars->Args) > 4) Vars->lpGroupFile->Slots[1]	= atoi(GetStringIndexStatic(Vars->Args, 4));

	//	Log event
	if (Vars->lpAdmin)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed user slots for group '%s' from '%i + %i' to '%i + %i'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid), Vars->tszGroupName,
			Normal, Leech, Vars->lpGroupFile->Slots[0], Vars->lpGroupFile->Slots[1]);
		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged user slots for group '%s' from '%i + %i' to '%i + %i'.\r\n"),
				Vars->tszMultilinePrefix, Vars->tszGroupName,
				Normal, Leech, Vars->lpGroupFile->Slots[0], Vars->lpGroupFile->Slots[1]);
		}
	}
	return TRUE;
}



/*

  Change_Group_Description() - Changes group's description

  */
static BOOL Change_Group_Description(CHANGE_VARIABLES *Vars)
{
	LPTSTR	tszDescription;

	tszDescription	= GetStringRange(Vars->Args, 3, STR_END);

	if (!_tcsicmp(tszDescription, _T("none")) || !_tcsicmp(tszDescription, _T("<none>")))
	{
		tszDescription = _T("");
	}
	//	Log event, if it was executed by administrator
	if (Vars->lpAdmin)
	{
		Putlog(LOG_SYSOP, _TEXT("'%s' changed description for group '%s' from '%s' to '%.*s'.\r\n"),
			Uid2User(Vars->lpAdmin->Uid),
			Vars->tszGroupName, Vars->lpGroupFile->szDescription, 128, tszDescription);
		if (Vars->lpBuffer)
		{
			FormatString(Vars->lpBuffer, _TEXT("%sChanged description for group '%s' from '%s' to '%.*s'.\r\n"),
				Vars->tszMultilinePrefix, 
				Vars->tszGroupName, Vars->lpGroupFile->szDescription, 128, tszDescription);
		}
	}
	//	Update description
	wsprintf(Vars->lpGroupFile->szDescription, _TEXT("%.128s"), tszDescription);

	return TRUE;
}








LPTSTR Admin_Change(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	CHANGE_VARIABLES	ChangeVars;
	LPUSERSEARCH		hUserSearch;
	LPBUFFER			lpBuffer;
	LPTSTR				tszCommand, tszTarget;
	DWORD				n, dwCommand, dwError;
	BOOL				bResult;
	INT32				Gid;
	USERFILE            UserFile;
	GROUPFILE           GroupFile;


	//	Check # of arguments
	if (GetStringItems(Args) < 4) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));

	dwCommand	= GetStringIndexLength(Args, 2);
	tszTarget	= GetStringIndexStatic(Args, 1);
	tszCommand	= GetStringIndexStatic(Args, 2);
	lpBuffer	= (lpUser ? &lpUser->CommandChannel.Out : NULL);
	dwError     = NO_ERROR;

	for (n = 0;ChangeCommand[n].Trigger;n++)
	{
		//	Search for command
		if (ChangeCommand[n].l_Trigger == dwCommand &&
			! _tcsnicmp(ChangeCommand[n].Trigger, tszCommand, dwCommand))
		{
			//	Check access to sub-command
			if (lpUser && lpUser->UserFile &&
				Config_Get_Permission2(&IniConfigFile, _TEXT("Change_Permissions"), tszCommand, lpUser->UserFile))
			{
				return tszCommand;
			}

			//	Setup change structure
			ChangeVars.lpGroupFile	= NULL;
			ChangeVars.Args			= Args;
			ChangeVars.tszMultilinePrefix = tszMultilinePrefix;
			ChangeVars.lpBuffer     = lpBuffer;
			ChangeVars.bDefault     = FALSE;
			if (lpUser)
			{
				ChangeVars.Section		= 0;
				ChangeVars.Service		= lpUser->Connection.lpService->dwType;
				ChangeVars.lpAdmin		= lpUser->UserFile;
				ChangeVars.bLocalAdmin  = !IsLocalAdmin(lpUser->UserFile, &lpUser->Connection);
			}
			else
			{
				ChangeVars.Section		= 0;
				ChangeVars.Service		= C_FTP;
				ChangeVars.lpAdmin		= NULL;
				ChangeVars.bLocalAdmin  = FALSE;
			}

			if (ChangeCommand[n].bUserCommand)
			{
				if (!_tcsnicmp(tszTarget, _T("/Default"), 8))
				{
					if (!ChangeCommand[n].bDefaultCommand)
					{
						SetLastError(ERROR_INVALID_ARGUMENTS);
						return tszCommand;
					}

					if (!_tcsicmp(tszTarget, _T("/Default.User")))
					{
						Gid = -1;
					}
					else
					{
						if (tszTarget[8] != _T('='))
						{
							SetLastError(ERROR_INVALID_ARGUMENTS);
							return tszTarget;
						}
						if ((Gid = Group2Gid(&tszTarget[9])) == -1)
						{
							SetLastError(ERROR_GROUP_NOT_FOUND);
							return tszTarget;
						}
					}

					// handle default.user case
					bResult	= FALSE;
					ChangeVars.bDefault = TRUE;
					ChangeVars.lpTarget = &UserFile;

					if (!HasFlag(ChangeVars.lpAdmin, _TEXT("M")) &&
						!User_Default_Open(&UserFile, Gid))
					{
						//	Execute change command
						bResult	= ChangeCommand[n].Command(&ChangeVars);
						if (bResult)
						{
							if (User_Default_Write(&UserFile))
							{
								dwError = GetLastError();
								if (dwError == ERROR_USER_DELETED)
								{
									dwError = NO_ERROR;
									if (lpBuffer)
									{
										// todo: make sure / in name is stripped off tszTarget
										FormatString(lpBuffer, _TEXT("%s'%s' is no longer specialized.  Reverting to 'Default.User' for new users in group.\r\n"),
											tszMultilinePrefix, tszTarget);
									}
								}
							}
						}
						else
						{
							dwError = GetLastError();
						}
						if (!User_Default_Close(&UserFile) && !dwError)
						{
							dwError = GetLastError();
						}
					}
					else dwError	= GetLastError();

					if ((!bResult || dwError) && lpBuffer)
					{
						FormatString(lpBuffer, _TEXT("%s%s error: %E.\r\n"), tszMultilinePrefix, tszTarget, dwError);
					}
					else if (lpBuffer)
					{
						FormatString(lpBuffer, _TEXT("%s%s: Account modified.\r\n"), tszMultilinePrefix, tszTarget);
					}
					return NULL;
				}
				//	Initialize user search
				else if ((hUserSearch = FindFirstUser(tszTarget, &ChangeVars.lpTarget, NULL, lpUser, NULL)))
				{
					do
					{
						Gid	= -1;
						bResult	= FALSE;
						ChangeVars.lpGroupFile	= NULL;

						if (ChangeVars.lpAdmin &&
							ChangeVars.lpAdmin->Uid != ChangeVars.lpTarget->Uid &&
							! HasFlag(ChangeVars.lpTarget, _TEXT("M")) && !ChangeVars.bLocalAdmin)
						{
							//	lpTarget user is master (not self) and not locally connected
							dwError	= ERROR_MASTER;
						}
						else if (! ChangeVars.lpAdmin || !HasFlag(ChangeVars.lpAdmin, _T("M")) ||
							! User_IsAdmin(ChangeVars.lpAdmin, ChangeVars.lpTarget, &Gid))
						{
							//	Open groupfile
							if (Gid == -1 ||
								! GroupFile_OpenPrimitive(Gid, &ChangeVars.lpGroupFile, 0))
							{
								//	Lock target userfile
								if (! UserFile_Lock(&ChangeVars.lpTarget, 0))
								{
									//	Execute change command
									bResult	= ChangeCommand[n].Command(&ChangeVars);
									if (! bResult) dwError	= GetLastError();
									UserFile_Unlock(&ChangeVars.lpTarget, 0);
								}
								if (Gid != -1) GroupFile_Close(&ChangeVars.lpGroupFile, 0);
							}
						}
						else dwError	= GetLastError();
						//	Print message
						if (! bResult && lpBuffer)
						{
							FormatString(lpBuffer, _TEXT("%s%s: %E.\r\n"),
								tszMultilinePrefix, Uid2User(ChangeVars.lpTarget->Uid), dwError);
						}
						else if (lpBuffer)
						{
							FormatString(lpBuffer, _TEXT("%s%s: Account modified.\r\n"),
								tszMultilinePrefix, Uid2User(ChangeVars.lpTarget->Uid));
						}
						UserFile_Close(&ChangeVars.lpTarget, 0);

					} while (! FindNextUser(hUserSearch, &ChangeVars.lpTarget));
				}
				else ERROR_RETURN(ERROR_NO_MATCH, tszTarget);
			}
			else
			{
				ChangeVars.tszGroupName	= tszTarget;

				if (!_tcsicmp(tszTarget, _T("/Default.Group")))
				{
					// handle default.group case
					bResult	= FALSE;
					ChangeVars.bDefault = TRUE;
					ChangeVars.lpGroupFile = &GroupFile;

					if (!HasFlag(ChangeVars.lpAdmin, _TEXT("M")) &&
						!Group_Default_Open(&GroupFile))
					{
						// only M flagged users can change the default accounts

						//	Execute change command
						bResult	= ChangeCommand[n].Command(&ChangeVars);
						if (bResult)
						{
							if (Group_Default_Write(&GroupFile))
							{
								dwError = GetLastError();
							}
						}
						else
						{
							dwError = GetLastError();
						}
						if (!Group_Default_Close(&GroupFile) && !dwError)
						{
							dwError = GetLastError();
						}
					}
					else dwError	= GetLastError();

					if ((!bResult || dwError) && lpBuffer)
					{
						ERROR_RETURN(dwError, _T("Default.Group"));
					}
					else if (lpBuffer)
					{
						FormatString(lpBuffer, _TEXT("%sDefault.Group: Account modified.\r\n"), tszMultilinePrefix);
						return NULL;
					}
				}

				//	Open groupfile
				if (! GroupFile_Open(tszTarget, &ChangeVars.lpGroupFile, 0))
				{
					//	Execute change command
					if (! GroupFile_Lock(&ChangeVars.lpGroupFile, 0))
					{
						//	Execute change command
						bResult	= ChangeCommand[n].Command(&ChangeVars);
						if (! bResult) dwError	= GetLastError();
						GroupFile_Unlock(&ChangeVars.lpGroupFile, 0);
					}

					GroupFile_Close(&ChangeVars.lpGroupFile, 0);
					if (! bResult) ERROR_RETURN(dwError, tszTarget);
				}
				else return tszTarget;
			}
			return NULL;
		}
	}
	ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszCommand);
}

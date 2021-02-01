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

typedef struct _CONFIG_LINE
{
	LPSTR				Variable;
	INT					Variable_l;
	LPSTR				Value;
	INT					Value_l;
	CHAR				Active;
	LPSTR				Text;
	INT					Text_l;
	INT					Row;
	struct _CONFIG_LINE	*Next;

} CONFIG_LINE, * LPCONFIG_LINE;


typedef struct _CONFIG_LINE_ARRAY
{
	LPSTR						Name;
	LPCONFIG_LINE				*Sorted;
	LPCONFIG_LINE				First_Line;
	INT							Name_Len;
	INT							Lines;
	INT							SSize;
	struct _CONFIG_LINE_ARRAY	*Next;

} CONFIG_LINE_ARRAY, * LPCONFIG_LINE_ARRAY;


typedef struct _CONFIG_FILE
{
	LPTSTR              tszConfigFile;
	LOCKOBJECT			loConfig;
	LPCONFIG_LINE_ARRAY	lpLineArray;
} CONFIG_FILE, *LPCONFIG_FILE;


typedef struct _SECTION_INFO
{
	INT    iCredit;
	INT    iStat;
	INT    iShare;
	DWORD  dwSectionName;
	TCHAR  tszSectionName[_MAX_NAME+1];
	TCHAR  tszPath[1];
} SECTION_INFO, *LPSECTION_INFO;


extern CONFIG_FILE IniConfigFile;
extern LPSECTION_INFO *lpSectionInfoArray;
extern DWORD           dwSectionInfoArray;
extern DWORD           dwMaxSectionInfoArray;


#define CONFIG_INSERT	0
#define CONFIG_REPLACE	1
#define CONFIG_DEL		2

BOOL Config_Init(BOOL bFirstInitialization, LPTSTR tszConfigFile);
VOID Config_DeInit(VOID);

LPCONFIG_FILE Config_GetIniFile(VOID);
BOOL Config_Load(LPTSTR tszFileName, LPCONFIG_FILE *plpConfigFile);
VOID Config_Free(LPCONFIG_FILE lpConfigFile);
VOID Config_Lock(LPCONFIG_FILE lpConfigFile, BOOL bExclusive);
VOID Config_Unlock(LPCONFIG_FILE lpConfigFile, BOOL bExclusive);

LPSTR Config_Get(LPCONFIG_FILE lpConfigFile, LPSTR szArray, LPSTR szVariable, LPSTR szBuffer, LPINT lpOffset);
LPSTR Config_Get_Path(LPCONFIG_FILE lpConfigFile, LPSTR szArray, LPSTR szVariable, LPSTR szSuffix, LPSTR szBuffer);
LPSTR Config_Get_Path_Shared(LPCONFIG_FILE lpConfigFile, LPSTR szArray, LPSTR szVariable, LPSTR szSuffix);
LPSTR Config_Get_Linear(LPCONFIG_FILE lpConfigFile, LPSTR szArray, LPSTR szVariable, LPSTR szBuffer, LPVOID *lpOffset);
VOID Config_Get_Linear_End(LPCONFIG_FILE lpConfigFile);
BOOL Config_Get_Int(LPCONFIG_FILE lpConfigFile, LPSTR szArray, LPSTR szVariable, LPINT lpValue);
BOOL Config_Get_Bool(LPCONFIG_FILE lpConfigFile, LPSTR szArray, LPSTR szVariable, LPBOOL lpValue);
BOOL Config_Get_Permission(LPCONFIG_FILE lpConfigFile, LPSTR szArray, LPSTR szVariable, LPUSERFILE lpUserFile);
BOOL Config_Get_Permission2(LPCONFIG_FILE lpConfigFile, LPSTR szArray, LPSTR szVariable, LPUSERFILE lpUserFile);

BOOL PathCheck(LPUSERFILE lpUserFile, LPSTR szVirtualPath, LPSTR szAccessMethod);
VOID Config_Get_Section(LPSTR szVirtualPath, LPSTR szSection, LPINT lpCreditSection,
						LPINT lpStatsSection, LPINT lpShareSection);
BOOL Config_Get_SectionNum(INT iSectionNum, LPTSTR tszSectionName,
						   LPINT lpCreditSection, LPINT lpStatsSection, LPINT lpShareSection);

BOOL Config_Set(LPCONFIG_FILE lpConfigFile, LPSTR Array, INT Line, LPSTR Value, INT Mode);
BOOL Config_Read(LPCONFIG_FILE lpConfigFile);
BOOL Config_Write(LPCONFIG_FILE lpConfigFile);
VOID Config_Print(LPCONFIG_FILE lpConfigFile, LPBUFFER lpBuffer, LPSTR szArray, LPSTR szLinePrefix);

LPCONFIG_LINE Config_Get_Primitive(LPCONFIG_FILE lpConfigFile, LPTSTR tszArray, LPTSTR tszVariable, LPINT lpOffset);



// Shim functions to keep the old syntax because nxMyDB uses these and isn't updated yet...
LPSTR Config_Get_Old(LPSTR szArray, LPSTR szVariable, LPSTR szBuffer, LPINT lpOffset);
BOOL Config_Get_Int_Old(LPSTR szArray, LPSTR szVariable, LPINT lpValue);
BOOL Config_Get_Bool_Old(LPSTR szArray, LPSTR szVariable, LPBOOL lpValue);
LPSTR Config_Get_Path_Old(LPSTR szArray, LPSTR szVariable, LPSTR szSuffix, LPSTR szBuffer);

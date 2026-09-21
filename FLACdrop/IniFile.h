#pragma once
#include "stdafx.h"
#include <atlstr.h>

class CIniFile
{
public:
	CIniFile();
	~CIniFile();
	void SetIniFile(CString strFile);
	CString GetIniFIle();
	DWORD GetPrivateProfile(CString strSection, CString strKey, CString strDefault, CString *pstrValue);
	DWORD GetPrivateProfile(CString strSection, CString strKey, int nDefault, INT *pnValue);
	BOOL WritePrivateProfile(CString strSection, CString strKey, CString strValue);
	BOOL WritePrivateProfile(CString strSection, CString strKey, int nValue);

protected:

protected:
	CString m_strIniFile;
};


#include "stdafx.h"
#include "IniFile.h"


CIniFile::CIniFile()
{
	TCHAR path[MAX_PATH];
	GetModuleFileName(NULL, path, MAX_PATH);

	CString ini = path;
	int pos = ini.ReverseFind(_T('.'));
	if (pos != -1)
		ini = ini.Left(pos) + _T(".ini");
	else
		ini += _T(".ini");

	m_strIniFile = ini;
}


CIniFile::~CIniFile()
{
}

void CIniFile::SetIniFile(CString strFile)
{
	m_strIniFile = strFile;
}

CString CIniFile::GetIniFIle()
{
	return m_strIniFile;
}

DWORD CIniFile::GetPrivateProfile(CString strSection, CString strKey, CString strDefault, CString *pstrValue)
{
	DWORD dwRet;
	TCHAR cBuff[512];

	*pstrValue = "";

	if (m_strIniFile.GetLength() == 0) {
		return 0;
	}

	dwRet = ::GetPrivateProfileString(strSection, strKey, strDefault, cBuff, sizeof(cBuff), m_strIniFile);
	if (dwRet > 0) {
		*pstrValue = cBuff;
	}

	return dwRet;
}

DWORD CIniFile::GetPrivateProfile(CString strSection, CString strKey, int nDefault, INT *pnValue)
{
	INT unRet;

	*pnValue = 0;

	if (m_strIniFile.GetLength() == 0) {
		return 0;
	}

	unRet = (INT)::GetPrivateProfileInt(strSection, strKey, nDefault, m_strIniFile);
	*pnValue = unRet;

	return unRet;
}

BOOL CIniFile::WritePrivateProfile(CString strSection, CString strKey, CString strValue)
{
	BOOL bRet;

	if (m_strIniFile.GetLength() == 0) {
		return FALSE;
	}

	bRet = ::WritePrivateProfileString(strSection, strKey, strValue, m_strIniFile);

	return bRet;
}

BOOL CIniFile::WritePrivateProfile(CString strSection, CString strKey, int nValue)
{
	CString strValue;
	BOOL bRet;

	if (m_strIniFile.GetLength() == 0) {
		return FALSE;
	}

	strValue.Format(_T("%d"), nValue);

	bRet = ::WritePrivateProfileString(strSection, strKey, strValue, m_strIniFile);

	return bRet;
}

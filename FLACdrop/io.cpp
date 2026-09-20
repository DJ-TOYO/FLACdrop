#include "stdafx.h"
#include "encoders.h"

extern sEncoderSettings EncSettings;

void CenterWindowOnPrimaryMonitor(HWND hWnd);
DWORD GetPhysicalCoreCount();

// Setting Registry Reset *delete Registry
int ResetRegistrySettings()
{
	LONG result = RegDeleteTree(HKEY_CURRENT_USER, L"SOFTWARE\\FLACdrop");

	if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
		return 0;

	return FAIL_REGISTRY_WRITE;
}

//
//  FUNCTION: RegOut()
//
//  PURPOSE: Writes the settings to the registry
//
int WriteSettings()
{
	HKEY hKey;
	DWORD err, temp;

	// create registry key
	err = RegCreateKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\FLACdrop", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_SET_VALUE | KEY_CREATE_SUB_KEY, 0, &hKey, NULL);
	if(err != ERROR_SUCCESS) return FAIL_REGISTRY_OPEN;
	
	// write encoder settings
	err = RegSetValueEx(hKey, L"FLAC_Quality", 0, REG_DWORD, (LPBYTE) &EncSettings.FLAC_EncodingQuality, sizeof(DWORD));
	if(err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	if(EncSettings.FLAC_Verify == true) temp = 1;
	else temp = 0;
	err = RegSetValueEx(hKey, L"FLAC_Verify",0, REG_DWORD, (LPBYTE)&temp, sizeof(DWORD));
	if(err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	if(EncSettings.FLAC_MD5check == true) temp = 1;
	else temp = 0;
	err = RegSetValueEx(hKey, L"FLAC_MD5check",0, REG_DWORD, (LPBYTE)&temp, sizeof(DWORD));
	if(err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	err = RegSetValueEx(hKey, L"MP3_Int_Quality", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_InternalEncodingQuality, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	err = RegSetValueEx(hKey, L"MP3_CBR_Bitrate", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_CBRBitrate, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	err = RegSetValueEx(hKey, L"MP3_VBR_Quality", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_VBRQuality, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	err = RegSetValueEx(hKey, L"MP3_Enc_Type", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_EncodingMode, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	err = RegSetValueEx(hKey, L"OUT_Type", 0, REG_DWORD, (LPBYTE)&EncSettings.enOutType, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	err = RegSetValueEx(hKey, L"OUT_Threads", 0, REG_DWORD, (LPBYTE)&EncSettings.OUT_Threads, sizeof(DWORD));
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FAIL_REGISTRY_WRITE;
	}

	RegCloseKey(hKey);
	return 0;
}

//
//  FUNCTION:	RegIn()
//
//  PURPOSE:	Reads the settings from the registry
//
int ReadSettings()
{
	HKEY hKey;
	DWORD cb, type, err, temp;

	// check if registry manipulating is working
	err = RegCreateKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\FLACdrop", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_SET_VALUE | KEY_CREATE_SUB_KEY, NULL, &hKey, NULL);
	if(err != ERROR_SUCCESS) return FAIL_REGISTRY_OPEN;

	// load FLAC quality setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"FLAC_Quality", 0, &type, (LPBYTE)&EncSettings.FLAC_EncodingQuality, &cb);
	if((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.FLAC_EncodingQuality = FLAC_ENCODINGQUALITY;
		RegDeleteValue(hKey, L"FLAC_Quality");
		RegSetValueEx(hKey, L"FLAC_Quality", 0, REG_DWORD, (LPBYTE) &EncSettings.FLAC_EncodingQuality, sizeof(DWORD));
	}
	else if (EncSettings.FLAC_EncodingQuality > FLAC_MAXENCODINGQUALITY) EncSettings.FLAC_EncodingQuality = FLAC_MAXENCODINGQUALITY; // check the loaded value if it is below the max setting
	
	// load FLAC verify setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"FLAC_Verify", 0, &type, (LPBYTE)&temp, &cb);
	if((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.FLAC_Verify = FLAC_VERIFY;
		if (EncSettings.FLAC_Verify == true) temp = 1;
		else temp = 0;
		RegDeleteValue(hKey, L"FLAC_Verify");
		RegSetValueEx(hKey, L"FLAC_Verify", 0, REG_DWORD, (LPBYTE) &temp, sizeof(DWORD));
	}
	if (temp == 0) EncSettings.FLAC_Verify = false;
	else EncSettings.FLAC_Verify = true;

	// load FLAC MD5 check setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"FLAC_MD5check", 0, &type, (LPBYTE)&temp, &cb);
	if((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.FLAC_MD5check = FLAC_MD5CHECK;
		if (EncSettings.FLAC_MD5check == true) temp = 1;
		else temp = 0;
		RegDeleteValue(hKey, L"FLAC_MD5check");
		RegSetValueEx(hKey, L"FLAC_MD5check", 0, REG_DWORD, (LPBYTE) &temp, sizeof(DWORD));
	}
	if (temp == 0) EncSettings.FLAC_MD5check = false;
	else EncSettings.FLAC_MD5check = true;

	// load MP3 internal encoder quality setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"MP3_Int_Quality", 0, &type, (LPBYTE)&EncSettings.LAME_InternalEncodingQuality, &cb);
	if ((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.LAME_InternalEncodingQuality = LAME_INTERNALQUALITY;
		RegDeleteValue(hKey, L"MP3_Int_Quality");
		RegSetValueEx(hKey, L"MP3_Int_Quality", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_InternalEncodingQuality, sizeof(DWORD));
	}
	else if (EncSettings.LAME_InternalEncodingQuality > LAME_MAXINTERNALQUALITY) EncSettings.LAME_InternalEncodingQuality = LAME_MAXINTERNALQUALITY; // check the loaded value if it is below the max setting
	
	// load MP3 encoding type (CBR or VBR)
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"MP3_Enc_Type", 0, &type, (LPBYTE)&EncSettings.LAME_EncodingMode, &cb);
	if ((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.LAME_EncodingMode = LAME_ENCTYPE;
		RegDeleteValue(hKey, L"MP3_Enc_Type");
		RegSetValueEx(hKey, L"MP3_Enc_Type", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_EncodingMode, sizeof(DWORD));
	}
	else if (EncSettings.LAME_EncodingMode<0 && LAME_ENCTYPE>1) EncSettings.LAME_EncodingMode = LAME_ENCTYPE;

	// load MP3 encoder CBR bitrate setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"MP3_CBR_Bitrate", 0, &type, (LPBYTE)&EncSettings.LAME_CBRBitrate, &cb);
	if ((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.LAME_CBRBitrate = LAME_CBRBITRATE;
		RegDeleteValue(hKey, L"MP3_CBR_Bitrate");
		RegSetValueEx(hKey, L"MP3_CBR_Bitrate", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_CBRBitrate, sizeof(DWORD));
	}
	else if (EncSettings.LAME_CBRBitrate > LAME_CBRBITRATES_QUANTITY) EncSettings.LAME_CBRBitrate = LAME_CBRBITRATES_QUANTITY; // check the loaded value if it is below the max setting

	// load MP3 encoder VBR quality setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"MP3_VBR_Quality", 0, &type, (LPBYTE)&EncSettings.LAME_VBRQuality, &cb);
	if ((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.LAME_VBRQuality = LAME_VBRQUALITY;
		RegDeleteValue(hKey, L"MP3_VBR_Quality");
		RegSetValueEx(hKey, L"MP3_VBR_Quality", 0, REG_DWORD, (LPBYTE)&EncSettings.LAME_VBRQuality, sizeof(DWORD));
	}
	else if (EncSettings.LAME_VBRQuality> LAME_MAXVBRQUALITY) EncSettings.LAME_VBRQuality = LAME_MAXVBRQUALITY; // check the loaded value if it is below the max setting

	// load output type setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"OUT_Type", 0, &type, (LPBYTE)&EncSettings.enOutType, &cb);
	if ((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		EncSettings.enOutType = TYPE_AUTO;
		RegDeleteValue(hKey, L"OUT_Type");
		RegSetValueEx(hKey, L"OUT_Type", 0, REG_DWORD, (LPBYTE)&EncSettings.enOutType, sizeof(DWORD));
	}
	else if (EncSettings.enOutType < TYPE_AUTO || EncSettings.enOutType > TYPE_WAV) // check the loaded value
	{
		EncSettings.enOutType = TYPE_AUTO;
	}

	// load thread number setting
	cb = sizeof(DWORD);
	err = RegQueryValueEx(hKey, L"OUT_Threads", 0, &type, (LPBYTE)&EncSettings.OUT_Threads, &cb);
	if ((err != ERROR_SUCCESS) || (type != REG_DWORD))
	{
		// create the registry entry if it is missing or has a different type
		// Get the number of physical CPU cores
		DWORD cores = GetPhysicalCoreCount();
#if 0	
		// CPUコア数を80%に制限する
		DWORD autoThreads = (DWORD)(cores * 0.8);
#else
		// CPUコア数をスレッド数に設定する
		DWORD autoThreads = cores;
#endif
		// 最低は OUT_THREADS（=1）
		if (autoThreads < OUT_THREADS) autoThreads = OUT_THREADS;

		// 最大は MAX_THREADS（=16）
		if (autoThreads > MAX_THREADS) autoThreads = MAX_THREADS;

		EncSettings.OUT_Threads = autoThreads;
		RegDeleteValue(hKey, L"OUT_Threads");
		RegSetValueEx(hKey, L"OUT_Threads", 0, REG_DWORD, (LPBYTE)&EncSettings.OUT_Threads, sizeof(DWORD));
	}
	// check the loaded value
	if (EncSettings.OUT_Threads < 1) EncSettings.OUT_Threads = 1;
	if (EncSettings.OUT_Threads > MAX_THREADS) EncSettings.OUT_Threads = MAX_THREADS;

	RegCloseKey(hKey);
	return 0;
}

// Write Window Position
int WriteWindowPos(HWND hWnd)
{
	HKEY hKey;
	DWORD err;

	err = RegCreateKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\FLACdrop", 0, NULL,
						 REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_SET_VALUE,
						 NULL, &hKey, NULL);

	if (err != ERROR_SUCCESS)
		return FAIL_REGISTRY_OPEN;

	RECT rc;
	GetWindowRect(hWnd, &rc);

	RegSetValueEx(hKey, L"WindowPosX", 0, REG_DWORD, (BYTE*)&rc.left, sizeof(DWORD));
	RegSetValueEx(hKey, L"WindowPosY", 0, REG_DWORD, (BYTE*)&rc.top, sizeof(DWORD));

	RegCloseKey(hKey);
	return 0;
}

// Read Window Position
int ReadWindowPos(HWND hWnd)
{
	HKEY hKey;
	DWORD cb, type;
	int xPos, yPos;

	// ウィンドウの実サイズを取得
	RECT win;
	GetWindowRect(hWnd, &win);
	int winWidth  = win.right  - win.left;
	int winHeight = win.bottom - win.top;

	// -----------------------------
	// １． レジストリが無い → 画面中央
	// -----------------------------
	if (RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\FLACdrop", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
	{
		CenterWindowOnPrimaryMonitor(hWnd);
		return 0;
	}

	// -----------------------------
	// ２． レジストリから位置読み込み
	// -----------------------------
	cb = sizeof(DWORD);
	RegQueryValueEx(hKey, L"WindowPosX", 0, &type, (BYTE*)&xPos, &cb);
	RegQueryValueEx(hKey, L"WindowPosY", 0, &type, (BYTE*)&yPos, &cb);

	RegCloseKey(hKey);

	// 仮位置に移動してモニタ判定
	SetWindowPos(hWnd, NULL, xPos, yPos, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

	// -----------------------------
	// ３． 範囲外判定
	// -----------------------------
	HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONULL);

	if (hMon == NULL)
	{
		// -----------------------------
		// ４． 範囲外 → メインモニタ中央
		// -----------------------------
		CenterWindowOnPrimaryMonitor(hWnd);
		return 0;
	}

	// -----------------------------
	// ５． 範囲内 → レジストリ位置を使用
	// -----------------------------
	SetWindowPos(hWnd, NULL, xPos, yPos, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	return 0;
}

// メインモニタ中央
void CenterWindowOnPrimaryMonitor(HWND hWnd)
{
	RECT rcWork;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);

	RECT win;
	GetWindowRect(hWnd, &win);

	int winWidth  = win.right  - win.left;
	int winHeight = win.bottom - win.top;

	int cx = rcWork.left + (rcWork.right - rcWork.left - winWidth)	/ 2;
	int cy = rcWork.top  + (rcWork.bottom - rcWork.top - winHeight) / 2;

	SetWindowPos(hWnd, NULL, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

// Get the number of physical CPU cores
DWORD GetPhysicalCoreCount()
{
	DWORD len = 0;
	// First call to get the required buffer size
	GetLogicalProcessorInformation(NULL, &len);

	SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer =
		(SYSTEM_LOGICAL_PROCESSOR_INFORMATION*) new char[len];

	if (!buffer)
		// Memory allocation failed
		// CPU core count is returned as a DWORD, so we return 1 as a fallback
		return 1;

	// Second call to get the actual information
	if (GetLogicalProcessorInformation(buffer, &len) == FALSE)
	{
		delete[](char*)buffer;
		return 1;
	}

	// Calculate the number of entries in the buffer
	DWORD physicalCores = 0;
	int count = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

	// Count the number of physical cores
	for (int i = 0; i < count; i++)
	{
		if (buffer[i].Relationship == RelationProcessorCore)
			physicalCores++;
	}

	delete[](char*)buffer;

	// If no physical cores were found, return 1 as a fallback
	if (physicalCores == 0)
		return 1;

	// CPU core count is returned as a DWORD
	return physicalCores;
}

#include "stdafx.h"
#include <ShObjIdl.h>
#include <atlstr.h>
#include <process.h>
#include "FLACdrop.h"
#include "io.h"
#include "encoders.h"
#include "format.h"
#include "lame.h"


// Global variables defined in FLACdrop.cpp
extern sEncoderSettings EncSettings;					// Variable to store encoder settings
extern TCHAR *EventLogTXT;								// variable to store event log history

void ProcessCommandLineFiles(sUIParameters& ui);
void EnableUI(HWND hDlg, BOOL bEnable = TRUE);

//
//	FUNCTION:	About(HWND, UINT, WPARAM, LPARAM)
//
//	PURPOSE:	Message handler for about box.
//
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
		case WM_INITDIALOG:
		{
			// FLACdrop Kai File Version
			TCHAR szPath[MAX_PATH];
			GetModuleFileName(NULL, szPath, MAX_PATH);

			DWORD handle = 0;
			DWORD size = GetFileVersionInfoSize(szPath, &handle);

			CString verApp = _T("?.??");
			if (size > 0) {
				std::vector<BYTE> data(size);
				if (GetFileVersionInfo(szPath, handle, size, data.data())) {
					VS_FIXEDFILEINFO* pInfo = nullptr;
					UINT len = 0;
					if (VerQueryValue(data.data(), _T("\\"), (LPVOID*)&pInfo, &len)) {
						int major = HIWORD(pInfo->dwFileVersionMS);
						int minor = LOWORD(pInfo->dwFileVersionMS);
						int patch = HIWORD(pInfo->dwFileVersionLS);
						int build = LOWORD(pInfo->dwFileVersionLS);

						if (build == 0) {
							verApp.Format(_T("%d.%d%d"), major, minor, patch);
						} else {
							verApp.Format(_T("%d.%d%dÉ¿%d"), major, minor, patch, build);
						}
					}
				}
			}

			// FLAC Version
			CString verFLAC;
			verFLAC.Format(_T("%S"), FLAC__VERSION_STRING);   // const char* Å® CString

			// LAME Version
			const char* lameVer = get_lame_version();
			CString verLAME;
			verLAME.Format(_T("%S"), lameVer);

			// About Dialog Out String.
			CString about;
			about.Format(
				_T("FLACdrop Kai Ver %s\n")
				_T("Based on FLACdrop Ver 1.34\n\n")
				_T("Using libraries:\n")
				_T("- libFLAC Ver %s\n")
				_T("- libMP3lame(SSE2) Ver %s"),
				verApp.GetString(),
				verFLAC.GetString(),
				verLAME.GetString()
			);

			// Set IDC_STATIC control
			SetDlgItemText(hDlg, IDC_STATIC, about);

			return (INT_PTR)TRUE;
		}
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

//
//	FUNCTION:	Settings(HWND, UINT, WPARAM, LPARAM)
//
//	PURPOSE:	Message handler for settings box
//
INT_PTR CALLBACK Settings(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	HWND hDlgFLACQuality = GetDlgItem(hDlg, IDC_FLAC_QUALITY);
	HWND hDlgFLACQualityView = GetDlgItem(hDlg, IDC_VIEW_FLAC_QUALITY);
	HWND hDlgFLACVerify = GetDlgItem(hDlg, IDC_FLAC_VERIFY);
	HWND hDlgFLACMD5check = GetDlgItem(hDlg, IDC_FLAC_MD5CHECK);
	HWND hDlgMP3InternalQuality = GetDlgItem(hDlg, IDC_MP3_ENCQ);
	HWND hDlgMP3InternalQualityView = GetDlgItem(hDlg, IDC_VIEW_MP3_INTERNAL_QUALITY);
	HWND hDlgMP3CBRBitrate = GetDlgItem(hDlg, IDC_MP3_BITRATE);
	HWND hDlgMP3VBRQuality = GetDlgItem(hDlg, IDC_MP3_VBR_Q);
	HWND hDlgMP3VBRQualityView = GetDlgItem(hDlg, IDC_VIEW_MP3_VBR_QUALITY);
	HWND hDlgThreads = GetDlgItem(hDlg, IDC_THREADS);
	HWND hDlgThreadsView = GetDlgItem(hDlg, IDC_VIEW_THREADS_NUMBER);
	HWND hDlgStaticMaxThreads = GetDlgItem(hDlg, IDC_STATICMAXTHREAD);

	TCHAR AA[16];
	LRESULT result;

	switch (message)
	{
		case WM_INITDIALOG:
			// Setup FLAC encoding quality slider and number view
			SendMessage(hDlgFLACQuality, TBM_SETRANGE, false, MAKELONG(1, 8));
			SendMessage(hDlgFLACQuality, TBM_SETPOS, true, EncSettings.FLAC_EncodingQuality);
			_itow_s(EncSettings.FLAC_EncodingQuality, AA, sizeof(AA) / sizeof (TCHAR), 10);
			SendMessage(hDlgFLACQualityView, WM_SETTEXT, NULL, (LPARAM)AA);
			
			// Setup FLAC verify checkbox
			if(EncSettings.FLAC_Verify == true) SendMessage(hDlgFLACVerify, BM_SETCHECK, BST_CHECKED, 0);
			else SendMessage(hDlgFLACVerify, BM_SETCHECK, BST_UNCHECKED, 0);
			
			//Setup FLAC MD5 checkbox
			if(EncSettings.FLAC_MD5check == true) SendMessage(hDlgFLACMD5check, BM_SETCHECK, BST_CHECKED, 0);
			else SendMessage(hDlgFLACMD5check, BM_SETCHECK, BST_UNCHECKED, 0);

			// Setup LAME internal encoding quality slider and number view
			SendMessage(hDlgMP3InternalQuality, TBM_SETRANGE, false, MAKELONG(0, 9));
			SendMessage(hDlgMP3InternalQuality, TBM_SETPOS, true, EncSettings.LAME_InternalEncodingQuality);
			_itow_s(EncSettings.LAME_InternalEncodingQuality, AA, sizeof(AA) / sizeof(TCHAR), 10);
			SendMessage(hDlgMP3InternalQualityView, WM_SETTEXT, NULL, (LPARAM)AA);

			// Setup LAME VBR quality slider and number view
			SendMessage(hDlgMP3VBRQuality, TBM_SETRANGE, false, MAKELONG(0, 9));
			SendMessage(hDlgMP3VBRQuality, TBM_SETPOS, true, EncSettings.LAME_VBRQuality);
			_itow_s(EncSettings.LAME_VBRQuality, AA, sizeof(AA) / sizeof(TCHAR), 10);
			SendMessage(hDlgMP3VBRQualityView, WM_SETTEXT, NULL, (LPARAM)AA);

			// Fill up LAME combobox CBR bitrates
			memset(&AA, 0, sizeof(AA));
			for (int i = 0; i <= LAME_CBRBITRATES_QUANTITY; i++)
			{
				wcscpy_s(AA, sizeof(AA) / sizeof(TCHAR), (TCHAR*)LAME_CBRBITRATES_TEXT[i]);

				// Add string to combobox
				SendMessage(hDlgMP3CBRBitrate, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)AA);
			}
			// Send the CB_SETCURSEL message to display an initial item in the selection field
			SendMessage(hDlgMP3CBRBitrate, CB_SETCURSEL, (WPARAM)EncSettings.LAME_CBRBitrate, (LPARAM)0);

			// Setup LAME MP3 output type
			switch (EncSettings.LAME_EncodingMode)
			{
				case 0:
					CheckRadioButton(hDlg, IDC_CBR, IDC_VBR, IDC_CBR);
					break;
				case 1:
					CheckRadioButton(hDlg, IDC_CBR, IDC_VBR, IDC_VBR);
					break;
			}
			
			// Setup thread number slider and number view
			SendMessage(hDlgThreads, TBM_SETRANGE, false, MAKELONG(1, MAX_THREADS));
			SendMessage(hDlgThreads, TBM_SETPOS, true, EncSettings.OUT_Threads);
			_itow_s(EncSettings.OUT_Threads, AA, sizeof(AA) / sizeof(TCHAR), 10);
			SendMessage(hDlgThreadsView, WM_SETTEXT, NULL, (LPARAM)AA);
			_itow_s(MAX_THREADS, AA, sizeof(AA) / sizeof(TCHAR), 10);
			SendMessage(hDlgStaticMaxThreads, WM_SETTEXT, NULL, (LPARAM)AA);
			return (INT_PTR)TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case IDCANCEL:
					EndDialog(hDlg, LOWORD(wParam));
					return (INT_PTR)TRUE;

				case IDOK:
					EncSettings.FLAC_EncodingQuality = (int)SendMessage(hDlgFLACQuality, TBM_GETPOS, 0, 0);
					if(SendMessage(hDlgFLACVerify, BM_GETCHECK, 0, 0) == BST_CHECKED) EncSettings.FLAC_Verify = true;
					else EncSettings.FLAC_Verify = false;
					if(SendMessage(hDlgFLACMD5check, BM_GETCHECK, 0, 0) == BST_CHECKED) EncSettings.FLAC_MD5check = true;
					else EncSettings.FLAC_MD5check = false;
					EncSettings.LAME_InternalEncodingQuality = (int)SendMessage(hDlgMP3InternalQuality, TBM_GETPOS, 0, 0);
					EncSettings.LAME_CBRBitrate = (int)SendMessage(hDlgMP3CBRBitrate, CB_GETCURSEL, 0, 0);
					EncSettings.LAME_VBRQuality = (int)SendMessage(hDlgMP3VBRQuality, TBM_GETPOS, 0, 0);
					EncSettings.OUT_Threads = (int)SendMessage(hDlgThreads, TBM_GETPOS, 0, 0);

					// Read status of radio buttons
					if (IsDlgButtonChecked(hDlg, IDC_CBR) == BST_CHECKED) EncSettings.LAME_EncodingMode = 0;
					if (IsDlgButtonChecked(hDlg, IDC_VBR) == BST_CHECKED) EncSettings.LAME_EncodingMode = 1;

					EndDialog(hDlg, LOWORD(wParam));
					return (INT_PTR)TRUE;
			}
			break;
		
		case WM_HSCROLL:
			// one of the sliders got moved so we get the slider control positions and write the positions on the screen
			result = SendMessage(hDlgFLACQuality, TBM_GETPOS, 0, 0);
			_itow_s(result, AA, sizeof(AA) / sizeof(TCHAR), 10);
			SendMessage(hDlgFLACQualityView, WM_SETTEXT, NULL, (LPARAM)AA);
			
			result = SendMessage(hDlgMP3InternalQuality, TBM_GETPOS, 0, 0);
			_itow_s(result, AA, sizeof(AA) / sizeof(TCHAR), 10);
			SendMessage(hDlgMP3InternalQualityView, WM_SETTEXT, NULL, (LPARAM)AA);
			
			result = SendMessage(hDlgMP3VBRQuality, TBM_GETPOS, 0, 0);
			_itow_s(result, AA, sizeof(AA) / sizeof(TCHAR), 10);
			SendMessage(hDlgMP3VBRQualityView, WM_SETTEXT, NULL, (LPARAM)AA);
			
			result = SendMessage(hDlgThreads, TBM_GETPOS, 0, 0);
			_itow_s(result, AA, sizeof(AA) / sizeof(TCHAR), 10);
			SendMessage(hDlgThreadsView, WM_SETTEXT, NULL, (LPARAM)AA);
			
			return (INT_PTR)TRUE;
	}
	return (INT_PTR)FALSE;
}

//
//	FUNCTION:	EventLog(HWND, UINT, WPARAM, LPARAM)
//
//	PURPOSE:	Message handler for event log dialog
//
INT_PTR CALLBACK EventLog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	HWND hDlgLogWindow = GetDlgItem(hDlg, IDC_LOGWINDOW);
	WCHAR wcnull[] = L"\0";

	switch (message)
	{
		case WM_INITDIALOG:
			SendMessage(hDlgLogWindow, WM_SETTEXT, 0, (LPARAM)EventLogTXT);
			return (INT_PTR)TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
				case IDCANCEL:
					EndDialog(hDlg, LOWORD(wParam));
					return (INT_PTR)TRUE;

				case IDC_CLEAR:
					wcscpy_s(EventLogTXT, EVENTLOGSIZE, wcnull);
					SendMessage(hDlgLogWindow, WM_SETTEXT, 0, (LPARAM)EventLogTXT);
					break;
			}
			break;
	}
	return (INT_PTR)FALSE;
}

//
//	FUNCTION:	MainForm(HWND, UINT, WPARAM, LPARAM)
static LRESULT CALLBACK ProgressPercentSubclassProc(HWND progress, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR subclassId, DWORD_PTR refData)
{
	if (message == WM_APP_PROGRESS_PERCENT) {
		SetWindowSubclass(progress, ProgressPercentSubclassProc, subclassId, static_cast<DWORD_PTR>(static_cast<LONG_PTR>(wParam)));
		InvalidateRect(progress, nullptr, FALSE);
		return 0;
	}

	LRESULT result = DefSubclassProc(progress, message, wParam, lParam);
	if (message == WM_PAINT && static_cast<LONG_PTR>(refData) >= 0 && static_cast<LONG_PTR>(refData) <= 100) {
		HDC dc = GetDC(progress);
		if (dc != nullptr) {
			RECT rect;
			GetClientRect(progress, &rect);
			WCHAR text[8];
			swprintf_s(text, L"%d%%", static_cast<int>(refData));
			HFONT font = reinterpret_cast<HFONT>(SendMessage(progress, WM_GETFONT, 0, 0));
			HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
			int oldBkMode = SetBkMode(dc, TRANSPARENT);
			PBRANGE range = {};
			SendMessage(progress, PBM_GETRANGE, TRUE, reinterpret_cast<LPARAM>(&range));
			LRESULT position = SendMessage(progress, PBM_GETPOS, 0, 0);
			int width = rect.right - rect.left;
			int filledWidth = range.iHigh > range.iLow
				? static_cast<int>((static_cast<LONGLONG>(position - range.iLow) * width) / (range.iHigh - range.iLow))
				: 0;
			if (filledWidth < 0) filledWidth = 0;
			if (filledWidth > width) filledWidth = width;

			SetTextColor(dc, RGB(0, 0, 0));
			DrawTextW(dc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			int savedDc = SaveDC(dc);
			IntersectClipRect(dc, rect.left, rect.top, rect.left + filledWidth, rect.bottom);
			SetTextColor(dc, RGB(255, 255, 255));
			DrawTextW(dc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			RestoreDC(dc, savedDc);
			SetBkMode(dc, oldBkMode);
			if (oldFont) SelectObject(dc, oldFont);
			ReleaseDC(progress, dc);
		}
	}
	if (message == WM_NCDESTROY) RemoveWindowSubclass(progress, ProgressPercentSubclassProc, subclassId);
	return result;
}

static void InitializeProgressPercent(HWND progress)
{
	SetWindowSubclass(progress, ProgressPercentSubclassProc, 1, static_cast<DWORD_PTR>(static_cast<LONG_PTR>(-1)));
}

static const UINT_PTR ELAPSED_TIME_TIMER_ID = 1;

static void UpdateElapsedTime(HWND dialog, ULONGLONG startTick)
{
	ULONGLONG elapsedSeconds = (GetTickCount64() - startTick) / 1000;
	WCHAR text[64];
	swprintf_s(text, L"Overall progress: %02llu:%02llu:%02llu",
		elapsedSeconds / 3600, (elapsedSeconds / 60) % 60, elapsedSeconds % 60);

	SetDlgItemTextW(dialog, IDC_OVERALL_PROGRESS, text);
}

//
//	PURPOSE:	Handle the "drag and drop" audio files and pass them to the encoder scheduler
//
INT_PTR CALLBACK MainForm(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	static sUIParameters UIParameters;
	static ITaskbarList3* taskbarList = nullptr;
	static HWND taskbarWindow = nullptr;
	static UINT totalFiles = 0;
	static UINT completedFiles = 0;
	static ULONGLONG conversionStartTick = 0;
	static bool elapsedTimeTimerActive = false;
	int result;
	int argc = 0;

	switch (message)
	{
		// initialize the dialog box
		case WM_INITDIALOG:
			if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&taskbarList)))) {
				if (FAILED(taskbarList->HrInit())) {
					taskbarList->Release();
					taskbarList = nullptr;
				} else {
					taskbarWindow = GetAncestor(hDlg, GA_ROOT);
				}
			}

			UIParameters.MainThreadId = GetCurrentThreadId();
			UIParameters.hMainWnd = hDlg;
			UIParameters.EncoderInUse = FALSE;
			UIParameters.CancelRequested = FALSE;
			UIParameters.CloseWhenDone = FALSE;
			UIParameters.bCommandLineMode = FALSE;
			UIParameters.progress[0] = GetDlgItem(hDlg, IDC_PROGRESS0);
			UIParameters.progress[1] = GetDlgItem(hDlg, IDC_PROGRESS1);
			UIParameters.progress[2] = GetDlgItem(hDlg, IDC_PROGRESS2);
			UIParameters.progress[3] = GetDlgItem(hDlg, IDC_PROGRESS3);
			UIParameters.progress[4] = GetDlgItem(hDlg, IDC_PROGRESS4);
			UIParameters.progress[5] = GetDlgItem(hDlg, IDC_PROGRESS5);
			UIParameters.progress[6] = GetDlgItem(hDlg, IDC_PROGRESS6);
			UIParameters.progress[7] = GetDlgItem(hDlg, IDC_PROGRESS7);
			UIParameters.progress[8] = GetDlgItem(hDlg, IDC_PROGRESS8);
			UIParameters.progress[9] = GetDlgItem(hDlg, IDC_PROGRESS9);
			UIParameters.progress[10] = GetDlgItem(hDlg, IDC_PROGRESS10);
			UIParameters.progress[11] = GetDlgItem(hDlg, IDC_PROGRESS11);
			UIParameters.progress[12] = GetDlgItem(hDlg, IDC_PROGRESS12);
			UIParameters.progress[13] = GetDlgItem(hDlg, IDC_PROGRESS13);
			UIParameters.progress[14] = GetDlgItem(hDlg, IDC_PROGRESS14);
			UIParameters.progress[15] = GetDlgItem(hDlg, IDC_PROGRESS15);
			for (int i = 0; i < MAX_THREADS; ++i)
				InitializeProgressPercent(UIParameters.progress[i]);
			UIParameters.progresstotal = GetDlgItem(hDlg, IDC_PROGRESSTOTAL);
			InitializeProgressPercent(UIParameters.progresstotal);
			UIParameters.text = GetDlgItem(hDlg, IDC_MESSAGES);
			SendMessage(UIParameters.text, WM_SETTEXT, 0, (LPARAM)L"Waiting for audio files to be dropped...");
			
			result = ReadSettings();			// load the encoder settings from registry
			if (result != 0) SendMessage(UIParameters.text, WM_SETTEXT, 0, (LPARAM)ErrMessage[result]);

			// setup output type radio buttons
			switch (EncSettings.enOutType)
			{
				case TYPE_FLAC:
					CheckRadioButton(hDlg, IDC_RADIO_OUT_FLAC, IDC_RADIO_OUT_AUTO, IDC_RADIO_OUT_FLAC);
					break;

				case TYPE_MP3	:
					CheckRadioButton(hDlg, IDC_RADIO_OUT_FLAC, IDC_RADIO_OUT_AUTO, IDC_RADIO_OUT_MP3);
					break;

				case TYPE_WAV	:
					CheckRadioButton(hDlg, IDC_RADIO_OUT_FLAC, IDC_RADIO_OUT_AUTO, IDC_RADIO_OUT_WAV);
					break;

				case TYPE_AUTO:
					CheckRadioButton(hDlg, IDC_RADIO_OUT_FLAC, IDC_RADIO_OUT_AUTO, IDC_RADIO_OUT_AUTO);
					break;
				default:
					CheckRadioButton(hDlg, IDC_RADIO_OUT_FLAC, IDC_RADIO_OUT_AUTO, IDC_RADIO_OUT_AUTO);
					EncSettings.enOutType = TYPE_AUTO;
					break;
			}
			UIParameters.enOutType = EncSettings.enOutType;

			// Command Line?
			if (__argc > 1) {
				ProcessCommandLineFiles(UIParameters);	// process any files passed on the command line
			}

			return (INT_PTR)TRUE;

		// process the dropped files
		case WM_DROPFILES:
		{
			HDROP hDrop = (HDROP)wParam;
			if (InterlockedCompareExchange(&UIParameters.EncoderInUse, 0, 0) == 0 && !UIParameters.CloseWhenDone)
			{
				InterlockedExchange(&UIParameters.CancelRequested, FALSE);
				UIParameters.CloseWhenDone = FALSE;
				InterlockedExchange(&UIParameters.EncoderInUse, TRUE);
				UIParameters.bCommandLineMode = FALSE;

				// Get Drop file
				UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
				UIParameters.files.clear();
				UIParameters.files.reserve(count);

				for (UINT i = 0; i < count; i++) {
					WCHAR buf[MAXFILENAMELENGTH];
					DragQueryFile(hDrop, i, buf, MAXFILENAMELENGTH);
					UIParameters.files.emplace_back(buf);
				}

				DragFinish(hDrop);

				EnableUI(hDlg, FALSE);	// disable the UI while encoding is in progress

				_beginthreadex(
					NULL,
					0,
					EncoderScheduler,
					&UIParameters,
					0,
					NULL
				);
			} else {
				DragFinish(hDrop);
			}
		}
		break;

		case WM_COMMAND:
			switch (HIWORD(wParam))
			{
				// process the changes in the radio buttons
				case BN_CLICKED:
					if (IsDlgButtonChecked(hDlg, IDC_RADIO_OUT_FLAC) == BST_CHECKED) UIParameters.enOutType = TYPE_FLAC;
					if (IsDlgButtonChecked(hDlg, IDC_RADIO_OUT_MP3) == BST_CHECKED) UIParameters.enOutType = TYPE_MP3;
					if (IsDlgButtonChecked(hDlg, IDC_RADIO_OUT_WAV) == BST_CHECKED) UIParameters.enOutType = TYPE_WAV;
					if (IsDlgButtonChecked(hDlg, IDC_RADIO_OUT_AUTO) == BST_CHECKED) UIParameters.enOutType = TYPE_AUTO;

					EncSettings.enOutType = UIParameters.enOutType;

					break;
			}
			break;

		case WM_APP_START_CONVERSION:
			conversionStartTick = GetTickCount64();
			UpdateElapsedTime(hDlg, conversionStartTick);
			elapsedTimeTimerActive = SetTimer(hDlg, ELAPSED_TIME_TIMER_ID, 1000, nullptr) != 0;
			if (!elapsedTimeTimerActive)
				SetDlgItemTextW(hDlg, IDC_OVERALL_PROGRESS, L"Overall progress: unavailable");
			return (INT_PTR)TRUE;

		case WM_TIMER:
			if (wParam == ELAPSED_TIME_TIMER_ID && elapsedTimeTimerActive)
				UpdateElapsedTime(hDlg, conversionStartTick);
			return (INT_PTR)TRUE;

		case WM_USER_ENABLE_UI:
			if (elapsedTimeTimerActive) {
				KillTimer(hDlg, ELAPSED_TIME_TIMER_ID);
				UpdateElapsedTime(hDlg, conversionStartTick);
				elapsedTimeTimerActive = false;
			}
			EnableUI(hDlg, TRUE);
			if (UIParameters.CloseWhenDone) DestroyWindow(GetParent(hDlg));
			break;

		case WM_APP_REQUEST_CLOSE:
			if (InterlockedCompareExchange(&UIParameters.EncoderInUse, 0, 0) == 0) {
				DestroyWindow(GetParent(hDlg));
			} else if (InterlockedCompareExchange(&UIParameters.CancelRequested, 0, 0) == 0 &&
				MessageBoxW(hDlg, L"Cancel?", L"FLACdrop", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
				UIParameters.CloseWhenDone = TRUE;
				InterlockedExchange(&UIParameters.CancelRequested, TRUE);
			}
			return (INT_PTR)TRUE;

		case WM_APP_SET_TOTAL_PROGRESS:						// Initialize total and taskbar progress bars
			totalFiles = static_cast<UINT>(wParam);
			completedFiles = 0;

			// total progress bars
			SendMessage(UIParameters.progresstotal, PBM_SETRANGE32, 0, 100);
			SendMessage(UIParameters.progresstotal, PBM_SETPOS, 0, 0);
			SendMessage(UIParameters.progresstotal, WM_APP_PROGRESS_PERCENT, 0, 0);

			// taskbar progress bars
			if (taskbarList != NULL && totalFiles > 0) {
				if(totalFiles > 0) {
					taskbarList->SetProgressState(taskbarWindow, TBPF_NORMAL);
					taskbarList->SetProgressValue(taskbarWindow, 0, totalFiles);
				} else {
					taskbarList->SetProgressState(taskbarWindow, TBPF_NOPROGRESS);
				}
			}
			break;

		case WM_APP_INCREMENT_TOTAL_PROGRESS:				// // Increment total and taskbar progress bars
		{
			// total progress bars
			++completedFiles;
			UINT percent = totalFiles > 0 ? static_cast<UINT>((static_cast<ULONGLONG>(completedFiles) * 100) / totalFiles) : 0;
			if (percent > 100) percent = 100;
			SendMessage(UIParameters.progresstotal, PBM_SETPOS, percent, 0);
			SendMessage(UIParameters.progresstotal, WM_APP_PROGRESS_PERCENT, percent, 0);
			LRESULT position = completedFiles;

			// taskbar progress bars
			if (taskbarList && totalFiles > 0) {
				if (static_cast<UINT>(position) >= totalFiles) {
					taskbarList->SetProgressState(taskbarWindow, TBPF_NOPROGRESS);
				} else {
					taskbarList->SetProgressValue(taskbarWindow, position, totalFiles);
				}
			}
			break;
		}

		case WM_DESTROY:
			if (elapsedTimeTimerActive) {
				KillTimer(hDlg, ELAPSED_TIME_TIMER_ID);
				elapsedTimeTimerActive = false;
			}
			if (taskbarList) {
				taskbarList->Release();
				taskbarList = nullptr;
			}
			break;
	}
	return (INT_PTR)FALSE;
}

void EnableUI(HWND hDlg, BOOL bEnable)
{
	EnableWindow(GetDlgItem(hDlg, IDC_RADIO_OUT_FLAC), bEnable);
	EnableWindow(GetDlgItem(hDlg, IDC_RADIO_OUT_MP3), bEnable);
	EnableWindow(GetDlgItem(hDlg, IDC_RADIO_OUT_WAV), bEnable);
	EnableWindow(GetDlgItem(hDlg, IDC_RADIO_OUT_AUTO), bEnable);

	HMENU hMenu = GetMenu(GetParent(hDlg));
	EnableMenuItem(hMenu, IDM_OPTIONS, bEnable ? MF_ENABLED : MF_GRAYED);
	EnableMenuItem(hMenu, IDM_EVENTLOG, bEnable ? MF_ENABLED : MF_GRAYED);
	EnableMenuItem(hMenu, IDM_EXIT, bEnable ? MF_ENABLED : MF_GRAYED);
}

// Command line processing function to handle files passed as arguments
void ProcessCommandLineFiles(sUIParameters& ui)
{
	// Add command-line arguments to the file list
	ui.files.clear();

	for (int i = 1; i < __argc; ++i) {
		ui.files.emplace_back(__wargv[i]);
	}

	// If command-line files were provided, start encoding immediately
	if (!ui.files.empty()) {
		InterlockedExchange(&ui.CancelRequested, FALSE);
		ui.CloseWhenDone = FALSE;
		InterlockedExchange(&ui.EncoderInUse, TRUE);
		ui.bCommandLineMode = TRUE;
		ui.enOutType = TYPE_AUTO;

		EnableUI(ui.hMainWnd, FALSE);

		_beginthreadex(
			NULL,
			0,
			EncoderScheduler,
			&ui,
			0,
			NULL
		);

	}
}

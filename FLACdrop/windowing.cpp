#include "stdafx.h"
#include "FLACdrop.h"
#include "io.h"
#include "encoders.h"

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
		return (INT_PTR)TRUE;

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
//
//	PURPOSE:	Handle the "drag and drop" audio files and pass them to the encoder scheduler
//
INT_PTR CALLBACK MainForm(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	static sUIParameters UIParameters;
	int result;
	int argc = 0;

	switch (message)
	{
		// initialize the dialog box
		case WM_INITDIALOG:
			UIParameters.MainThreadId = GetCurrentThreadId();
			UIParameters.hMainWnd = hDlg;
			UIParameters.EncoderInUse = false;
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
			UIParameters.progresstotal = GetDlgItem(hDlg, IDC_PROGRESSTOTAL);
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
			}

			// Command Line?
			if (__argc > 1) {
				ProcessCommandLineFiles(UIParameters);	// process any files passed on the command line
			}

			return (INT_PTR)TRUE;

		// process the dropped files
		case WM_DROPFILES:
		{
			if (!UIParameters.EncoderInUse)
			{
				UIParameters.EncoderInUse = true;
				UIParameters.bCommandLineMode = FALSE;

				// Get Drop file
				HDROP hDrop = (HDROP)wParam;

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
				CreateThread(NULL, 0,
					(LPTHREAD_START_ROUTINE)&EncoderScheduler,
					&UIParameters, 0, NULL);
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

		case WM_USER_ENABLE_UI:
			EnableUI(hDlg, TRUE);
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

void ProcessCommandLineFiles(sUIParameters& ui)
{
	// Add command-line arguments to the file list
	ui.files.clear();

	for (int i = 1; i < __argc; ++i) {
		ui.files.emplace_back(__wargv[i]);
	}

	// If command-line files were provided, start encoding immediately
	if (!ui.files.empty()) {
		ui.EncoderInUse = true;
		ui.bCommandLineMode = TRUE;
		ui.enOutType = TYPE_AUTO;

		EnableUI(ui.hMainWnd, FALSE);
		CreateThread(
			NULL,
			0,
			(LPTHREAD_START_ROUTINE)&EncoderScheduler,
			&ui,
			0,
			NULL
		);
	}
}

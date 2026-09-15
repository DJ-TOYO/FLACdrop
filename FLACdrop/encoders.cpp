#include "stdafx.h"
#include <process.h>
#include "FLACdrop.h"
#include "encoders.h"
#include "lame.h"
#include "libFLAC_callbacks.h"
#include "encoderFlac.h"
#include "encoderWav.h"
#include "encoderMp3.h"

extern sEncoderSettings EncSettings;			// variable to store encoder settings
extern TCHAR *EventLogTXT;						// variable to store event log history
HANDLE ghSemaphore;								// handle for the semaphore

unsigned __stdcall EncoderFunctionExecThread(LPVOID p);
void ExitEncThread(int ExitCode, HANDLE Semaphore, HWND progresstotal, WCHAR *filename, int type);

//
//	FUNCTION: SearchFreeThread()
//
//	PURPOSE: Search for a free slot in the thread data block
//
int SearchFreeThread(sEncodingParameters EncParams[])
{
	int i = 0;

	while (EncParams[i].ThreadInUse == true) i++;

	return i;
}

//
//	FUNCTION:	unsigned __stdcall EncoderScheduler(sUIParameters* )
//
//	PURPOSE:	Collects the dropped files list and schedules the encoding threads
//
ENC_FUNC wavTable[] = {
	Encode_WAV2FLAC,   // TYPE_AUTO → FLAC
	Encode_WAV2FLAC,   // TYPE_FLAC (this case does not occur: WAV -> FLAC is forced)
	Encode_WAV2MP3,    // TYPE_MP3
	Encode_WAV2FLAC    // TYPE_WAV (this case does not occur: WAV -> WAV is not meaningful, so convert to FLAC)
};

ENC_FUNC flacTable[] = {
	Encode_FLAC2WAV,   // TYPE_AUTO -> WAV
	Encode_FLAC2MP3,   // TYPE_FLAC (this case does not occur: FLAC -> FLAC is not meaningful, so convert to MP3)
	Encode_FLAC2MP3,   // TYPE_MP3
	Encode_FLAC2WAV    // TYPE_WAV
};

unsigned __stdcall EncoderScheduler(LPVOID params)
{
	UINT NumFiles;
	static HANDLE aThread[MAX_THREADS];																// array for the thread identifiers
	sUIParameters *myparams =(sUIParameters*)params;												// load the parameter list to the internal structure
	static sEncodingParameters EncParams[MAX_THREADS];												// parameter list for each encoding thread
	WCHAR Filename[MAXFILENAMELENGTH];
	WCHAR *FilenameExt;
	int tID = 0;																					// actual thread id
	bool ThreadStarted;																				// was a thread started within the loop?
	UINT startedThreads = 0;
	HANDLE waitHandles[MAX_THREADS] = { 0 };

	NumFiles = (UINT)myparams->files.size();														// get the number of files stored in the vector
	SendMessage(myparams->progresstotal, PBM_SETPOS, 0, 0);											// reset the total progress bar
	for (int i = 0; i<MAX_THREADS; i++) SendMessage(myparams->progress[i], PBM_SETPOS, 0, 0);		// reset each thread's progress bar
	SendMessage(myparams->progresstotal, PBM_SETRANGE, 0, MAKELONG(0, NumFiles));					// set the total progress bar boundaries
	SendMessage(myparams->text, WM_SETTEXT, 0, (LPARAM)L"Converting is in progress");

	// number of the semaphore is the number of threads we would like to use in parallel
	// if the number of files are less than the number of available threads then we have to create the semaphore for the number of files
	if (EncSettings.OUT_Threads > NumFiles) ghSemaphore = CreateSemaphore(NULL, NumFiles - 1, MAX_THREADS, NULL);
	else ghSemaphore = CreateSemaphore(NULL, EncSettings.OUT_Threads-1, MAX_THREADS, NULL);
	
	// setup the variales
	for (int i=0; i<MAX_THREADS; i++) EncParams[i].ThreadInUse = false;								// reset the thread status flags
	for (int i=0; i<MAX_THREADS; i++) EncParams[i].text = myparams->text;
	for (int i=0; i<MAX_THREADS; i++) EncParams[i].progresstotal = myparams->progresstotal;

	for(UINT i=0; i < NumFiles; i++)
	{
		ThreadStarted = false;
		wcscpy_s(Filename, MAXFILENAMELENGTH, myparams->files[i].c_str());							// get files name

		// check if the extension in the filename is WAV or FLAC and start encoder algorithm accordingly
		wchar_t ch=L'.';
		FilenameExt = wcsrchr(Filename, ch);	// search for the last "." in the filename

		// 拡張子判定
		bool isWav = (_wcsnicmp(FilenameExt, L".wav", 4) == 0);
		bool isFlac = (_wcsnicmp(FilenameExt, L".flac", 5) == 0);

		ENC_FUNC func = nullptr;

		// 同じ形式なら無視する
		if ((isWav && myparams->enOutType == TYPE_WAV) ||
			(isFlac && myparams->enOutType == TYPE_FLAC))
		{
			// 進捗バーだけ進める
			SendMessage(myparams->progresstotal, PBM_DELTAPOS, 1, 0);
			continue;
		}

		if (isWav) {
			func = wavTable[myparams->enOutType];
		}
		else if (isFlac) {
			func = flacTable[myparams->enOutType];
		}

		if (func) {
			// Thread
			tID = SearchFreeThread(EncParams);
			EncParams[tID].ThreadInUse = true;
			EncParams[tID].progress = myparams->progress[tID];
			wcscpy_s(EncParams[tID].filename, MAXFILENAMELENGTH, Filename);
			EncParams[tID].func = func;
			EncParams[tID].OutputType = OUT_TYPE_UNKNOWN;
			EncParams[tID].ExitCode = 0;

			uintptr_t h = _beginthreadex(
				NULL,				 // security
				0,					 // stack size
				EncoderFunctionExecThread, // thread function
				&EncParams[tID],	 // argument
				0,					 // start immediately
				NULL				 // thread ID (unused)
			);

			aThread[tID] = (HANDLE)h;
			waitHandles[startedThreads] = (HANDLE)h;
			startedThreads ++;

			ThreadStarted = true;
		}

		if (ThreadStarted == true) WaitForSingleObject(ghSemaphore, INFINITE);	// a thread was started so we have to decrease the count of the semaphore with one, continue only if at least one thread is free
		else SendMessage(myparams->progresstotal, PBM_DELTAPOS, 1, 0);			// no thread was started (file extension was not recognized), but we still have to increase the total progress bar
	}

	// wait for all threads to terminate
	WaitForMultipleObjects(startedThreads, waitHandles, TRUE, INFINITE);

	myparams->EncoderInUse = false;

	CloseHandle(ghSemaphore);

	for (UINT i = 0; i < startedThreads; i++) {
		if(waitHandles[i] != NULL) {
			CloseHandle(waitHandles[i]);
		}
	}

	for (int i = 0; i < MAX_THREADS; i++) {
		aThread[i] = NULL;
	}

	SendMessage(myparams->text, WM_SETTEXT, 0, (LPARAM)L"Waiting for audio files to be dropped...");

	// Commando Line mode? If yes, then exit the application after processing all files
	if (myparams->bCommandLineMode) {
		// Exit the application
		PostThreadMessage(myparams->MainThreadId, WM_QUIT, 0, 0);
	}

	// Post Message UI Enable
	PostMessage(myparams->hMainWnd, WM_USER_ENABLE_UI, 0, 0);

	return ALL_OK;		// only to prevent compiler warning message
}

//
//	FUNCTION:	unsigned __stdcall EncoderFunctionExecThread(LPVOID p)
//
//	PURPOSE:	the encoder function thread run.
//
unsigned __stdcall EncoderFunctionExecThread(LPVOID p)
{
	sEncodingParameters* prm = (sEncodingParameters*)p;

	// call encoder function
	DWORD exitCode = prm->func(prm);

	// store exit code
	prm->ExitCode = exitCode;

	// unified thread exit handler
	ExitEncThread(exitCode,
				  ghSemaphore,
				  prm->progresstotal,
				  prm->filename,
				  prm->OutputType);

	return exitCode;
}

//
//	FUNCTION:	ExitEncThread(int, HANDLE, HWND)
//
//	PURPOSE:	Exits the encoder thread, releases the semaphore and updates event log
//
void ExitEncThread(int ExitCode, HANDLE Semaphore, HWND progresstotal, WCHAR *filename, int type)
{
	WCHAR rn[] = L"\r\n";

	static const WCHAR* OutputTypeText[] = {
		L"Output type: UNKNOWN\r\n",	// OUT_TYPE_UNKNOWN = 0
		L"Output type: FLAC\r\n",		// OUT_TYPE_FLAC = 1
		L"Output type: MP3\r\n",		// OUT_TYPE_MP3  = 2
		L"Output type: WAV\r\n" 		// OUT_TYPE_WAV  = 3
	};

	wcscat_s(EventLogTXT, EVENTLOGSIZE, filename);
	wcscat_s(EventLogTXT, EVENTLOGSIZE, rn);

	int idx = type;
	switch(type){
		case OUT_TYPE_UNKNOWN:
		case OUT_TYPE_FLAC:
		case OUT_TYPE_MP3:
		case OUT_TYPE_WAV:
			break;
		default:
			idx = OUT_TYPE_UNKNOWN;
			break;
	}

	wcscat_s(EventLogTXT, EVENTLOGSIZE, OutputTypeText[idx]);
	wcscat_s(EventLogTXT, EVENTLOGSIZE, ErrMessage[ExitCode]);
	wcscat_s(EventLogTXT, EVENTLOGSIZE, rn);

	SendMessage(progresstotal, PBM_DELTAPOS, 1, 0);
	ReleaseSemaphore(Semaphore, 1, NULL);
}

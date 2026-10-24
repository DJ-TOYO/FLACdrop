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

// Initialize total and taskbar progress bars
void InitializeTotalProgress(HWND progressTotal, UINT total)
{
	SendMessage(GetParent(progressTotal), WM_APP_SET_TOTAL_PROGRESS, total, 0);
}

// Increment total and taskbar progress bars
void IncrementTotalProgress(HWND progressTotal)
{
	SendMessage(GetParent(progressTotal), WM_APP_INCREMENT_TOTAL_PROGRESS, 0, 0);
}

//
//	FUNCTION: SearchFreeThread()
//
//	PURPOSE: Search for a free slot in the thread data block
//
int SearchFreeThread(sEncodingParameters EncParams[])
{
	while (1) {
		for (int i = 0; i < MAX_THREADS; i++) {
			if (!EncParams[i].ThreadInUse)
				return i;
		}
		// 空きがないので、誰かが ReleaseSemaphore するまで待つ ※基本的にはここに来ない保険
		WaitForSingleObject(ghSemaphore, INFINITE);
	}
}

//
//	FUNCTION:	unsigned __stdcall EncoderScheduler(sUIParameters* )
//
//	PURPOSE:	Collects the dropped files list and schedules the encoding threads
//
const ENC_FUNC wavTable[] = {
	Encode_WAV2FLAC,	// TYPE_AUTO → FLAC
	Encode_WAV2FLAC,	// TYPE_FLAC (this case does not occur: WAV -> FLAC is forced)
	Encode_WAV2MP3,		// TYPE_MP3
	NULL,				// TYPE_WAV (this case does not occur: WAV -> WAV is not meaningful)
};

const ENC_FUNC flacTable[] = {
	Encode_FLAC2WAV,	// TYPE_AUTO -> WAV
	NULL,				// TYPE_FLAC (this case does not occur: FLAC -> FLAC is not meaningful)
	Encode_FLAC2MP3,	// TYPE_MP3
	Encode_FLAC2WAV,	// TYPE_WAV
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
	UINT startedThreads = 0;

	NumFiles = (UINT)myparams->files.size();														// get the number of files stored in the vector
	if (NumFiles == 0){
		return 0;   // nothing to do
	}
	HANDLE* waitHandles = new HANDLE[NumFiles];														// 作られた全てのスレッド数を管理する
	for (UINT i = 0; i < NumFiles; i++){
		waitHandles[i] = NULL;
	}

	InitializeTotalProgress(myparams->progresstotal, NumFiles);											// reset the total progress bar
	for (int i = 0; i<MAX_THREADS; i++) SendMessage(myparams->progress[i], PBM_SETPOS, 0, 0);		// reset each thread's progress bar
	SendMessage(myparams->text, WM_SETTEXT, 0, (LPARAM)L"Converting is in progress");

	// The initial count of the semaphore is (number of threads to run in parallel - 1).
	// Since the scheduler always waits until a thread finishes, no adjustment based on the number of files is required.
	ghSemaphore = CreateSemaphore(NULL, EncSettings.OUT_Threads - 1, MAX_THREADS, NULL);

	// setup the variales
	for (int i=0; i<MAX_THREADS; i++) EncParams[i].ThreadInUse = false;								// reset the thread status flags
	for (int i=0; i<MAX_THREADS; i++) EncParams[i].text = myparams->text;
	for (int i=0; i<MAX_THREADS; i++) EncParams[i].progresstotal = myparams->progresstotal;
	for (int i=0; i<MAX_THREADS; i++) EncParams[i].cancelRequested = &myparams->CancelRequested;

	for(UINT i=0; i < NumFiles; i++)
	{
		if (EncParams[0].IsCancellationRequested()) break;
		wcscpy_s(Filename, MAXFILENAMELENGTH, myparams->files[i].c_str());							// get files name

		// check if the extension in the filename is WAV or FLAC and start encoder algorithm accordingly
		wchar_t ch=L'.';
		FilenameExt = wcsrchr(Filename, ch);	// search for the last "." in the filename

		// 拡張子判定
		bool isWav = (FilenameExt && _wcsnicmp(FilenameExt, L".wav", 4) == 0);
		bool isFlac = (FilenameExt && _wcsnicmp(FilenameExt, L".flac", 5) == 0);

		ENC_FUNC func = NULL;

		if (isWav) {
			func = wavTable[myparams->enOutType];
		}
		else if (isFlac) {
			func = flacTable[myparams->enOutType];
		}

		if (func) {
			// Convert Thread
			tID = SearchFreeThread(EncParams);
			if (EncParams[0].IsCancellationRequested()) break;
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

			// a thread was started so we have to decrease the count of the semaphore with one, continue only if at least one thread is free
			WaitForSingleObject(ghSemaphore, INFINITE);
		} else {
			// no thread was started (file extension was not recognized), but we still have to increase the total progress bar
			IncrementTotalProgress(myparams->progresstotal);
		}
	}

	// wait for all threads to terminate
	if (startedThreads > 0) {
		WaitForMultipleObjects(startedThreads, waitHandles, TRUE, INFINITE);
		for (UINT i = 0; i < startedThreads; ++i) CloseHandle(waitHandles[i]);
	}

	InterlockedExchange(&myparams->EncoderInUse, FALSE);

	CloseHandle(ghSemaphore);

	delete[] waitHandles;

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

	prm->ThreadInUse = false;

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

	IncrementTotalProgress(progresstotal);
	ReleaseSemaphore(Semaphore, 1, NULL);
}

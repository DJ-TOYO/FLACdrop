#include "stdafx.h"
#include "FLACdrop.h"
#include "encoders.h"
#include "lame.h"
#include "libFLAC_callbacks.h"

//
//	FUNCTION:	Encode_FLAC2WAV(sEncodingParameters* )
//
//	PURPOSE:	Encode FLAC stream to WAV stream
//
DWORD WINAPI Encode_FLAC2WAV(LPVOID params)
{
	sEncodingParameters* myparams = (sEncodingParameters*)params;
	FLAC__StreamDecoder* decoder = 0;
	FILE* fin, * fout;
	int err = 0;
	sClientData ClientData;

	// libFLAC: allocate the decoder
	{
		if ((decoder = FLAC__stream_decoder_new()) == NULL)
		{
			err = FAIL_LIBFLAC_ALLOC;
		}
	}

	// libFLAC: set the decoder parameters
	if (err == ALL_OK)
	{
		FLAC__stream_decoder_set_md5_checking(decoder, EncSettings.FLAC_MD5check);
	}

	// libFLAC: open the input file
	if (err == ALL_OK)
	{
		if ((_wfopen_s(&fin, myparams->filename, L"r+b")) != NULL)
		{
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_FILE_OPEN;
		}
		else ClientData.fin = fin;
	}

	// libFLAC: initialize decoder
	if (err == ALL_OK)
	{
		if (FLAC__stream_decoder_init_stream(decoder, read_callback_2WAV, seek_callback_2WAV, tell_callback_2WAV, length_callback_2WAV, eof_callback_2WAV, write_callback_2WAV, metadata_callback_2WAV, error_callback_2WAV, &ClientData) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_ALLOC;
		}
	}

	// libFLAC: check the FLAC file parameters
	if (err == ALL_OK)
	{
		// check if the first block really contained the metadata
		if (FLAC__stream_decoder_process_until_end_of_metadata(decoder) == FALSE)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_BAD_HEADER;
		}
	}
	if (err == ALL_OK)
	{
		// check if FLAC file has 16 or 24 bit resolution
		switch (ClientData.bps)
		{
		case 16:
		case 24:
			break;
		default:
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_ONLY_16_24_BIT;
		}
	}
	if (err == ALL_OK)
	{
		// check if total_samples count is in the STREAMINFO
		if (ClientData.total_samples == 0)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_BAD_HEADER;
		}
	}

	// libFLAC: open the output file
	if (err == ALL_OK)
	{
		WCHAR* OutFileName;

		OutFileName = new WCHAR[MAXFILENAMELENGTH];
		wcsncpy_s(OutFileName, MAXFILENAMELENGTH, myparams->filename, wcsnlen(myparams->filename, MAXFILENAMELENGTH) - 4);	// leave out the "flac" from the end
		wcscat_s(OutFileName, MAXFILENAMELENGTH, L"wav");
		if ((_wfopen_s(&fout, OutFileName, L"w+b")) != NULL)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_FILE_OPEN;
		}
		ClientData.fout = fout;

		delete[]OutFileName;
	}

	// libFLAC: start the decoding
	int blocks = (ClientData.total_samples + ClientData.blocksize - 1) / ClientData.blocksize;
	int processed = 0;
	int percent;			// current progress value
	int last_percent = -1;	// last progress value

	if (err == ALL_OK)
	{
		FLAC__bool ok = TRUE;
		FLAC__StreamDecoderState state;

		// set up the progress bar boundaries, block size is around 4k depending on resolution
		SendMessage(myparams->progress, PBM_SETRANGE, 0, MAKELONG(0, 100));
		SendMessage(myparams->progress, PBM_SETPOS, 0, 0);

		// loop the decoder until it reaches the end of the input file or returns an error
		do
		{
			ok = FLAC__stream_decoder_process_single(decoder);
			state = FLAC__stream_decoder_get_state(decoder);

			percent = (processed * 100) / blocks;
			if (percent != last_percent) {
				last_percent = percent;
				SendMessage(myparams->progress, PBM_SETPOS, percent, 0);
#if _DEBUG
				WCHAR dbg[128];
				swprintf_s(dbg, L"FLAC2WAV percent = %d / %d (processed=%d, blocks=%d)\n",
					percent, 100, processed, blocks);
				OutputDebugString(dbg);
#endif
			}

			// count up
			processed ++;

		} while ((state != FLAC__STREAM_DECODER_END_OF_STREAM && FLAC__STREAM_DECODER_SEEK_ERROR && FLAC__STREAM_DECODER_ABORTED && FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR) && ok == TRUE);

		// 100%
		SendMessage(myparams->progress, PBM_SETPOS, 100, 0);
	}

	// libFLAC: close the decoder
	if (err == ALL_OK)
	{
		switch (FLAC__stream_decoder_get_state(decoder))
		{
		case FLAC__STREAM_DECODER_SEEK_ERROR:
		case FLAC__STREAM_DECODER_ABORTED:
		case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
			fclose(fout);
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_ENCODE;
			break;
		default:
			break;
		}
	}
	if (err == ALL_OK)
	{
		if (FLAC__stream_decoder_finish(decoder) == FALSE) err = WARN_LIBFLAC_MD5;
		fclose(fout);
		fclose(fin);
		FLAC__stream_decoder_delete(decoder);
	}

	myparams->ThreadInUse = false;
	ExitEncThread(err, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_WAV);
	return ALL_OK;
}

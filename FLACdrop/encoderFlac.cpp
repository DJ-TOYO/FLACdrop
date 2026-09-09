#include "stdafx.h"
#include "FLACdrop.h"
#include "encoders.h"
#include "RiffParser.h"
#include "lame.h"
#include "libFLAC_callbacks.h"

//
//	FUNCTION:	Encode_WAV2FLAC(sEncodingParameters* )
//
//	PURPOSE:	Encode WAV stream to FLAC stream
//
DWORD WINAPI Encode_WAV2FLAC(LPVOID params)
{
	sEncodingParameters *myparams = (sEncodingParameters*)params;
	FLAC__StreamEncoder *encoder = NULL;
	unsigned int total_samples = 0;		// can use a 32-bit number due to WAV file size limitations in specification
	FILE* fin, * fout;
	int err = 0;

	sWAVEheader WAVEheader;
	sFMTheader FMTheader;
	sDATAheader DATAheader;
	sClientData ClientData;

	// WAV: open file
	{
		if ((_wfopen_s(&fin, myparams->filename, L"rb")) != NULL)
			err = FAIL_FILE_OPEN;
	}

	// Wav Analyse
	if (err == ALL_OK) {
		if (!ParseWavFile(fin, &WAVEheader, &FMTheader, &DATAheader, &total_samples)) {
			fclose(fin);
			err = FAIL_WAV_BAD_HEADER;
		}
	}

	// Wav Check
	bool is_float = false;

	if (FMTheader.AudioFormat == WAVE_FORMAT_PCM)
	{
		is_float = false;
	}
	else if (FMTheader.AudioFormat == WAVE_FORMAT_IEEE_FLOAT)
	{
		is_float = true;
	}
	else if (FMTheader.AudioFormat == WAVE_FORMAT_EXTENSIBLE)
	{
		if (FMTheader.SubFormat_AudioFormat == WAVE_FORMAT_PCM)
			is_float = false;
		else if (FMTheader.SubFormat_AudioFormat == WAVE_FORMAT_IEEE_FLOAT)
			is_float = true;
		else
			err = FAIL_WAV_UNSUPPORTED;
	}
	else
	{
		err = FAIL_WAV_UNSUPPORTED;
	}

	// libFLAC: allocate the libFLAC encoder and data buffers
	if (err == ALL_OK)
	{
		if ((encoder = FLAC__stream_encoder_new()) == NULL)
		{
			fclose(fin);
			err = FAIL_LIBFLAC_ALLOC;
		}
	}

	// libFLAC: set the encoder parameters
	if (err == ALL_OK)
	{
		FLAC__bool ok = true;

		ok &= FLAC__stream_encoder_set_verify(encoder, EncSettings.FLAC_Verify);
		ok &= FLAC__stream_encoder_set_compression_level(encoder, EncSettings.FLAC_EncodingQuality);
		ok &= FLAC__stream_encoder_set_channels(encoder, FMTheader.NumChannels);
		ok &= FLAC__stream_encoder_set_sample_rate(encoder, FMTheader.SampleRate);
		ok &= FLAC__stream_encoder_set_total_samples_estimate(encoder, total_samples);

		// FLAC: bits_per_sample must be <= 24
		unsigned flac_bps = 0;
		if (!is_float)
		{
			if (FMTheader.BitsPerSample == 20){
				// PCM: use original bit depth (16 or 24)
				flac_bps = 24;
			} else {
				// PCM: use original bit depth (20)
				flac_bps = FMTheader.BitsPerSample;
			}
		}
		else
		{
			// FLOAT: always convert to 24-bit integer for FLAC
			flac_bps = 24;
		}
		ok &= FLAC__stream_encoder_set_bits_per_sample(encoder, flac_bps);

		if (ok == false)
		{
			fclose(fin);
			FLAC__stream_encoder_delete(encoder);
			err = FAIL_LIBFLAC_ALLOC;
		}
	}

	// libFLAC: now add some metadata; we'll add some tags and a padding block
//	if(err == ALL_OK)
//	{
//		FLAC__StreamMetadata *metadata[2];
//		FLAC__StreamMetadata_VorbisComment_Entry entry;

//		if((metadata[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT)) == NULL ||
//			(metadata[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING)) == NULL ||
			// there are many tag (vorbiscomment) functions but these are convenient for this particular use:
//			!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ARTIST", "Some Artist") ||
//			!FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy=*/false) || // copy=false: let metadata object take control of entry's allocated string
//			!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "YEAR", "1984") ||
//			!FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy=*/false))
//		{
			// out of memory or tag error
//			ok = false;
//			err = FAIL_LIBFLAC_METADATA;
//		}
//		metadata[1]->length = 1234;	// set the padding length
//		ok = FLAC__stream_encoder_set_metadata(encoder, metadata, 2);
//	}

	// libFLAC: initialize the libFLAC encoder
	if(err == ALL_OK)
	{
		WCHAR* OutFileName;

		OutFileName = new WCHAR[MAXFILENAMELENGTH];
		wcsncpy_s(OutFileName, MAXFILENAMELENGTH, myparams->filename, wcsnlen(myparams->filename, MAXFILENAMELENGTH)-3);	// leave out the "wav" from the end
		wcscat_s(OutFileName, MAXFILENAMELENGTH, L"flac");
		if ((_wfopen_s(&fout, OutFileName, L"w+b")) != NULL)
		{
			fclose(fin);
			err = FAIL_FILE_OPEN;
		}
		else
		{
			ClientData.fout = fout;
			if (FLAC__stream_encoder_init_stream(encoder, write_callback_2FLAC, seek_callback_2FLAC, tell_callback_2FLAC, metadata_callback_2FLAC, &ClientData))
			{
				fclose(fin);
				fclose(fout);
				FLAC__stream_encoder_delete(encoder);
				err = FAIL_LIBFLAC_ALLOC;
			}
		}

		delete[]OutFileName;
	}

	// libFLAC: read blocks of samples from WAVE file and feed to the encoder
	if(err == ALL_OK)
	{
		size_t left, need, i;
		FLAC__byte *buffer_wav; 			// READSIZE * bytes per sample * channels, we read the WAVE data into here
		FLAC__int32 *buffer_flac;			// READSIZE * channels
		bool ok = true;

		// total_samples は WAV のサンプル数（ParseWavFile が正しく計算済み）
		int blocks = (total_samples + READSIZE_FLAC - 1) / READSIZE_FLAC;
		int processed = 0;
		int percent;
		int last_percent = -1;

		// set up the progress bar boundaries and reset it
		SendMessage(myparams->progress, PBM_SETRANGE, 0, MAKELONG(0, 100));
		SendMessage(myparams->progress, PBM_SETPOS, 0, 0);

		// ★ コンテナのバイト数（20bit → 3byte）
		int bytes_per_sample = FMTheader.ContainerBytesPerSample;

		buffer_wav	= new FLAC__byte[READSIZE_FLAC * FMTheader.NumChannels * bytes_per_sample];
		buffer_flac = new FLAC__int32[READSIZE_FLAC * FMTheader.NumChannels];

		left = (size_t)total_samples;

		while(ok && left)
		{
			need = (left > READSIZE_FLAC ? READSIZE_FLAC : left);

			if(fread(buffer_wav,
					 (size_t)FMTheader.NumChannels * bytes_per_sample,
					 need,
					 fin) != need)
			{
				ok = false;
			}
			else
			{
				// convert the packed little-endian PCM samples from WAVE file into an interleaved FLAC__int32 buffer for libFLAC
				switch(bytes_per_sample)
				{
					case 2: // 16bit
						for(i = 0; i < need * FMTheader.NumChannels; i++)
						{
							buffer_flac[i] = buffer_wav[2*i+1] << 8;
							buffer_flac[i] |= buffer_wav[2*i];
							if (buffer_flac[i] & 0x8000) buffer_flac[i] |= 0xffff0000;
						}
						break;

					case 3: // 20bit or 24bit → 24bit として扱う
						for(i = 0; i < need * FMTheader.NumChannels; i++)
						{
							buffer_flac[i] = buffer_wav[3*i+2] << 16;
							buffer_flac[i] |= buffer_wav[3*i+1] << 8;
							buffer_flac[i] |= buffer_wav[3*i];
							if (buffer_flac[i] & 0x800000) buffer_flac[i] |= 0xff000000;
						}
						break;

					case 4: // 32bit float or 32bit int
						if (is_float)
						{
							for (i = 0; i < need * FMTheader.NumChannels; i++)
							{
								float f;
								memcpy(&f, buffer_wav + 4 * i, 4);
								double d = (double)f;
								if (d > 1.0) d = 1.0;
								if (d < -1.0) d = -1.0;
								buffer_flac[i] = (FLAC__int32)(d * 8388607.0);
							}
						}
						else
						{
							int32_t* src = (int32_t*)buffer_wav;
							for (i = 0; i < need * FMTheader.NumChannels; i++)
							{
								buffer_flac[i] = src[i] >> 8;
							}
						}
						break;

					case 8: // 64bit float
						if (is_float)
						{
							for (i = 0; i < need * FMTheader.NumChannels; i++)
							{
								double d;
								memcpy(&d, buffer_wav + 8 * i, 8);
								if (d > 1.0) d = 1.0;
								if (d < -1.0) d = -1.0;
								buffer_flac[i] = (FLAC__int32)(d * 8388607.0);
							}
						}
						break;
				}

				// feed samples to the encoder
				ok = FLAC__stream_encoder_process_interleaved(encoder, buffer_flac, (uint32_t)need);

				processed ++;

				percent = (processed * 100) / blocks;
				if (percent > 100) percent = 100;

				if (percent != last_percent)
				{
					last_percent = percent;
					SendMessage(myparams->progress, PBM_SETPOS, percent, 0);
#if _DEBUG
					WCHAR dbg[128];
					swprintf_s(dbg, L"WAV2FLAC percent = %d / %d (processed=%d, blocks=%d)\n",
						percent, 100, processed, blocks);
					OutputDebugString(dbg);
#endif
				}
			}

			left -= need;
		}

		delete[]buffer_wav;
		delete[]buffer_flac;

		// 100%
		SendMessage(myparams->progress, PBM_SETPOS, 100, 0);
	}

	// libFLAC: close the encoder
	if (err == ALL_OK)
	{
		if (FLAC__stream_encoder_finish(encoder) == false) err = FAIL_LIBFLAC_RELEASE;
		// now that encoding is finished, the metadata can be freed
		//FLAC__metadata_object_delete(metadata[0]);
		//FLAC__metadata_object_delete(metadata[1]);
		fclose(fin);
		fclose(fout);
		FLAC__stream_encoder_delete(encoder);
	}

	myparams->ThreadInUse = false;
	myparams->OutputType = OUT_TYPE_FLAC;

	return ALL_OK;
}

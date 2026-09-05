#include "stdafx.h"
#include "FLACdrop.h"
#include "encoders.h"
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
	
	// WAV: read wav header and check if it is valid
	if (err == ALL_OK)
	{
		if (fread(&WAVEheader, 1, 12, fin) != 12)
		{
			fclose(fin);
			err = FAIL_FILE_OPEN;
		}
		if (memcmp(WAVEheader.ChunkID, "RIFF", 4) || memcmp(WAVEheader.Format, "WAVE", 4))
		{
			fclose(fin);
			err = FAIL_WAV_BAD_HEADER;
		}
	}

	// WAV: read the format chunk's header only to get its chunk size
	if (err == ALL_OK)
	{
		if (fread(&DATAheader, 1, 8, fin) != 8)
		{
			fclose(fin);
			err = FAIL_WAV_BAD_HEADER;
		}
	}
	
	// WAV: read the complete wave file header according to its actual chunk size (16, 18 or 40 byte), ChunkSize does not include the size of the header
	if (err == ALL_OK)
	{
		fseek(fin, -8, SEEK_CUR);
		if (fread(&FMTheader, 1, (size_t)DATAheader.ChunkSize + 8, fin) != (size_t)DATAheader.ChunkSize + 8)
		{
			fclose(fin);
			err = FAIL_WAV_BAD_HEADER;
		}
	}

	// WAV: check if the wav file has PCM uncompressed data
	if (err == ALL_OK)
	{
		switch (FMTheader.AudioFormat)
		{
			case WAVE_FORMAT_PCM:
			case WAVE_FORMAT_IEEE_FLOAT:
				break;
			case WAVE_FORMAT_EXTENSIBLE:
				// in this case the first two byte of the SubFormat is defining the audio format
				if (FMTheader.SubFormat_AudioFormat != WAVE_FORMAT_PCM &&
					FMTheader.SubFormat_AudioFormat != WAVE_FORMAT_IEEE_FLOAT)
				{
					fclose(fin);
					err = FAIL_WAV_UNSUPPORTED;
				}
				break;
			default:
				fclose(fin);
				err = FAIL_WAV_UNSUPPORTED;
		}
	}

	// WAV: determine whether the input format is PCM or IEEE float
	bool is_float = false;

	if (err == ALL_OK)
	{
		WORD fmt = FMTheader.AudioFormat;
		WORD sub = FMTheader.SubFormat_AudioFormat;

		// Check AudioFormat (PCM / FLOAT / EXTENSIBLE)
		if (fmt == WAVE_FORMAT_PCM)
		{
			is_float = false;
		}
		else if (fmt == WAVE_FORMAT_IEEE_FLOAT)
		{
			is_float = true;
		}
		else if (fmt == WAVE_FORMAT_EXTENSIBLE)
		{
			// EXTENSIBLE: the first two bytes of SubFormat define PCM or FLOAT
			if (sub == WAVE_FORMAT_PCM)
				is_float = false;
			else if (sub == WAVE_FORMAT_IEEE_FLOAT)
				is_float = true;
			else
			{
				fclose(fin);
				err = FAIL_WAV_UNSUPPORTED;
			}
		}
		else
		{
			fclose(fin);
			err = FAIL_WAV_UNSUPPORTED;
		}
	}

	// WAV: check bit depth (PCM and FLOAT have different valid ranges)
	if (err == ALL_OK)
	{
		if (!is_float)
		{
			// PCM: FLAC supports 16, 20, and 24-bit integer samples (20-bit is stored in 24-bit container)
			if (FMTheader.BitsPerSample != 16 &&
				FMTheader.BitsPerSample != 20 &&
				FMTheader.BitsPerSample != 24)
			{
				fclose(fin);
				err = FAIL_LIBFLAC_ONLY_16_24_BIT;
			}
		}
		else
		{
			// FLOAT: allow 32-bit float and 64-bit float input
			// (conversion to 24-bit integer will be done later)
			if (FMTheader.BitsPerSample != 32 &&
				FMTheader.BitsPerSample != 64)
			{
				fclose(fin);
				err = FAIL_WAV_UNSUPPORTED;
			}
		}
	}

	// WAV: search for the data chunk
	if (err == ALL_OK)
	{
		do
		{
			fread(&DATAheader, 1, 8, fin);
			fseek(fin, DATAheader.ChunkSize, SEEK_CUR);
		} while (memcmp(DATAheader.ChunkID, "data", 4));
		fseek(fin, -DATAheader.ChunkSize, SEEK_CUR);													// go back to the beginning of the data chunk
		total_samples = DATAheader.ChunkSize / FMTheader.NumChannels / (FMTheader.BitsPerSample / 8);	// sound data's size divided by one sample's size
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
			// PCM: use original bit depth (16 or 24)
			flac_bps = FMTheader.BitsPerSample;
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
		FLAC__byte *buffer_wav;				// READSIZE * bytes per sample * channels, we read the WAVE data into here
		FLAC__int32 *buffer_flac;			// READSIZE * channels
		bool ok = true;

		// total_samples ÇÕ WAV ÇÃÉTÉìÉvÉãêî
		int blocks = (total_samples + READSIZE_FLAC - 1) / READSIZE_FLAC;
		int processed = 0;
		int percent;
		int last_percent = -1;
    
		// set up the progress bar boundaries and reset it
		SendMessage(myparams->progress, PBM_SETRANGE, 0, MAKELONG(0, 100));
		SendMessage(myparams->progress, PBM_SETPOS, 0, 0);

		buffer_wav = new FLAC__byte[READSIZE_FLAC * FMTheader.NumChannels * (FMTheader.BitsPerSample / 8)];
		buffer_flac = new FLAC__int32[READSIZE_FLAC * FMTheader.NumChannels];

		left = (size_t)total_samples;
		while(ok && left)
		{
			need = (left>READSIZE_FLAC? (size_t)READSIZE_FLAC : (size_t)left);	// calculate the number of samples to read
			if(fread(buffer_wav, (size_t)FMTheader.NumChannels * (FMTheader.BitsPerSample/8), need, fin) != need)
			{
				// error during reading from WAVE file
				ok = false;
			}
			else
			{
				// convert the packed little-endian PCM samples from WAVE file into an interleaved FLAC__int32 buffer for libFLAC
				switch(FMTheader.BitsPerSample)
				{
					case 16:
						for(i = 0; i < need * FMTheader.NumChannels; i++)
						{
							// convert the 16 bit values stored in byte array into 32 bit signed integer values
							buffer_flac[i] = buffer_wav[2*i+1] << 8;
							buffer_flac[i] |= buffer_wav[2*i];
							if (buffer_flac[i] & 0x8000) buffer_flac[i] |= 0xffff0000; // correct the top 16 bit to have correct 2nd complement code
						}
						break;
					case 24:
						for(i = 0; i < need * FMTheader.NumChannels; i++)
						{
							// convert the 24 bit values stored in byte array into 32 bit signed integer values
							buffer_flac[i] = buffer_wav[3*i+2] << 16;
							buffer_flac[i] |= buffer_wav[3*i+1] << 8;
							buffer_flac[i] |= buffer_wav[3*i];
							if (buffer_flac[i] & 0x800000) buffer_flac[i] |= 0xff000000; // correct the top 8 bit to have correct 2nd complement code
						}
						break;
					case 32:
						if (is_float)
						{
							// convert 32-bit IEEE float [-1.0, +1.0] to 24-bit integer range
							for (i = 0; i < need * FMTheader.NumChannels; i++)
							{
								float f;
								memcpy(&f, buffer_wav + 4 * i, 4); // little-endian float
								double d = (double)f;
								if (d > 1.0) d = 1.0;
								if (d < -1.0) d = -1.0;
								buffer_flac[i] = (FLAC__int32)(d * 8388607.0); // 0x7FFFFF
							}
						}
						else
						{
							// convert 32-bit integer PCM Å® 24-bit integer (FLAC)
							int32_t* src = (int32_t*)buffer_wav;
							for (i = 0; i < need * FMTheader.NumChannels; i++)
							{
								buffer_flac[i] = src[i] >> 8;	// è„à 24bitÇíäèo
							}
						}
						break;
					case 64:
						if (is_float)
						{
							// convert 64-bit IEEE double [-1.0, +1.0] to 24-bit integer range
							for (i = 0; i < need * FMTheader.NumChannels; i++)
							{
								double d;
								memcpy(&d, buffer_wav + 8 * i, 8); // little-endian double
								if (d > 1.0) d = 1.0;
								if (d < -1.0) d = -1.0;
								buffer_flac[i] = (FLAC__int32)(d * 8388607.0);
							}
						}
						break;
				}

				// feed samples to the encoder
				ok = FLAC__stream_encoder_process_interleaved(encoder, buffer_flac, need);

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
	ExitEncThread(err, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_FLAC);
	return ALL_OK;
}

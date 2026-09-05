#include "stdafx.h"
#include "FLACdrop.h"
#include "encoders.h"
#include "lame.h"
#include "libFLAC_callbacks.h"

//
//	FUNCTION:	Encode_WAV2MP3(sEncodingParameters* )
//
//	PURPOSE:	Encode WAV stream to MP3 stream
//
DWORD WINAPI Encode_WAV2MP3(LPVOID params)
{
	sEncodingParameters *myparams = (sEncodingParameters*)params;
	sWAVEheader WAVEheader;
	sFMTheader FMTheader;
	sDATAheader DATAheader;

	lame_global_flags *lame_gfp;
	FILE *fin, *fout;
	unsigned int total_samples = 0;	// can use a 32-bit number due to WAV file size limitation
	int err = 0;


	// WAV: open the input WAVE file
	{
		if ((_wfopen_s(&fin, myparams->filename, L"rb")) != NULL)
		{
			err = FAIL_FILE_OPEN;
		}
	}

	// WAV: read wav header and check if it is valid
	if (err == ALL_OK)
	{
		if (fread(&WAVEheader, 1, 12, fin) != 12)
		{
			fclose(fin);
			err = FAIL_FILE_OPEN;
		}
	}
	if (err == ALL_OK)
	{
		if (memcmp(WAVEheader.ChunkID, "RIFF", 4) || memcmp(WAVEheader.Format, "WAVE", 4))
		{
			fclose(fin);
			err = FAIL_WAV_BAD_HEADER;
		}
	}

	// WAV: read the format chunk's header only for its chunk size
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
		case WAVE_FORMAT_UNKNOWN:
			if (FMTheader.NumChannels < 1 || FMTheader.NumChannels > 2)
			{
				fclose(fin);
				err = FAIL_WAV_UNSUPPORTED;
			}
			else if (FMTheader.BitsPerSample != 16 &&
				FMTheader.BitsPerSample != 20 &&
				FMTheader.BitsPerSample != 24 &&
				FMTheader.BitsPerSample != 32 &&
				FMTheader.BitsPerSample != 64)
			{
				fclose(fin);
				err = FAIL_WAV_UNSUPPORTED;
			}
			break;
		default:
			fclose(fin);
			err = FAIL_WAV_UNSUPPORTED;
			break;
		}
	}

	// WAV: check if the WAVE file has 16 bit resolution, MP3 stream does not support 24 bit
	if (err == ALL_OK)
	{
		switch (FMTheader.BitsPerSample)
		{
		case 16:   // そのまま
		case 20:   // 後段で 20→16bit に変換
		case 24:   // 後段で 24→16bit に変換
		case 32:   // 後段で 32→16bit に変換（float / int 両方）
		case 64:   // 後段で 64→16bit に変換（float）
			break;

		default:
			fclose(fin);
			err = FAIL_LAME_ONLY_16_BIT;
			break;
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
	
	// libmp3lame: initialize lame encoder
	if (err == ALL_OK)
	{
		lame_gfp = lame_init();
		if (lame_gfp == NULL)
		{
			fclose(fin);
			err = FAIL_LAME_INIT;
		}
	}
	
	// libmp3lame: set encoder parameters
	if (err == ALL_OK)
	{
		switch (FMTheader.NumChannels)
		{
		case 1:
			lame_set_mode(lame_gfp, MONO);
			break;
		case 2:
			lame_set_mode(lame_gfp, JOINT_STEREO);
			break;
		default:
			// only mono and stereo streams are supported by libmp3lame
			fclose(fin);
			err = FAIL_LAME_MAX_2_CHANNEL;
			break;
		}
	}
	if (err == ALL_OK)
	{
		// Internal algorithm selection. True quality is determined by the bitrate but this variable will effect quality by selecting expensive or cheap algorithms.
		// quality=0..9.  0=best (very slow).  9=worst.
		// recommended:  2     near-best quality, not too slow
		// 5     good quality, fast
		// 7     ok quality, really fast
		lame_set_quality(lame_gfp, EncSettings.LAME_InternalEncodingQuality);

		// turn off automatic writing of ID3 tag data into mp3 stream we have to call it before 'lame_init_params', because that function would spit out ID3v2 tag data.
		lame_set_write_id3tag_automatic(lame_gfp, 0);

		// set lame encoder parameters for CBR encoding
		lame_set_num_channels(lame_gfp, FMTheader.NumChannels);
		lame_set_in_samplerate(lame_gfp, FMTheader.SampleRate);
		lame_set_brate(lame_gfp, LAME_CBRBITRATES[EncSettings.LAME_CBRBitrate]);	// load encoding bitrate setting from LUT

		// set lame encoder parameters for VBR encoding
		switch (EncSettings.LAME_EncodingMode)
		{
		case 0:	// CBR
			lame_set_VBR(lame_gfp, vbr_off);
			break;
		case 1:	// VBR
			lame_set_VBR(lame_gfp, vbr_mtrh);
			break;
		}

		lame_set_VBR_q(lame_gfp, EncSettings.LAME_VBRQuality); // VBR quality level.  0=highest  9=lowest

		// now that all the options are set, lame needs to analyze them and set some more internal options and check for problems
		if (lame_init_params(lame_gfp) != 0)
		{
			fclose(fin);
			err = FAIL_LAME_INIT;
		}
	}

	// libmp3lame: open output file
	if (err == ALL_OK)
	{
		WCHAR* OutFileName;

		OutFileName = new WCHAR[MAXFILENAMELENGTH];
		wcsncpy_s(OutFileName, MAXFILENAMELENGTH, myparams->filename, wcsnlen(myparams->filename, MAXFILENAMELENGTH) - 3);	// leave out the "wav" from the end
		wcscat_s(OutFileName, MAXFILENAMELENGTH, L"mp3");
		if ((_wfopen_s(&fout, OutFileName, L"w+b")) != NULL)
		{
			fclose(fin);
			err = FAIL_FILE_OPEN;
		}
		delete[]OutFileName;
	}

	// libmp3lame: start encoding
	if (err == ALL_OK)
	{
		size_t left, need;
		BYTE* buffer_mp3, * buffer_wav, * buffer_wav_tmp;
		int imp3, owrite;
		bool ok = true;
		UINT processed_samples = 0;
		int last_percent = -1;

		// set up the progress bar boundaries
		SendMessage(myparams->progress, PBM_SETPOS, 0, 0);
		SendMessage(myparams->progress, PBM_SETRANGE, 0, MAKELONG(0, 100));

		// allocate memory buffers
		buffer_wav_tmp = new BYTE[READSIZE_MP3 * FMTheader.NumChannels * (FMTheader.BitsPerSample / 8)];
		buffer_wav = new BYTE[READSIZE_MP3 * FMTheader.NumChannels * 2];
		buffer_mp3 = new BYTE[LAME_MAXMP3BUFFER];

//		size_t  id3v2_size;
//		unsigned char *id3v2tag;

//		id3v2_size = lame_get_id3v2_tag(lame_gfp, 0, 0);
//		if (id3v2_size > 0)
//		{
//			id3v2tag = new unsigned char[id3v2_size];
//			if (id3v2tag != 0)
//			{
//				imp3 = lame_get_id3v2_tag(lame_gfp, id3v2tag, id3v2_size);
//				owrite = (int) fwrite(id3v2tag, 1, imp3, fout);
//				delete []id3v2tag;
//				if (owrite != imp3) return FAIL_LAME_ID3TAG;
//			}
//		}
//		else
//		{
//			unsigned char* id3v2tag = getOldTag(gf);
//			id3v2_size = sizeOfOldTag(gf);
//			if ( id3v2_size > 0 )
//			{
//				size_t owrite = fwrite(id3v2tag, 1, id3v2_size, fout);
//				if (owrite != id3v2_size) return FAIL_LAME_ID3TAG;
//			}
//		}
//		if (LAME_FLUSH == true) fflush(fout);

		left = (size_t)total_samples;
		// read blocks of samples from WAVE file and feed to the encoder
		while (ok && left)
		{
			need = (left > READSIZE_MP3 ? (size_t)READSIZE_MP3 : left);	// calculate the number of samples to read
			if (fread(buffer_wav_tmp, (size_t)FMTheader.NumChannels * (FMTheader.BitsPerSample / 8), need, fin) != need)
			{
				// error during reading from WAVE file
				ok = false;
			}
			else
			{
				// 16bit → 16bit
				if (FMTheader.BitsPerSample == 16)
				{
					memcpy(buffer_wav, buffer_wav_tmp, need * FMTheader.NumChannels * 2);
				}
				// 20bit → 16bit
				else if (FMTheader.BitsPerSample == 20)
				{
					for (size_t i = 0; i < need * FMTheader.NumChannels; i++)
					{
						int32_t s20 =
							buffer_wav_tmp[i * 3 + 0] |
							buffer_wav_tmp[i * 3 + 1] << 8 |
							buffer_wav_tmp[i * 3 + 2] << 16;

						((short*)buffer_wav)[i] = (short)(s20 >> 12); // 20bit → 16bit
					}
				}
				// 24bit → 16bit
				else if (FMTheader.BitsPerSample == 24)
				{
					for (size_t i = 0; i < need * FMTheader.NumChannels; i++)
					{
						int32_t s24 =
							buffer_wav_tmp[i * 3 + 0] |
							buffer_wav_tmp[i * 3 + 1] << 8 |
							buffer_wav_tmp[i * 3 + 2] << 16;

						((short*)buffer_wav)[i] = (short)(s24 >> 8);
					}
				}
				// 32bit int → 16bit（PCM / EXTENSIBLE-PCM）
				else if (FMTheader.BitsPerSample == 32 &&
					(FMTheader.AudioFormat == WAVE_FORMAT_PCM ||
						(FMTheader.AudioFormat == WAVE_FORMAT_EXTENSIBLE &&
							FMTheader.SubFormat_AudioFormat == WAVE_FORMAT_PCM)))
				{
					int32_t* src = (int32_t*)buffer_wav_tmp;
					short* dst = (short*)buffer_wav;

					for (size_t i = 0; i < need * FMTheader.NumChannels; i++)
					{
						dst[i] = (short)(src[i] >> 16);
					}
				}
				// 32bit float → 16bit（IEEE_FLOAT / EXTENSIBLE-FLOAT）
				else if (FMTheader.BitsPerSample == 32 &&
					(FMTheader.AudioFormat == WAVE_FORMAT_IEEE_FLOAT ||
						(FMTheader.AudioFormat == WAVE_FORMAT_EXTENSIBLE &&
							FMTheader.SubFormat_AudioFormat == WAVE_FORMAT_IEEE_FLOAT)))
				{
					float* src = (float*)buffer_wav_tmp;
					short* dst = (short*)buffer_wav;

					for (size_t i = 0; i < need * FMTheader.NumChannels; i++)
					{
						dst[i] = (short)(src[i] * 32767.0f);
					}
				}
				// 64bit float → 16bit（IEEE_FLOAT / EXTENSIBLE-FLOAT）
				else if (FMTheader.BitsPerSample == 64 &&
					(FMTheader.AudioFormat == WAVE_FORMAT_IEEE_FLOAT ||
						(FMTheader.AudioFormat == WAVE_FORMAT_EXTENSIBLE &&
							FMTheader.SubFormat_AudioFormat == WAVE_FORMAT_IEEE_FLOAT)))
				{
					double* src = (double*)buffer_wav_tmp;
					short* dst = (short*)buffer_wav;

					for (size_t i = 0; i < need * FMTheader.NumChannels; i++)
					{
						dst[i] = (short)(src[i] * 32767.0);
					}
				}


				// feed samples to the encoder
				switch (FMTheader.NumChannels)
				{
				case 2:
					imp3 = lame_encode_buffer_interleaved(lame_gfp, (short int*)buffer_wav, need, buffer_mp3, LAME_MAXMP3BUFFER);
					break;
				case 1:	// the interleaved version corrupts the mono stream
					imp3 = lame_encode_buffer(lame_gfp, (short int*)buffer_wav, nullptr, need, buffer_mp3, LAME_MAXMP3BUFFER);
					break;
				}

				// was our output buffer big enough?
				if (imp3 < 0) ok = false;
				else
				{
					owrite = (int)fwrite(buffer_mp3, 1, imp3, fout);
					if (owrite != imp3) ok = false;
					if (LAME_FLUSH == true) fflush(fout);
				}

			    processed_samples += (unsigned int)need;

				int percent = (int)(processed_samples * 100 / total_samples);
				if (percent > 100) percent = 100;

				if (percent != last_percent)
				{
					last_percent = percent;
					SendMessage(myparams->progress, PBM_SETPOS, percent, 0);
#if _DEBUG
					WCHAR dbg[128];
					swprintf_s(dbg, L"WAV2MP3 percent = %d / %d (processed=%u, total=%u)\n",
							   percent, 100, processed_samples, total_samples);
					OutputDebugString(dbg);
#endif
				}
//				SendMessage(myparams->progress, PBM_DELTAPOS, 1, 0);	// increase the progress bar
			}
			left -= need;
		}

		// may return one more mp3 frame
		if (ok == true)
		{
			if (LAME_NOGAP == true) imp3 = lame_encode_flush_nogap(lame_gfp, buffer_mp3, LAME_MAXMP3BUFFER);
			else imp3 = lame_encode_flush(lame_gfp, buffer_mp3, LAME_MAXMP3BUFFER);
			if (imp3 < 0) ok = false;
		}

		if (ok == true)
		{
			owrite = (int)fwrite(buffer_mp3, 1, imp3, fout);
			if (owrite != imp3) ok = false;
		}

		if (LAME_FLUSH == true && ok == true) fflush(fout);

		delete[]buffer_wav_tmp;
		delete[]buffer_wav;
		delete[]buffer_mp3;

		if (ok == false)
		{
			fclose(fin);
			fclose(fout);
			err = FAIL_LAME_ENCODE;
		}
	}

	// libmp3lame: close encoder
	if (err == ALL_OK)
	{
		if (lame_close(lame_gfp) != 0) err = FAIL_LAME_CLOSE;
		fclose(fin);
		fclose(fout);
	}

	myparams->ThreadInUse = false;
	ExitEncThread(err, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	return ALL_OK;
}

//
//	FUNCTION:	Encode_FLAC2MP3(sEncodingParameters* )
//
//	PURPOSE:	Encode FLAC stream to MP3 stream, transfer the tags also
//
DWORD WINAPI Encode_FLAC2MP3(LPVOID params)
{
	sEncodingParameters* myparams = (sEncodingParameters*)params;
	sMetaData MetaDataTrans[MD_NUMBER];
	sClientData ClientData;

	FILE* fin, * fout;
	FLAC__StreamDecoder* decoder = 0;
	lame_global_flags* lame_gfp;
	int err = 0;

	// libFLAC: allocate the libFLAC decoder
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

	// libFLAC: open the input file and give the handle to the libFLAC decoder
	if (err == ALL_OK)
	{
		if ((_wfopen_s(&fin, myparams->filename, L"r+b")) != NULL)
		{
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_FILE_OPEN;
		}
		else ClientData.fin = fin;
	}

	// libFLAC: initialize decoder, write callback function is "_2MEM"
	if (err == ALL_OK)
	{
		if (FLAC__stream_decoder_init_stream(decoder, read_callback_2WAV, seek_callback_2WAV, tell_callback_2WAV, length_callback_2WAV, eof_callback_2WAV, write_callback_2MEM, metadata_callback_2WAV, error_callback_2WAV, &ClientData) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
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

	// libFLAC: check if FLAC file has 16 bit resolution, mp3 streams support only 16 bit resolution
	if (err == ALL_OK)
	{
		switch (ClientData.bps)
		{
		case 16:
			break;
		case 24:
		default:
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LAME_ONLY_16_BIT;
		}
	}

	// libFLAC: check if total_samples count is in the STREAMINFO
	if (err == ALL_OK)
	{
		if (ClientData.total_samples == 0)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_BAD_HEADER;
		}
	}

	// libFLAC: initialize the metadata reader
	// https://xiph.org/flac/api/group__flac__metadata__level2.html
	if (err == ALL_OK)
	{
		FLAC__StreamMetadata_VorbisComment_Entry commententry;
		FLAC__Metadata_Chain* FLACchain;
		FLAC__Metadata_Iterator* FLACchainIterator;
		FLAC__StreamMetadata* FLACMetaData;
		FLAC__IOCallbacks metadata_callbacks;
		FLAC__bool MetaDataOK;
		FLAC__MetadataType FLACMetaDataType;
		int vorbiscommentoffset;
		char* commentname, * commentvalue;

		for (int i = 0; i < MD_NUMBER; i++) MetaDataTrans[i].present = false; // clear the metadata transfer variables
		metadata_callbacks.read = read_iocallback;
		metadata_callbacks.write = write_iocallback;
		metadata_callbacks.tell = tell_iocallback;
		metadata_callbacks.eof = eof_iocallback;
		metadata_callbacks.seek = seek_iocallback;
		metadata_callbacks.close = close_iocallback;

		FLACchain = FLAC__metadata_chain_new();
		if (FLAC__metadata_chain_read_with_callbacks(FLACchain, ClientData.fin, metadata_callbacks) == FALSE)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LIBFLAC_METADATA;
		}
		
		if (err == ALL_OK)
		{
			FLACchainIterator = FLAC__metadata_iterator_new();
			FLAC__metadata_iterator_init(FLACchainIterator, FLACchain);

			// go through the FLAC metadata blocks and find the "FLAC__METADATA_TYPE_VORBIS_COMMENT" block
			do
			{
				FLACMetaData = FLAC__metadata_iterator_get_block(FLACchainIterator);

				FLACMetaDataType = FLAC__metadata_iterator_get_block_type(FLACchainIterator);
				if (FLACMetaDataType == FLAC__METADATA_TYPE_VORBIS_COMMENT)
				{
					// copy the metadata to the transfer variables
					// https://xiph.org/flac/api/group__flac__metadata__object.html
					/*
					TITLE
						Track / Work name
					VERSION
						The version field may be used to differentiate multiple versions of the same track title in a single collection. (e.g.remix info)
					ALBUM
						The collection name to which this track belongs
					TRACKNUMBER
						The track number of this piece if part of a specific larger collection or album
					ARTIST
						The artist generally considered responsible for the work.In popular music this is usually the performing band or singer.For classical music it would be the composer.For an audio book it would be the author of the original text.
					PERFORMER
						The artist(s) who performed the work.In classical music this would be the conductor, orchestra, soloists.In an audio book it would be the actor who did the reading.In popular music this is typically the same as the ARTIST and is omitted.
					COPYRIGHT
						Copyright attribution, e.g., '2001 Nobody's Band' or '1999 Jack Moffitt'
					LICENSE
						License information, eg, 'All Rights Reserved', 'Any Use Permitted', a URL to a license such as a Creative Commons license("www.creativecommons.org/blahblah/license.html") or the EFF Open Audio License('distributed under the terms of the Open Audio License. see http://www.eff.org/IP/Open_licenses/eff_oal.html for details'), etc.
					ORGANIZATION
						Name of the organization producing the track(i.e.the 'record label')
					DESCRIPTION
						A short text description of the contents
					GENRE
						A short text indication of music genre
					DATE
						Date the track was recorded
					LOCATION
						Location where track was recorded
					CONTACT
						Contact information for the creators or distributors of the track.This could be a URL, an email address, the physical address of the producing label.
					ISRC
						ISRC number for the track; see the ISRC intro page for more information on ISRC numbers.
					*/
					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "ALBUM");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_ALBUM].text = new char[MAXMETADATA];
							MetaDataTrans[MD_ALBUM].present = true;
							strcpy_s(MetaDataTrans[MD_ALBUM].text, MAXMETADATA, commentvalue);
						}
					}

					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "ARTIST");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_ARTIST].text = new char[MAXMETADATA];
							MetaDataTrans[MD_ARTIST].present = true;
							strcpy_s(MetaDataTrans[MD_ARTIST].text, MAXMETADATA, commentvalue);
						}
					}

					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "DATE");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_DATE].text = new char[MAXMETADATA];
							MetaDataTrans[MD_DATE].present = true;
							strcpy_s(MetaDataTrans[MD_DATE].text, MAXMETADATA, commentvalue);
						}
					}

					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "DISCNUMBER");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_DISCNUMBER].text = new char[MAXMETADATA];
							MetaDataTrans[MD_DISCNUMBER].present = true;
							strcpy_s(MetaDataTrans[MD_DISCNUMBER].text, MAXMETADATA, commentvalue);
						}
					}

					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "GENRE");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_GENRE].text = new char[MAXMETADATA];
							MetaDataTrans[MD_GENRE].present = true;
							strcpy_s(MetaDataTrans[MD_GENRE].text, MAXMETADATA, commentvalue);
						}
					}

					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "TITLE");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_TITLE].text = new char[MAXMETADATA];
							MetaDataTrans[MD_TITLE].present = true;
							strcpy_s(MetaDataTrans[MD_TITLE].text, MAXMETADATA, commentvalue);
						}
					}

					vorbiscommentoffset = FLAC__metadata_object_vorbiscomment_find_entry_from(FLACMetaData, 0, "TRACKNUMBER");
					if (vorbiscommentoffset != -1)
					{
						commententry = FLACMetaData->data.vorbis_comment.comments[vorbiscommentoffset];
						if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(commententry, &commentname, &commentvalue) == TRUE)
						{
							MetaDataTrans[MD_TRACKNUMBER].text = new char[MAXMETADATA];
							MetaDataTrans[MD_TRACKNUMBER].present = true;
							strcpy_s(MetaDataTrans[MD_TRACKNUMBER].text, MAXMETADATA, commentvalue);
						}
					}
				}

				MetaDataOK = FLAC__metadata_iterator_next(FLACchainIterator);
			} while (MetaDataOK == TRUE);

			FLAC__metadata_chain_delete(FLACchain);
			if (FLAC__stream_decoder_reset(decoder) != TRUE) // reset the FLAC decoder because the metadata reader has changed the file pointer and not the correct audio stream will be read for the decoder
			{
				fclose(fin);
				FLAC__stream_decoder_delete(decoder);
				err = FAIL_LIBFLAC_ALLOC;
			}
			else FLAC__stream_decoder_process_until_end_of_metadata(decoder);	// go back to the exact position where we were before the metadata reading, otherwise there will be an additional pop sound at the beginning of the converted stream
		}
	}

	// libmp3lame: initialize encoder
	if (err == ALL_OK)
	{
		lame_gfp = lame_init();
		if (lame_gfp == NULL)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LAME_INIT;
		}
	}

	// libmp3lame: set encoder parameters
	if (err == ALL_OK)
	{
		switch (ClientData.channels)
		{
		case 1:
			lame_set_mode(lame_gfp, MONO);
			break;
		case 2:
			lame_set_mode(lame_gfp, JOINT_STEREO);
			break;
		default:
			// only mono and stereo streams are supported by libmp3lame
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LAME_MAX_2_CHANNEL;
			break;
		}
	}
	if (err == ALL_OK)
	{	
		// Internal algorithm selection. True quality is determined by the bitrate but this variable will effect quality by selecting expensive or cheap algorithms.
		// quality=0..9.  0=best (very slow).  9=worst.
		// recommended:  2     near-best quality, not too slow
		// 5     good quality, fast
		// 7     ok quality, really fast
		lame_set_quality(lame_gfp, EncSettings.LAME_InternalEncodingQuality);

		// turn off automatic writing of ID3 tag data into mp3 stream we have to call it before 'lame_init_params', because that function would spit out ID3v2 tag data.
		lame_set_write_id3tag_automatic(lame_gfp, 0);

		// set libmp3lame encoder parameters for CBR encoding
		lame_set_num_channels(lame_gfp, ClientData.channels);
		lame_set_in_samplerate(lame_gfp, ClientData.sample_rate);
		lame_set_brate(lame_gfp, LAME_CBRBITRATES[EncSettings.LAME_CBRBitrate]);	// load bitrate setting from LUT

		// set libmp3lame encoder parameters for VBR encoding
		switch (EncSettings.LAME_EncodingMode)
		{
		case 0:	// CBR
			lame_set_VBR(lame_gfp, vbr_off);
			break;
		case 1:	// VBR
			lame_set_VBR(lame_gfp, vbr_mtrh);
			break;
		}

		lame_set_VBR_q(lame_gfp, EncSettings.LAME_VBRQuality); // VBR quality level.  0=highest  9=lowest

		// now that all the options are set, libmp3lame needs to analyze them and set some more internal options and check for problems
		if (lame_init_params(lame_gfp) != 0)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			err = FAIL_LAME_INIT;
		}
	}

	// libmp3lame: open the output file
	if (err == ALL_OK)
	{
		WCHAR* OutFileName;

		OutFileName = new WCHAR[MAXFILENAMELENGTH];
		wcsncpy_s(OutFileName, MAXFILENAMELENGTH, myparams->filename, wcsnlen(myparams->filename, MAXFILENAMELENGTH) - 4);	// leave out the "flac" from the end
		wcscat_s(OutFileName, MAXFILENAMELENGTH, L"mp3");
		if ((_wfopen_s(&fout, OutFileName, L"w+b")) != NULL)
		{
			fclose(fin);
			FLAC__stream_decoder_delete(decoder);
			lame_close(lame_gfp);
			err = FAIL_FILE_OPEN;
		}
		delete[]OutFileName;
	}
			
	// libFLAC / libmp3lame: move the tags from the FLAC stream to the MP3 stream
	if (err == ALL_OK)
	{
		size_t  id3v2_size;
		unsigned char* id3v2tag;
		int imp3, owrite;

		// switch on ID3 v2 tags in the MP3 stream
		id3tag_init(lame_gfp);
		id3tag_v2_only(lame_gfp);

		// add the tags
		if (MetaDataTrans[MD_ALBUM].present == true) id3tag_set_album(lame_gfp, MetaDataTrans[MD_ALBUM].text);
		if (MetaDataTrans[MD_ARTIST].present == true) id3tag_set_artist(lame_gfp, MetaDataTrans[MD_ARTIST].text);
		if (MetaDataTrans[MD_DATE].present == true) id3tag_set_year(lame_gfp, MetaDataTrans[MD_DATE].text);
		if (MetaDataTrans[MD_GENRE].present == true) id3tag_set_genre(lame_gfp, MetaDataTrans[MD_GENRE].text);
		if (MetaDataTrans[MD_TITLE].present == true) id3tag_set_title(lame_gfp, MetaDataTrans[MD_TITLE].text);
		if (MetaDataTrans[MD_TRACKNUMBER].present == true) id3tag_set_track(lame_gfp, MetaDataTrans[MD_TRACKNUMBER].text);
		// http://id3.org/id3v2.3.0#Text_information_frames

		id3v2_size = lame_get_id3v2_tag(lame_gfp, 0, 0);
		id3v2tag = new unsigned char[id3v2_size];
		if (id3v2tag != 0)
		{
			imp3 = lame_get_id3v2_tag(lame_gfp, id3v2tag, id3v2_size);
			owrite = (int)fwrite(id3v2tag, 1, imp3, fout);
			if (owrite != imp3)
			{
				fclose(fin);
				fclose(fout);
				FLAC__stream_decoder_delete(decoder);
				lame_close(lame_gfp);
				err = FAIL_LAME_ID3TAG;
			}
			if (LAME_FLUSH == true) fflush(fout);
			delete[]id3v2tag;
		}

		for (int i = 0; i < MD_NUMBER; i++) if (MetaDataTrans[i].present == true) delete[] MetaDataTrans[i].text;	// free up the metadata transfer block
	}

	// libFLAC / libmp3lame: start the transcoding
	if (err == ALL_OK)
	{
		int imp3, owrite;
		BYTE *buffer_mp3, *buffer_raw;
		FLAC__bool ok = TRUE;
		FLAC__StreamDecoderState state;

		// allocate memory buffers
		buffer_raw = new BYTE[SIZE_RAW_BUFFER];
		buffer_mp3 = new BYTE[LAME_MAXMP3BUFFER];
		ClientData.buffer_out = buffer_raw;

		// set up the progress bar boundaries, block size is around 4k depending on resolution
		SendMessage(myparams->progress, PBM_SETPOS, 0, 0);
		SendMessage(myparams->progress, PBM_SETRANGE, 0, MAKELONG(0, ClientData.total_samples / ClientData.blocksize));	

		// loop the libFLAC decoder until it reaches the end of the input file or returns an error
		do
		{
			ok = FLAC__stream_decoder_process_single(decoder);
			state = FLAC__stream_decoder_get_state(decoder);

			// feed the block of samples to the encoder
			switch (ClientData.channels)
			{
			case 2:
				imp3 = lame_encode_buffer_interleaved(lame_gfp, (short int*)buffer_raw, ClientData.blocksize, buffer_mp3, LAME_MAXMP3BUFFER);
				break;
			case 1:	// the interleaved version corrupts the mono stream
				imp3 = lame_encode_buffer(lame_gfp, (short int*)buffer_raw, NULL, ClientData.blocksize, buffer_mp3, LAME_MAXMP3BUFFER);
				break;
			}

			// was our output buffer big enough?
			if (imp3 < 0) ok = false;
			else
			{
				owrite = (int)fwrite(buffer_mp3, 1, imp3, fout);
				if (owrite != imp3) ok = false;
				if (LAME_FLUSH == true) fflush(fout);
			}

			SendMessage(myparams->progress, PBM_DELTAPOS, 1, 0);	// increase the progress bar
		} while ((state != FLAC__STREAM_DECODER_END_OF_STREAM && FLAC__STREAM_DECODER_SEEK_ERROR && FLAC__STREAM_DECODER_ABORTED && FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR) && ok == TRUE);


		// may return one more mp3 frame
		if (ok)
		{
			if (LAME_NOGAP == true) imp3 = lame_encode_flush_nogap(lame_gfp, buffer_mp3, LAME_MAXMP3BUFFER);
			else imp3 = lame_encode_flush(lame_gfp, buffer_mp3, LAME_MAXMP3BUFFER);
			if (imp3 < 0) ok = false;
		}

		if (ok)
		{
			owrite = (int)fwrite(buffer_mp3, 1, imp3, fout);
			if (owrite != imp3) ok = false;
		}

		if (LAME_FLUSH == true && ok == TRUE) fflush(fout);

		delete[]buffer_raw;
		delete[]buffer_mp3;

		if (ok == false)
		{
			fclose(fin);
			fclose(fout);
			FLAC__stream_decoder_delete(decoder);
			lame_close(lame_gfp);
			err = FAIL_LAME_ENCODE;
		}
	}

	// libFLAC: close the decoder
	if (err == ALL_OK)
	{
		switch (FLAC__stream_decoder_get_state(decoder))
		{
		case FLAC__STREAM_DECODER_SEEK_ERROR:
		case FLAC__STREAM_DECODER_ABORTED:
		case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
			fclose(fin);
			fclose(fout);
			FLAC__stream_decoder_delete(decoder);
			lame_close(lame_gfp);
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

	// libmp3lame: close the encoder
	if (err == ALL_OK)
	{
		if (lame_close(lame_gfp) != 0) err = FAIL_LAME_CLOSE;
		fclose(fin);
		fclose(fout);
	}
	
	myparams->ThreadInUse = false;
	ExitEncThread(err, ghSemaphore, myparams->progresstotal, myparams->filename, OUT_TYPE_MP3);
	return ALL_OK;
}

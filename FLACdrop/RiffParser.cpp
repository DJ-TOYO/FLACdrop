#include "stdafx.h"
#include "RiffParser.h"

static uint32_t ReadUint32LE(FILE* fp)
{
	uint8_t b[4];
	if (fread(b, 1, 4, fp) != 4) return 0;
	return (uint32_t)b[0]
		| ((uint32_t)b[1] << 8)
		| ((uint32_t)b[2] << 16)
		| ((uint32_t)b[3] << 24);
}

static uint16_t ReadUint16LE(FILE* fp)
{
	uint8_t b[2];
	if (fread(b, 1, 2, fp) != 2) return 0;
	return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

// WAV file Analysis
// -Seeking to the Data Chunk Position
int ParseWavFile(FILE* fp, sWAVEheader* outWave, sFMTheader* outFmt, sDATAheader* outData, unsigned int* outTotalSamples)
{
	if (!fp || !outWave || !outFmt || !outData || !outTotalSamples)
		return FAIL_FILE_OPEN;

	// Init
	memset(outWave, 0, sizeof(sWAVEheader));
	memset(outFmt,	0, sizeof(sFMTheader));
	memset(outData, 0, sizeof(sDATAheader));
	*outTotalSamples = 0;

	// ---------------------------------------------------------
	// RIFF / WAVE ヘッダ
	// ---------------------------------------------------------
	if (fread(outWave, 1, 12, fp) != 12)
		return FAIL_WAV_BAD_HEADER;

	if (memcmp(outWave->ChunkID, "RIFF", 4) ||
		memcmp(outWave->Format, "WAVE", 4))
		return FAIL_WAV_BAD_HEADER;

	// ---------------------------------------------------------
	// チャンク走査
	// ---------------------------------------------------------
	bool fmtFound  = false;
	bool dataFound = false;
	bool bitEnable = false;
	bool FormatEnable = false;

	while (!fmtFound || !dataFound)
	{
		char	 chunkId[4];
		uint32_t chunkSize;

		if (fread(chunkId, 1, 4, fp) != 4)
			break;

		chunkSize = ReadUint32LE(fp);
		long chunkDataPos = ftell(fp);

		// ---------------------------------------------------------
		// fmt チャンク
		// ---------------------------------------------------------
		if (!fmtFound && memcmp(chunkId, "fmt ", 4) == 0)
		{
			fmtFound = true;

			memcpy(outFmt->ChunkID, "fmt ", 4);
			outFmt->ChunkSize = (int)chunkSize;

			outFmt->AudioFormat   = ReadUint16LE(fp);
			outFmt->NumChannels   = (short)ReadUint16LE(fp);
			outFmt->SampleRate	  = (int)ReadUint32LE(fp);
			outFmt->ByteRate	  = (int)ReadUint32LE(fp);
			outFmt->BlockAlign	  = (short)ReadUint16LE(fp);
			outFmt->BitsPerSample = (short)ReadUint16LE(fp);

			long consumed = ftell(fp) - chunkDataPos;
			long remain   = (long)chunkSize - consumed;

			// EXTENSIBLE 初期化
			outFmt->ExtensionSize		 = 0;
			outFmt->ValidBitsPerSample	 = outFmt->BitsPerSample;
			outFmt->ChannelMask 		 = 0;
			outFmt->SubFormat_AudioFormat = 0;
			memset(&outFmt->SubFormat_GUID, 0, sizeof(GUID));

			// ---------------------------------------------------------
			// EXTENSIBLE (ChunkSize >= 18)
			// ---------------------------------------------------------
			if (remain >= 2)
			{
				outFmt->ExtensionSize = (short)ReadUint16LE(fp);
				remain -= 2;

				if (outFmt->ExtensionSize >= 22 && remain >= 22)
				{
					outFmt->ValidBitsPerSample = (short)ReadUint16LE(fp);
					outFmt->ChannelMask 	   = (int)ReadUint32LE(fp);

					// GUID (16 bytes)
					fread(&outFmt->SubFormat_GUID, 1, sizeof(GUID), fp);

					// GUID の先頭2byteが AudioFormat（Data1の下位16bit）
					outFmt->SubFormat_AudioFormat =
						(short)(outFmt->SubFormat_GUID.Data1 & 0xFFFF);

					remain -= 22;
				}
			}

			// 残りスキップ
			if (remain > 0)
				fseek(fp, remain, SEEK_CUR);

			// コンテナBYTE数
			outFmt->ContainerBytesPerSample = outFmt->BlockAlign / outFmt->NumChannels;
		}

		// ---------------------------------------------------------
		// data チャンク
		// ---------------------------------------------------------
		else if (!dataFound && memcmp(chunkId, "data", 4) == 0)
		{
			dataFound = true;

			memcpy(outData->ChunkID, "data", 4);
			outData->ChunkSize = (int)chunkSize;

			// data チャンクのDATA位置(PCMデータ)に戻す。
			fseek(fp, chunkDataPos, SEEK_SET);

			// total_samples 計算
			*outTotalSamples =
				chunkSize / (uint32_t)outFmt->BlockAlign;

			// data位置で終了
			break;
		}
		else
		{
			// その他チャンクはスキップ
			if (fseek(fp, chunkSize, SEEK_CUR) != 0) {
				// ファイルサイズを超えた。
				break;
			}

		}

		// パディング※1BIT目が1の場合は奇数チェンクなので1byteパディングされる
		if (chunkSize & 1) {
			if(fseek(fp, 1, SEEK_CUR) != 0){
				// ファイルサイズを超えた。
				break;
			}
		}
	}

	if (!fmtFound || !dataFound)
		return FAIL_WAV_BAD_HEADER;

	// Format Check
	if (outFmt->AudioFormat == WAVE_FORMAT_PCM){
		FormatEnable = true;
	} else if (outFmt->AudioFormat == WAVE_FORMAT_IEEE_FLOAT){
		FormatEnable = true;
	} else if (outFmt->AudioFormat == WAVE_FORMAT_EXTENSIBLE) {
		if (outFmt->SubFormat_AudioFormat == WAVE_FORMAT_PCM ||
			outFmt->SubFormat_AudioFormat == WAVE_FORMAT_IEEE_FLOAT)
			FormatEnable = true;
	}

	if (!FormatEnable)
		return FAIL_WAV_UNSUPPORTED;

	// BITチェック
	switch (outFmt->BitsPerSample)
	{
		case 16:
		case 20:
		case 24:
		case 32:
		case 64:
			bitEnable = true;
			break;
		default:
			return FAIL_WAV_UNSUPPORTED;
	}

	if (!fmtFound || !dataFound)
		return FAIL_WAV_BAD_HEADER;

	return ALL_OK;
}

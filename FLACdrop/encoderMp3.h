#pragma once
#include <vector>
#include <string>

#include "stream_encoder.h"
#include "stream_decoder.h"
#include "metadata.h"

DWORD WINAPI Encode_WAV2MP3(LPVOID params);
DWORD WINAPI Encode_FLAC2MP3(LPVOID params);

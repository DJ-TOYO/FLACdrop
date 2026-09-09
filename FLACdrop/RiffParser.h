#pragma once
#include "encoders.h"

bool ParseWavFile(FILE* fp, sWAVEheader* outWave, sFMTheader* outFmt, sDATAheader* outData, unsigned int* outTotalSamples);

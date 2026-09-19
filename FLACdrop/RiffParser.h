#pragma once
#include "encoders.h"

int ParseWavFile(FILE* fp, sWAVEheader* outWave, sFMTheader* outFmt, sDATAheader* outData, unsigned int* outTotalSamples);

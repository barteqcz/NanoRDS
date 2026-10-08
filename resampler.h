#ifndef NANORDS_RESAMPLER_H
#define NANORDS_RESAMPLER_H
#include <samplerate.h>
#ifndef CONVERTER_TYPE
#define CONVERTER_TYPE SRC_SINC_MEDIUM_QUALITY
#endif
int resampler_init(SRC_STATE **src_state, int channels);
int resample(SRC_STATE *src_state, SRC_DATA *src_data);
void resampler_exit(SRC_STATE *src_state);
#endif

#include <samplerate.h>

#define CONVERTER_TYPE SRC_SINC_FASTEST

extern int8_t resampler_init(SRC_STATE **src_state, uint8_t channels);
extern int8_t resample(SRC_STATE *src_state, SRC_DATA src_data, size_t *frames_generated);
extern void resampler_exit(SRC_STATE *src_state);

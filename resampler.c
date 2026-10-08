#include "common.h"
#include "resampler.h"
int resampler_init(SRC_STATE **state, int channels) {
    int error;
    *state = src_new(CONVERTER_TYPE, channels, &error);
    if (!*state) { fprintf(stderr, "Resampler: %s\n", src_strerror(error)); return -1; }
    return 0;
}
int resample(SRC_STATE *state, SRC_DATA *data) {
    int error = src_process(state, data);
    if (error) { fprintf(stderr, "Resampler: %s\n", src_strerror(error)); return -1; }
    return 0;
}
void resampler_exit(SRC_STATE *state) { if (state) src_delete(state); }

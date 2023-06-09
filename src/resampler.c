#include "common.h"
#include "resampler.h"

int8_t resampler_init(SRC_STATE **src_state, uint8_t channels) {
	int src_error;

	*src_state = src_new(CONVERTER_TYPE, channels, &src_error);

	if (*src_state == NULL) {
		fprintf(stderr, "Error: src_new failed: %s\n", src_strerror(src_error));
		return -1;
	}

	return 0;
}

int8_t resample(SRC_STATE *src_state, SRC_DATA src_data, size_t *frames_generated) {
	int src_error;

	src_error = src_process(src_state, &src_data);

	if (src_error) {
		fprintf(stderr, "Error: src_process failed: %s\n", src_strerror(src_error));
		return -1;
	}

	*frames_generated = src_data.output_frames_gen;

	return 0;
}

void resampler_exit(SRC_STATE *src_state) {
	src_delete(src_state);
}

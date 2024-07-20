#include "common.h"
#include "rds.h"
#include "fm_mpx.h"
#include "waveforms.h"
#include "modulator.h"

static struct rds_t **rds_ctx;
static float **waveform;

/*
 * Create the RDS objects
 *
 */
void init_rds_objects() {
	rds_ctx = malloc(1 * sizeof(struct rds_t));

	for (uint8_t i = 0; i < 1; i++) {
		rds_ctx[i] = malloc(sizeof(struct rds_t));
		rds_ctx[i]->bit_buffer = malloc(BITS_PER_GROUP);
		rds_ctx[i]->sample_buffer =
			malloc(SAMPLE_BUFFER_SIZE * sizeof(float));
	}

	waveform = malloc(2 * sizeof(float));

	for (uint8_t i = 0; i < 2; i++) {
		waveform[i] = malloc(FILTER_SIZE * sizeof(float));
		for (uint16_t j = 0; j < FILTER_SIZE; j++) {
			waveform[i][j] = i ?
				+waveform_biphase[j] : -waveform_biphase[j];
		}
	}
}

void exit_rds_objects() {
	for (uint8_t i = 0; i < 1; i++) {
		free(rds_ctx[i]->sample_buffer);
		free(rds_ctx[i]->bit_buffer);
		free(rds_ctx[i]);
	}

	free(rds_ctx);

	for (uint8_t i = 0; i < 2; i++) {
		free(waveform[i]);
	}

	free(waveform);
}

/* Get an RDS sample. This generates the envelope of the waveform using
 * pre-generated elementary waveform samples.
 */
float get_rds_sample(uint8_t stream_num) {
	struct rds_t *rds = rds_ctx[stream_num];
	uint16_t idx;
	float *cur_waveform;
	float sample;

	if (rds->sample_count == SAMPLES_PER_BIT) {
		if (rds->bit_pos == BITS_PER_GROUP) {
			get_rds_bits(rds->bit_buffer);
			rds->bit_pos = 0;
		}

		/* do differential encoding */
		rds->cur_bit = rds->bit_buffer[rds->bit_pos++];
		rds->prev_output = rds->cur_output;
		rds->cur_output = rds->prev_output ^ rds->cur_bit;

		idx = rds->in_sample_index;
		cur_waveform = waveform[rds->cur_output];

		for (uint16_t i = 0; i < FILTER_SIZE; i++) {
			rds->sample_buffer[idx++] += *cur_waveform++;
			if (idx == SAMPLE_BUFFER_SIZE) idx = 0;
		}

		rds->in_sample_index += SAMPLES_PER_BIT;
		if (rds->in_sample_index == SAMPLE_BUFFER_SIZE)
			rds->in_sample_index = 0;

		rds->sample_count = 0;
	}
	rds->sample_count++;

	sample = rds->sample_buffer[rds->out_sample_index];
	rds->sample_buffer[rds->out_sample_index++] = 0;
	if (rds->out_sample_index == SAMPLE_BUFFER_SIZE)
		rds->out_sample_index = 0;

	return sample;
}

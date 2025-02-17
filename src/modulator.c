#include "common.h"
#include "rds.h"
#include "fm_mpx.h"
#include "waveforms.h"
#include "modulator.h"

static struct rds_t **rds_ctx;
static float **waveform;

void init_rds_objects() {
    rds_ctx = malloc(NUM_STREAMS * sizeof(struct rds_t));

    for (uint8_t i = 0; i < NUM_STREAMS; i++) {
        rds_ctx[i] = malloc(sizeof(struct rds_t));
        rds_ctx[i]->bit_buffer = malloc(BITS_PER_GROUP);
        rds_ctx[i]->sample_buffer = malloc(SAMPLE_BUFFER_SIZE * sizeof(float));
        rds_ctx[i]->symbol_shift_buf_idx = 0;
    }

    waveform = malloc(2 * sizeof(float));

    for (uint8_t i = 0; i < 2; i++) {
        waveform[i] = malloc(FILTER_SIZE * sizeof(float));
        for (uint16_t j = 0; j < FILTER_SIZE; j++) {
            waveform[i][j] = i ? +waveform_biphase[j] : -waveform_biphase[j];
        }
    }
}

void exit_rds_objects() {
    for (uint8_t i = 0; i < NUM_STREAMS; i++) {
        free(rds_ctx[i]->sample_buffer);
        free(rds_ctx[i]->bit_buffer);
        free(rds_ctx[i]);
        if (rds_ctx[i]->symbol_shift) {
            free(rds_ctx[i]->symbol_shift_buf);
        }
    }

    free(rds_ctx);

    for (uint8_t i = 0; i < 2; i++) {
        free(waveform[i]);
    }

    free(waveform);
}

float get_rds_sample(uint8_t stream_num) {
    struct rds_t *rds;
    uint16_t idx;
    float *cur_waveform;
    float sample;

    rds = rds_ctx[stream_num];

    if (rds->sample_count == SAMPLES_PER_BIT) {
        if (rds->bit_pos == BITS_PER_GROUP) {
            get_rds_bits(rds->bit_buffer);
            rds->bit_pos = 0;
        }

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

    if (rds->symbol_shift) {
        rds->symbol_shift_buf[rds->symbol_shift_buf_idx++] = rds->sample_buffer[rds->out_sample_index];

        if (rds->symbol_shift_buf_idx == rds->symbol_shift)
            rds->symbol_shift_buf_idx = 0;

        sample = rds->symbol_shift_buf[rds->symbol_shift_buf_idx];

        goto done;
    }

    sample = rds->sample_buffer[rds->out_sample_index];

done:
    rds->sample_buffer[rds->out_sample_index++] = 0;
    if (rds->out_sample_index == SAMPLE_BUFFER_SIZE)
        rds->out_sample_index = 0;

    return sample;
}

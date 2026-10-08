#include "common.h"
#include "rds.h"
#include "waveforms.h"
#include "modulator.h"

typedef struct rds_t {
    uint8_t bits[BITS_PER_GROUP];
    float samples[SAMPLE_BUFFER_SIZE];
    size_t bit_pos, sample_count, in_index, out_index;
    uint8_t output;
} rds_t;
static rds_t streams[NUM_STREAMS];

int init_rds_objects(void) {
    memset(streams, 0, sizeof streams);
    for (size_t i = 0; i < NUM_STREAMS; ++i) {
        streams[i].bit_pos = BITS_PER_GROUP;
        streams[i].sample_count = SAMPLES_PER_BIT;
    }
    return 0;
}
void exit_rds_objects(void) { memset(streams, 0, sizeof streams); }

float get_rds_sample(uint8_t stream_num) {
    rds_t *rds;
    float sample;
    if (stream_num >= NUM_STREAMS) return 0.0f;
    rds = &streams[stream_num];
    if (rds->sample_count == SAMPLES_PER_BIT) {
        size_t idx = rds->in_index;
        float sign;
        if (rds->bit_pos == BITS_PER_GROUP) {
            get_rds_bits(rds->bits);
            rds->bit_pos = 0;
        }
        rds->output ^= rds->bits[rds->bit_pos++];
        sign = rds->output ? 1.0f : -1.0f;
        for (size_t i = 0; i < FILTER_SIZE; ++i) {
            rds->samples[idx] += sign * waveform_biphase[i];
            if (++idx == SAMPLE_BUFFER_SIZE) idx = 0;
        }
        rds->in_index = (rds->in_index + SAMPLES_PER_BIT) % SAMPLE_BUFFER_SIZE;
        rds->sample_count = 0;
    }
    ++rds->sample_count;
    sample = rds->samples[rds->out_index];
    rds->samples[rds->out_index] = 0.0f;
    if (++rds->out_index == SAMPLE_BUFFER_SIZE) rds->out_index = 0;
    return sample;
}

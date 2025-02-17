#include "common.h"
#include "osc.h"

static void create_wave(uint32_t rate, float freq, float *sin_wave, float *cos_wave, uint16_t *max_phase) {
    double sin_sample, cos_sample;
    uint8_t zero_crossings = 0;
    uint16_t i = 1;
    const double w = M_2PI * freq;
    double phase;

    *sin_wave++ = 0.0f;
    *cos_wave++ = 1.0f;

    while (zero_crossings < 2 && i < rate) {
        phase = (double)i / (double)rate;
        sin_sample = sin(w * phase);
        cos_sample = cos(w * phase);
        if (sin_sample > -0.1e-4 && sin_sample < 0.1e-4) {
            zero_crossings++;
            sin_sample = 0.0f;
        }
        *sin_wave++ = (float)sin_sample;
        *cos_wave++ = (float)cos_sample;
        i++;
    }

    *max_phase = i - 1;
}

void osc_init(struct osc_t *osc, uint32_t sample_rate, float freq) {
    osc->sample_rate = sample_rate;

    osc->sin_wave = malloc(osc->sample_rate * sizeof(float));
    osc->cos_wave = malloc(osc->sample_rate * sizeof(float));

    osc->cur = 0;

    create_wave(osc->sample_rate, freq,
        osc->sin_wave, osc->cos_wave,
        &osc->max);
}

float osc_get_cos(struct osc_t *osc) {
    return osc->cos_wave[osc->cur];
}

float osc_get_sin(struct osc_t *osc) {
    return osc->sin_wave[osc->cur];
}

void osc_update_pos(struct osc_t *osc) {
    if (++osc->cur == osc->max) osc->cur = 0;
}

void osc_exit(struct osc_t *osc) {
    free(osc->sin_wave);
    free(osc->cos_wave);
    osc->cur = 0;
    osc->max = 0;
}

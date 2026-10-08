#include "common.h"
#include "osc.h"
static uint32_t gcd(uint32_t a, uint32_t b) {
    while (b) { uint32_t r = a % b; a = b; b = r; }
    return a;
}
int osc_init(osc_t *osc, uint32_t rate, float freq) {
    uint32_t period;
    if (!rate || !isfinite(freq) || freq <= 0 || freq >= (float)rate / 2 || floorf(freq) != freq)
        return -1;
    period = rate / gcd(rate, (uint32_t)freq);
    memset(osc, 0, sizeof *osc);
    osc->sin_wave = malloc((size_t)period * sizeof *osc->sin_wave);
    osc->cos_wave = malloc((size_t)period * sizeof *osc->cos_wave);
    if (!osc->sin_wave || !osc->cos_wave) { osc_exit(osc); return -1; }
    osc->max = period;
    for (uint32_t i = 0; i < period; ++i) {
        double phase = M_2PI * (double)freq * (double)i / (double)rate;
        osc->sin_wave[i] = (float)sin(phase);
        osc->cos_wave[i] = (float)cos(phase);
    }
    return 0;
}
float osc_get_cos(osc_t *osc) { return osc->cos_wave[osc->cur]; }
float osc_get_sin(osc_t *osc) { return osc->sin_wave[osc->cur]; }
void osc_update_pos(osc_t *osc) { if (++osc->cur == osc->max) osc->cur = 0; }
void osc_exit(osc_t *osc) {
    free(osc->sin_wave); free(osc->cos_wave);
    memset(osc, 0, sizeof *osc);
}

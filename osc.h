#ifndef NANORDS_OSC_H
#define NANORDS_OSC_H
#include <stdint.h>
typedef struct osc_t {
    float *sin_wave, *cos_wave;
    uint32_t cur, max;
} osc_t;
int osc_init(osc_t *osc, uint32_t sample_rate, float freq);
float osc_get_sin(osc_t *osc);
float osc_get_cos(osc_t *osc);
void osc_update_pos(osc_t *osc);
void osc_exit(osc_t *osc);
#endif

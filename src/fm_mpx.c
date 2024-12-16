#include "common.h"
#include "rds.h"
#include "fm_mpx.h"
#include "osc.h"

/*
 * Local oscillator objects
 * this is where the MPX waveforms are stored
 */
static struct osc_t osc_19k;
static struct osc_t osc_57k;

/* initial subcarrier volumes */
static float volumes[] = {
        0.09f, /* Stereo pilot: 9% */
        0.09f /* RDS modulation: 4.5% */
};

void set_carrier_volume(uint8_t carrier, uint8_t new_volume) {
    /* don't allow Stereo pilot level to be over 18%. If higher, set to 18% */
    if (carrier == 0) {
        if (new_volume > 18) {
            volumes[0] = 0.18f;
        } else {
            volumes[0] = (float)new_volume / 100.0f;
        }
    }

    /* don't allow RDS subcarrier level to be over 9%. If higher, set to 9% */
    if (carrier == 1) {
        if (new_volume > 9) {
            volumes[1] = 0.18f;
        } else {
            volumes[1] = (float)new_volume / 100.0f;
        }
    }
}

void fm_mpx_init(uint32_t sample_rate) {
        /* initialize the subcarrier oscillators */
        osc_init(&osc_19k, sample_rate, 19000.0f);
        osc_init(&osc_57k, sample_rate, 57000.0f);
}

void fm_rds_get_frames(float *outbuf, size_t num_frames) {
        size_t j = 0;
        float out;

        for (size_t i = 0; i < num_frames; i++) {
                out = 0.0f;

                /* Pilot tone for calibration */
                out += osc_get_cos(&osc_19k) * volumes[0];
                out += osc_get_cos(&osc_57k) * get_rds_sample(0) * volumes[1];

                /* update oscillator */
                osc_update_pos(&osc_19k);
                osc_update_pos(&osc_57k);

                /* clipper */
                out = fminf(+1.0f, out);
                out = fmaxf(-1.0f, out);

                /* adjust volume and put into both channels */
                outbuf[j+0] = outbuf[j+1] = out;
                j += 2;

        }
}

void fm_mpx_exit() {
        osc_exit(&osc_19k);
        osc_exit(&osc_57k);
}

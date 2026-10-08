#include "common.h"
#include "fm_mpx.h"
#include "osc.h"
static osc_t osc_19k, osc_rds;
static float volumes[2] = {0.09f, 0.09f};
void set_carrier_volume(uint8_t carrier, float volume) {
    if (carrier > 1 || !isfinite(volume) || volume < 0 || volume > 100) return;
    volumes[carrier] = volume / 100.0f * (carrier == 1 ? 2.0f : 1.0f);
}
int fm_mpx_init(uint32_t rate) {
    if (osc_init(&osc_19k, rate, 19000.0f) || osc_init(&osc_rds, rate, 57000.0f)) {
        fm_mpx_exit();
        return -1;
    }
    return 0;
}
void fm_rds_get_frames(float *outbuf, size_t frames) {
    for (size_t i = 0; i < frames; ++i) {
        float out = osc_get_cos(&osc_19k) * volumes[0] +
                    osc_get_cos(&osc_rds) * get_rds_sample(0) * volumes[1];
        osc_update_pos(&osc_19k);
        osc_update_pos(&osc_rds);
        outbuf[i] = fmaxf(-1.0f, fminf(1.0f, out));
    }
}
void fm_mpx_exit(void) { osc_exit(&osc_19k); osc_exit(&osc_rds); }

#include "common.h"
#include "rds.h"
#include "modulator.h"
#include "lib.h"

static struct rds_params_t rds_data;
static uint8_t basic_groups_state;
static uint8_t af_state, ps_state, rt_state, ptyn_state, group_counter;
static uint8_t group_counter_ecc, group_counter_lic, group_counter_rtp_oda;
static uint8_t group_counter_ptyn, group_counter_rtp;
static int latest_minutes = -1;
static unsigned char ps_text[8], rt_text[64], ptyn_text[8];

static struct {
    uint8_t ps_update;
    uint8_t rt_update;
    uint8_t rt_segments;
    uint8_t rt_bursting;
    uint8_t rt_ab;
    uint8_t ptyn_update;
} rds_state;

static struct {
    uint8_t running;
    uint8_t toggle;
    uint8_t type[2];
    uint8_t start[2];
    uint8_t len[2];
} rtp_cfg;

/* RDS VHF AF codes 1..204 represent 87.6..107.9 MHz in 100 kHz steps.
 * Reject off-grid values rather than silently broadcasting another frequency. */
static int vhf_af_code(float mhz) {
    long code;
    float scaled;
    if (!isfinite(mhz) || mhz < 87.6f || mhz > 107.9f) return -1;
    scaled = mhz * 10.0f - 875.0f;
    code = lroundf(scaled);
    if (code < 1 || code > 204 || fabsf(scaled - (float)code) > 0.001f) return -1;
    return (int)code;
}

int init_rds_af_method_b(struct rds_af_t *list, float tuning_mhz) {
    int code = vhf_af_code(tuning_mhz);
    if (!list || code < 0) return -1;
    memset(list, 0, sizeof *list);
    list->method = AF_METHOD_B;
    list->tuning_code = (uint8_t)code;
    return 0;
}

int add_rds_af_method_b(struct rds_af_t *list, float frequency_mhz, int regional) {
    int af;
    uint8_t tuned;
    uint16_t pair;
    if (!list || list->method != AF_METHOD_B || !list->tuning_code ||
        list->num_b_pairs >= AF_B_MAX_PAIRS) return -1;
    af = vhf_af_code(frequency_mhz);
    if (af < 0 || af == list->tuning_code) return -1;
    tuned = list->tuning_code;
    /* Ascending pairs mean same programme. Descending pairs mean a
     * regional variant (IEC 62106 / NRSC-4 Method B convention). */
    if (regional)
        pair = ((uint16_t)(af > tuned ? af : tuned) << 8) |
               (uint16_t)(af < tuned ? af : tuned);
    else
        pair = ((uint16_t)(af < tuned ? af : tuned) << 8) |
               (uint16_t)(af > tuned ? af : tuned);

    for (uint8_t i = 0; i < list->num_b_pairs; ++i) {
        uint16_t other = list->b_pairs[i];
        if ((uint8_t)(other >> 8) == af || (uint8_t)other == af) return -1;
    }
    list->b_pairs[list->num_b_pairs++] = pair;
    return 0;
}

static uint16_t get_next_af(void) {
    uint16_t out;

    if (rds_data.af.method == AF_METHOD_B && rds_data.af.num_b_pairs) {
        if (af_state > rds_data.af.num_b_pairs) af_state = 0;
        if (af_state == 0) {
            /* Count every AF frequency code: one tuning frequency and two codes per pair. */
            out = (uint16_t)(AF_CODE_NUM_AFS_BASE + 2 * rds_data.af.num_b_pairs + 1) << 8;
            out |= rds_data.af.tuning_code;
        } else {
            out = rds_data.af.b_pairs[af_state - 1];
        }
        if (++af_state > rds_data.af.num_b_pairs) af_state = 0;
    } else if (rds_data.af.method == AF_METHOD_A && rds_data.af.num_afs) {
        if (af_state >= rds_data.af.num_entries) af_state = 0;
        if (af_state == 0) {
            out = (AF_CODE_NUM_AFS_BASE + rds_data.af.num_afs) << 8;
            out |= rds_data.af.afs[0];
            af_state += 1;
        } else {
            out = rds_data.af.afs[af_state] << 8;
            if (af_state + 1 < rds_data.af.num_entries)
                out |= rds_data.af.afs[af_state + 1];
            else
                out |= AF_CODE_FILLER;
            af_state += 2;
        }
        if (af_state >= rds_data.af.num_entries) af_state = 0;
    } else {
        out = AF_CODE_NO_AF << 8 | AF_CODE_FILLER;
    }

    return out;
}

static void get_rds_ps_group(uint16_t *blocks) {

    if (ps_state == 0 && rds_state.ps_update) {
        memcpy(ps_text, rds_data.ps, 8);
        rds_state.ps_update = 0;
    }

    blocks[1] |= rds_data.ta << 4;
    blocks[1] |= rds_data.ms << 3;
    blocks[1] |= ((rds_data.di >> (3 - ps_state)) & INT8_0) << 2;
    blocks[1] |= ps_state;
    blocks[2] = get_next_af();
    blocks[3] = ps_text[ps_state * 2] << 8 | ps_text[ps_state * 2 + 1];

    ps_state++;
    if (ps_state == 4) ps_state = 0;
}

static void get_rds_ecc_group(uint16_t *blocks) {
    blocks[1] |= 1 << 12;
    blocks[2] |= rds_data.ecc;
}

static void get_rds_lic_group(uint16_t *blocks) {
    blocks[1] |= 1 << 12;
    blocks[2] |= rds_data.lic | 0x3000;
}

static void get_rds_rt_group(uint16_t *blocks) {

    if (rds_state.rt_bursting) rds_state.rt_bursting--;

    if (rds_state.rt_update) {
        memcpy(rt_text, rds_data.rt, 64);
        rds_state.rt_ab ^= 1;
        rds_state.rt_update = 0;
        rt_state = 0;
    }

    blocks[1] |= 2 << 12;
    blocks[1] |= rds_state.rt_ab << 4;
    blocks[1] |= rt_state;
    blocks[2] = rt_text[rt_state * 4] << 8;
    blocks[2] |= rt_text[rt_state * 4 + 1];
    blocks[3] = rt_text[rt_state * 4 + 2] << 8;
    blocks[3] |= rt_text[rt_state * 4 + 3];

    rt_state++;
    if (rt_state == rds_state.rt_segments) rt_state = 0;
}

static void get_rds_rtp_oda_group(uint16_t *blocks) {
    blocks[1] |= 3 << 12;
    blocks[1] |= 0x0016;
    blocks[3] |= 0x4BD7;
}

static uint8_t get_rds_ct_group(uint16_t *blocks) {
    struct tm utc_value, local_value, *utc, *local_time, *temp;
    time_t now;
    uint8_t l;
    uint32_t mjd;
    int16_t offset;

    now = time(NULL);
    temp = gmtime(&now);
    if (!temp) return 0;
    utc_value = *temp;
    utc = &utc_value;

    if (utc->tm_min != latest_minutes) {
        latest_minutes = utc->tm_min;

        l = utc->tm_mon <= 1 ? 1 : 0;
        mjd = 14956 + utc->tm_mday +
              (uint32_t)((utc->tm_year - l) * 365.25f) +
              (uint32_t)((utc->tm_mon + 2 + l * 12) * 30.6001f);

        blocks[1] |= 4 << 12 | (mjd >> 15);
        blocks[2] = (mjd << 1) | (utc->tm_hour >> 4);
        blocks[3] = (utc->tm_hour & INT16_L4) << 12 | utc->tm_min << 6;

        temp = localtime(&now);
        if (!temp) return 0;
        local_value = *temp;
        local_time = &local_value;
        /* UTC offset without libc-specific tm members; handles year boundaries. */
        int days = local_time->tm_yday - utc->tm_yday;
        if (days > 1) days = -1;
        if (days < -1) days = 1;
        offset = (int16_t)(((days * 24 + local_time->tm_hour - utc->tm_hour) * 60 +
                           local_time->tm_min - utc->tm_min) / 30);
        if (offset < 0) {
            blocks[3] |= 1 << 5;
            blocks[3] |= abs(offset);
        } else {
            blocks[3] |= offset;
        }

        return 1;
    }

    return 0;
}

static void get_rds_ptyn_group(uint16_t *blocks) {

    if (ptyn_state == 0 && rds_state.ptyn_update) {
        memcpy(ptyn_text, rds_data.ptyn, 8);
        rds_state.ptyn_update = 0;
    }

    blocks[1] |= 10 << 12 | ptyn_state;
    blocks[2] = ptyn_text[ptyn_state * 4] << 8;
    blocks[2] |= ptyn_text[ptyn_state * 4 + 1];
    blocks[3] = ptyn_text[ptyn_state * 4 + 2] << 8;
    blocks[3] |= ptyn_text[ptyn_state * 4 + 3];

    ptyn_state++;
    if (ptyn_state == 2) ptyn_state = 0;
}

static void get_rds_rtp_group(uint16_t *blocks) {
    blocks[1] |= 11 << 12;
    blocks[1] |= rtp_cfg.toggle << 4 | rtp_cfg.running << 3;
    blocks[1] |= (rtp_cfg.type[0] & INT8_U5) >> 3;

    blocks[2] = (rtp_cfg.type[0] & INT8_L3) << 13;
    blocks[2] |= (rtp_cfg.start[0] & INT8_L6) << 7;
    blocks[2] |= (rtp_cfg.len[0] & INT8_L6) << 1;
    blocks[2] |= (rtp_cfg.type[1] & INT8_U3) >> 5;

    blocks[3] = (rtp_cfg.type[1] & INT8_L5) << 11;
    blocks[3] |= (rtp_cfg.start[1] & INT8_L6) << 5;
    blocks[3] |= rtp_cfg.len[1] & INT8_L5;
}

static uint8_t get_rds_other_groups(uint16_t *blocks) {

    if (basic_groups_state) {
        if (rds_data.ecc) {
            if (++group_counter_ecc == 10) {
                group_counter_ecc = 0;
                get_rds_ecc_group(blocks);
                return 1;
            }
        }

        if (rds_data.lic) {
            if (++group_counter_lic == 10) {
                group_counter_lic = 0;
                get_rds_lic_group(blocks);
                return 1;
            }
        }

        if (rtp_cfg.running || rtp_cfg.toggle) {
            if (++group_counter_rtp_oda == 10) {
                group_counter_rtp_oda = 0;
                get_rds_rtp_oda_group(blocks);
                return 1;
            }
        }

        if (rds_data.ptyn[0]) {
            if (++group_counter_ptyn == 10) {
                group_counter_ptyn = 0;
                get_rds_ptyn_group(blocks);
                return 1;
            }
        }

        if (rtp_cfg.running || rtp_cfg.toggle) {
            if (++group_counter_rtp == 10) {
                group_counter_rtp = 0;
                get_rds_rtp_group(blocks);
                return 1;
            }
        }
    }

    return 0;
}

static void get_rds_group(uint16_t *blocks) {

    blocks[0] = rds_data.pi;
    blocks[1] = rds_data.tp << 10;
    blocks[1] |= rds_data.pty << 5;
    blocks[2] = 0;
    blocks[3] = 0;

    if (!(rds_data.tx_ctime && get_rds_ct_group(blocks))) {
        if (!get_rds_other_groups(blocks)) {
            if (group_counter < 4) {
                get_rds_ps_group(blocks);
            } else if (group_counter < 8) {
                get_rds_rt_group(blocks);
            }

            group_counter++;

            if (group_counter > 0 && group_counter < 4) {
                basic_groups_state = 0;
            }else if (group_counter == 4) {
                basic_groups_state = 1;
            } else if (group_counter > 4 && group_counter < 8) {
                basic_groups_state = 0;
            } else if (group_counter == 8) {
                basic_groups_state = 1;
                group_counter = 0;
            }
        }
    }
}

void get_rds_bits(uint8_t *bits) {
    uint16_t out_blocks[GROUP_LENGTH];
    get_rds_group(out_blocks);
    add_checkwords(out_blocks, bits);
}

int init_rds_encoder(struct rds_params_t rds_params) {
    memset(&rds_data, 0, sizeof rds_data);
    memset(&rds_state, 0, sizeof rds_state);
    memset(&rtp_cfg, 0, sizeof rtp_cfg);
    memset(ps_text, 0, sizeof ps_text);
    memset(rt_text, 0, sizeof rt_text);
    memset(ptyn_text, 0, sizeof ptyn_text);
    basic_groups_state = af_state = ps_state = rt_state = ptyn_state = group_counter = 0;
    group_counter_ecc = group_counter_lic = group_counter_rtp_oda = 0;
    group_counter_ptyn = group_counter_rtp = 0;
    latest_minutes = -1;
    if (rds_params.af.num_afs ||
        (rds_params.af.method == AF_METHOD_B && rds_params.af.num_b_pairs)) {
        set_rds_af(rds_params.af);
    }

    set_rds_pi(rds_params.pi);
    set_rds_ecc(rds_params.ecc);
    set_rds_lic(rds_params.lic);
    set_rds_ps(rds_params.ps);
    set_rds_rt(rds_params.rt);
    set_rds_pty(rds_params.pty);
    set_rds_ptyn(rds_params.ptyn);
    set_rds_tp(rds_params.tp);
    set_rds_ms(rds_params.ms);
    set_rds_di(rds_params.di);
    set_rds_ct(1);

    rds_state.rt_ab = 1;

    return init_rds_objects();
}

void exit_rds_encoder(void) {
    exit_rds_objects();
}

void set_rds_pi(uint16_t pi_code) {
    rds_data.pi = pi_code;
}

void set_rds_ecc(uint16_t ecc_code) {
    rds_data.ecc = ecc_code;
}

void set_rds_lic(uint16_t lic_code) {
    rds_data.lic = lic_code;
}

void set_rds_rt(unsigned char *rt) {
    uint8_t len = 0;

    rds_state.rt_update = 1;
    memset(rds_data.rt, ' ', 64);

    while (len < 64 && *rt != 0) {
        rds_data.rt[len++] = *rt++;
    }

    if (len < 64) {
        rds_state.rt_segments = 0;
        rds_data.rt[len++] = '\r';

        while (len % 4 != 0) {
            rds_data.rt[len++] = ' ';
        }

        rds_state.rt_segments = len / 4;
    } else {
        rds_state.rt_segments = 16;
    }

    rds_state.rt_bursting = rds_state.rt_segments;
}

void set_rds_ps(unsigned char *ps) {
    uint8_t len = 0;
    uint8_t spaces_on_left = 0;
    uint8_t remaining_spaces = 0;

    rds_state.ps_update = 1;
    memset(rds_data.ps, ' ', 8);

    while (len < 8 && *ps != 0) {
        rds_data.ps[len++] = *ps++;
    }

    if (len < 8) {
        remaining_spaces = 8 - len;
        if (remaining_spaces % 2 == 0) {
            spaces_on_left = remaining_spaces / 2;
        } else {
            spaces_on_left = remaining_spaces / 2 + 1;
        }

        for (int i = len - 1; i >= 0; i--) {
            rds_data.ps[i + spaces_on_left] = rds_data.ps[i];
        }

        for (int i = 0; i < spaces_on_left; i++) {
            rds_data.ps[i] = ' ';
        }
    }
}

void set_rds_rtp_flags(uint8_t flags) {
    rtp_cfg.running = (flags & INT8_1) >> 1;
    rtp_cfg.toggle = flags & INT8_0;
}

void set_rds_rtp_tags(uint8_t *tags) {
    rtp_cfg.type[0] = tags[0] & INT8_L6;
    rtp_cfg.start[0] = tags[1] & INT8_L6;
    rtp_cfg.len[0] = tags[2] & INT8_L6;
    rtp_cfg.type[1] = tags[3] & INT8_L6;
    rtp_cfg.start[1] = tags[4] & INT8_L6;
    rtp_cfg.len[1] = tags[5] & INT8_L5;
}

void set_rds_af(struct rds_af_t new_af_list) {
    if (new_af_list.method == AF_METHOD_B) {
        if (!new_af_list.tuning_code || new_af_list.tuning_code > 204 ||
            !new_af_list.num_b_pairs || new_af_list.num_b_pairs > AF_B_MAX_PAIRS) return;
    } else if (new_af_list.method == AF_METHOD_A) {
        if (new_af_list.num_entries > sizeof new_af_list.afs || new_af_list.num_afs > 25 ||
            new_af_list.num_afs > new_af_list.num_entries) return;
    } else return;
    rds_data.af = new_af_list;
    af_state = 0;
}

void set_rds_pty(uint8_t pty) {
    rds_data.pty = pty & INT8_L5;
}

void set_rds_ptyn(unsigned char *ptyn) {
    uint8_t len = 0;

    if (!ptyn[0]) {
        memset(rds_data.ptyn, 0, 8);
        return;
    }

    rds_state.ptyn_update = 1;
    memset(rds_data.ptyn, ' ', 8);
    while (len < 8 && *ptyn != 0)
        rds_data.ptyn[len++] = *ptyn++;
}

void set_rds_ta(uint8_t ta) {
    rds_data.ta = ta & INT8_0;
}

void set_rds_tp(uint8_t tp) {
    rds_data.tp = tp & INT8_0;
}

void set_rds_ms(uint8_t ms) {
    rds_data.ms = ms & INT8_0;
}

void set_rds_di(uint8_t di) {
    rds_data.di = di & INT8_L4;
}

void set_rds_ct(uint8_t ct) {
    rds_data.tx_ctime = ct & INT8_0;
}

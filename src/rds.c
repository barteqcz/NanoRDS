#include "common.h"
#include "rds.h"
#include "modulator.h"
#include "lib.h"

static struct rds_params_t rds_data;

/* RDS data control */
static struct {
	uint8_t ps_update;
	uint8_t rt_update;
	uint8_t rt_segments;
	uint8_t rt_bursting;
	uint8_t rt_ab;
	uint8_t ptyn_update;
} rds_state;

/* RT+ data control */
static struct {
	uint8_t running;
	uint8_t toggle;
	uint8_t type[2];
	uint8_t start[2];
	uint8_t len[2];
} rtp_cfg;

/* Get the next AF entry */
static uint16_t get_next_af() {
	static uint8_t af_state;
	uint16_t out;

	if (rds_data.af.num_afs) {
		if (af_state == 0) {
			out = (AF_CODE_NUM_AFS_BASE + rds_data.af.num_afs) << 8;
			out |= rds_data.af.afs[0];
			af_state += 1;
		} else {
			out = rds_data.af.afs[af_state] << 8;
			if (rds_data.af.afs[af_state + 1])
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

/* 0A */
static void get_rds_ps_group(uint16_t *blocks) {
	static unsigned char ps_text[PS_LENGTH];
	static uint8_t ps_state;

	if (ps_state == 0 && rds_state.ps_update) {
		memcpy(ps_text, rds_data.ps, PS_LENGTH);
		rds_state.ps_update = 0; /* rewind */
	}

	/* TA */
	blocks[1] |= rds_data.ta << 4;

	/* MS */
	blocks[1] |= rds_data.ms << 3;

	/* DI */
	blocks[1] |= ((rds_data.di >> (3 - ps_state)) & INT8_0) << 2;

	/* PS segment address */
	blocks[1] |= ps_state;

	/* AF */
	blocks[2] = get_next_af();

	/* PS */
	blocks[3] = ps_text[ps_state * 2] << 8 | ps_text[ps_state * 2 + 1];

	ps_state++;
	if (ps_state == 4) ps_state = 0;
}

/* ECC (1A) */
static void get_rds_ecc_group(uint16_t *blocks) {
	blocks[1] |= 1 << 12;
	blocks[2] |= rds_data.ecc;
}

/* LIC (1A) */
static void get_rds_lic_group(uint16_t *blocks) {
	blocks[1] |= 1 << 12;
	blocks[2] |= rds_data.lic | 0x3000;
}

/* RT (2A) */
static void get_rds_rt_group(uint16_t *blocks) {
	static unsigned char rt_text[RT_LENGTH];
	static uint8_t rt_state;

	if (rds_state.rt_bursting) rds_state.rt_bursting--;

	if (rds_state.rt_update) {
		memcpy(rt_text, rds_data.rt, RT_LENGTH);
		rds_state.rt_ab ^= 1;
		rds_state.rt_update = 0;
		rt_state = 0; /* rewind when new RT arrives */
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

/* RT+ ODA (3A) */
static void get_rds_rtp_oda_group(uint16_t *blocks) {
	blocks[1] |= 3 << 12;
	blocks[1] |= 0x0016;
	blocks[3] |= 0x4BD7;
}

/* CT (4A) */
static uint8_t get_rds_ct_group(uint16_t *blocks) {
	static uint8_t latest_minutes;
	struct tm *utc, *local_time;
	time_t now;
	uint8_t l;
	uint32_t mjd;
	int16_t offset;

	/* check time */
	now = time(NULL);
	utc = gmtime(&now);

	if (utc->tm_min != latest_minutes) {
		/* generate CT group */
		latest_minutes = utc->tm_min;

		l = utc->tm_mon <= 1 ? 1 : 0;
		mjd = 14956 + utc->tm_mday +
			(uint32_t)((utc->tm_year - l) * 365.25f) +
			(uint32_t)((utc->tm_mon + 2 + l * 12) * 30.6001f);

		blocks[1] |= 4 << 12 | (mjd >> 15);
		blocks[2] = (mjd << 1) | (utc->tm_hour >> 4);
		blocks[3] = (utc->tm_hour & INT16_L4) << 12 | utc->tm_min << 6;

		/* get local time (for the offset) */
		local_time = localtime(&now);

		/* tm_gmtoff doesn't exist in POSIX but __tm_gmtoff does */
		offset = local_time->__tm_gmtoff / (30 * 60);
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

/* PTYN (10A) */
static void get_rds_ptyn_group(uint16_t *blocks) {
	static unsigned char ptyn_text[PTYN_LENGTH];
	static uint8_t ptyn_state;

	if (ptyn_state == 0 && rds_state.ptyn_update) {
		memcpy(ptyn_text, rds_data.ptyn, PTYN_LENGTH);
		rds_state.ptyn_update = 0;
	}

	blocks[1] |= 10 << 12 | ptyn_state;
	blocks[2] =  ptyn_text[ptyn_state * 4    ] << 8;
	blocks[2] |= ptyn_text[ptyn_state * 4 + 1];
	blocks[3] =  ptyn_text[ptyn_state * 4 + 2] << 8;
	blocks[3] |= ptyn_text[ptyn_state * 4 + 3];

	ptyn_state++;
	if (ptyn_state == 2) ptyn_state = 0;
}

/* RT+ (11A)*/
static void get_rds_rtp_group(uint16_t *blocks) {
	blocks[1] |= 11 << 12;;
	blocks[1] |= rtp_cfg.toggle << 4 | rtp_cfg.running << 3;
	blocks[1] |= (rtp_cfg.type[0] & INT8_U5) >> 3;

	blocks[2] =  (rtp_cfg.type[0] & INT8_L3) << 13;
	blocks[2] |= (rtp_cfg.start[0] & INT8_L6) << 7;
	blocks[2] |= (rtp_cfg.len[0] & INT8_L6) << 1;
	blocks[2] |= (rtp_cfg.type[1] & INT8_U3) >> 5;

	blocks[3] =  (rtp_cfg.type[1] & INT8_L5) << 11;
	blocks[3] |= (rtp_cfg.start[1] & INT8_L6) << 5;
	blocks[3] |= rtp_cfg.len[1] & INT8_L5;
}

/* Subsequence for lower priority groups */
static uint8_t get_rds_other_groups(uint16_t *blocks) {
	static uint8_t group_counter_ecc = 0, group_counter_lic = 0, group_counter_rtp_oda = 0, group_counter_ptyn = 0, group_counter_rtp = 0;

	/* ECC (1A) */
	if (rds_data.ecc) {
		if (++group_counter_ecc == 25) {
			group_counter_ecc = 0;
			get_rds_ecc_group(blocks);
			return 1;
		}
	}

	/* LIC (1A) */
	if (rds_data.lic) {
		if (++group_counter_lic == 25) {
			group_counter_lic = 0;
			get_rds_lic_group(blocks);
			return 1;
		}
	}
	
	/* RT+ ODA (3A) */
	if (rtp_cfg.running || rtp_cfg.toggle) {
		if (++group_counter_rtp_oda == 25) {
			group_counter_rtp_oda = 0;
			get_rds_rtp_oda_group(blocks);
			return 1;
		}
	}

	/* PTYN (10A) */
	if (rds_data.ptyn[0]) {
		if (++group_counter_ptyn == 10) {
			group_counter_ptyn = 0;
			get_rds_ptyn_group(blocks);
			return 1;
		}
	}

	/* RT+ (11A) */
	if (rtp_cfg.running || rtp_cfg.toggle) {
		if (++group_counter_rtp == 25) {
			group_counter_rtp = 0;
			get_rds_rtp_group(blocks);
			return 1;
		}
	}

	return 0;
}

/* Creates an RDS group */
static void get_rds_group(uint16_t *blocks) {
	static uint8_t group_counter;

	/* Basic block data - this applies to all the groups */
	blocks[0] = rds_data.pi;
	blocks[1] = rds_data.tp << 10;
	blocks[1] |= rds_data.pty << 5;
	blocks[2] = 0;
	blocks[3] = 0;

	/* Generate the main RDS groups sequence */
	if (!(rds_data.tx_ctime && get_rds_ct_group(blocks))) {
		if (!get_rds_other_groups(blocks)) {
			if (group_counter < 4 ) {
				get_rds_ps_group(blocks);
			} else if (group_counter < 8) {
				get_rds_rt_group(blocks);
			}

			group_counter++;
			if (group_counter == 8) {
				group_counter = 0;
			}
		}
	}
}

void get_rds_bits(uint8_t *bits) {
	static uint16_t out_blocks[GROUP_LENGTH];
	get_rds_group(out_blocks);
	add_checkwords(out_blocks, bits);
}

void init_rds_encoder(struct rds_params_t rds_params) {

	/* AF */
	if (rds_params.af.num_afs) {
		set_rds_af(rds_params.af);
		fprintf(stderr, show_af_list(rds_params.af));
	}

	set_rds_pi(rds_params.pi);
	set_rds_ecc(rds_params.ecc);
	set_rds_lic(rds_params.lic);
	set_rds_ps(rds_params.ps);
	set_rds_rt(rds_params.rt);
	rds_state.rt_ab = 1;
	set_rds_pty(rds_params.pty);
	set_rds_ptyn(rds_params.ptyn);
	set_rds_tp(rds_params.tp);
	set_rds_ct(1);
	set_rds_ms(1);
	set_rds_di(DI_STEREO);

	init_rds_objects();
}

void exit_rds_encoder() {
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

    if (strlen((char*)rt) > 64) {
        memcpy((char*)rds_data.rt, (char*)rt, 64);
        rds_data.rt[63] = '\0'; // Ensure null termination
    }

    rds_state.rt_update = 1;
    memset(rds_data.rt, ' ', RT_LENGTH);

    while (*rt != 0 && len < RT_LENGTH) {
        rds_data.rt[len++] = *rt++;
    }

    if (len < RT_LENGTH) {
        rds_state.rt_segments = 0;
        rds_data.rt[len++] = '\r';

        while (len % 4 != 0) { // Pad with spaces if necessary for segment alignment
            rds_data.rt[len++] = ' ';
        }

        rds_state.rt_segments = len / 4;
    } else {
        rds_state.rt_segments = 16; // Maximum segments
    }

    rds_state.rt_bursting = rds_state.rt_segments;
}

void set_rds_ps(unsigned char *ps) {
    uint8_t len = 0;
    uint8_t spaces_on_left = 0;
    uint8_t spaces_on_right = 0;
    uint8_t remaining_spaces = 0;

    rds_state.ps_update = 1;
    memset(rds_data.ps, ' ', PS_LENGTH);

    while (*ps != 0 && len < PS_LENGTH) {
        rds_data.ps[len++] = *ps++;
    }

    if (len < PS_LENGTH) {
        remaining_spaces = PS_LENGTH - len;
        if (remaining_spaces % 2 == 0) {
            spaces_on_left = spaces_on_right = remaining_spaces / 2;
        } else {
            spaces_on_left = remaining_spaces / 2 + 1;
            spaces_on_right = remaining_spaces / 2;
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
	rtp_cfg.running	= (flags & INT8_1) >> 1;
	rtp_cfg.toggle	= flags & INT8_0;
}

void set_rds_rtp_tags(uint8_t *tags) {
	rtp_cfg.type[0]	= tags[0] & INT8_L6;
	rtp_cfg.start[0] = tags[1] & INT8_L6;
	rtp_cfg.len[0] = tags[2] & INT8_L6;
	rtp_cfg.type[1] = tags[3] & INT8_L6;
	rtp_cfg.start[1] = tags[4] & INT8_L6;
	rtp_cfg.len[1] = tags[5] & INT8_L5;
}

void set_rds_af(struct rds_af_t new_af_list) {
	memcpy(&rds_data.af, &new_af_list, sizeof(struct rds_af_t));
}

void clear_rds_af() {
	memset(&rds_data.af, 0, sizeof(struct rds_af_t));
}

void set_rds_pty(uint8_t pty) {
	rds_data.pty = pty & INT8_L5;
}

void set_rds_ptyn(unsigned char *ptyn) {
	uint8_t len = 0;

	if (!ptyn[0]) {
		memset(rds_data.ptyn, 0, PTYN_LENGTH);
		return;
	}

	rds_state.ptyn_update = 1;
	memset(rds_data.ptyn, ' ', PTYN_LENGTH);
	while (*ptyn != 0 && len <= PTYN_LENGTH)
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

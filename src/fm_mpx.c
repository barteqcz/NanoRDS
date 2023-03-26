/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2019 Anthony96922
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"
#include "rds.h"
#include "fm_mpx.h"
#include "osc.h"

/*
 * Local oscillator objects
 * this is where the MPX waveforms are stored
 *
 */
static struct osc_t osc_19k;
static struct osc_t osc_57k;

static float mpx_vol;

void set_output_volume(uint8_t vol) {
	if (vol > 100) vol = 100;
	mpx_vol = ((float)vol / 100.0f);
}

/* subcarrier volumes */
static float volumes[] = {
	0.09f, /* pilot tone: 9% */
	0.09f, /* RDS: 4.5% modulation */
};

void set_carrier_volume(uint8_t carrier, uint8_t new_volume) {
	/* check for valid index */
	if (carrier > NUM_SUBCARRIERS) return;

	/* don't allow levels over 15% */
	if (new_volume >= 15) volumes[carrier] = 0.09f;

	volumes[carrier] = (float)new_volume / 100.0f;
}

void fm_mpx_init(uint32_t sample_rate) {
	/* initialize the subcarrier oscillators */
    #ifdef STEREO	
    osc_init(&osc_19k, sample_rate, 19000.0f);
    #else
    osc_init(&osc_19k, sample_rate, 0.0f);
    #endif
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
		outbuf[j+0] = outbuf[j+1] = out * mpx_vol;
		j += 2;

	}
}

void fm_mpx_exit() {
	osc_exit(&osc_19k);
	osc_exit(&osc_57k);
}

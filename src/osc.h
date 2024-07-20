/* context for MPX oscillator */
typedef struct osc_t {
	/* the sample rate at which the oscillator operates */
	uint32_t sample_rate;

	/* frequency for this instance */
	float freq;

	/* Arrays of carrier wave constants */
	float *sin_wave;
	float *cos_wave;

	/* Wave phase */
	uint16_t cur;
	uint16_t max;
} osc_t;

extern void osc_init(struct osc_t *osc, uint32_t sample_rate, const float freq);
extern float osc_get_sin(struct osc_t *osc);
extern float osc_get_cos(struct osc_t *osc);
extern void osc_update_pos(struct osc_t *osc);
extern void osc_exit(struct osc_t *osc);

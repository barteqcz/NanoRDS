typedef struct osc_t {
	uint32_t sample_rate;

	float freq;

	float *sin_wave;
	float *cos_wave;

	uint16_t cur;
	uint16_t max;
} osc_t;

extern void osc_init(struct osc_t *osc, uint32_t sample_rate, const float freq);
extern float osc_get_sin(struct osc_t *osc);
extern float osc_get_cos(struct osc_t *osc);
extern void osc_update_pos(struct osc_t *osc);
extern void osc_exit(struct osc_t *osc);
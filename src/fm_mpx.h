/* MPX */
#define NUM_MPX_FRAMES_IN	1024
#define NUM_MPX_FRAMES_OUT	(NUM_MPX_FRAMES_IN * 2)

/* The sample rate at which the MPX generation runs at */
#define MPX_SAMPLE_RATE		RDS_SAMPLE_RATE
#define OUTPUT_SAMPLE_RATE	192000

extern void fm_mpx_init(uint32_t sample_rate);
extern void fm_rds_get_frames(float *outbuf, size_t num_frames);
extern void fm_mpx_exit();
extern void set_output_volume(uint8_t vol);
extern void set_carrier_volume(uint8_t carrier, uint8_t new_volume);

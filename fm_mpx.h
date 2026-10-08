#ifndef NANORDS_FM_MPX_H
#define NANORDS_FM_MPX_H
#include <stdint.h>
#include <stddef.h>
#include "rds.h"
#define NUM_MPX_FRAMES_IN 1024
#define NUM_MPX_FRAMES_OUT (NUM_MPX_FRAMES_IN * 2)
#define MPX_SAMPLE_RATE RDS_SAMPLE_RATE
#define OUTPUT_SAMPLE_RATE 192000
int fm_mpx_init(uint32_t sample_rate);
/* Generates mono float frames; stereo packing happens at the device boundary. */
void fm_rds_get_frames(float *outbuf, size_t num_frames);
void fm_mpx_exit(void);
void set_carrier_volume(uint8_t carrier, float new_volume);
#endif

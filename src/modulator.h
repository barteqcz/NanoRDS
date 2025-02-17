#define NUM_STREAMS	1

typedef struct rds_t {
	uint8_t *bit_buffer;
	uint8_t bit_pos;
	float *sample_buffer;
	uint8_t prev_output;
	uint8_t cur_output;
	uint8_t cur_bit;
	uint8_t sample_count;
	uint16_t in_sample_index;
	uint16_t out_sample_index;
	uint8_t symbol_shift;
	float *symbol_shift_buf;
	uint8_t symbol_shift_buf_idx;
} rds_t;

extern void init_rds_objects();
extern void exit_rds_objects();
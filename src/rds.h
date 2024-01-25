#ifndef RDS_H
#define RDS_H

/* The RDS error-detection code generator polynomial is
 * x^10 + x^8 + x^7 + x^5 + x^4 + x^3 + x^0
 */
#define POLY			0x1B9
#define POLY_DEG		10
#define MSB_BIT			0x8000
#define BLOCK_SIZE		16

#define GROUP_LENGTH		4
#define BITS_PER_GROUP		(GROUP_LENGTH * (BLOCK_SIZE+POLY_DEG))
#define RDS_SAMPLE_RATE		190000
#define SAMPLES_PER_BIT		160
#define FILTER_SIZE		1120
#define SAMPLE_BUFFER_SIZE	(SAMPLES_PER_BIT + FILTER_SIZE)

/* Text items
 *
 */
#define RT_LENGTH 64
#define PS_LENGTH 8
#define PTYN_LENGTH 8
#define LPS_LENGTH	32

/* AF list size
 *
 */
#define MAX_AFS 25

typedef struct rds_af_t {
	uint8_t num_entries;
	uint8_t num_afs;
	uint8_t afs[MAX_AFS*2]; // doubled for LF/MF coding
} rds_af_t;

// AF codes
#define AF_CODE_FILLER		205
#define AF_CODE_NO_AF		224
#define AF_CODE_NUM_AFS_BASE	AF_CODE_NO_AF
#define AF_CODE_LFMF_FOLLOWS	250

typedef struct rds_params_t {
	uint16_t pi;
	uint8_t ta;
	uint8_t pty;
	uint8_t tp;
	uint8_t ms;
	uint8_t di;
	// PS
	char ps[PS_LENGTH];
	// RT
	char rt[RT_LENGTH];
	// PTYN
	char ptyn[PTYN_LENGTH];

	// AF
	struct rds_af_t af;

	uint8_t tx_ctime;

	/* Long PS */
	char lps[LPS_LENGTH];
} rds_params_t;
/* Here, the first member of the struct must be a scalar to avoid a
   warning on -Wmissing-braces with GCC < 4.8.3
   (bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119)
*/

/* Group type
 *
 * 0-15
 */
#define GROUP_TYPE_0	(0 << 4)
#define GROUP_TYPE_1	(1 << 4)
#define GROUP_TYPE_2	(2 << 4)
#define GROUP_TYPE_3	(3 << 4)
#define GROUP_TYPE_4	(4 << 4)
#define GROUP_TYPE_5	(5 << 4)
#define GROUP_TYPE_6	(6 << 4)
#define GROUP_TYPE_7	(7 << 4)
#define GROUP_TYPE_8	(8 << 4)
#define GROUP_TYPE_9	(9 << 4)
#define GROUP_TYPE_10	(10 << 4)
#define GROUP_TYPE_11	(11 << 4)
#define GROUP_TYPE_12	(12 << 4)
#define GROUP_TYPE_13	(13 << 4)
#define GROUP_TYPE_14	(14 << 4)
#define GROUP_TYPE_15	(15 << 4)

/* Group versions
 *
 * The first 4 bits are the group number and the remaining 4 are
 * the group version
 */
#define GROUP_VER_A	0
#define GROUP_VER_B	1

// Version A groups
#define GROUP_0A	(GROUP_TYPE_0 | GROUP_VER_A)
#define GROUP_1A	(GROUP_TYPE_1 | GROUP_VER_A)
#define GROUP_2A	(GROUP_TYPE_2 | GROUP_VER_A)
#define GROUP_3A	(GROUP_TYPE_3 | GROUP_VER_A)
#define GROUP_4A	(GROUP_TYPE_4 | GROUP_VER_A)
#define GROUP_5A	(GROUP_TYPE_5 | GROUP_VER_A)
#define GROUP_6A	(GROUP_TYPE_6 | GROUP_VER_A)
#define GROUP_7A	(GROUP_TYPE_7 | GROUP_VER_A)
#define GROUP_8A	(GROUP_TYPE_8 | GROUP_VER_A)
#define GROUP_9A	(GROUP_TYPE_9 | GROUP_VER_A)
#define GROUP_10A	(GROUP_TYPE_10 | GROUP_VER_A)
#define GROUP_11A	(GROUP_TYPE_11 | GROUP_VER_A)
#define GROUP_12A	(GROUP_TYPE_12 | GROUP_VER_A)
#define GROUP_13A	(GROUP_TYPE_13 | GROUP_VER_A)
#define GROUP_14A	(GROUP_TYPE_14 | GROUP_VER_A)
#define GROUP_15A	(GROUP_TYPE_15 | GROUP_VER_A)

// Version B groups
#define GROUP_0B	(GROUP_TYPE_0 | GROUP_VER_B)
#define GROUP_1B	(GROUP_TYPE_1 | GROUP_VER_B)
#define GROUP_2B	(GROUP_TYPE_2 | GROUP_VER_B)
#define GROUP_3B	(GROUP_TYPE_3 | GROUP_VER_B)
#define GROUP_4B	(GROUP_TYPE_4 | GROUP_VER_B)
#define GROUP_5B	(GROUP_TYPE_5 | GROUP_VER_B)
#define GROUP_6B	(GROUP_TYPE_6 | GROUP_VER_B)
#define GROUP_7B	(GROUP_TYPE_7 | GROUP_VER_B)
#define GROUP_8B	(GROUP_TYPE_8 | GROUP_VER_B)
#define GROUP_9B	(GROUP_TYPE_9 | GROUP_VER_B)
#define GROUP_10B	(GROUP_TYPE_10 | GROUP_VER_B)
#define GROUP_11B	(GROUP_TYPE_11 | GROUP_VER_B)
#define GROUP_12B	(GROUP_TYPE_12 | GROUP_VER_B)
#define GROUP_13B	(GROUP_TYPE_13 | GROUP_VER_B)
#define GROUP_14B	(GROUP_TYPE_14 | GROUP_VER_B)
#define GROUP_15B	(GROUP_TYPE_15 | GROUP_VER_B)

#define GET_GROUP_TYPE(x)	((x >> 4) & 15)
#define GET_GROUP_VER(x)	(x & 1) // only check bit 0

#define DI_STEREO	(1 << 0) // 1 - Stereo
#define DI_AH		(1 << 1) // 2 - Artificial Head
#define DI_COMPRESSED	(1 << 2) // 4 - Compressed
#define DI_DPTY		(1 << 3) // 8 - Dynamic PTY

// Bit mask
// Lower
#define INT8_L1		0x01
#define INT8_L2		0x03
#define INT8_L3		0x07
#define INT8_L4		0x0f
#define INT8_L5		0x1f
#define INT8_L6		0x3f
#define INT8_L7		0x7f
// Upper
#define INT8_U7		0xfe
#define INT8_U6		0xfc
#define INT8_U5		0xf8
#define INT8_U4		0xf0
#define INT8_U3		0xe0
#define INT8_U2		0xc0
#define INT8_U1		0x80
// Single
#define INT8_0		0x01
#define INT8_1		0x02
#define INT8_2		0x04
#define INT8_3		0x08
#define INT8_4		0x10
#define INT8_5		0x20
#define INT8_6		0x40
#define INT8_7		0x80

/* RDS ODA ID group
 *
 * This struct is for defining ODAs that will be transmitted
 *
 * Can signal version A or B data groups
 */
typedef struct rds_oda_t {
	uint8_t group;
	uint16_t aid;
	uint16_t scb;
} rds_oda_t;

#define MAX_ODAS	8

extern void init_rds_encoder(struct rds_params_t rds_params, char *call_sign);
extern void exit_rds_encoder();
extern void get_rds_bits(uint8_t *bits);
extern void set_rds_pi(uint16_t pi_code);
extern void set_rds_rt(char *rt);
extern void set_rds_ps(char *ps);
extern void set_rds_lps(char *lps);
extern void set_rds_rtplus_flags(uint8_t running, uint8_t toggle);
extern void set_rds_rtplus_tags(uint8_t *tags);
extern void set_rds_ta(uint8_t ta);
extern void set_rds_pty(uint8_t pty);
extern void set_rds_ptyn(char *ptyn);
extern void set_rds_af(struct rds_af_t new_af_list);
extern void set_rds_tp(uint8_t tp);
extern void set_rds_ms(uint8_t ms);
extern void set_rds_ct(uint8_t ct);
extern void set_rds_di(uint8_t di);
extern float get_rds_sample(uint8_t stream_num);

#endif /* RDS_H */

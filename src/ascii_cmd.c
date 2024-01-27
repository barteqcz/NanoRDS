#include "common.h"
#include "rds.h"
#include "fm_mpx.h"
#include "lib.h"

/*
 * If a command is received, process it and update the RDS data.
 *
 */
void process_ascii_cmd(char *cmd) {
	char *arg;
	uint8_t cmd_len = 0;

	while (cmd[cmd_len] != 0 && cmd_len < 255)
		cmd_len++;

	if (cmd_len > 3 && cmd[2] == ' ') {
		arg = cmd + 3;
	
		if (strncmp(cmd, "AF", 2) == 0) {
			uint8_t arg_count;
			rds_af_t new_af;
			char af_cmd;
			float af[MAX_AFS], *af_iter;

			arg_count = sscanf((char *)arg,
				"%c " /* AF command */
				"%f %f %f %f %f " /* AF list */
				"%f %f %f %f %f "
				"%f %f %f %f %f "
				"%f %f %f %f %f "
				"%f %f %f %f %f",
			&af_cmd,
			&af[0],  &af[1],  &af[2],  &af[3],  &af[4],
			&af[5],  &af[6],  &af[7],  &af[8],  &af[9],
			&af[10], &af[11], &af[12], &af[13], &af[14],
			&af[15], &af[16], &af[17], &af[18], &af[19],
			&af[20], &af[21], &af[22], &af[23], &af[24]);
			switch (af_cmd) {
			case 'a': /* add */
				af_iter = af;
				memset(&new_af, 0, sizeof(struct rds_af_t));
				while ((arg_count-- - 1) != 0) {
					add_rds_af(&new_af, *af_iter++);
				}
				set_rds_af(new_af);
				break;
			case 'r': /* reset */
				memset(&new_af, 0, sizeof(struct rds_af_t));
				set_rds_af(new_af);
				break;
			default: /* other */
				return;
			}
		}

		if (strncmp(cmd, "PI", 2) == 0) {
			arg[4] = 0;
			set_rds_pi(strtoul(arg, NULL, 16));
			return;
		}
		if (strncmp(cmd, "PS", 2) == 0) {
			arg[PS_LENGTH] = 0;
			set_rds_ps(arg);
			return;
		}
		if (strncmp(cmd, "RT", 2) == 0) {
			arg[RT_LENGTH] = 0;
			set_rds_rt(arg);
			return;
		}
		if (strncmp(cmd, "TA", 2) == 0) {
			set_rds_ta(arg[0] == '1' ? 1 : 0);
			return;
		}
		if (strncmp(cmd, "TP", 2) == 0) {
			set_rds_tp(arg[0] == '1' ? 1 : 0);
			return;
		}
		if (strncmp(cmd, "MS", 2) == 0) {
			set_rds_ms(arg[0] == '1' ? 1 : 0);
			return;
		}
		if (strncmp(cmd, "DI", 2) == 0) {
			arg[2] = 0;
			set_rds_di(strtoul(arg, NULL, 10));
			return;
		}
	}

	if (cmd_len > 4 && cmd[3] == ' ') {
		arg = cmd + 4;

		if (strncmp(cmd, "PTY", 3) == 0) {
			set_rds_pty(strtoul(arg, NULL, 10));
			return;
		}
		if (strncmp(cmd, "RTP", 3) == 0) {
			uint8_t tags[8];
			if (sscanf(arg, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
				&tags[0], &tags[1], &tags[2], &tags[3],
				&tags[4], &tags[5]) == 6) {
				set_rds_rtplus_tags(tags);
			}
			return;
		}
		if (strncmp(cmd, "MPX", 3) == 0) {
			uint8_t gains[5];
			if (sscanf(arg, "%hhu,%hhu", &gains[0], &gains[1]) == 2) {
				for (uint8_t i = 0; i < 2; i++) {
					set_carrier_volume(i, gains[i]);
				}
			}
			return;
		}
		if (strncmp(cmd, "VOL", 3) == 0) {
			arg[4] = 0;
			set_output_volume(strtoul(arg, NULL, 10));
			return;
		}
	}

	if (cmd_len > 5 && cmd[4] == ' ') {
		arg = cmd + 5;

		if (strncmp(cmd, "RTPF", 4) == 0) {
			uint8_t rtp_flags[2];
			if (sscanf(arg, "%hhu,%hhu",
				&rtp_flags[0], &rtp_flags[1]) == 2) {
				set_rds_rtplus_flags(
					rtp_flags[0], rtp_flags[1]);
			}
			return;
		}
		if (strncmp(cmd, "PTYN", 4) == 0) {
			arg[8] = 0;
			if (arg[0] == '-') {
				char tmp[1];
				tmp[0] = 0;
				set_rds_ptyn(tmp);
			} else {
				set_rds_ptyn(arg);
			}
			return;
		}
	}
}

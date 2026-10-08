#include "common.h"
#include "rds.h"
#include "fm_mpx.h"
#include "lib.h"
#include "ascii_cmd.h"
#include <ctype.h>
#include <errno.h>

static int set_af_command(char *arg) {
    rds_af_t list = {0};
    float seen[25];
    size_t count = 0;
    char *cursor;
    if (!strcmp(arg, "c")) { set_rds_af(list); return 0; }
    if (arg[0] != 's' || (arg[1] && !isspace((unsigned char)arg[1]))) return -1;
    cursor = arg + 1;
    while (*cursor) {
        char *end;
        float frequency;
        int duplicate = 0;
        while (isspace((unsigned char)*cursor)) ++cursor;
        if (!*cursor) break;
        errno = 0;
        frequency = strtof(cursor, &end);
        if (errno || end == cursor || !isfinite(frequency) ||
            (*end && !isspace((unsigned char)*end))) return -1;
        for (size_t i = 0; i < count; ++i) if (seen[i] == frequency) duplicate = 1;
        if (!duplicate) {
            if (count == 25 || add_rds_af(&list, frequency)) return -1;
            seen[count++] = frequency;
        }
        cursor = end;
    }
    set_rds_af(list);
    return 0;
}

static int set_rtp_command(char *arg) {
    char *parts[6];
    uint8_t tags[6];
    char *cursor = arg;
    for (size_t i = 0; i < 6; ++i) {
        char *comma;
        unsigned long number;
        parts[i] = cursor;
        comma = strchr(cursor, ',');
        if ((i < 5 && !comma) || (i == 5 && comma)) return -1;
        if (comma) { *comma = 0; cursor = comma + 1; }
        if (parse_uint(parts[i], 10, i == 5 ? 31 : 63, &number)) {
            if (i != 0 && i != 3) return -1;
            tags[i] = get_rtp_tag_id(parts[i]);
            if (strcmp(get_rtp_tag_name(tags[i]), parts[i])) return -1;
        } else tags[i] = (uint8_t)number;
    }
    if ((unsigned)tags[1] + tags[2] > 63 || (unsigned)tags[4] + tags[5] > 63) return -1;
    set_rds_rtp_tags(tags);
    return 0;
}

void process_ascii_cmd(unsigned char *text) {
    char *command = (char *)text;
    char *arg;
    unsigned long number;
    float level;
    static const struct {
        const char *name;
        unsigned long limit;
        void (*set)(uint8_t);
    } numeric[] = {
        {"TA", 1, set_rds_ta}, {"TP", 1, set_rds_tp}, {"MS", 1, set_rds_ms},
        {"DI", 15, set_rds_di}, {"PTY", 31, set_rds_pty},
        {"RTPF", 3, set_rds_rtp_flags}, {"CT", 1, set_rds_ct}
    };
    if (!strcmp(command, "RESET")) {
        uint8_t tags[6] = {0};
        rds_af_t af = {0};
        set_rds_pi(0x1000); set_rds_ecc(0); set_rds_lic(0);
        set_rds_ps((unsigned char *)"NanoRDS");
        set_rds_rt((unsigned char *)"NanoRDS - software RDS encoder");
        set_rds_ptyn((unsigned char *)"");
        set_rds_pty(0); set_rds_ta(0); set_rds_tp(0); set_rds_ms(0); set_rds_di(0);
        set_rds_af(af); set_rds_rtp_tags(tags); set_rds_rtp_flags(0); set_rds_ct(1);
        return;
    }
    arg = strchr(command, ' ');
    if (!arg) goto invalid;
    *arg++ = 0;
    if (!strcmp(command, "PS")) { set_rds_ps(xlat((unsigned char *)arg)); return; }
    if (!strcmp(command, "RT")) { set_rds_rt(xlat((unsigned char *)arg)); return; }
    if (!strcmp(command, "PTYN")) {
        set_rds_ptyn(!strcmp(arg, "-") ? (unsigned char *)"" : xlat((unsigned char *)arg));
        return;
    }
    if (!strcmp(command, "AF")) { if (set_af_command(arg)) goto invalid; return; }
    if (!strcmp(command, "RTP")) { if (set_rtp_command(arg)) goto invalid; return; }
    for (size_t i = 0; i < sizeof numeric / sizeof numeric[0]; ++i) {
        if (!strcmp(command, numeric[i].name)) {
            if (parse_uint(arg, 10, numeric[i].limit, &number)) goto invalid;
            numeric[i].set((uint8_t)number);
            return;
        }
    }
    if (!strcmp(command, "PI")) {
#ifdef RBDS
        if (arg[0] == 'K' || arg[0] == 'k' || arg[0] == 'W' || arg[0] == 'w') {
            uint16_t pi = callsign2pi((unsigned char *)arg);
            if (!pi) goto invalid;
            set_rds_pi(pi);
            return;
        }
#endif
        if (parse_uint(arg, 16, 65535, &number)) goto invalid;
        set_rds_pi((uint16_t)number);
        return;
    }
    if (!strcmp(command, "ECC") || !strcmp(command, "LIC")) {
        int ecc = !strcmp(command, "ECC");
        if (parse_uint(arg, 16, ecc ? 255 : 4095, &number)) goto invalid;
        if (ecc) set_rds_ecc((uint16_t)number); else set_rds_lic((uint16_t)number);
        return;
    }
    if (!strcmp(command, "RDS") || !strcmp(command, "STEREO")) {
        if (parse_float(arg, 0, 100, &level)) goto invalid;
        set_carrier_volume((uint8_t)(!strcmp(command, "RDS")), level);
        return;
    }
invalid:
    fprintf(stderr, "Invalid control command or value: %s\n", command);
}

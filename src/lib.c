#include "common.h"
#include "rds.h"

extern int nanosleep(const struct timespec *req, struct timespec *rem);

void msleep(unsigned long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000ul;
    ts.tv_nsec = (ms % 1000ul) * 1000;
    nanosleep(&ts, NULL);
}

int ustrcmp(const unsigned char *s1, const unsigned char *s2) {
    unsigned char c1, c2;

    do {
        c1 = *s1++;
        c2 = *s2++;
        if (c1 == '\0')
            return c1 - c2;
    } while (c1 == c2);

    return c1 - c2;
}

static char *rtp_content_types[64] = {
    "DUMMY_CLASS",
    "ITEM.TITLE",
    "ITEM.ALBUM",
    "ITEM.TRACKNUMBER",
    "ITEM.ARTIST",
    "ITEM.COMPOSITION",
    "ITEM.MOVEMENT",
    "ITEM.CONDUCTOR",
    "ITEM.COMPOSER",
    "ITEM.BAND",
    "ITEM.COMMENT",
    "ITEM.GENRE",
    "INFO.NEWS",
    "INFO.NEWS.LOCAL",
    "INFO.STOCKMARKET",
    "INFO.SPORT",
    "INFO.LOTTERY",
    "INFO.HOROSCOPE",
    "INFO.DAILY_DIVERSION",
    "INFO.HEALTH",
    "INFO.EVENT",
    "INFO.SCENE",
    "INFO.CINEMA",
    "INFO.TV",
    "INFO.DATE_TIME",
    "INFO.WEATHER",
    "INFO.TRAFFIC",
    "INFO.ALARM",
    "INFO.ADVERTISEMENT",
    "INFO.URL",
    "INFO.OTHER",
    "STATIONNAME.SHORT",
    "STATIONNAME.LONG",
    "PROGRAMME.NOW",
    "PROGRAMME.NEXT",
    "PROGRAMME.PART",
    "PROGRAMME.HOST",
    "PROGRAMME.EDITORIAL_STAFF",
    "PROGRAMME.FREQUENCY",
    "PROGRAMME.HOMEPAGE",
    "PROGRAMME.SUBCHANNEL",
    "PHONE.HOTLINE",
    "PHONE.STUDIO",
    "PHONE.OTHER",
    "SMS.STUDIO",
    "SMS.OTHER",
    "EMAIL.HOTLINE",
    "EMAIL.STUDIO",
    "EMAIL.OTHER",
    "MMS.OTHER",
    "CHAT",
    "CHAT.CENTRE",
    "VOTE.QUESTION",
    "VOTE.CENTRE",
    "RFU_1",
    "RFU_2",
    "PRIVATE_1",
    "PRIVATE_2",
    "PRIVATE_3",
    "PLACE",
    "APPOINTMENT",
    "IDENTIFIER",
    "PURCHASE",
    "GET_DATA"
};

uint8_t get_rtp_tag_id(char *rtp_tag_name) {
    uint8_t tag_id = 0;
    for (uint8_t i = 0; i < 64; i++) {
        if (strcmp(rtp_tag_name, rtp_content_types[i]) == 0) {
            tag_id = i;
            break;
        }
    }
    return tag_id;
}

char *get_rtp_tag_name(uint8_t rtp_tag) {
    if (rtp_tag > 63) rtp_tag = 0;
    return rtp_content_types[rtp_tag];
}

static uint16_t offset_words[] = {
    0x0FC,
    0x198,
    0x168,
    0x1B4,
    0x350
};

uint16_t crc16(uint8_t *data, size_t len) {
    uint16_t crc = 0xffff;

    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) | (crc << 8);
        crc ^= data[i];
        crc ^= (crc & 0xff) >> 4;
        crc ^= (crc << 8) << 4;
        crc ^= ((crc & 0xff) << 4) << 1;
    }

    return crc ^ 0xffff;
}

void add_checkwords(uint16_t *blocks, uint8_t *bits) {
    size_t i, j;
    uint8_t bit, msb;
    uint16_t block, block_crc, check, offset_word;
    bool group_type_b = false;

    for (i = 0; i < GROUP_LENGTH; i++) {
        if (i == 2 && group_type_b) {
            offset_word = offset_words[4];
        } else {
            offset_word = offset_words[i];
        }

        block = blocks[i];

        block_crc = 0;
        for (j = 0; j < BLOCK_SIZE; j++) {
            bit = (block & (INT16_15 >> j)) != 0;
            msb = (block_crc >> (POLY_DEG - 1)) & 1;
            block_crc <<= 1;
            if (msb ^ bit) block_crc ^= POLY;
            *bits++ = bit;
        }
        check = block_crc ^ offset_word;
        for (j = 0; j < POLY_DEG; j++) {
            *bits++ = (check & ((1 << (POLY_DEG - 1)) >> j)) != 0;
        }
    }
}

#ifdef RBDS
uint16_t callsign2pi(unsigned char *callsign) {
    uint16_t pi_code = 0;

    if (callsign[0] == 'K' || callsign[0] == 'k') {
        pi_code += 4096;
    } else if (callsign[0] == 'W' || callsign[0] == 'w') {
        pi_code += 21672;
    } else {
        return 0;
    }

    pi_code +=
        (callsign[1] - (callsign[1] >= 'a' ? 0x61 : 0x41)) * 676 +
        (callsign[2] - (callsign[2] >= 'a' ? 0x61 : 0x41)) * 26 +
        (callsign[3] - (callsign[3] >= 'a' ? 0x61 : 0x41));

    if ((pi_code & 0x0F00) == 0) {
        pi_code = 0xA000 +
            ((pi_code & 0xF000) >> 4) + (pi_code & 0x00FF);
    }

    if ((pi_code & 0x00FF) == 0) {
        pi_code = 0xAF00 + ((pi_code & 0xFF00) >> 8);
    }

    return pi_code;
}
#endif

uint8_t add_rds_af(struct rds_af_t *af_list, float freq) {
    uint16_t af;
    uint8_t entries_reqd = 1;

    if (freq < 87.6f || freq > 107.9f) {
        entries_reqd = 2;
    }

    if (af_list->num_afs + entries_reqd > 25) {
        printf("Too many AF entries\n");
        return 1;
    }

    if (freq >= 87.6f && freq <= 107.9f) {
        af = (uint16_t)(freq * 10.0f) - 875;
        af_list->afs[af_list->num_entries] = af;
        af_list->num_entries += 1;
#ifdef RBDS
    } else if (freq >= 540.0f && freq <= 1700.0f) {
        af = (uint16_t)(freq - 540.0f) / 10 + 17;
        af_list->afs[af_list->num_entries + 0] = AF_CODE_LFMF_FOLLOWS;
        af_list->afs[af_list->num_entries + 1] = af;
        af_list->num_entries += 2;
    } else {
#else
    } else if (freq >= 153.0f && freq <= 279.0f) {
        af = (uint16_t)(freq - 153.0f) / 9 + 1;
        af_list->afs[af_list->num_entries + 0] = AF_CODE_LFMF_FOLLOWS;
        af_list->afs[af_list->num_entries + 1] = af;
        af_list->num_entries += 2;
    } else if (freq >= 531.0f && freq <= 1602.0f) {
        af = (uint16_t)(freq - 531.0f) / 9 + 16;
        af_list->afs[af_list->num_entries + 0] = AF_CODE_LFMF_FOLLOWS;
        af_list->afs[af_list->num_entries + 1] = af;
        af_list->num_entries += 2;
    } else {
#endif
        printf("One of the provided AF frequencies is out of range\n");
        return 1;
    }

    af_list->num_afs++;

    return 0;
}

#define XLATSTRLEN 255
unsigned char *xlat(unsigned char *str) {
    static unsigned char new_str[XLATSTRLEN];
    uint8_t i = 0;

    while (*str != 0 && i < XLATSTRLEN) {
        switch (*str) {
            case 0xc2:
                str++;
                switch (*str) {
                    case 0xa1: new_str[i] = 0x8e; break;
                    case 0xa3: new_str[i] = 0xaa; break;
                    case 0xa7: new_str[i] = 0xbf; break;
                    case 0xa9: new_str[i] = 0xa2; break;
                    case 0xaa: new_str[i] = 0xa0; break;
                    case 0xb0: new_str[i] = 0xbb; break;
                    case 0xb1: new_str[i] = 0xb4; break;
                    case 0xb2: new_str[i] = 0xb2; break;
                    case 0xb3: new_str[i] = 0xb3; break;
                    case 0xb5: new_str[i] = 0xb8; break;
                    case 0xb9: new_str[i] = 0xb1; break;
                    case 0xba: new_str[i] = 0xb0; break;
                    case 0xbc: new_str[i] = 0xbc; break;
                    case 0xbd: new_str[i] = 0xbd; break;
                    case 0xbe: new_str[i] = 0xbe; break;
                    case 0xbf: new_str[i] = 0xb9; break;
                    default: new_str[i] = ' '; break;
                }
                break;

            case 0xc3:
                str++;
                switch (*str) {
                    case 0x80: new_str[i] = 0xc1; break;
                    case 0x81: new_str[i] = 0xc0; break;
                    case 0x82: new_str[i] = 0xd0; break;
                    case 0x83: new_str[i] = 0xe0; break;
                    case 0x84: new_str[i] = 0xd1; break;
                    case 0x85: new_str[i] = 0xe1; break;
                    case 0x86: new_str[i] = 0xe2; break;
                    case 0x87: new_str[i] = 0x8b; break;
                    case 0x88: new_str[i] = 0xc3; break;
                    case 0x89: new_str[i] = 0xc2; break;
                    case 0x8a: new_str[i] = 0xd2; break;
                    case 0x8b: new_str[i] = 0xd3; break;
                    case 0x8c: new_str[i] = 0xc5; break;
                    case 0x8d: new_str[i] = 0xc4; break;
                    case 0x8e: new_str[i] = 0xd4; break;
                    case 0x8f: new_str[i] = 0xd5; break;
                    case 0x90: new_str[i] = 0xce; break;
                    case 0x91: new_str[i] = 0x8a; break;
                    case 0x92: new_str[i] = 0xc7; break;
                    case 0x93: new_str[i] = 0xc6; break;
                    case 0x94: new_str[i] = 0xd6; break;
                    case 0x95: new_str[i] = 0xe6; break;
                    case 0x96: new_str[i] = 0xd7; break;
                    case 0x98: new_str[i] = 0xe7; break;
                    case 0x99: new_str[i] = 0xc9; break;
                    case 0x9a: new_str[i] = 0xc8; break;
                    case 0x9b: new_str[i] = 0xd8; break;
                    case 0x9c: new_str[i] = 0xd9; break;
                    case 0x9d: new_str[i] = 0xe5; break;
                    case 0x9e: new_str[i] = 0xe8; break;
                    case 0xa0: new_str[i] = 0x81; break;
                    case 0xa1: new_str[i] = 0x80; break;
                    case 0xa2: new_str[i] = 0x90; break;
                    case 0xa3: new_str[i] = 0xf0; break;
                    case 0xa4: new_str[i] = 0x91; break;
                    case 0xa5: new_str[i] = 0xf1; break;
                    case 0xa6: new_str[i] = 0xf2; break;
                    case 0xa7: new_str[i] = 0x9b; break;
                    case 0xa8: new_str[i] = 0x83; break;
                    case 0xa9: new_str[i] = 0x82; break;
                    case 0xaa: new_str[i] = 0x92; break;
                    case 0xab: new_str[i] = 0x93; break;
                    case 0xac: new_str[i] = 0x85; break;
                    case 0xad: new_str[i] = 0x84; break;
                    case 0xae: new_str[i] = 0x94; break;
                    case 0xaf: new_str[i] = 0x95; break;
                    case 0xb0: new_str[i] = 0xef; break;
                    case 0xb1: new_str[i] = 0x9a; break;
                    case 0xb2: new_str[i] = 0x87; break;
                    case 0xb3: new_str[i] = 0x86; break;
                    case 0xb4: new_str[i] = 0x96; break;
                    case 0xb5: new_str[i] = 0xf6; break;
                    case 0xb6: new_str[i] = 0x97; break;
                    case 0xb7: new_str[i] = 0xba; break;
                    case 0xb8: new_str[i] = 0xf7; break;
                    case 0xb9: new_str[i] = 0x89; break;
                    case 0xba: new_str[i] = 0x88; break;
                    case 0xbb: new_str[i] = 0x98; break;
                    case 0xbc: new_str[i] = 0x99; break;
                    case 0xbd: new_str[i] = 0xf5; break;
                    case 0xbe: new_str[i] = 0xf8; break;
                    default: new_str[i] = ' '; break;
                }
                break;

            case 0xc4:
                str++;
                switch (*str) {
                    case 0x87: new_str[i] = 0xfb; break;
                    case 0x8c: new_str[i] = 0xcb; break;
                    case 0x8d: new_str[i] = 0xdb; break;
                    case 0x91: new_str[i] = 0xde; break;
                    case 0x9b: new_str[i] = 0xa5; break;
                    case 0xb0: new_str[i] = 0xb5; break;
                    case 0xb1: new_str[i] = 0x9f; break;
                    case 0xb2: new_str[i] = 0x8f; break;
                    case 0xb3: new_str[i] = 0x9f; break;
                    case 0xbf: new_str[i] = 0xcf; break;
                    default: new_str[i] = ' '; break;
                }
                break;

            case 0xc5:
                str++;
                switch (*str) {
                    case 0x80: new_str[i] = 0xdf; break;
                    case 0x84: new_str[i] = 0xb6; break;
                    case 0x88: new_str[i] = 0xa6; break;
                    case 0x8a: new_str[i] = 0xe9; break;
                    case 0x8b: new_str[i] = 0xf9; break;
                    case 0x91: new_str[i] = 0xa7; break;
                    case 0x92: new_str[i] = 0xe3; break;
                    case 0x93: new_str[i] = 0xf3; break;
                    case 0x94: new_str[i] = 0xea; break;
                    case 0x95: new_str[i] = 0xfa; break;
                    case 0x98: new_str[i] = 0xca; break;
                    case 0x99: new_str[i] = 0xda; break;
                    case 0x9a: new_str[i] = 0xec; break;
                    case 0x9b: new_str[i] = 0xfc; break;
                    case 0x9e: new_str[i] = 0x8c; break;
                    case 0x9f: new_str[i] = 0x9c; break;
                    case 0xa0: new_str[i] = 0xcc; break;
                    case 0xa1: new_str[i] = 0xdc; break;
                    case 0xa6: new_str[i] = 0xee; break;
                    case 0xa7: new_str[i] = 0xfe; break;
                    case 0xb1: new_str[i] = 0xb7; break;
                    case 0xb5: new_str[i] = 0xf4; break;
                    case 0xb7: new_str[i] = 0xe4; break;
                    case 0xb9: new_str[i] = 0xed; break;
                    case 0xba: new_str[i] = 0xfd; break;
                    case 0xbd: new_str[i] = 0xcd; break;
                    case 0xbe: new_str[i] = 0xdd; break;
                    default: new_str[i] = ' '; break;
                }
                break;

            case 0xc7:
                str++;
                switch (*str) {
                    case 0xa6: new_str[i] = 0xa4; break;
                    case 0xa7: new_str[i] = 0x9d; break;
                    default: new_str[i] = ' '; break;
                }
                break;

            case 0xce:
                str++;
                switch (*str) {
                    case 0xb1: new_str[i] = 0xa1; break;
                    case 0xb2: new_str[i] = 0x8d; break;
                    default: new_str[i] = ' '; break;
                }
                break;

            case 0xcf:
                str++;
                switch (*str) {
                    case 0x80: new_str[i] = 0xa8; break;
                    default: new_str[i] = ' '; break;
                }
                break;

            default:
                switch (*str) {
                    case '$': new_str[i] = 0xab; break;
                    default: new_str[i] = *str; break;
                }
                break;
        }

        i++;
        str++;
    }

    new_str[i] = 0;
    return new_str;
}
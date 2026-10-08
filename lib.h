#ifndef NANORDS_LIB_H
#define NANORDS_LIB_H
#include <stdint.h>
#include <stddef.h>
#include "rds.h"
void msleep(unsigned long ms);
int ustrcmp(const unsigned char *s1, const unsigned char *s2);
uint8_t get_rtp_tag_id(char *rtp_tag_name);
char *get_rtp_tag_name(uint8_t rtp_tag);
void add_checkwords(uint16_t *blocks, uint8_t *bits);
uint16_t callsign2pi(unsigned char *callsign);
uint8_t add_rds_af(struct rds_af_t *af_list, float freq);
uint16_t crc16(uint8_t *data, size_t len);
unsigned char *xlat(unsigned char *str);
int parse_uint(const char *text, int base, unsigned long limit, unsigned long *value);
int parse_float(const char *text, float minimum, float maximum, float *value);

#endif

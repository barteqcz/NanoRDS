extern void msleep(unsigned long ms);

extern int ustrcmp(const unsigned char *s1, const unsigned char *s2);

extern uint8_t get_pty_code(char *pty_str);
extern char *get_pty_str(uint8_t pty_code);
extern uint8_t get_rtp_tag_id(char *rtp_tag_name);
extern char *get_rtp_tag_name(uint8_t rtp_tag);
extern void add_checkwords(uint16_t *blocks, uint8_t *bits);
extern uint16_t callsign2pi(unsigned char *callsign);
extern uint8_t add_rds_af(struct rds_af_t *af_list, float freq);
extern char *show_af_list(struct rds_af_t af_list);
extern uint16_t crc16(uint8_t *data, size_t len);
extern unsigned char *xlat(unsigned char *str);

extern void msleep(unsigned long ms);
extern char *get_pty(uint8_t pty);
extern void add_checkwords(uint16_t *blocks, uint8_t *bits);
extern uint16_t callsign2pi(char *callsign);
extern uint8_t add_rds_af(struct rds_af_t *af_list, float freq);
extern void show_af_list(struct rds_af_t af_list);
extern uint16_t crc16(uint8_t *data, size_t len);

// TMC
extern uint16_t tmc_encrypt(uint16_t loc, uint16_t key);
extern uint16_t tmc_decrypt(uint16_t loc, uint16_t key);

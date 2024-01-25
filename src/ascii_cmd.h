#define CMD_BUFFER_SIZE	160
#define CTL_BUFFER_SIZE	(CMD_BUFFER_SIZE * 2)
#define READ_TIMEOUT_MS	100

extern void process_ascii_cmd(unsigned char *cmd);

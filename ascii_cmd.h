#ifndef NANORDS_ASCII_CMD_H
#define NANORDS_ASCII_CMD_H
#define CMD_BUFFER_SIZE 255
#define CTL_BUFFER_SIZE (CMD_BUFFER_SIZE * 2)
void process_ascii_cmd(unsigned char *cmd);
#endif

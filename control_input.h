#ifndef NANORDS_CONTROL_INPUT_H
#define NANORDS_CONTROL_INPUT_H
int open_control_input(const char *filename);
int poll_control_input(void);
void close_control_input(void);
#endif

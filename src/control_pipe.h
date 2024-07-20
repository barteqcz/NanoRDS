#include <fcntl.h>
#include <poll.h>
#include <sys/select.h>

extern int open_control_pipe(char *filename);
extern void close_control_pipe();
extern void poll_control_pipe();

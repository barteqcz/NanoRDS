#include "common.h"
#include "ascii_cmd.h"
#include "control_pipe.h"

static int fd;
static struct pollfd poller;

/*
 * Opens a file (pipe) to be used to control the RDS coder.
 */
int open_control_pipe(unsigned char *filename) {
	fd = open(filename, O_RDONLY);
	if (fd == -1) return -1;

	/* setup the poller */
	poller.fd = fd;
	poller.events = POLLIN;

	return 0;
}

/*
 * Polls the control file (pipe), and if a command is received,
 * calls process_ascii_cmd.
 */
void poll_control_pipe() {
	static unsigned char pipe_buf[CTL_BUFFER_SIZE];
	static unsigned char cmd_buf[CMD_BUFFER_SIZE];
	unsigned char *token;

	/* check for new commands */
	if (poll(&poller, 1, READ_TIMEOUT_MS) <= 0) return;

	/* return early if there are no new commands */
	if (poller.revents == 0) return;

	memset(pipe_buf, 0, CTL_BUFFER_SIZE);
	read(fd, pipe_buf, CTL_BUFFER_SIZE - 1);

	/* handle commands per line */
	token = strtok(pipe_buf, "\n");
	while (token != NULL) {
		memset(cmd_buf, 0, CMD_BUFFER_SIZE);
		strncpy(cmd_buf, token, CMD_BUFFER_SIZE - 1);
		token = strtok(NULL, "\n");

		process_ascii_cmd(cmd_buf);
	}
}

void close_control_pipe() {
	if (fd > 0) close(fd);
}

#ifndef NANORDS_COMMAND_STREAM_H
#define NANORDS_COMMAND_STREAM_H
#include <stddef.h>
#include "ascii_cmd.h"
typedef struct command_stream {
    unsigned char line[CMD_BUFFER_SIZE];
    size_t used;
    int dropping;
} command_stream;
void command_stream_reset(command_stream *stream);
void command_stream_feed(command_stream *stream, const unsigned char *data, size_t size);
#endif

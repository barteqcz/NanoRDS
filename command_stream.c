#include "common.h"
#include "command_stream.h"
void command_stream_reset(command_stream *stream) { memset(stream, 0, sizeof *stream); }
void command_stream_feed(command_stream *stream, const unsigned char *data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        unsigned char c = data[i];
        if (c == '\n') {
            if (!stream->dropping && stream->used) {
                if (stream->line[stream->used - 1] == '\r') --stream->used;
                stream->line[stream->used] = 0;
                if (stream->used) process_ascii_cmd(stream->line);
            }
            command_stream_reset(stream);
        } else if (!stream->dropping) {
            if (c == 0 || stream->used == sizeof stream->line - 1) {
                stream->dropping = 1;
                fprintf(stderr, "Discarding invalid or overlong control command.\n");
            } else stream->line[stream->used++] = c;
        }
    }
}

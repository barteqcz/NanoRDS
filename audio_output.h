#ifndef NANORDS_AUDIO_OUTPUT_H
#define NANORDS_AUDIO_OUTPUT_H
#include <stdint.h>
#include <stddef.h>
typedef struct audio_output audio_output;
int audio_output_init(void);
void audio_output_shutdown(void);
int audio_output_list(void);
audio_output *audio_output_open(uint32_t sample_rate, int device_index);
int audio_output_write(audio_output *device, const int16_t *samples, size_t frames);
void audio_output_close(audio_output *device);
#endif

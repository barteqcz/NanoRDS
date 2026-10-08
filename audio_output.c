#include "common.h"
#include "audio_output.h"
#ifdef _WIN32
#include <portaudio.h>
#include <pa_win_wasapi.h>
struct audio_output { PaStream *stream; int reported_underflow; };
static int audio_error(const char *operation, PaError error) {
    fprintf(stderr, "%s: %s\n", operation, Pa_GetErrorText(error));
    return -1;
}
int audio_output_init(void) {
    PaError e = Pa_Initialize();
    return e == paNoError ? 0 : audio_error("Audio initialization failed", e);
}
void audio_output_shutdown(void) { Pa_Terminate(); }
int audio_output_list(void) {
    PaDeviceIndex count = Pa_GetDeviceCount();
    if (count < 0) return audio_error("Audio device listing failed", count);
    for (PaDeviceIndex i = 0; i < count; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        const PaHostApiInfo *host = info ? Pa_GetHostApiInfo(info->hostApi) : NULL;
        if (host && host->type == paWASAPI && info->maxOutputChannels >= 2)
            printf("%d: %s (WASAPI)\n", i, info->name);
    }
    return 0;
}
audio_output *audio_output_open(uint32_t rate, int index) {
    PaStreamParameters output = {0};
    PaWasapiStreamInfo wasapi = {0};
    PaHostApiIndex api = Pa_HostApiTypeIdToHostApiIndex(paWASAPI);
    const PaHostApiInfo *host;
    const PaDeviceInfo *info;
    audio_output *device;
    PaError e;
    if (api < 0 || !(host = Pa_GetHostApiInfo(api))) {
        fprintf(stderr, "This PortAudio build has no WASAPI backend.\n");
        return NULL;
    }
    output.device = index < 0 ? host->defaultOutputDevice : index;
    info = Pa_GetDeviceInfo(output.device);
    if (!info || info->hostApi != api || info->maxOutputChannels < 2) {
        fprintf(stderr, "Select a stereo WASAPI device with --list-devices / --device.\n");
        return NULL;
    }
    wasapi.size = sizeof wasapi;
    wasapi.hostApiType = paWASAPI;
    wasapi.version = 1;
    wasapi.flags = paWinWasapiExclusive | paWinWasapiPolling;
    output.channelCount = 2;
    output.sampleFormat = paInt16;
    output.suggestedLatency = info->defaultHighOutputLatency;
    output.hostApiSpecificStreamInfo = &wasapi;
    e = Pa_IsFormatSupported(NULL, &output, (double)rate);
    if (e != paFormatIsSupported) {
        audio_error("Device does not support the required exclusive 192 kHz stream", e);
        return NULL;
    }
    device = calloc(1, sizeof *device);
    if (!device) return NULL;
    e = Pa_OpenStream(&device->stream, NULL, &output, (double)rate,
                      paFramesPerBufferUnspecified, paDitherOff, NULL, NULL);
    if (e == paNoError) e = Pa_StartStream(device->stream);
    if (e != paNoError) {
        audio_error("Opening audio failed", e);
        audio_output_close(device);
        return NULL;
    }
    return device;
}
int audio_output_write(audio_output *device, const int16_t *samples, size_t frames) {
    PaError e = Pa_WriteStream(device->stream, samples, (unsigned long)frames);
    if (e == paOutputUnderflowed) {
        if (!device->reported_underflow) {
            fprintf(stderr, "Audio underrun: check CPU load and the audio device.\n");
            device->reported_underflow = 1;
        }
        return 0;
    }
    return e == paNoError ? 0 : audio_error("Audio write failed", e);
}
void audio_output_close(audio_output *device) {
    if (!device) return;
    if (device->stream) {
        Pa_AbortStream(device->stream);
        Pa_CloseStream(device->stream);
    }
    free(device);
}
#else
#include <ao/ao.h>
struct audio_output { ao_device *device; };
int audio_output_init(void) { ao_initialize(); return 0; }
void audio_output_shutdown(void) { ao_shutdown(); }
int audio_output_list(void) {
    puts("Linux uses the default libao output device and its configuration.");
    return 0;
}
audio_output *audio_output_open(uint32_t rate, int index) {
    ao_sample_format format = {0};
    audio_output *device;
    if (index >= 0) {
        fprintf(stderr, "--device is a Windows option; configure libao on Linux.\n");
        return NULL;
    }
    device = calloc(1, sizeof *device);
    if (!device) return NULL;
    format.channels = 2;
    format.bits = 16;
    format.rate = (int)rate;
    format.byte_format = AO_FMT_NATIVE;
    device->device = ao_open_live(ao_default_driver_id(), &format, NULL);
    if (!device->device) { free(device); return NULL; }
    return device;
}
int audio_output_write(audio_output *device, const int16_t *samples, size_t frames) {
    return ao_play(device->device, (char *)samples,
                   (uint32_t)(frames * 2 * sizeof *samples)) ? 0 : -1;
}
void audio_output_close(audio_output *device) {
    if (!device) return;
    if (device->device) ao_close(device->device);
    free(device);
}
#endif

#include "common.h"
#include <signal.h>
#include <pthread.h>
#include <ao/ao.h>
#include "rds.h"
#include "fm_mpx.h"
#include "control_pipe.h"
#include "resampler.h"
#include "lib.h"
#include "ascii_cmd.h"

static uint8_t stop_rds;

static void stop() {
    stop_rds = 1;
}

static inline void float2char2channel(float *inbuf, char *outbuf, size_t frames) {
    uint16_t j = 0, k = 0;
    int16_t sample;
    int8_t lower, upper;

    for (uint16_t i = 0; i < frames; i++) {
        sample = lroundf(inbuf[j] * 16383.5f);

        lower = sample & 255;
        sample >>= 8;
        upper = sample & 255;

        outbuf[k + 0] = lower;
        outbuf[k + 1] = upper;
        outbuf[k + 2] = 0;
        outbuf[k + 3] = 0;

        j += 2;
        k += 4;
    }
}

static void *control_pipe_worker(void *arg) {
    (void)arg;

    while (!stop_rds) {
        poll_control_pipe();
        msleep(READ_TIMEOUT_MS);
    }

    fprintf(stderr, "Control pipe thread exiting...\n");
    close_control_pipe();
    return NULL;
}

static ao_device *open_audio_device(ao_sample_format *format) {
    ao_device *dev;

    while (!stop_rds) {
        dev = ao_open_live(ao_default_driver_id(), format, NULL);
        if (dev) {
            return dev;
        }

        fprintf(stderr, "Audio device unavailable, retrying...\n");
        msleep(5000);
    }

    return NULL;
}

static void show_help() {
    printf(
        "\n"
        " Usage: nanords [options]\n"
        "\n"
#ifdef RBDS
        " --pi <value>        Program Identification code or callsign\n"
#else
        " --pi <value>        Program Identification code\n"
#endif
        " --ps <value>        Program Service\n"
        " --rt <value>        RadioText\n"
        " --pty <value>       Program Type\n"
        " --ptyn <value>      Program Type Name\n"
        " --ms <value>        Music/speech flag\n"
        " --tp <value>        Traffic Program flag\n"
        " --di <value>        Decoder Information\n"
        " --af <value>        Alternative Frequency\n"
        " --ecc <value>       ECC code\n"
        " --lic <value>       LIC code\n"
        " --stereo <value>    Stereo pilot volume\n"
        " --rds <value>       RDS subcarrier volume\n"
        " --fifo <value>      FIFO control pipe path\n"
        " --help              Show this help text and exit\n"
        "\n"
    );
}

int main(int argc, char **argv) {
    char control_pipe[51] = {0};

    struct rds_params_t rds_params = {
        .ps = "NanoRDS",
        .rt = "NanoRDS - software RDS encoder for Linux",
        .pi = 0x1000
    };

    float *mpx_buffer = NULL;
    float *out_buffer = NULL;
    char  *dev_out    = NULL;

    size_t frames;
    int r;

    SRC_STATE *src_state = NULL;
    SRC_DATA src_data;

    ao_device *device = NULL;
    ao_sample_format format;

    pthread_t control_thread;
    pthread_attr_t attr;
    int control_thread_running = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pi") == 0 && i + 1 < argc) {
#ifdef RBDS
            if (argv[i + 1][0] == 'K' || argv[i + 1][0] == 'W' ||
                argv[i + 1][0] == 'k' || argv[i + 1][0] == 'w') {
                rds_params.pi = callsign2pi((unsigned char *)argv[i + 1]);
            } else
#endif
                rds_params.pi = strtoul(argv[i + 1], NULL, 16);
            i++;
        } else if (strcmp(argv[i], "--ps") == 0 && i + 1 < argc) {
            memcpy(rds_params.ps, xlat((unsigned char *)argv[++i]), 8);
        } else if (strcmp(argv[i], "--rt") == 0 && i + 1 < argc) {
            memcpy(rds_params.rt, xlat((unsigned char *)argv[++i]), 64);
        } else if (strcmp(argv[i], "--pty") == 0 && i + 1 < argc) {
            rds_params.pty = strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--ptyn") == 0 && i + 1 < argc) {
            memcpy(rds_params.ptyn, xlat((unsigned char *)argv[++i]), 8);
        } else if (strcmp(argv[i], "--ms") == 0 && i + 1 < argc) {
            rds_params.ms = strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--tp") == 0 && i + 1 < argc) {
            rds_params.tp = strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--di") == 0 && i + 1 < argc) {
            rds_params.di = strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--af") == 0 && i + 1 < argc) {
            add_rds_af(&rds_params.af, strtof(argv[++i], NULL));
        } else if (strcmp(argv[i], "--ecc") == 0 && i + 1 < argc) {
            rds_params.ecc = strtoul(argv[++i], NULL, 16);
        } else if (strcmp(argv[i], "--lic") == 0 && i + 1 < argc) {
            rds_params.lic = strtoul(argv[++i], NULL, 16);
        } else if (strcmp(argv[i], "--stereo") == 0 && i + 1 < argc) {
            set_carrier_volume(0, atoi(argv[++i]));
        } else if (strcmp(argv[i], "--rds") == 0 && i + 1 < argc) {
            set_carrier_volume(1, atoi(argv[++i]));
        } else if (strcmp(argv[i], "--fifo") == 0 && i + 1 < argc) {
            strncpy(control_pipe, argv[++i], sizeof(control_pipe) - 1);
        } else if (strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        } else {
            show_help();
            return 1;
        }
    }

    mpx_buffer = malloc(NUM_MPX_FRAMES_IN  * 2 * sizeof(float));
    out_buffer = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(float));
    dev_out    = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(int16_t));

    if (!mpx_buffer || !out_buffer || !dev_out)
        goto exit;

    signal(SIGINT, stop);
    signal(SIGTERM, stop);

    fm_mpx_init(MPX_SAMPLE_RATE);
    init_rds_encoder(rds_params);

    memset(&format, 0, sizeof(format));
    format.channels = 2;
    format.bits = 16;
    format.rate = OUTPUT_SAMPLE_RATE;
    format.byte_format = AO_FMT_LITTLE;

    ao_initialize();
    device = open_audio_device(&format);
    if (!device)
        goto exit;

    memset(&src_data, 0, sizeof(src_data));
    src_data.input_frames  = NUM_MPX_FRAMES_IN;
    src_data.output_frames = NUM_MPX_FRAMES_OUT;
    src_data.src_ratio =
        (double)OUTPUT_SAMPLE_RATE / (double)MPX_SAMPLE_RATE;
    src_data.data_in  = mpx_buffer;
    src_data.data_out = out_buffer;

    src_state = src_new(SRC_LINEAR, 2, &r);
    if (!src_state) {
        fprintf(stderr, "Resampler error: %s\n", src_strerror(r));
        goto exit;
    }

    if (control_pipe[0]) {
        if (open_control_pipe(control_pipe) != 0) {
            fprintf(stderr,
                    "Error: failed to open control pipe '%s'\n",
                    control_pipe);
            goto exit;
        }
    
        pthread_attr_init(&attr);
        if (pthread_create(&control_thread, &attr,
                           control_pipe_worker, NULL) != 0) {
            fprintf(stderr,
                    "Error: failed to create control pipe thread\n");
            close_control_pipe();
            goto exit;
        }
    
        control_thread_running = 1;
        fprintf(stderr,
                "Reading control commands on '%s'...\n",
                control_pipe);
    }

    while (!stop_rds) {
        fm_rds_get_frames(mpx_buffer, NUM_MPX_FRAMES_IN);

        src_process(src_state, &src_data);
        frames = src_data.output_frames_gen;

        float2char2channel(out_buffer, dev_out, frames);

        if (!ao_play(device,
                     dev_out,
                     frames * 2 * sizeof(int16_t))) {

            fprintf(stderr,
                    "Audio write failed, reopening device...\n");

            ao_close(device);
            device = open_audio_device(&format);
            if (!device)
                break;
        }
    }

exit:
    if (control_thread_running)
        pthread_join(control_thread, NULL);

    if (device)
        ao_close(device);

    ao_shutdown();

    if (src_state)
        resampler_exit(src_state);

    fm_mpx_exit();
    exit_rds_encoder();

    free(mpx_buffer);
    free(out_buffer);
    free(dev_out);

    return 0;
}

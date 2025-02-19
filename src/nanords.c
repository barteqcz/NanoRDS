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

        outbuf[k+0] = lower;
        outbuf[k+1] = upper;

        outbuf[k+2] = 0;
        outbuf[k+3] = 0;

        j += 2;
        k += 4;
    }
}

static void *control_pipe_worker() {
    while (!stop_rds) {
        poll_control_pipe();
        msleep(READ_TIMEOUT_MS);
    }
    fprintf(stderr, "Control pipe thread exiting...\n");
    close_control_pipe();
    pthread_exit(NULL);
}

static void show_help() {
    printf(
        "\n"
        " Usage: nanords [options]\n"
        "\n"
#ifdef RBDS
        " --pi <value>        Program Identification code or callsign (PI code will be calculated from callsign)\n"
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
        " --fifo <value>      FIFO control pipe path (FIFO commands are available on the project's website)\n"
        " --help              Show this help text and exit\n"
        "\n"
    );
}

int main(int argc, char **argv) {
    char control_pipe[51];
    struct rds_params_t rds_params = {
        .ps = "NanoRDS",
        .rt = "NanoRDS - software RDS encoder for Linux",
        .pi = 0x1000
    };

    float *mpx_buffer;
    float *out_buffer;
    char *dev_out;

    int r;
    size_t frames;

    SRC_STATE *src_state;
    SRC_DATA src_data;

    ao_device *device;
    ao_sample_format format;

    pthread_attr_t attr;
    pthread_t control_pipe_thread;
    pthread_mutex_t control_pipe_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t control_pipe_cond;

    memset(control_pipe, 0, 51);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pi") == 0) {
            if (i + 1 < argc) {
#ifdef RBDS
                if (argv[i + 1][0] == 'K' || argv[i + 1][0] == 'W' ||
                    argv[i + 1][0] == 'k' || argv[i + 1][0] == 'w') {
                    rds_params.pi = callsign2pi((unsigned char *)argv[i + 1]);
                } else
#endif
                    rds_params.pi = strtoul(argv[i + 1], NULL, 16);
                i++;
            } else {
                printf("--pi needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--ps") == 0) {
            if (i + 1 < argc) {
                memcpy(rds_params.ps, xlat((unsigned char *)argv[i + 1]), 8);
                i++;
            } else {
                printf("--ps needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--rt") == 0) {
            if (i + 1 < argc) {
                memcpy(rds_params.rt, xlat((unsigned char *)argv[i + 1]), 64);
                i++;
            } else {
                printf("--rt needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--pty") == 0) {
            if (i + 1 < argc) {
                rds_params.pty = strtoul(argv[i + 1], NULL, 10);
                i++;
            } else {
                printf("--pty needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--ptyn") == 0) {
            if (i + 1 < argc) {
                memcpy(rds_params.ptyn, xlat((unsigned char *)argv[i + 1]), 8);
                i++;
            } else {
                printf("--ptyn needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--ms") == 0) {
            if (i + 1 < argc) {
                rds_params.ms = strtoul(argv[i + 1], NULL, 10);
                i++;
            } else {
                printf("--ms needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--tp") == 0) {
            if (i + 1 < argc) {
                rds_params.tp = strtoul(argv[i + 1], NULL, 10);
                i++;
            } else {
                printf("--tp needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--di") == 0) {
            if (i + 1 < argc) {
                rds_params.di = strtoul(argv[i + 1], NULL, 10);
                i++;
            } else {
                printf("--di needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--af") == 0) {
            if (i + 1 < argc) {
                if (add_rds_af(&rds_params.af, strtof(argv[i + 1], NULL))) {
                    return 1;
                }
                i++;
            } else {
                printf("--af needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--ecc") == 0) {
            if (i + 1 < argc) {
                rds_params.ecc = strtoul(argv[i + 1], NULL, 16);
                i++;
            } else {
                printf("--ecc needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--lic") == 0) {
            if (i + 1 < argc) {
                rds_params.lic = strtoul(argv[i + 1], NULL, 16);
                i++;
            } else {
                printf("--lic needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--stereo") == 0) {
            if (i + 1 < argc) {
                set_carrier_volume(0, atoi(argv[i + 1]));
                i++;
            } else {
                printf("--stereo needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--rds") == 0) {
            if (i + 1 < argc) {
                set_carrier_volume(1, atoi(argv[i + 1]));
                i++;
            } else {
                printf("--rds needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--fifo") == 0) {
            if (i + 1 < argc) {
                memcpy(control_pipe, argv[i + 1], 50);
                i++;
            } else {
                printf("--fifo needs an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            show_help();
            return 1;
        } else {
            printf("\n");
            printf(" Invalid option: %s", argv[i]);
            printf("\n");
            show_help();
            return 1;
        }
    }

    pthread_mutex_init(&control_pipe_mutex, NULL);
    pthread_cond_init(&control_pipe_cond, NULL);
    pthread_attr_init(&attr);

    mpx_buffer = malloc(NUM_MPX_FRAMES_IN * 2 * sizeof(float));
    out_buffer = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(float));
    dev_out = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(int16_t));

    signal(SIGINT, stop);
    signal(SIGTERM, stop);

    fm_mpx_init(MPX_SAMPLE_RATE);
    init_rds_encoder(rds_params);

    memset(&format, 0, sizeof(struct ao_sample_format));
    format.channels = 2;
    format.bits = 16;
    format.rate = OUTPUT_SAMPLE_RATE;
    format.byte_format = AO_FMT_LITTLE;

    ao_initialize();
    device = ao_open_live(ao_default_driver_id(), &format, NULL);
    if (device == NULL) {
        fprintf(stderr, "Error: cannot open sound device\n");
        ao_shutdown();
        goto exit;
    }

    memset(&src_data, 0, sizeof(SRC_DATA));
    src_data.input_frames = NUM_MPX_FRAMES_IN;
    src_data.output_frames = NUM_MPX_FRAMES_OUT;
    src_data.src_ratio = (double)OUTPUT_SAMPLE_RATE / (double)MPX_SAMPLE_RATE;
    src_data.data_in = mpx_buffer;
    src_data.data_out = out_buffer;

    src_state = src_new(SRC_LINEAR, 2, &r);
    if (!src_state) {
        fprintf(stderr, "Error: resampler error - %s.\n", src_strerror(r));
        goto exit;
    }

    if (control_pipe[0]) {
        if (open_control_pipe(control_pipe) == 0) {
            fprintf(stderr, "Reading control commands on %s...\n", control_pipe);
            r = pthread_create(&control_pipe_thread, &attr, control_pipe_worker, NULL);
            if (r < 0) {
                fprintf(stderr, "Error: could not create control pipe thread\n");
                control_pipe[0] = 0;
                goto exit;
            }
        } else {
            fprintf(stderr, "Error: failed to open control pipe - %s.\n", control_pipe);
            control_pipe[0] = 0;
            goto exit;
        }
    }

    while (!stop_rds) {
        fm_rds_get_frames(mpx_buffer, NUM_MPX_FRAMES_IN);

        src_process(src_state, &src_data);
        frames = src_data.output_frames_gen;

        float2char2channel(out_buffer, dev_out, frames);

        if (!ao_play(device, dev_out, frames * 2 * sizeof(int16_t))) {
            fprintf(stderr, "Error: audio write failure.\n");
            break;
        }
    }

    resampler_exit(src_state);

exit:
    if (control_pipe[0]) {
        fprintf(stderr, "Waiting for pipe thread to shut down...\n");
        pthread_cond_signal(&control_pipe_cond);
        pthread_join(control_pipe_thread, NULL);
    }

    pthread_attr_destroy(&attr);

    fm_mpx_exit();
    exit_rds_encoder();

    free(mpx_buffer);
    free(out_buffer);
    free(dev_out);

    return 0;
}

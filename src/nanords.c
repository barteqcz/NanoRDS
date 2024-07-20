#include <signal.h>
#include <getopt.h>
#include <pthread.h>
#include <ao/ao.h>
#include "ascii_cmd.h"
#include "common.h"
#include "control_pipe.h"
#include "fm_mpx.h"
#include "rds.h"
#include "resampler.h"
#include "lib.h"

static uint8_t stop_rds;

static void stop() {
        stop_rds = 1;
}

static inline void float2char2channel(
        float *inbuf, char *outbuf, size_t frames) {
        uint16_t j = 0, k = 0;
        int16_t sample;
        int8_t lower, upper;

        for (uint16_t i = 0; i < frames; i++) {
                sample = lroundf((inbuf[j] + inbuf[j+1]) * 16383.5f);

                /* convert from short to char */
                lower = sample & 255;
                sample >>= 8;
                upper = sample & 255;

                outbuf[k+0] = +lower;
                outbuf[k+1] = +upper;
                outbuf[k+2] = -lower;
                outbuf[k+3] = -upper;

                j += 2;
                k += 4;
        }
}

/* threads */
static void *control_pipe_worker() {
        while (!stop_rds) {
                poll_control_pipe();
                msleep(READ_TIMEOUT_MS);
        }

        close_control_pipe();
        pthread_exit(NULL);
}

static void show_help(char *name) {
        printf(
                "\n"
                " Usage: %s [options]\n"
                "\n"
#ifdef RBDS
                " -i, --pi           Program Identification code or callsign\n"
                "                       (PI code will be calculated from callsign)\n"
#else
                " -i, --pi           Program Identification code\n"
#endif
                " -s, --ps           Program Service\n"
                " -r, --rt           RadioText\n"
                " -p, --pty          Program Type\n"
                " -t, --tp           Traffic Program\n"
                " -e, --ecc          ECC code\n"
                " -l, --lic          LIC code\n"
                " -P, --ptyn         Program Type Name\n"
                " -S, --stereo       Stereo pilot volume\n"
                " -R, --rds          RDS subcarrier volume\n"
                " -c, --ctl          FIFO control pipe path (the FIFO commands are\n"
                "                        available on the project's website).\n"
                " -h, --help         Show this help text and exit\n"
                "\n",
                name
        );
}

int main(int argc, char **argv) {
        int opt;
        char control_pipe[51];
        struct rds_params_t rds_params = {
                .ps = "NanoRDS",
                .rt = "NanoRDS - software RDS encoder for Linux",
                .pi = 0x1000
        };

        /* buffers */
        float *mpx_buffer;
        float *out_buffer;
        char *dev_out;

        int8_t r;
        size_t frames;

        /* SRC */
        SRC_STATE *src_state;
        SRC_DATA src_data;

        /* AO */
        ao_device *device;
        ao_sample_format format;

        /* pthread */
        pthread_attr_t attr;
        pthread_t control_pipe_thread;
        pthread_mutex_t control_pipe_mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_cond_t control_pipe_cond;

        const char      *short_opt = "i:s:r:p:t:e:l:P:S:R:c:h";

        struct option   long_opt[] =
        {
                {"pi",          required_argument, NULL, 'i'},
                {"ps",          required_argument, NULL, 's'},
                {"rt",          required_argument, NULL, 'r'},
                {"pty",         required_argument, NULL, 'p'},
                {"tp",          required_argument, NULL, 't'},
                {"ecc",         required_argument, NULL, 'e'},
                {"lic",         required_argument, NULL, 'l'},
                {"ptyn",        required_argument, NULL, 'P'},
                {"stereo",      required_argument, NULL, 'S'},
                {"rds",         required_argument, NULL, 'R'},
                {"ctl",         required_argument, NULL, 'c'},
                {"help",        no_argument, NULL, 'h'},
                { 0,            0,              0,      0 }
        };

        memset(control_pipe, 0, 51);

keep_parsing_opts:

        opt = getopt_long(argc, argv, short_opt, long_opt, NULL);
        if (opt == -1) goto done_parsing_opts;

        switch (opt) {
                case 'i': /* pi */
#ifdef RBDS
                        if (optarg[0] == 'K' || optarg[0] == 'W' ||
                                optarg[0] == 'k' || optarg[0] == 'w') {
                                rds_params.pi = callsign2pi((unsigned char *)optarg);
                        } else
#endif
                                rds_params.pi = strtoul(optarg, NULL, 16);
                        break;

                case 's': /* PS */
                        memcpy(rds_params.ps, xlat((unsigned char *)optarg), PS_LENGTH);
                        break;

                case 'r': /* RT */
                        memcpy(rds_params.rt, xlat((unsigned char *)optarg), RT_LENGTH);
                        break;

                case 'p': /* PTY */
                        rds_params.pty = strtoul(optarg, NULL, 10);
                        break;

                case 't': /* TP */
                        rds_params.tp = strtoul(optarg, NULL, 10);
                        break;
                        
                case 'e': /* ECC */
                        rds_params.ecc = strtoul(optarg, NULL, 16);
                        break;
                        
                case 'l': /* LIC */
                        rds_params.lic = strtoul(optarg, NULL, 16);
                        break;

                case 'P': /* PTYN */
                        memcpy(rds_params.ptyn, xlat((unsigned char *)optarg), PTYN_LENGTH);
                        break;
                
                case 'S': /* Stereo pilot */
                        set_carrier_volume(0, atoi(optarg));
                        break;

                case 'R': /* RDS subcarrier */
                        set_carrier_volume(1, atoi(optarg));
                        break;

                case 'c': /* FIFO pipe */
                        memcpy(control_pipe, optarg, 50);
                        break;

                case 'h': /* Help */
                case '?':
                default:
                        show_help(argv[0]);
                        return 1;
        }

        goto keep_parsing_opts;

done_parsing_opts:

        /* Initialize pthread stuff */
        pthread_mutex_init(&control_pipe_mutex, NULL);
        pthread_cond_init(&control_pipe_cond, NULL);
        pthread_attr_init(&attr);

        /* Setup buffers */
        mpx_buffer = malloc(NUM_MPX_FRAMES_IN * 2 * sizeof(float));
        out_buffer = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(float));
        dev_out = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(int16_t) * sizeof(char));

        /* Gracefully stop the encoder on SIGINT or SIGTERM */
        signal(SIGINT, stop);
        signal(SIGTERM, stop);

        /* Initialize the baseband generator */
        fm_mpx_init(MPX_SAMPLE_RATE);

        /* Initialize the RDS modulator */
        init_rds_encoder(rds_params);

        /* AO format */
        memset(&format, 0, sizeof(struct ao_sample_format));
        format.channels = 2;
        format.bits = 16;
        format.rate = OUTPUT_SAMPLE_RATE;
        format.byte_format = AO_FMT_LITTLE;

        ao_initialize();

        device = ao_open_live(ao_default_driver_id(), &format, NULL);
        if (device == NULL) {
                fprintf(stderr, "Error: cannot open sound device.\n");
                goto exit;
        }

        /* SRC out (MPX -> output) */
        memset(&src_data, 0, sizeof(SRC_DATA));
        src_data.input_frames = NUM_MPX_FRAMES_IN;
        src_data.output_frames = NUM_MPX_FRAMES_OUT;
        src_data.src_ratio = (double)OUTPUT_SAMPLE_RATE / (double)MPX_SAMPLE_RATE;
        src_data.data_in = mpx_buffer;
        src_data.data_out = out_buffer;

        r = resampler_init(&src_state, 2);
        if (r < 0) {
                fprintf(stderr, "Could not create output resampler.\n");
                goto exit;
        }

        /* Initialize the control pipe reader */
        if (control_pipe[0]) {
                if (open_control_pipe(control_pipe) == 0) {
                        fprintf(stderr, "Reading control commands on %s.\n", control_pipe);
                        /* Create control pipe polling worker */
                        r = pthread_create(&control_pipe_thread, &attr, control_pipe_worker, NULL);
                        if (r < 0) {
                                fprintf(stderr, "Could not create control pipe thread.\n");
                                control_pipe[0] = 0;
                                goto exit;
                        } else {
                                fprintf(stderr, "Created control pipe thread.\n");
                        }
                } else {
                        fprintf(stderr, "Failed to open control pipe: %s.\n", control_pipe);
                        control_pipe[0] = 0;
                        goto exit;
                }
        }

        for (;;) {
                fm_rds_get_frames(mpx_buffer, NUM_MPX_FRAMES_IN);

                if (resample(src_state, src_data, &frames) < 0) break;

                float2char2channel(out_buffer, dev_out, frames);

                /* num_bytes = audio frames * channels * bytes per sample */
                if (!ao_play(device, dev_out, frames * 2 * sizeof(int16_t))) {
                        fprintf(stderr, "Error: could not play audio.\n");
                        break;
                }

                if (stop_rds) {
                        fprintf(stderr, "Stopping...\n");
                        break;
                }
        }

        resampler_exit(src_state);

exit:
        if (control_pipe[0]) {
                /* shut down threads */
                fprintf(stderr, "Waiting for pipe thread to shut down.\n");
                pthread_cond_signal(&control_pipe_cond);
                pthread_join(control_pipe_thread, NULL);
        }

        ao_shutdown();
        pthread_attr_destroy(&attr);

        fm_mpx_exit();
        exit_rds_encoder();

        free(mpx_buffer);
        free(out_buffer);
        free(dev_out);

        return 0;
}

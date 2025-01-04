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
#include <curl/curl.h>
#include <string.h>
#include <ctype.h>

#define CURRENT_VERSION "1.1.0"

static uint8_t stop_rds;

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *data) {
        strcat((char *)data, (char *)ptr);
        return size * nmemb;
}

static int check_for_new_version(const char *url) {
        CURL *curl;
        char latest_version[16] = {0};

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();

        if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, url);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)latest_version);

                if (curl_easy_perform(curl) == CURLE_OK) {
                        for (size_t i = 0; i < strlen(latest_version); i++) {
                                if (latest_version[i] == '\n' || latest_version[i] == '\r' || latest_version[i] == ' ') {
                                        latest_version[i] = '\0';
                                        break;
                                }
                        }

                        if (strcmp(latest_version, CURRENT_VERSION) > 0) {
                                printf("A newer version is available: %s -> %s\n", CURRENT_VERSION, latest_version);
                        }
                }

                curl_easy_cleanup(curl);
        }

        curl_global_cleanup();
        return 0;
}

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

        static char* executable_name;

        static void show_help() {
                printf(
                        "\n"
                        " Usage: %s [options]\n"
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
                        " --ecc <value>       ECC code\n"
                        " --lic <value>       LIC code\n"
                        " --stereo <value>    Stereo pilot volume\n"
                        " --rds <value>       RDS subcarrier volume\n"
                        " --fifo <value>      FIFO control pipe path (the FIFO commands are available on the project's website).\n"
                        " --help              Show this help text and exit\n"
                        "\n",
                       executable_name
                );
                exit(1);
        }

        int main(int argc, char **argv) {
                int opt;
                char control_pipe[51];
                struct rds_params_t rds_params = {
                        .ps = "NanoRDS",
                        .rt = "NanoRDS - software RDS encoder for Linux",
                        .pi = 0x1000
                };

                executable_name = argv[0];

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

                int option_index = 0;

                static struct option long_options[] = {
                        {"pi", required_argument, 0, 0},
                        {"ps", required_argument, 0, 0},
                        {"rt", required_argument, 0, 0},
                        {"pty", required_argument, 0, 0},
                        {"ptyn", required_argument, 0, 0},
                        {"ms", required_argument, 0, 0},
                        {"tp", required_argument, 0, 0},
                        {"di", required_argument, 0, 0},
                        {"ecc", required_argument, 0, 0},
                        {"lic", required_argument, 0, 0},
                        {"stereo", required_argument, 0, 0},
                        {"rds", required_argument, 0, 0},
                        {"help", no_argument, 0, 0},
                        {0, 0, 0, 0}
                };

                while (1) {
                        int c = getopt_long(argc, argv, "", long_options, &option_index);

                        if (c == -1) {
                                break;
                        }

                        switch (c) {
                                case 0:
                                        if (strcmp(long_options[option_index].name, "ps") == 0) {
                                                memcpy(rds_params.ps, xlat((unsigned char *)optarg), PS_LENGTH);
                                        } else if (strcmp(long_options[option_index].name, "rt") == 0) {
                                                memcpy(rds_params.rt, xlat((unsigned char *)optarg), RT_LENGTH);
                                        } else if (strcmp(long_options[option_index].name, "pty") == 0) {
                                                rds_params.pty = strtoul(optarg, NULL, 10);
                                        } else if (strcmp(long_options[option_index].name, "ptyn") == 0) {
                                                memcpy(rds_params.ptyn, xlat((unsigned char *)optarg), PTYN_LENGTH);
                                        } else if (strcmp(long_options[option_index].name, "ms") == 0) {
                                                rds_params.ms = strtoul(optarg, NULL, 10);
                                        } else if (strcmp(long_options[option_index].name, "tp") == 0) {
                                                rds_params.tp = strtoul(optarg, NULL, 10);
                                        } else if (strcmp(long_options[option_index].name, "di") == 0) {
                                                rds_params.di = strtoul(optarg, NULL, 10);
                                        } else if (strcmp(long_options[option_index].name, "ecc") == 0) {
                                                rds_params.ecc = strtoul(optarg, NULL, 16);
                                        } else if (strcmp(long_options[option_index].name, "lic") == 0) {
                                                rds_params.lic = strtoul(optarg, NULL, 16);
                                        } else if (strcmp(long_options[option_index].name, "stereo") == 0) {
                                                set_carrier_volume(0, atoi(optarg));
                                        } else if (strcmp(long_options[option_index].name, "rds") == 0) {
                                                set_carrier_volume(1, atoi(optarg));
                                        } else if (strcmp(long_options[option_index].name, "fifo") == 0) {
                                                memcpy(control_pipe, optarg, 50);
                                        } else if (strcmp(long_options[option_index].name, "help") == 0) {
                                                show_help();
                                        }
                                        break;
                                case '?':
                                        show_help();
                                        return 1;
                        }
                }

                if (argc > 1 && argv[1][0] != '\0' && argv[1][0] != '-') {
                        printf("Invalid format. Options must start with '--'\n");
                        exit(1);
                }

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

                if (check_for_new_version("https://barteqcz.github.io/NanoRDS/version") != 0) {
                        return 1;
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

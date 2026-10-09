#include "common.h"
#include "rds.h"
#include "fm_mpx.h"
#include "control_input.h"
#include "resampler.h"
#include "lib.h"
#include "audio_output.h"
#include "platform.h"

static void pack_output(const float *input, int16_t *output, size_t frames) {
    for (size_t i = 0; i < frames; ++i) {
        float value = fmaxf(-1.0f, fminf(1.0f, input[i]));
        output[2 * i] = (int16_t)lroundf(value * 16383.5f);
        output[2 * i + 1] = 0;
    }
}

static void show_help(FILE *stream) {
    fputs("Usage: nanords [options]\n"
          "  --pi HEX           Program ID / RBDS callsign\n"
          "  --ps TEXT          Station name\n"
          "  --rt TEXT          RadioText\n"
          "  --pty N            Programme type (0-31)\n"
          "  --ptyn TEXT        Programme type name\n"
          "  --ms 0|1           Music/speech\n"
          "  --tp 0|1           Traffic programme\n"
          "  --di N             Decoder info (0-15)\n"
          "  --af a FREQ...     AF Method A\n"
          "  --af b TX FREQ... [regional FREQ...]  AF Method B\n"
          "  --ecc HEX          Extended country code\n"
          "  --lic HEX          Language code\n"
          "  --stereo N         Pilot level (0-100)\n"
          "  --rds N            RDS level (0-100)\n"
          "  --file PATH        Reload commands on file changes\n"
          "  --help             Show help\n"
    )
}

static int run(int argc, char **argv) {
    struct rds_params_t defaults = {
        .pi = 0x1000, .ps = "NanoRDS",
        .rt = "NanoRDS - software RDS encoder"
    };
    struct rds_af_t frequencies = {0};
    float af_same[25], af_regional[AF_B_MAX_PAIRS], af_tuned = 0;
    size_t af_same_count = 0, af_regional_count = 0;
    int af_method = AF_METHOD_A, af_specified = 0;
    const char *control_name = NULL;
    SRC_STATE *converter = NULL;
    audio_output *device = NULL;
    float *mpx = NULL, *output = NULL;
    int16_t *pcm = NULL;
    int result = EXIT_FAILURE, audio_ready = 0;

    if (init_rds_encoder(defaults) != 0) goto done;
    for (int i = 1; i < argc; ++i) {
        const char *option = argv[i], *value;
        unsigned long number;
        float level;
        if (!strcmp(option, "--help")) { show_help(stdout); result = EXIT_SUCCESS; goto done; }
        if (strcmp(option, "--pi") && strcmp(option, "--ps") &&
            strcmp(option, "--rt") && strcmp(option, "--pty") &&
            strcmp(option, "--ptyn") && strcmp(option, "--ms") &&
            strcmp(option, "--tp") && strcmp(option, "--di") &&
            strcmp(option, "--af") && strcmp(option, "--ecc") &&
            strcmp(option, "--lic") && strcmp(option, "--stereo") &&
            strcmp(option, "--rds") && strcmp(option, "--file")) {
            fprintf(stderr, "Unknown option: %s\n", option);
            goto usage_error;
        }
        if (i + 1 == argc || !strncmp(argv[i + 1], "--", 2)) {
            fprintf(stderr, "Missing value for %s\n", option);
            goto usage_error;
        }
        value = argv[++i];
        if (!strcmp(option, "--pi")) {
#ifdef RBDS
            if (value[0] == 'K' || value[0] == 'k' || value[0] == 'W' || value[0] == 'w') {
                uint16_t pi = callsign2pi((unsigned char *)value);
                if (!pi) goto bad_option;
                set_rds_pi(pi);
                continue;
            }
#endif
            if (parse_uint(value, 16, 65535, &number)) goto bad_option;
            set_rds_pi((uint16_t)number);
        } else if (!strcmp(option, "--ps")) set_rds_ps(xlat((unsigned char *)value));
        else if (!strcmp(option, "--rt")) set_rds_rt(xlat((unsigned char *)value));
        else if (!strcmp(option, "--ptyn")) set_rds_ptyn(xlat((unsigned char *)value));
        else if (!strcmp(option, "--pty")) {
            if (parse_uint(value, 10, 31, &number)) goto bad_option;
            set_rds_pty((uint8_t)number);
        } else if (!strcmp(option, "--ms")) {
            if (parse_uint(value, 10, 1, &number)) goto bad_option;
            set_rds_ms((uint8_t)number);
        } else if (!strcmp(option, "--tp")) {
            if (parse_uint(value, 10, 1, &number)) goto bad_option;
            set_rds_tp((uint8_t)number);
        } else if (!strcmp(option, "--di")) {
            if (parse_uint(value, 10, 15, &number)) goto bad_option;
            set_rds_di((uint8_t)number);
        } else if (!strcmp(option, "--af")) {
            int regional = 0, needs_regional_frequency = 0;
            if (af_specified++) {
                fprintf(stderr, "Specify --af only once, with a or b and its frequencies.\n");
                goto usage_error;
            }
            if (!strcmp(value, "a")) af_method = AF_METHOD_A;
            else if (!strcmp(value, "b")) {
                af_method = AF_METHOD_B;
                if (i + 1 == argc || argv[i + 1][0] == '-' ||
                    parse_float(argv[++i], 87.6f, 107.9f, &af_tuned)) {
                    fprintf(stderr, "--af b requires a transmitter frequency first.\n");
                    goto usage_error;
                }
            } else goto bad_option;

            while (i + 1 < argc && argv[i + 1][0] != '-') {
                int duplicate = 0;
                value = argv[++i];
                if (af_method == AF_METHOD_B && !strcmp(value, "regional")) {
                    if (regional) goto bad_option;
                    regional = needs_regional_frequency = 1;
                    continue;
                }
                if (parse_float(value, 0, 2000, &level)) goto bad_option;
                if (af_method == AF_METHOD_B && regional) {
                    if (af_regional_count == AF_B_MAX_PAIRS) goto bad_option;
                    af_regional[af_regional_count++] = level;
                    needs_regional_frequency = 0;
                } else {
                    if (af_method == AF_METHOD_A)
                        for (size_t j = 0; j < af_same_count; ++j)
                            if (af_same[j] == level) duplicate = 1;
                    if (!duplicate) {
                        if (af_same_count == 25) goto bad_option;
                        af_same[af_same_count++] = level;
                    }
                }
            }
            if ((!af_same_count && !af_regional_count) || needs_regional_frequency) {
                fprintf(stderr, "--af requires frequencies; 'regional' must be followed by frequencies.\n");
                goto usage_error;
            }
        } else if (!strcmp(option, "--ecc")) {
            if (parse_uint(value, 16, 255, &number)) goto bad_option;
            set_rds_ecc((uint16_t)number);
        } else if (!strcmp(option, "--lic")) {
            if (parse_uint(value, 16, 4095, &number)) goto bad_option;
            set_rds_lic((uint16_t)number);
        } else if (!strcmp(option, "--stereo") || !strcmp(option, "--rds")) {
            if (parse_float(value, 0, 100, &level)) goto bad_option;
            set_carrier_volume((uint8_t)(!strcmp(option, "--rds")), level);
        } else if (!strcmp(option, "--file")) control_name = value;
        else goto bad_option;
        continue;
    bad_option:
        fprintf(stderr, "Invalid option or value: %s %s\n", option, value);
        goto usage_error;
    }
    if (af_method == AF_METHOD_B) {
        if (init_rds_af_method_b(&frequencies, af_tuned)) {
            fprintf(stderr, "Invalid AF Method B transmitter frequency (87.6..107.9 in 0.1 steps).\n");
            goto usage_error;
        }
        for (size_t i = 0; i < af_same_count; ++i) {
            if (add_rds_af_method_b(&frequencies, af_same[i], 0)) {
                fprintf(stderr, "Invalid or duplicate Method B AF, or over 12 alternatives.\n");
                goto usage_error;
            }
        }
        for (size_t i = 0; i < af_regional_count; ++i) {
            if (add_rds_af_method_b(&frequencies, af_regional[i], 1)) {
                fprintf(stderr, "Invalid or duplicate Method B regional AF, or over 12 alternatives.\n");
                goto usage_error;
            }
        }
    } else {
        for (size_t i = 0; i < af_same_count; ++i) {
            if (add_rds_af(&frequencies, af_same[i])) {
                fprintf(stderr, "Invalid Method A alternative frequency.\n");
                goto usage_error;
            }
        }
    }
    set_rds_af(frequencies);
    if (platform_init_shutdown() || audio_output_init()) goto done;
    audio_ready = 1;
    if (fm_mpx_init(MPX_SAMPLE_RATE) || resampler_init(&converter, 1)) goto done;
    mpx = malloc(NUM_MPX_FRAMES_IN * sizeof *mpx);
    output = malloc(NUM_MPX_FRAMES_OUT * sizeof *output);
    pcm = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof *pcm);
    if (!mpx || !output || !pcm) { fprintf(stderr, "Out of memory.\n"); goto done; }
    if (control_name && open_control_input(control_name)) {
        fprintf(stderr, "Cannot open --file control source '%s'.\n", control_name);
        goto done;
    }
    device = audio_output_open(OUTPUT_SAMPLE_RATE);
    if (!device) { fprintf(stderr, "Unable to open the 192 kHz audio output.\n"); goto done; }
    if (control_name) fprintf(stderr, "Control source: %s\n", control_name);
    fprintf(stderr, "NanoRDS running; press Ctrl+C to stop.\n");

    while (!platform_stop_requested()) {
        long consumed = 0;
        if (poll_control_input()) goto done;
        fm_rds_get_frames(mpx, NUM_MPX_FRAMES_IN);
        while (consumed < NUM_MPX_FRAMES_IN && !platform_stop_requested()) {
            SRC_DATA data = {0};
            data.data_in = mpx + consumed;
            data.input_frames = NUM_MPX_FRAMES_IN - consumed;
            data.data_out = output;
            data.output_frames = NUM_MPX_FRAMES_OUT;
            data.src_ratio = (double)OUTPUT_SAMPLE_RATE / MPX_SAMPLE_RATE;
            if (resample(converter, &data)) goto done;
            if (!data.input_frames_used && !data.output_frames_gen) {
                fprintf(stderr, "Resampler made no progress.\n");
                goto done;
            }
            consumed += data.input_frames_used;
            if (!data.output_frames_gen) continue;
            pack_output(output, pcm, (size_t)data.output_frames_gen);
            if (audio_output_write(device, pcm, (size_t)data.output_frames_gen)) {
                audio_output_close(device);
                device = NULL;
                while (!platform_stop_requested() && !device) {
                    fprintf(stderr, "Audio disconnected; retrying in 5 seconds...\n");
                    for (int n = 0; n < 20 && !platform_stop_requested(); ++n) {
                        if (poll_control_input()) goto done;
                        msleep(250);
                    }
                    if (!platform_stop_requested()) device = audio_output_open(OUTPUT_SAMPLE_RATE);
                }
            }
        }
    }
    result = EXIT_SUCCESS;
    goto done;
 usage_error:
    fputc('\n', stderr);
    show_help(stderr);
 done:
    close_control_input();
    audio_output_close(device);
    if (audio_ready) audio_output_shutdown();
    resampler_exit(converter);
    fm_mpx_exit();
    exit_rds_encoder();
    free(mpx); free(output); free(pcm);
    return result;
}
int main(int argc, char **argv) {
    int result;
    if (platform_utf8_arguments(&argc, &argv)) return EXIT_FAILURE;
    result = run(argc, argv);
    platform_free_arguments(argc, argv);
    return result;
}

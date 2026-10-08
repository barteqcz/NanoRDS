#ifndef NANORDS_PLATFORM_H
#define NANORDS_PLATFORM_H
int platform_init_shutdown(void);
int platform_stop_requested(void);
int platform_utf8_arguments(int *argc, char ***argv);
void platform_free_arguments(int argc, char **argv);
#endif

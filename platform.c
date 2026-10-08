#include "common.h"
#include "platform.h"
#include <signal.h>
static volatile sig_atomic_t signal_stop;
static void on_signal(int signo) { (void)signo; signal_stop = 1; }
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
static volatile LONG console_stop;
static BOOL WINAPI on_console(DWORD event) {
    if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT) return FALSE;
    InterlockedExchange(&console_stop, 1);
    return TRUE;
}
#endif
int platform_init_shutdown(void) {
    signal_stop = 0;
#ifdef _WIN32
    InterlockedExchange(&console_stop, 0);
    if (!SetConsoleCtrlHandler(on_console, TRUE)) return -1;
#else
    if (signal(SIGINT, on_signal) == SIG_ERR) return -1;
#endif
    return signal(SIGTERM, on_signal) == SIG_ERR ? -1 : 0;
}
int platform_stop_requested(void) {
#ifdef _WIN32
    return signal_stop || InterlockedCompareExchange(&console_stop, 0, 0);
#else
    return signal_stop != 0;
#endif
}
int platform_utf8_arguments(int *argc, char ***argv) {
#ifdef _WIN32
    int count;
    wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &count);
    char **args;
    if (!wide) return -1;
    args = calloc((size_t)count + 1, sizeof *args);
    if (!args) { LocalFree(wide); return -1; }
    for (int i = 0; i < count; ++i) {
        int size = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, NULL, 0, NULL, NULL);
        if (size <= 0 || !(args[i] = malloc((size_t)size)) ||
            !WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, args[i], size, NULL, NULL)) {
            platform_free_arguments(count, args);
            LocalFree(wide);
            return -1;
        }
    }
    LocalFree(wide);
    *argc = count;
    *argv = args;
#else
    (void)argc; (void)argv;
#endif
    return 0;
}
void platform_free_arguments(int argc, char **argv) {
#ifdef _WIN32
    if (argv) {
        for (int i = 0; i < argc; ++i) free(argv[i]);
        free(argv);
    }
#else
    (void)argc; (void)argv;
#endif
}

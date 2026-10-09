#include "common.h"
#include "control_input.h"
#include "command_stream.h"

static command_stream commands;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

#define MAX_CONTROL_FILE_SIZE (64 * 1024)
#define CONTROL_POLL_INTERVAL_MS 250

static wchar_t *control_path;
static WIN32_FILE_ATTRIBUTE_DATA last_read;
static int have_last_read;
static int error_logged;
static ULONGLONG next_check;

static int same_file_metadata(const WIN32_FILE_ATTRIBUTE_DATA *a,
                              const WIN32_FILE_ATTRIBUTE_DATA *b) {
    return CompareFileTime(&a->ftLastWriteTime, &b->ftLastWriteTime) == 0 &&
           CompareFileTime(&a->ftCreationTime, &b->ftCreationTime) == 0 &&
           a->nFileSizeHigh == b->nFileSizeHigh &&
           a->nFileSizeLow == b->nFileSizeLow;
}

static int reload_control_file(const WIN32_FILE_ATTRIBUTE_DATA *before) {
    HANDLE file = INVALID_HANDLE_VALUE;
    WIN32_FILE_ATTRIBUTE_DATA after;
    LARGE_INTEGER length;
    unsigned char *data = NULL;
    DWORD received, offset = 0;
    size_t size, first = 0;
    int result = -1;

    file = CreateFileW(control_path, GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto done;
    if (!GetFileSizeEx(file, &length) || length.QuadPart < 0 ||
        length.QuadPart > MAX_CONTROL_FILE_SIZE) {
        fprintf(stderr, "Control .txt file must be at most %d bytes.\n",
                MAX_CONTROL_FILE_SIZE);
        goto done;
    }
    size = (size_t)length.QuadPart;
    data = malloc(size + 1);
    if (!data) goto done;
    while (offset < size) {
        if (!ReadFile(file, data + offset, (DWORD)(size - offset), &received, NULL) ||
            !received) goto done;
        offset += received;
    }
    if (!GetFileAttributesExW(control_path, GetFileExInfoStandard, &after)) goto done;
    if (!same_file_metadata(before, &after)) { result = 1; goto done; }

    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
        first = 3;
    command_stream_reset(&commands);
    command_stream_feed(&commands, data + first, size - first);
    if (commands.used || commands.dropping) {
        const unsigned char newline = '\n';
        command_stream_feed(&commands, &newline, 1);
    }
    last_read = after;
    have_last_read = 1;
    result = 0;
done:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    free(data);
    return result;
}

int open_control_input(const char *filename) {
    int wide_size;
    const wchar_t *extension;
    if (control_path) return -1;
    wide_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                    filename, -1, NULL, 0);
    if (!wide_size) return -1;
    control_path = calloc((size_t)wide_size, sizeof *control_path);
    if (!control_path) return -1;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                             filename, -1, control_path, wide_size)) {
        close_control_input();
        return -1;
    }
    extension = wcsrchr(control_path, L'.');
    if (!extension || _wcsicmp(extension, L".txt") != 0) {
        fprintf(stderr, "On Windows, --file expects an existing .txt file.\n");
        close_control_input();
        return -1;
    }
    command_stream_reset(&commands);
    have_last_read = error_logged = 0;
    next_check = 0;
    if (poll_control_input() || !have_last_read) {
        close_control_input();
        return -1;
    }
    return 0;
}

int poll_control_input(void) {
    WIN32_FILE_ATTRIBUTE_DATA current;
    int status;
    ULONGLONG now;
    if (!control_path) return 0;
    now = GetTickCount64();
    if (now < next_check) return 0;
    next_check = now + CONTROL_POLL_INTERVAL_MS;

    if (!GetFileAttributesExW(control_path, GetFileExInfoStandard, &current) ||
        (current.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        if (!error_logged) {
            fprintf(stderr, "Cannot find/read control .txt file.\n");
            error_logged = 1;
        }
        return have_last_read ? 0 : -1;
    }
    if (have_last_read && same_file_metadata(&current, &last_read)) return 0;
    status = reload_control_file(&current);
    if (status < 0) {
        if (!error_logged) {
            fprintf(stderr, "Cannot load control .txt file; retrying.\n");
            error_logged = 1;
        }
        return have_last_read ? 0 : -1;
    }
    if (status == 0) error_logged = 0;
    return 0;
}

void close_control_input(void) {
    free(control_path);
    control_path = NULL;
    have_last_read = error_logged = 0;
    next_check = 0;
    command_stream_reset(&commands);
}

#else
#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_CONTROL_FILE_SIZE (64 * 1024)
#define CONTROL_POLL_INTERVAL_NS 250000000L

static char *control_path;
static struct stat last_read;
static int have_last_read;
static int error_logged;
static struct timespec next_check;
static int have_next_check;

static int same_file_metadata(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino &&
           a->st_size == b->st_size &&
           a->st_mtim.tv_sec == b->st_mtim.tv_sec &&
           a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec &&
           a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
}

static int stat_control_file(struct stat *info) {
    if (stat(control_path, info) != 0) return -1;
    if (!S_ISREG(info->st_mode)) { errno = EINVAL; return -1; }
    return 0;
}

static int reload_control_file(const struct stat *before) {
    int fd = -1, result = -1;
    struct stat opened, after_fd, after_path;
    unsigned char *data = NULL;
    size_t size, offset = 0, first = 0;

    fd = open(control_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) goto done;
    if (fstat(fd, &opened) != 0 || !S_ISREG(opened.st_mode)) goto done;
    if (!same_file_metadata(before, &opened)) { result = 1; goto done; }
    if (opened.st_size < 0 || opened.st_size > MAX_CONTROL_FILE_SIZE) {
        fprintf(stderr, "Control .txt file must be at most %d bytes.\n",
                MAX_CONTROL_FILE_SIZE);
        goto done;
    }

    size = (size_t)opened.st_size;
    data = malloc(size + 1);
    if (!data) goto done;
    while (offset < size) {
        ssize_t got = read(fd, data + offset, size - offset);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) goto done;
        offset += (size_t)got;
    }
    if (fstat(fd, &after_fd) != 0 ||
        stat_control_file(&after_path) != 0) goto done;
    if (!same_file_metadata(before, &after_fd) ||
        !same_file_metadata(before, &after_path)) {
        result = 1;
        goto done;
    }

    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
        first = 3;
    command_stream_reset(&commands);
    command_stream_feed(&commands, data + first, size - first);
    if (commands.used || commands.dropping) {
        const unsigned char newline = '\n';
        command_stream_feed(&commands, &newline, 1);
    }
    last_read = after_path;
    have_last_read = 1;
    result = 0;
done:
    if (fd >= 0) close(fd);
    free(data);
    return result;
}

int open_control_input(const char *filename) {
    const char *extension;
    size_t length;
    if (control_path || !filename) return -1;
    extension = strrchr(filename, '.');
    if (!extension || strcasecmp(extension, ".txt") != 0) {
        fprintf(stderr, "On Linux, --file expects an existing .txt file.\n");
        return -1;
    }
    length = strlen(filename) + 1;
    control_path = malloc(length);
    if (!control_path) return -1;
    memcpy(control_path, filename, length);
    command_stream_reset(&commands);
    have_last_read = error_logged = have_next_check = 0;
    if (poll_control_input() || !have_last_read) {
        close_control_input();
        return -1;
    }
    return 0;
}

int poll_control_input(void) {
    struct stat current;
    struct timespec now;
    int status;
    if (!control_path) return 0;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
        if (have_next_check &&
            (now.tv_sec < next_check.tv_sec ||
             (now.tv_sec == next_check.tv_sec && now.tv_nsec < next_check.tv_nsec)))
            return 0;
        next_check = now;
        next_check.tv_nsec += CONTROL_POLL_INTERVAL_NS;
        if (next_check.tv_nsec >= 1000000000L) {
            ++next_check.tv_sec;
            next_check.tv_nsec -= 1000000000L;
        }
        have_next_check = 1;
    }

    if (stat_control_file(&current) != 0) {
        if (!error_logged) {
            fprintf(stderr, "Cannot find/read regular control .txt file.\n");
            error_logged = 1;
        }
        return have_last_read ? 0 : -1;
    }
    if (have_last_read && same_file_metadata(&current, &last_read)) return 0;
    status = reload_control_file(&current);
    if (status < 0) {
        if (!error_logged) {
            fprintf(stderr, "Cannot load control .txt file; retrying.\n");
            error_logged = 1;
        }
        return have_last_read ? 0 : -1;
    }
    if (status == 0) error_logged = 0;
    return 0;
}

void close_control_input(void) {
    free(control_path);
    control_path = NULL;
    have_last_read = error_logged = have_next_check = 0;
    command_stream_reset(&commands);
}
#endif

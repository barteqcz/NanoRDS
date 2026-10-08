#include "common.h"
#include "control_input.h"
#include "command_stream.h"

static command_stream commands;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

/* Windows --fifo uses a UTF-8 .txt file (NOT a Windows named pipe).
 * Poll file metadata rather than blocking the audio generation thread. */
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

/* 0 = loaded, 1 = file changed during reading; -1 = failed. */
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

    /* Notepad can save UTF-8 with a BOM. Accept it, plus CRLF and a final
     * line without a newline terminator. */
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
        fprintf(stderr, "On Windows, --fifo expects an existing .txt file.\n");
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
#include <sys/stat.h>
#include <unistd.h>

/* Linux --fifo retains the original POSIX FIFO semantics. */
static int fd = -1;
int open_control_input(const char *filename) {
    struct stat status;
    if (fd >= 0) return -1;
    fd = open(filename, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return -1;
    if (fstat(fd, &status) != 0 || !S_ISFIFO(status.st_mode)) {
        close(fd);
        fd = -1;
        fprintf(stderr, "Control path must be an existing FIFO (use mkfifo).\n");
        return -1;
    }
    command_stream_reset(&commands);
    return 0;
}
int poll_control_input(void) {
    unsigned char input[CTL_BUFFER_SIZE];
    if (fd < 0) return 0;
    for (int attempt = 0; attempt < 4; ++attempt) {
        ssize_t bytes = read(fd, input, sizeof input);
        if (bytes > 0) command_stream_feed(&commands, input, (size_t)bytes);
        else if (bytes == 0) { command_stream_reset(&commands); return 0; }
        else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
        else { perror("Control FIFO read"); return -1; }
    }
    return 0;
}
void close_control_input(void) {
    if (fd >= 0) close(fd);
    fd = -1;
    command_stream_reset(&commands);
}
#endif

#define _GNU_SOURCE
#include "storage_log.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef PI_DOS_STORAGE_TTY
#define PI_DOS_STORAGE_TTY "/dev/tty1"
#endif

/* Keep the most recent output if a tool is unexpectedly noisy. The failure
 * summary is separate so it survives truncation. No allocation or I/O is
 * needed on an already-expanded, healthy boot. */
static char transcript[64 * 1024], failure[256];
static size_t used;
static bool truncated, progress_started;

static void capture(const char *data, size_t length)
{
    if (length > sizeof(transcript)) {
        data += length - sizeof(transcript);
        length = sizeof(transcript);
        truncated = true;
    }
    if (used + length > sizeof(transcript)) {
        size_t drop = used + length - sizeof(transcript);
        memmove(transcript, transcript + drop, used - drop);
        used -= drop;
        truncated = true;
    }
    memcpy(transcript + used, data, length);
    used += length;
}

static void record(const char *message)
{
    capture(message, strlen(message));
    dprintf(2, "%s", message);
}

void pi_dos_storage_note(const char *format, ...)
{
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    record(message);
}

void pi_dos_storage_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(failure, sizeof(failure), format, args);
    va_end(args);
    pi_dos_storage_note("Pi-DOS: %s\n", failure);
}

void pi_dos_storage_progress(const char *stage)
{
    pi_dos_storage_note("Pi-DOS storage: %s\n", stage);
    /* Best effort on HDMI, independent of the selected kernel console.
     * Never wait for a display or change console/VT settings. */
    int tty = open(PI_DOS_STORAGE_TTY, O_WRONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (tty >= 0) {
        if (!progress_started)
            dprintf(tty, "\r\nPi-DOS storage expansion - do not remove power.\r\n");
        dprintf(tty, "%s\r\n", stage);
        close(tty);
        progress_started = true;
    }
}

pid_t pi_dos_storage_spawn(int *output_fd)
{
    int output[2] = {-1, -1};
    if (pipe2(output, O_CLOEXEC) || fcntl(output[0], F_SETFL, O_NONBLOCK) < 0) {
        pi_dos_storage_note("Pi-DOS: tool output capture unavailable: %s\n", strerror(errno));
        if (output[0] >= 0) close(output[0]);
        if (output[1] >= 0) close(output[1]);
        output[0] = output[1] = -1;
    }
    pid_t child = fork();
    if (!child) {
        if (output[0] >= 0) {
            close(output[0]);
            dup2(output[1], STDOUT_FILENO);
            dup2(output[1], STDERR_FILENO);
            close(output[1]);
        }
        /* libparted runs in a child ending with _exit; keep its stdio visible
         * when stdout is a pipe rather than the original terminal. */
        setvbuf(stdout, NULL, _IONBF, 0);
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);
        int null = open("/dev/null", O_RDONLY);
        if (null >= 0) {
            dup2(null, STDIN_FILENO);
            if (null > 2) close(null);
        }
        setenv("LC_ALL", "C", 1);
    } else {
        int saved = errno;
        if (output[1] >= 0) close(output[1]);
        if (child < 0) {
            if (output[0] >= 0) close(output[0]);
            output[0] = -1;
            pi_dos_storage_note("Pi-DOS: cannot start storage tool: %s\n", strerror(saved));
        }
    }
    *output_fd = output[0];
    return child;
}

static void drain_output(int fd)
{
    /* Bound each pass so even a noisy tool cannot prevent timeout handling. */
    for (int pass = 0; fd >= 0 && pass < 16; ++pass) {
        char buffer[4096];
        ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        capture(buffer, (size_t)count);
        /* Preserve the existing serial diagnostics as well. */
        size_t at = 0;
        while (at < (size_t)count) {
            ssize_t written = write(2, buffer + at, (size_t)count - at);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) break;
            at += (size_t)written;
        }
    }
}

int pi_dos_storage_wait(pid_t child, const char *name, int output_fd)
{
    /* Keep the existing five-minute timeout and 100 ms polling interval. */
    struct timespec start, now, pause = {0, 100000000};
    clock_gettime(CLOCK_MONOTONIC, &start);
    int result = -1;
    for (;;) {
        drain_output(output_fd);
        int status;
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) {
            if (WIFEXITED(status)) result = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                pi_dos_storage_note("Pi-DOS: %s killed by signal %d\n", name, WTERMSIG(status));
            break;
        }
        if (waited < 0 && errno != EINTR) {
            pi_dos_storage_note("Pi-DOS: waiting for %s failed: %s\n", name, strerror(errno));
            break;
        }
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec >= 300) {
            kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
            pi_dos_storage_note("Pi-DOS: %s timed out; PI-DOS will not be mounted writable\n", name);
            break;
        }
        nanosleep(&pause, NULL);
    }
    drain_output(output_fd);
    if (output_fd >= 0) close(output_fd);
    clock_gettime(CLOCK_MONOTONIC, &now);
    pi_dos_storage_note("Pi-DOS: %s result=%d, elapsed=%ld seconds\n",
                        name, result, (long)(now.tv_sec - start.tv_sec));
    return result;
}

static void dos_text(FILE *file, const char *text, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\r') continue;
        if (c == '\n') fputs("\r\n", file);
        else fputc(c == '\t' || (c >= 32 && c < 127) ? c : '?', file);
    }
}

void pi_dos_storage_recovery(FILE *autoexec, const char *report)
{
    if (!*failure) pi_dos_storage_error("Persistent storage is unavailable");
    char summary[sizeof(failure)];
    snprintf(summary, sizeof(summary), "%s", failure);
    /* A diagnostic string must never become a DOS batch command. */
    for (unsigned char *p = (unsigned char *)summary; *p; ++p)
        if (*p < 32 || *p > 126 || strchr("&|<>^%\"", *p)) *p = '?';
    fprintf(autoexec, "ECHO Storage error: %.60s\n", summary);
    FILE *file = fopen(report, "w");
    if (!file) {
        fputs("ECHO Storage report unavailable; collect serial diagnostics.\n", autoexec);
        return;
    }
    fputs("Pi-DOS storage recovery report\r\nBoot-local RAM copy; lost at shutdown/reboot.\r\n\r\nFailure: ", file);
    dos_text(file, failure, strlen(failure));
    fputs("\r\n\r\n", file);
    if (truncated) fputs("[Earlier tool output truncated; most recent 64 KiB follows]\r\n", file);
    dos_text(file, transcript, used);
    bool ok = !ferror(file);
    if (fclose(file)) ok = false;
    if (ok) fputs("ECHO Details: MORE Z:\\SYSTEM\\STORAGE.TXT\n"
                  "ECHO This report is held in RAM; save it before rebooting.\n", autoexec);
    else fputs("ECHO Storage report could not be written; collect serial diagnostics.\n", autoexec);
}

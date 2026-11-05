#ifndef PI_DOS_STORAGE_LOG_H
#define PI_DOS_STORAGE_LOG_H
#include <stdio.h>
#include <sys/types.h>

/* Boot-local diagnostics only: no writes to either SD-card partition. */
void pi_dos_storage_note(const char *format, ...) __attribute__((format(printf, 1, 2)));
void pi_dos_storage_error(const char *format, ...) __attribute__((format(printf, 1, 2)));
void pi_dos_storage_progress(const char *stage);
pid_t pi_dos_storage_spawn(int *output_fd);
int pi_dos_storage_wait(pid_t child, const char *name, int output_fd);
void pi_dos_storage_recovery(FILE *autoexec, const char *report);
#endif

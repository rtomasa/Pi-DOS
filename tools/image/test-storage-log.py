#!/usr/bin/env python3
"""Check RAM-only storage diagnostics and real child-output capture without root."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix='pi-dos-storage-log.') as temporary:
    work = Path(temporary)
    tty, report = work / 'tty', work / 'STORAGE.TXT'
    tty.touch()
    wrapper = work / 'storage-log.c'
    wrapper.write_text(r'''
#include "storage_log.c"
#include <assert.h>
int main(int argc, char **argv)
{
    assert(argc == 3);
    if (!strcmp(argv[1], "failure")) {
        pi_dos_storage_progress("Checking FAT32 before expansion...");
        int output;
        pid_t child = pi_dos_storage_spawn(&output);
        assert(child >= 0);
        if (!child) {
            /* Exceed both a pipe buffer and the transcript's RAM limit. */
            for (int i = 0; i < 3000; ++i)
                puts("FAT32 check output that must never block the parent process.");
            fputs("stdout tail retained\n", stdout);
            fputs("stderr tail retained\n", stderr);
            _exit(1);
        }
        assert(pi_dos_storage_wait(child, "FAT32 check", output) == 1);
        pi_dos_storage_error("FAT32 check failed (1); %%PATH%% & ECHO injected > C:\\BAD");
    } else {
        /* A later mount failure needs a report, without expansion progress. */
        pi_dos_storage_error("Writable PI-DOS mount failed: Read-only file system");
    }
    pi_dos_storage_recovery(stdout, argv[2]);
    return 0;
}
''')
    binary = work / 'storage-log'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(ROOT / 'src/init'),
                    '-DPI_DOS_STORAGE_TTY="' + str(tty) + '"',
                    str(wrapper), '-o', str(binary)], check=True)
    result = subprocess.run([str(binary), 'failure', str(report)], check=True,
                            capture_output=True, timeout=10)
    data = report.read_bytes()
    assert len(data) < 68 * 1024
    assert b'Earlier tool output truncated' in data
    assert b'stdout tail retained\r\n' in data
    assert b'stderr tail retained\r\n' in data
    assert b'FAT32 check result=1' in data
    assert b'\n' not in data.replace(b'\r\n', b'')
    assert b'MORE Z:\\SYSTEM\\STORAGE.TXT' in result.stdout
    assert b'&' not in result.stdout and b'%' not in result.stdout and b'>' not in result.stdout
    progress = tty.read_bytes()
    assert b'do not remove power' in progress and b'Checking FAT32' in progress
    tty.write_bytes(b'')
    result = subprocess.run([str(binary), 'mount', str(report)], check=True,
                            capture_output=True, timeout=5)
    assert not tty.read_bytes(), 'Mount recovery must not show expansion progress'
    assert b'Writable PI-DOS mount failed' in result.stdout
    assert b'Writable PI-DOS mount failed' in report.read_bytes()
    result = subprocess.run([str(binary), 'mount', str(work / 'missing/report')],
                            check=True, capture_output=True, timeout=5)
    assert b'report unavailable' in result.stdout
    assert b'MORE Z:' not in result.stdout
    print('PASS: bounded RAM report, stdout/stderr capture without pipe stalls, '
          'failure status preserved, safe DOS output, expansion-only progress and report fallback.')

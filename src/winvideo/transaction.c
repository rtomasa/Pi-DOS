/* The same file transaction is exercised on the host and used by the Win16 CPL.
 * Stage every byte first. Renaming needs no additional file-data disk space,
 * so a failed write cannot truncate the current Windows configuration.
 */
#include <stdio.h>
#include <string.h>
#include "transaction.h"

#ifdef PV_HOST
#define SEPARATOR "/"
#else
#define SEPARATOR "\\"
#endif

static const char *names[] = {"SYSTEM.INI", "WIN.INI", "PROGMAN.INI", "MIDIMAP.CFG"};

static int present(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    fclose(file);
    return 1;
}

static int commit(const char *windows_directory, const char *system_directory,
                  const char *staging_directory)
{
    int moved[4] = {0, 0, 0, 0}, committed[4] = {0, 0, 0, 0};
    int i, count = system_directory ? 4 : 3, rollback_ok = 1;
    char target[144], stage[144], old[144];
    if (strlen(windows_directory) > 100 || strlen(staging_directory) > 120 ||
            (system_directory && strlen(system_directory) > 100)) return 0;
    /* Missing staged files must fail before any current file is touched. */
    for (i = 0; i < count; ++i) {
        sprintf(stage, "%s" SEPARATOR "%s", staging_directory, names[i]);
        if (!present(stage)) return 0;
    }
    for (i = 0; i < count; ++i) {
        sprintf(target, "%s" SEPARATOR "%s", i == 3 ? system_directory : windows_directory, names[i]);
        sprintf(stage, "%s" SEPARATOR "%s", staging_directory, names[i]);
        sprintf(old, "%s" SEPARATOR "PV%d.OLD", staging_directory, i);
        if (present(old) && remove(old)) break;
        if (present(target)) {
            if (rename(target, old)) break;
            moved[i] = 1;
        }
        if (rename(stage, target)) break;
        committed[i] = 1;
    }
    if (i != count) {
        for (i = count - 1; i >= 0; --i) {
            sprintf(target, "%s" SEPARATOR "%s", i == 3 ? system_directory : windows_directory, names[i]);
            sprintf(old, "%s" SEPARATOR "PV%d.OLD", staging_directory, i);
            if (committed[i] && remove(target)) rollback_ok = 0;
            if (moved[i] && rename(old, target)) rollback_ok = 0;
        }
        return rollback_ok ? 0 : -1;
    }
    for (i = 0; i < count; ++i) {
        sprintf(old, "%s" SEPARATOR "PV%d.OLD", staging_directory, i);
        remove(old);
    }
    return 1;
}

int pv_commit(const char *windows_directory, const char *staging_directory)
{
    return commit(windows_directory, NULL, staging_directory);
}

int pv_commit_audio(const char *windows_directory, const char *system_directory,
                    const char *staging_directory)
{
    return commit(windows_directory, system_directory, staging_directory);
}

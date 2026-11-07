#define _GNU_SOURCE
#include "video_boot.h"
#include "config.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

static const char header[] = "# Pi-DOS managed video output\n";
static const char disabled[] = "# Pi-DOS managed video output\n[all]\n"
    "# VGA666 disabled; use the normal HDMI boot configuration.\n";
static const char enabled[] = "# Pi-DOS managed video output\n[all]\n"
    "enable_uart=0\n"
    "dtoverlay=vc4-kms-vga666\n"
    "cmdline=pidos-vga666.txt\n";

bool pi_dos_vga666_active(const char *cmdline)
{
    FILE *file = fopen(cmdline, "r");
    if (!file) return false;
    char word[512]; bool active = false;
    while (fscanf(file, "%511s", word) == 1)
        if (!strcmp(word, "pi-dos.vga666=1")) active = true;
    fclose(file);
    return active;
}

int pi_dos_video_card(const char *sysroot, const char *devroot, bool vga666)
{
    char pattern[PATH_MAX], path[PATH_MAX], state[32];
    if (snprintf(pattern, sizeof(pattern), "%s/card*-%s/status", sysroot,
                 vga666 ? "VGA-*" : "HDMI-*") >= (int)sizeof(pattern)) return -1;
    glob_t paths = {0}; int selected = -1;
    if (!glob(pattern, 0, NULL, &paths)) for (size_t i = 0; i < paths.gl_pathc; ++i) {
        FILE *file = fopen(paths.gl_pathv[i], "r");
        bool connected = file && fgets(state, sizeof(state), file) && !strcmp(state, "connected\n");
        if (file) fclose(file);
        unsigned card;
        if (connected && sscanf(paths.gl_pathv[i] + strlen(sysroot), "/card%u-", &card) == 1 &&
            card <= INT_MAX && snprintf(path, sizeof(path), "%s/card%u", devroot, card) < (int)sizeof(path) &&
            !access(path, R_OK | W_OK)) { selected = (int)card; break; }
    }
    globfree(&paths);
    return selected;
}

/* Only replace our small include. Firmware config, boot arguments and user
 * customizations remain separate. A stale temporary file requires recovery. */
bool pi_dos_video_boot_write(const char *directory, bool use_vga)
{
    char path[PATH_MAX], temporary[PATH_MAX], check[512];
    if (snprintf(path, sizeof(path), "%s/config.txt", directory) >= (int)sizeof(path)) return false;
    FILE *file = fopen(path, "r");
    bool include = false;
    while (file && fgets(check, sizeof(check), file))
        if (!strcmp(check, "include pidos-video.txt\n") || !strcmp(check, "include pidos-video.txt\r\n")) include = true;
    if (file) fclose(file);
    if (!include) return false; /* Older BOOT assets need an explicit update. */
    if (use_vga) {
        const char *required[] = {"pidos-vga666.txt", "overlays/vc4-kms-vga666.dtbo"};
        for (size_t i = 0; i < 2; ++i)
            if (snprintf(path, sizeof(path), "%s/%s", directory, required[i]) >= (int)sizeof(path) || access(path, R_OK)) return false;
    }
    if (snprintf(path, sizeof(path), "%s/pidos-video.txt", directory) >= (int)sizeof(path) ||
        snprintf(temporary, sizeof(temporary), "%s/pidos-video.new", directory) >= (int)sizeof(temporary)) return false;
    file = fopen(path, "r");
    size_t count = file ? fread(check, 1, sizeof(check), file) : 0;
    bool valid = file && !ferror(file) && feof(file) && count >= strlen(header) && !memcmp(check, header, strlen(header));
    if (file) fclose(file);
    if (!valid) return false;
    const char *contents = use_vga ? enabled : disabled;
    size_t length = strlen(contents);
    if (count == length && !memcmp(check, contents, length)) return true;
    int fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd < 0) return false;
    size_t written = 0;
    while (written < length) {
        ssize_t result = write(fd, contents + written, length - written);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) break;
        written += (size_t)result;
    }
    valid = written == length && !fsync(fd);
    if (close(fd)) valid = false;
    if (valid && rename(temporary, path)) valid = false;
    if (!valid) unlink(temporary);
    int parent = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (parent < 0) return false;
    if (fsync(parent)) valid = false;
    close(parent);
    return valid;
}

/* Resolve partition 1 beside the actual data partition through sysfs. Never
 * infer a boot disk from enumeration order or mount a foreign labelled disk. */
static bool boot_device(const char *data, char *boot, size_t size)
{
    struct stat st;
    if (stat(data, &st) || !S_ISBLK(st.st_mode)) return false;
    char link[PATH_MAX], part[PATH_MAX], path[PATH_MAX], index[16];
    snprintf(link, sizeof(link), "/sys/dev/block/%u:%u", major(st.st_rdev), minor(st.st_rdev));
    if (!realpath(link, part)) return false;
    if (snprintf(path, sizeof(path), "%s/partition", part) >= (int)sizeof(path)) return false;
    FILE *file = fopen(path, "r");
    bool second = file && fgets(index, sizeof(index), file) && !strcmp(index, "2\n");
    if (file) fclose(file);
    char *slash = strrchr(part, '/');
    if (!second || !slash) return false;
    *slash = 0;
    const char *disk = strrchr(part, '/');
    if (!disk || snprintf(path, sizeof(path), "/dev/%s", disk + 1) >= (int)sizeof(path)) return false;
    unsigned char sector[512];
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    bool own = fd >= 0 && pread(fd, sector, sizeof(sector), 0) == sizeof(sector) &&
        !memcmp(sector + 440, "\x01\x50\xd0\xa4", 4) && sector[510] == 0x55 && sector[511] == 0xaa;
    if (fd >= 0) close(fd);
    if (!own) return false;
    DIR *directory = opendir(part);
    if (!directory) return false;
    struct dirent *entry; bool found = false;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;
        if (snprintf(path, sizeof(path), "%s/%s/partition", part, entry->d_name) >= (int)sizeof(path)) continue;
        file = fopen(path, "r");
        bool first = file && fgets(index, sizeof(index), file) && !strcmp(index, "1\n");
        if (file) fclose(file);
        if (first && snprintf(boot, size, "/dev/%s", entry->d_name) < (int)size) { found = true; break; }
    }
    closedir(directory);
    if (!found) return false;
    fd = open(boot, O_RDONLY | O_CLOEXEC);
    own = fd >= 0 && pread(fd, sector, sizeof(sector), 0) == sizeof(sector) &&
        sector[510] == 0x55 && sector[511] == 0xaa && !memcmp(sector + 71, "BOOT       ", 11);
    if (fd >= 0) close(fd);
    return own;
}

bool pi_dos_video_boot_sync(const char *config, const char *data)
{
    char value[16], device[PATH_MAX];
    if (!config || !pi_dos_config_value(config, "pidos", "vga666", value, sizeof(value))) return true;
    if (strcasecmp(value, "true") && strcasecmp(value, "false")) return false;
    bool use_vga = !strcasecmp(value, "true");
    if (use_vga == pi_dos_vga666_active("/proc/cmdline")) return true;
    if (!boot_device(data, device, sizeof(device))) return false;
    const char *mountpoint = "/run/pi-dos-boot";
    if (mkdir(mountpoint, 0755) && errno != EEXIST) return false;
    if (mount(device, mountpoint, "vfat", MS_NODEV | MS_NOSUID | MS_NOEXEC, "utf8=1,errors=remount-ro")) return false;
    bool valid = pi_dos_video_boot_write(mountpoint, use_vga);
    sync();
    if (umount(mountpoint)) valid = false;
    return valid;
}

#define _GNU_SOURCE
/* Original project code; license selection remains pending. */
#include "storage.h"
#include "storage_log.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/fs.h>
#include <libfdisk/libfdisk.h>
#include <parted/parted.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include <unistd.h>

#ifndef PI_DOS_FSCK_FAT
#define PI_DOS_FSCK_FAT "/sbin/fsck.fat"
#endif

static uint16_t le16(const unsigned char *p)
{
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool fat32_info(const char *device, uint64_t *sectors, unsigned *cluster)
{
    unsigned char boot[512];
    int fd = open(device, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    ssize_t count = pread(fd, boot, sizeof(boot), 0);
    close(fd);
    if (count != sizeof(boot) || boot[510] != 0x55 || boot[511] != 0xaa ||
        le16(boot + 11) != 512 || !boot[13] || boot[13] > 128 ||
        (boot[13] & (boot[13] - 1)) || !le16(boot + 14) || boot[16] != 2 ||
        le16(boot + 17) || le16(boot + 19) || le16(boot + 22) ||
        le16(boot + 42) || le32(boot + 44) < 2 ||
        memcmp(boot + 71, "PI-DOS     ", 11)) return false;
    uint64_t total = le32(boot + 32), fat = le32(boot + 36);
    uint64_t overhead = le16(boot + 14) + 2 * fat;
    if (!fat || total <= overhead) return false;
    uint64_t clusters = (total - overhead) / boot[13];
    if (clusters < 65525 || clusters > 0x0ffffff5 || fat * 128 < clusters + 2) return false;
    *sectors = total;
    *cluster = boot[13];
    return true;
}

bool pi_dos_is_fat32_data(const char *device)
{
    uint64_t sectors;
    unsigned cluster;
    return fat32_info(device, &sectors, &cluster);
}

static int check_fat(const char *device)
{
    int output;
    pid_t child = pi_dos_storage_spawn(&output);
    if (child < 0) return -1;
    if (!child) {
        pi_dos_storage_note("Pi-DOS: command: %s -n %s\n", PI_DOS_FSCK_FAT, device);
        execl(PI_DOS_FSCK_FAT, PI_DOS_FSCK_FAT, "-n", device, (char *)NULL);
        pi_dos_storage_note("Pi-DOS: cannot execute %s: %s\n", PI_DOS_FSCK_FAT, strerror(errno));
        _exit(127);
    }
    return pi_dos_storage_wait(child, "FAT32 check", output);
}

static PedExceptionOption fat_exception(PedException *exception)
{
    /* libparted offers FAT16 as an alternative below 2 GiB. This appliance
     * always retains FAT32, including on smaller cards used for testing. */
    if (exception->type == PED_EXCEPTION_INFORMATION &&
        (exception->options & PED_EXCEPTION_YES) &&
        strstr(exception->message, "Would you like to use FAT32?")) {
        pi_dos_storage_note("Pi-DOS: retaining FAT32 during expansion\n");
        return PED_EXCEPTION_YES;
    }
    pi_dos_storage_note("Pi-DOS: FAT32 resize: %s\n", exception->message);
    /* There is no interactive host console. Decline unexpected repairs or
     * format conversions rather than guessing an affirmative response. */
    if (exception->options & PED_EXCEPTION_CANCEL) return PED_EXCEPTION_CANCEL;
    if (exception->options & PED_EXCEPTION_NO) return PED_EXCEPTION_NO;
    if (exception->options & PED_EXCEPTION_OK) return PED_EXCEPTION_OK;
    return PED_EXCEPTION_UNHANDLED;
}

static int resize_fat(const char *disk, uint64_t start, uint64_t old_size, uint64_t new_size)
{
    int output;
    pid_t child = pi_dos_storage_spawn(&output);
    if (child < 0) return -1;
    if (!child) {
        pi_dos_storage_note("Pi-DOS: libparted FAT32 resize: %s start=%" PRIu64
                            " old=%" PRIu64 " new=%" PRIu64 " sectors\n",
                            disk, start, old_size, new_size);
        ped_exception_set_handler(fat_exception);
        PedDevice *device = ped_device_get(disk);
        PedGeometry *old = device ? ped_geometry_new(device, (PedSector)start, (PedSector)old_size) : NULL;
        PedGeometry *grown = device ? ped_geometry_new(device, (PedSector)start, (PedSector)new_size) : NULL;
        PedFileSystem *fs = old ? ped_file_system_open(old) : NULL;
        bool ok = fs && grown && !strcmp(fs->type->name, "fat32");
        PedConstraint *constraint = ok ? ped_file_system_get_resize_constraint(fs) : NULL;
        PedGeometry *fit = constraint ? ped_constraint_solve_nearest(constraint, grown) : NULL;
        /* libparted may reserve the final sector. Permit less than one maximum
         * FAT32 cluster of padding, but never move the filesystem start. */
        ok = fit && fit->start == (PedSector)start && fit->length <= (PedSector)new_size &&
             new_size - (uint64_t)fit->length < 128;
        if (!ok) pi_dos_storage_note("Pi-DOS: no supported FAT32 growth geometry\n");
        if (ok) ok = ped_file_system_resize(fs, fit, NULL) != 0;
        if (fit) ped_geometry_destroy(fit);
        if (constraint) ped_constraint_destroy(constraint);
        if (fs && !ped_device_sync(device)) {
            pi_dos_storage_note("Pi-DOS: FAT32 device sync failed: %s\n", strerror(errno));
            ok = false;
        }
        if (fs && !ped_file_system_close(fs)) {
            pi_dos_storage_note("Pi-DOS: FAT32 filesystem close failed: %s\n", strerror(errno));
            ok = false;
        }
        if (old) ped_geometry_destroy(old);
        if (grown) ped_geometry_destroy(grown);
        if (device) ped_device_destroy(device);
        _exit(ok ? 0 : 1);
    }
    return pi_dos_storage_wait(child, "FAT32 expansion", output);
}

static bool partition_bytes(const char *device, uint64_t *bytes)
{
    int fd = open(device, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    bool ok = ioctl(fd, BLKGETSIZE64, bytes) == 0;
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return ok;
}

int pi_dos_expand_data(const char *device)
{
    struct stat part_stat, disk_stat;
    char link[128], sys_part[PATH_MAX], disk[PATH_MAX];
    unsigned char mbr[512];
    uint64_t bytes, part_bytes;
    if (stat(device, &part_stat) || !S_ISBLK(part_stat.st_mode)) return 0;
    snprintf(link, sizeof(link), "/sys/dev/block/%u:%u",
             major(part_stat.st_rdev), minor(part_stat.st_rdev));
    if (!realpath(link, sys_part)) return 0;
    char *slash = strrchr(sys_part, '/');
    if (!slash) return 0;
    *slash = 0;
    char *parent = strrchr(sys_part, '/');
    if (!parent || snprintf(disk, sizeof(disk), "/dev/%s", parent + 1) >= (int)sizeof(disk)) return 0;
    /* Accept partition 2 of the image's own parent disk, never a guessed disk. */
    char sys_index[PATH_MAX], index[16];
    snprintf(sys_index, sizeof(sys_index), "%s/partition", link);
    FILE *file = fopen(sys_index, "r");
    if (!file) return 0;
    bool second = fgets(index, sizeof(index), file) && !strcmp(index, "2\n");
    fclose(file);
    if (!second || stat(disk, &disk_stat) || !S_ISBLK(disk_stat.st_mode) ||
        disk_stat.st_rdev == part_stat.st_rdev) return 0;

    uint64_t fat_sectors;
    unsigned cluster_size;
    if (!fat32_info(device, &fat_sectors, &cluster_size)) return 0;
    int fd = open(disk, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        pi_dos_storage_error("Cannot open parent disk %s: %s", disk, strerror(errno));
        return -1;
    }
    int sector_size = 0;
    ssize_t count = pread(fd, mbr, sizeof(mbr), 0);
    int saved_errno = errno;
    const char *geometry_stage = "read MBR";
    bool ok = count == sizeof(mbr);
    if (!ok && count >= 0) saved_errno = EIO;
    if (ok) {
        geometry_stage = "read disk size";
        ok = !ioctl(fd, BLKGETSIZE64, &bytes);
        saved_errno = errno;
    }
    if (ok) {
        geometry_stage = "read disk sector size";
        ok = !ioctl(fd, BLKSSZGET, &sector_size);
        saved_errno = errno;
    }
    close(fd);
    if (!ok) {
        pi_dos_storage_error("Cannot %s: %s", geometry_stage, strerror(saved_errno));
        return -1;
    }
    if (!partition_bytes(device, &part_bytes)) {
        pi_dos_storage_error("Cannot read partition size: %s", strerror(errno));
        return -1;
    }
    const unsigned char *boot = mbr + 446, *data = boot + 16;
    uint64_t start = le32(data + 8), sectors = le32(data + 12);
    uint64_t disk_sectors = bytes / 512;
    /* This release generates only this MBR layout. Reject GPT, extra partitions,
     * mismatched kernel geometry and cards beyond MBR's addressable range. */
    if (sector_size != 512 || bytes % 512 || disk_sectors > UINT32_MAX ||
        mbr[510] != 0x55 || mbr[511] != 0xaa || le32(mbr + 440) != 0xa4d05001 ||
        boot[0] != 0x80 || boot[4] != 0x0c || le32(boot + 8) != 2048 ||
        le32(boot + 12) != 524288 || data[0] != 0 || data[4] != 0x0c ||
        start != 526336 || !sectors || start + sectors > disk_sectors ||
        part_bytes != sectors * 512 || memcmp(mbr + 478, (unsigned char[32]){0}, 32)) {
        pi_dos_storage_note("Pi-DOS: automatic PI-DOS expansion skipped: nonstandard disk layout\n");
        return 0;
    }
    if (fat_sectors > sectors) {
        pi_dos_storage_error("FAT32 filesystem is larger than its partition");
        return -1;
    }
    uint64_t wanted = disk_sectors - start;
    if (sectors == wanted && wanted - fat_sectors < cluster_size) return 0;

    pi_dos_storage_note("Pi-DOS: expanding PI-DOS to remaining card space (%" PRIu64 " MiB)\n",
            wanted * 512 / 1048576);
    pi_dos_storage_note("Pi-DOS: disk=%s data=%s start=%" PRIu64
                        " disk_sectors=%" PRIu64 " partition_sectors=%" PRIu64
                        " fat_sectors=%" PRIu64 " cluster_sectors=%u target=%" PRIu64 "\n",
                        disk, device, start, disk_sectors, sectors, fat_sectors, cluster_size, wanted);
    pi_dos_storage_progress("Checking FAT32 before expansion...");
    int checked = check_fat(device);
    if (checked != 0) {
        pi_dos_storage_error("FAT32 check failed (%d); expansion stopped", checked);
        return -1;
    }
    if (sectors < wanted) {
        pi_dos_storage_progress("Growing PI-DOS partition...");
        struct fdisk_context *context = fdisk_new_context();
        struct fdisk_partition *partition = NULL;
        if (!context) {
            pi_dos_storage_error("Cannot allocate partition growth context");
            return -1;
        }
        fdisk_disable_dialogs(context, 1);
        const char *stage = "open disk";
        int result = fdisk_assign_device(context, disk, 0);
        if (!result) {
            stage = "validate disk label";
            if (!fdisk_is_label(context, DOS) || fdisk_get_sector_size(context) != 512) result = -EINVAL;
        }
        if (!result) { stage = "read partition"; result = fdisk_get_partition(context, 1, &partition); }
        if (!result) {
            stage = "validate partition geometry";
            if (fdisk_partition_get_start(partition) != start ||
                fdisk_partition_get_size(partition) != sectors) result = -EINVAL;
        }
        if (!result) { stage = "set size"; result = fdisk_partition_set_size(partition, wanted); }
        if (!result) { stage = "set explicit size"; result = fdisk_partition_size_explicit(partition, 1); }
        if (!result) { stage = "set partition"; result = fdisk_set_partition(context, 1, partition); }
        if (!result) { stage = "write partition table"; result = fdisk_write_disklabel(context); }
        if (!result) { stage = "reread partition table"; result = fdisk_reread_partition_table(context); }
        int saved_errno = errno;
        if (partition) fdisk_unref_partition(partition);
        fdisk_deassign_device(context, 0);
        fdisk_unref_context(context);
        if (result) {
            pi_dos_storage_note("Pi-DOS: libfdisk stage=%s result=%d errno=%d (%s)\n",
                                stage, result, saved_errno, strerror(saved_errno));
            pi_dos_storage_error("Partition growth failed at %s; retry on next boot", stage);
            return -1;
        }
        /* Rereading can briefly remove and recreate the devtmpfs node. */
        for (int retry = 0; retry < 20; ++retry) {
            if (partition_bytes(device, &part_bytes) && part_bytes == wanted * 512) break;
            struct timespec pause = {0, 100000000};
            nanosleep(&pause, NULL);
        }
        pi_dos_storage_note("Pi-DOS: kernel partition_bytes=%" PRIu64 " expected=%" PRIu64 "\n",
                            part_bytes, wanted * 512);
        if (part_bytes != wanted * 512) {
            pi_dos_storage_error("Kernel partition size did not update; retry on next boot");
            return -1;
        }
    }
    pi_dos_storage_progress("Growing FAT32 filesystem; this may take several minutes...");
    if (resize_fat(disk, start, fat_sectors, wanted) != 0) {
        pi_dos_storage_error("FAT32 resize failed; retry on next boot");
        return -1;
    }
    pi_dos_storage_progress("Verifying expanded FAT32 filesystem...");
    if (!fat32_info(device, &fat_sectors, &cluster_size)) {
        pi_dos_storage_error("Cannot validate FAT32 boot sector after expansion");
        return -1;
    }
    pi_dos_storage_note("Pi-DOS: expanded fat_sectors=%" PRIu64 " target=%" PRIu64 "\n", fat_sectors, wanted);
    if (fat_sectors > wanted || wanted - fat_sectors >= cluster_size) {
        pi_dos_storage_error("Expanded FAT32 geometry does not match target");
        return -1;
    }
    checked = check_fat(device);
    if (checked != 0) {
        pi_dos_storage_error("FAT32 check after expansion failed (%d)", checked);
        return -1;
    }
    pi_dos_storage_progress("PI-DOS expansion complete. Starting DOS...");
    return 1;
}

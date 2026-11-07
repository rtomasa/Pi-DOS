#ifndef PI_DOS_VIDEO_BOOT_H
#define PI_DOS_VIDEO_BOOT_H
#include <stdbool.h>
bool pi_dos_vga666_active(const char *cmdline);
bool pi_dos_video_boot_write(const char *directory, bool enabled);
bool pi_dos_video_boot_sync(const char *config, const char *data_device);
int pi_dos_video_card(const char *sysroot, const char *devroot, bool vga666);
#endif

#ifndef PI_DOS_WINVIDEO_H
#define PI_DOS_WINVIDEO_H
#define IDD_VIDEO 100
#define IDI_VIDEO 101
#define IDS_NAME 102
#define IDS_INFO 103
#define IDC_ADAPTER 110
#define IDC_RESOLUTION 111
#define IDC_COLOURS 112
#define IDC_FONTS 113
#define IDC_CURRENT 114
#define IDC_RESTORE 115
#define IDC_STATUS 116
#define IDD_CONTROL 200
#define IDC_DISPLAY 201
#define IDC_AUDIO 202
#define IDC_OPTIONS 203
#define IDC_RESTART_WINDOWS 204
#define IDD_OPTIONS 210
#define IDC_PAGE 211
#define IDC_FIELDS 212
#define IDC_EDIT 213
#define IDC_HELP 214
#define IDC_DEFAULTS 215
#define IDC_XMS 216
#define IDC_EMS 217
#define IDD_VALUE 220
#define IDC_VALUES 221
#define IDC_VALUE 222
#define IDD_AUDIO 230
#define IDC_INSTALL_AUDIO 231
#define IDC_CONFIG_AUDIO 232
#define IDC_MIDI_MAP 233
#define IDC_AUDIO_INFO 234
#define IDC_AUDIO_CARD 235
#define IDC_WINDOWS_DRIVERS 236
#define IDC_VOLUME 237
#define IDD_VOLUME 240
#define IDC_VOLUME_STATUS 241
#define IDC_VOLUME_ROW 400
#define IDD_SETUP 300
#define IDC_UNINSTALL 301

#ifdef _WINDOWS
#include <windows.h>
HINSTANCE pv_instance(void);
const char *pv_windows(void);
const char *pv_system(void);
int pv_path(char *out, const char *directory, const char *file);
int pv_copy(const char *from, const char *to);
int pv_same(const char *a, const char *b);
int pv_directory(const char *path);
void pv_report(HWND owner);
int pv_backup(void);
int pv_restore(HWND owner);
void pv_display(HWND owner);
void pv_control(HWND owner);
void pv_options(HWND owner);
void pv_audio(HWND owner);
void pv_volume(HWND owner);
void pv_restart_windows(HWND owner, const char *reason);
unsigned pv_volume_channels(void);
int pv_stage_audio(HWND owner, const char *source);
#endif
#endif

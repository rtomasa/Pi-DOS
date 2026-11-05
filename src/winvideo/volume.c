/* Native Win3.1 volume controls for the running emulator's mixer. */
#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include "video.h"
#include "config.h"
#include "volume_state.h"
#include "mixer_io.h"

static PvVolume levels[PIDOS_MIXER_CHANNELS];
static const char *labels[] = {"Master", "Digital sound", "FM music", "Gravis UltraSound",
    "Game Blaster / CMS", "PC speaker", "Tandy music", "Tandy digital", "Disney / Covox", "Internal MIDI",
    "CD audio (image)", "CD audio (USB)"};

unsigned pv_volume_channels(void)
{
    unsigned i, mask = 1;
    if (pidos_mixer_read(PIDOS_MIXER_INFO) != PIDOS_MIXER_VERSION) return mask;
    for (i = 1; i < PIDOS_MIXER_CHANNELS; ++i)
        if (pidos_mixer_read(PIDOS_MIXER_CHANNEL + i) != PIDOS_MIXER_MISSING) mask |= 1U << i;
    return mask;
}

static void place(HWND dialog, HWND control, int x, int y, int width, int height)
{
    RECT r;
    r.left = x; r.top = y; r.right = x + width; r.bottom = y + height;
    MapDialogRect(dialog, &r);
    MoveWindow(control, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
}

static int add_control(HWND dialog, const char *type, const char *text, DWORD style,
                       unsigned id, int x, int y, int width, int height)
{
    HWND child = CreateWindow(type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
                             dialog, (HMENU)id, pv_instance(), NULL);
    if (!child) return 0;
    SendMessage(child, WM_SETFONT, (WPARAM)SendMessage(dialog, WM_GETFONT, 0, 0), 0);
    place(dialog, child, x, y, width, height);
    return 1;
}

static void refresh(HWND dialog, unsigned channel)
{
    unsigned id = IDC_VOLUME_ROW + channel * 4;
    unsigned percent = pv_volume_percent(&levels[channel]);
    char text[24];
    SetScrollPos(GetDlgItem(dialog, id + 1), SB_CTL, percent > 100 ? 100 : percent, TRUE);
    sprintf(text, "%u%%", percent);
    SetDlgItemText(dialog, id + 2, text);
    CheckDlgButton(dialog, id + 3, levels[channel].muted);
}

static int restore_levels(void)
{
    unsigned i;
    int ok = 1;
    for (i = 0; i < PIDOS_MIXER_CHANNELS; ++i) {
        PvVolume *level = &levels[i];
        if (level->present && level->applied != level->original) {
            if (pidos_mixer_write(i, level->original)) level->applied = level->original;
            else ok = 0;
        }
    }
    return ok;
}

static void apply_level(HWND dialog, unsigned channel, const PvVolume *previous)
{
    unsigned long value = pv_volume_value(&levels[channel]);
    if (value != levels[channel].applied && !pidos_mixer_write(channel, value)) {
        pidos_mixer_write(channel, previous->applied);
        levels[channel] = *previous;
        MessageBox(dialog, "The running mixer could not apply this level. Try again.",
                   "Pi-DOS volume", MB_OK | MB_ICONEXCLAMATION);
    } else levels[channel].applied = value;
    refresh(dialog, channel);
}

static int save_levels(HWND dialog)
{
    unsigned i;
    char value[24];
    /* Reload just before saving, preserving other settings and edits on disk. */
    if (!cfg_load("C:\\SYSTEM\\PIDOS.CFG")) goto failed;
    for (i = 0; i < PIDOS_MIXER_CHANNELS; ++i) if (levels[i].present && levels[i].changed) {
        unsigned long raw = pv_volume_value(&levels[i]);
        sprintf(value, "%lu:%lu", ((raw & 65535UL) + 50) / 100, ((raw >> 16) + 50) / 100);
        if (!cfg_set(K_VOL_MASTER + i, value)) goto failed;
    }
    if (cfg_save_changes("C:\\SYSTEM\\PIDOS.CFG")) return 1;
failed:
    MessageBox(dialog, *cfg_error ? cfg_error : "Cannot save C:\\SYSTEM\\PIDOS.CFG. Check disk space and write access.",
               "Pi-DOS volume", MB_OK | MB_ICONEXCLAMATION);
    return 0;
}

BOOL FAR PASCAL __export VolumeDialog(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    unsigned i, id, channel, rows;
    int position, height;
    RECT window, client, units;
    PvVolume previous;
    if (message == WM_INITDIALOG) {
        if (pidos_mixer_read(PIDOS_MIXER_INFO) != PIDOS_MIXER_VERSION) {
            MessageBox(dialog, "Live volume requires the updated Pi-DOS runtime. Install the matching Pi-DOS image and reboot first.",
                       "Pi-DOS volume", MB_OK | MB_ICONINFORMATION);
            EndDialog(dialog, 0); return TRUE;
        }
        rows = 0;
        for (i = 0; i < PIDOS_MIXER_CHANNELS; ++i) {
            pv_volume_init(&levels[i], pidos_mixer_read(PIDOS_MIXER_CHANNEL + i));
            if (!levels[i].present) continue;
            id = IDC_VOLUME_ROW + i * 4;
            height = 35 + rows++ * 18;
            if (!add_control(dialog, "STATIC", labels[i], SS_LEFT, id, 10, height + 2, 99, 12) ||
                    !add_control(dialog, "SCROLLBAR", "", SBS_HORZ | WS_TABSTOP, id + 1, 113, height, 145, 14) ||
                    !add_control(dialog, "STATIC", "", SS_RIGHT, id + 2, 263, height + 2, 39, 12) ||
                    !add_control(dialog, "BUTTON", "Mute", BS_AUTOCHECKBOX | WS_TABSTOP, id + 3, 310, height, 43, 14)) {
                MessageBox(dialog, "Not enough Windows resources to open the volume controls.", "Pi-DOS volume", MB_OK);
                EndDialog(dialog, 0); return TRUE;
            }
            SetScrollRange(GetDlgItem(dialog, id + 1), SB_CTL, 0, 100, FALSE);
            refresh(dialog, i);
        }
        height = 35 + rows * 18;
        place(dialog, GetDlgItem(dialog, IDC_VOLUME_STATUS), 10, height, 344, 24);
        place(dialog, GetDlgItem(dialog, IDOK), 246, height + 31, 50, 14);
        place(dialog, GetDlgItem(dialog, IDCANCEL), 303, height + 31, 50, 14);
        GetWindowRect(dialog, &window); GetClientRect(dialog, &client);
        units.left = units.top = 0; units.right = 364; units.bottom = height + 55;
        MapDialogRect(dialog, &units);
        SetWindowPos(dialog, NULL, 0, 0, window.right - window.left,
                     units.bottom + window.bottom - window.top - client.bottom, SWP_NOMOVE | SWP_NOZORDER);
        return TRUE;
    }
    if (message == WM_HSCROLL) {
        id = GetDlgCtrlID((HWND)HIWORD(lparam));
        if (id < IDC_VOLUME_ROW || id >= IDC_VOLUME_ROW + PIDOS_MIXER_CHANNELS * 4 ||
                (id - IDC_VOLUME_ROW) % 4 != 1) return FALSE;
        channel = (id - IDC_VOLUME_ROW) / 4;
        previous = levels[channel];
        position = pv_volume_percent(&previous);
        switch (wparam) {
        case SB_LINEUP: position -= 5; break;
        case SB_LINEDOWN: position += 5; break;
        case SB_PAGEUP: position -= 20; break;
        case SB_PAGEDOWN: position += 20; break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: position = LOWORD(lparam); break;
        case SB_TOP: position = 0; break;
        case SB_BOTTOM: position = 100; break;
        default: return TRUE;
        }
        if (position < 0) position = 0;
        if (position > 100) position = 100;
        pv_volume_level(&levels[channel], position);
        apply_level(dialog, channel, &previous);
        return TRUE;
    }
    if (message == WM_CLOSE || (message == WM_COMMAND && wparam == IDCANCEL)) {
        if (restore_levels()) EndDialog(dialog, 0);
        else MessageBox(dialog, "Some opening levels could not be restored. Try Cancel again.", "Pi-DOS volume", MB_OK);
        return TRUE;
    }
    if (message == WM_COMMAND) {
        if (wparam == IDOK) { if (save_levels(dialog)) EndDialog(dialog, 1); return TRUE; }
        if (wparam >= IDC_VOLUME_ROW && wparam < IDC_VOLUME_ROW + PIDOS_MIXER_CHANNELS * 4 &&
                (wparam - IDC_VOLUME_ROW) % 4 == 3) {
            channel = (wparam - IDC_VOLUME_ROW) / 4;
            previous = levels[channel];
            pv_volume_mute(&levels[channel], IsDlgButtonChecked(dialog, wparam));
            apply_level(dialog, channel, &previous);
            return TRUE;
        }
    }
    return FALSE;
}

void pv_volume(HWND owner)
{
    DialogBox(pv_instance(), MAKEINTRESOURCE(IDD_VOLUME), owner, VolumeDialog);
}

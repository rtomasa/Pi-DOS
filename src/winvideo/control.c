/* Shared entry point for every Windows driver and Pi-DOS option page. */
#include <windows.h>
#include "video.h"

void pv_restart_windows(HWND owner, const char *reason)
{
    char text[768];
    wsprintf(text, "%s\n\nSave your work in all applications before continuing.\nRestart Windows now?", reason);
    if (MessageBox(owner, text, "Restart Windows", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES) return;
    if (!ExitWindows(EW_RESTARTWINDOWS, 0))
        MessageBox(owner, "Windows could not restart, or an application cancelled shutdown.\n"
                   "Your saved settings are kept. You can try Restart Windows again later.",
                   "Restart Windows", MB_OK | MB_ICONINFORMATION);
}

BOOL FAR PASCAL __export ControlDialog(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    if (message == WM_COMMAND) {
        switch (wparam) {
        case IDCANCEL: EndDialog(dialog, 0); return TRUE;
        case IDC_DISPLAY: pv_display(dialog); return TRUE;
        case IDC_AUDIO: pv_audio(dialog); return TRUE;
        case IDC_OPTIONS: pv_options(dialog); return TRUE;
        case IDC_RESTART_WINDOWS:
            pv_restart_windows(dialog, "Restart Windows to activate saved Windows driver settings.");
            return TRUE;
        case IDC_RESTORE:
            pv_restore(dialog);
            return TRUE;
        }
    }
    return FALSE;
}

void pv_control(HWND owner)
{
    DialogBox(pv_instance(), MAKEINTRESOURCE(IDD_CONTROL), owner, ControlDialog);
}

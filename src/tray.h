#ifndef CAPSSWITCH_TRAY_H
#define CAPSSWITCH_TRAY_H

#include <stdbool.h>
#include <windows.h>

#define IDI_APP 1 /* Active keycap, shared by the EXE and tray */
#define IDI_APP_PAUSED 2 /* Same artwork with reduced alpha */

/* Creates the hidden window, starts the keyboard hook and adds the tray
   icon. Returns NULL if the hook could not be installed. */
HWND tray_create(HINSTANCE instance);

#endif

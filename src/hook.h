#ifndef CAPSSWITCH_HOOK_H
#define CAPSSWITCH_HOOK_H

#include <stdbool.h>
#include <windows.h>

/* Marks the Win+Space events CapsSwitch sends, so its own hook skips them. */
#define HOOK_OWN_MARKER 0x43535754 /* "CSWT" */

/* Installs WH_KEYBOARD_LL on a dedicated thread, so a busy UI never delays
   keyboard input system-wide. A Caps Lock press that should switch the
   language posts switch_message to window. Returns false on failure. */
bool hook_start(HWND window, UINT switch_message);
void hook_stop(void);

void hook_set_paused(bool paused);
bool hook_paused(void);

/* Called on the UI thread for switch_message. If a modifier went down after
   Caps Lock, Win+Space would become another shortcut, so the switch waits
   for the modifiers to be released (timer_id on window, at most 2 s).
   Physically held keys are never released by CapsSwitch. */
void hook_switch_language(HWND window, UINT_PTR timer_id);
void hook_on_timer(HWND window, UINT_PTR timer_id);

#endif

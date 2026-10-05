#ifndef CAPSSWITCH_KEYSTATE_H
#define CAPSSWITCH_KEYSTATE_H

/* Caps Lock press logic. Pure C without Win32 so the tests can drive it
   directly; the key codes mirror the Win32 VK_* values. */

#include <stdbool.h>

enum {
    KS_VK_SHIFT    = 0x10, /* VK_SHIFT */
    KS_VK_CONTROL  = 0x11, /* VK_CONTROL */
    KS_VK_MENU     = 0x12, /* VK_MENU (Alt) */
    KS_VK_CAPITAL  = 0x14, /* VK_CAPITAL (Caps Lock) */
    KS_VK_LWIN     = 0x5B, /* VK_LWIN */
    KS_VK_RWIN     = 0x5C, /* VK_RWIN */
    KS_VK_LSHIFT   = 0xA0, /* VK_LSHIFT */
    KS_VK_RSHIFT   = 0xA1, /* VK_RSHIFT */
    KS_VK_LCONTROL = 0xA2, /* VK_LCONTROL */
    KS_VK_RCONTROL = 0xA3, /* VK_RCONTROL */
    KS_VK_LMENU    = 0xA4, /* VK_LMENU */
    KS_VK_RMENU    = 0xA5  /* VK_RMENU */
};

typedef enum { KS_PASS, KS_SUPPRESS, KS_SWITCH } ks_action;

/* Reports whether a key is physically down right now. */
typedef bool (*ks_is_down_fn)(int key);

/* Side-specific modifiers as the low-level hook reports them. */
#define KS_PHYSICAL_MODIFIER_COUNT 8
extern const int ks_physical_modifiers[KS_PHYSICAL_MODIFIER_COUNT];

/* A press keeps its original mode until release, even if paused meanwhile. */
typedef struct {
    unsigned modifiers; /* one bit per entry of the modifier table in keystate.c */
    bool caps_held, suppress_press;
    volatile bool enabled; /* written by the UI thread, read by the hook thread */
    ks_is_down_fn is_down; /* optional */
} ks_state;

/* Releases on the secure desktop (Win+L, Ctrl+Alt+Del) never reach the
   hook, so with is_down set the tracked modifiers are re-checked on every
   new Caps Lock press. Pass NULL to trust the hook events alone. */
void ks_init(ks_state *state, ks_is_down_fn is_down);
ks_action ks_process(ks_state *state, int key, bool down);
bool ks_is_modifier(int key);

#endif

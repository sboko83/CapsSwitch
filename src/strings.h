#ifndef CAPSSWITCH_STRINGS_H
#define CAPSSWITCH_STRINGS_H

/* User-visible text in English and Russian, chosen once by the UI language. */

typedef enum {
    STR_TITLE,
    STR_MENU_HEADER,
    STR_MENU_PAUSE,
    STR_MENU_ABOUT,
    STR_MENU_EXIT,
    STR_TIP_ACTIVE,
    STR_TIP_PAUSED,
    STR_ABOUT,
    STR_START_FAILED,
    STR_COUNT
} string_id;

const wchar_t *str(string_id id);

#endif

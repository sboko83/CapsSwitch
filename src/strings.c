#include <windows.h>
#include "strings.h"
#include "version.h"

static const wchar_t *const english[STR_COUNT] = {
    [STR_TITLE] = L"CapsSwitch",
    [STR_MENU_HEADER] = L"CapsSwitch \x2014 input language switch",
    [STR_MENU_PAUSE] = L"Pause",
    [STR_MENU_ABOUT] = L"About",
    [STR_MENU_EXIT] = L"Exit",
    [STR_TIP_ACTIVE] = L"CapsSwitch\n"
                       L"Caps Lock switches the keyboard layout\n"
                       L"Shift + Caps Lock \x2014 regular Caps Lock",
    [STR_TIP_PAUSED] = L"CapsSwitch\n"
                       L"Paused: Caps Lock works as usual\n"
                       L"Double-click the tray icon to resume",
    [STR_ABOUT] = L"CapsSwitch " VERSION_WSTRING L"\n\n"
                  L"Caps Lock \x2014 next Windows input language.\n"
                  L"Shift + Caps Lock \x2014 regular Caps Lock.\n"
                  L"With Ctrl, Alt or Win, Caps Lock works as usual.\n\n"
                  L"Double-click the tray icon to pause or resume.\n\n"
                  L"Everything runs locally: no keystrokes are recorded, no network is used.",
    [STR_START_FAILED] = L"CapsSwitch could not start: the keyboard hook was not installed.",
};

static const wchar_t *const russian[STR_COUNT] = {
    [STR_TITLE] = L"CapsSwitch",
    [STR_MENU_HEADER] = L"CapsSwitch \x2014 переключение языка",
    [STR_MENU_PAUSE] = L"Приостановить",
    [STR_MENU_ABOUT] = L"О программе",
    [STR_MENU_EXIT] = L"Выход",
    [STR_TIP_ACTIVE] = L"CapsSwitch\n"
                       L"Caps Lock переключает раскладку\n"
                       L"Shift + Caps Lock \x2014 обычный Caps Lock",
    [STR_TIP_PAUSED] = L"CapsSwitch\n"
                       L"Пауза: Caps Lock работает как обычно\n"
                       L"Двойной щелчок по значку \x2014 продолжить",
    [STR_ABOUT] = L"CapsSwitch " VERSION_WSTRING L"\n\n"
                  L"Caps Lock \x2014 следующая раскладка Windows.\n"
                  L"Shift + Caps Lock \x2014 обычный Caps Lock.\n"
                  L"С Ctrl, Alt или Win клавиша Caps Lock работает как обычно.\n\n"
                  L"Двойной щелчок по значку \x2014 пауза / продолжение.\n\n"
                  L"Программа работает локально: не записывает ввод и не использует сеть.",
    [STR_START_FAILED] = L"Не удалось запустить CapsSwitch: обработчик клавиатуры не установлен.",
};

const wchar_t *str(string_id id) {
    static const wchar_t *const *table;
    if (!table) table = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN ? russian : english;
    return table[id];
}

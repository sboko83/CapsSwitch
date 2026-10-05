#include <windows.h>
#include <shellapi.h>
#include "glyph.h"
#include "hook.h"
#include "strings.h"
#include "tray.h"

#define WM_APP_TRAY (WM_APP + 1)
#define WM_APP_SWITCH (WM_APP + 2)
#define TRAY_ICON_ID 1
#define TIMER_SWITCH 1
#define TIMER_ADD_ICON 2
#define ADD_ICON_RETRY_MS 2000

enum { CMD_PAUSE = 1, CMD_ABOUT, CMD_EXIT };

static const wchar_t window_class[] = L"CapsSwitch.Tray";
static const wchar_t theme_key[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

static HINSTANCE instance;
static HWND window;
static HICON icon;
static int icon_size;
static bool icon_light;
static bool icon_added;
static bool about_open;
static bool running; /* the hook is up: closing the window ends the program */
static UINT taskbar_created;

static bool light_taskbar(void) {
    DWORD value = 0, size = sizeof value;
    RegGetValueW(HKEY_CURRENT_USER, theme_key, L"SystemUsesLightTheme", RRF_RT_REG_DWORD, NULL, &value, &size);
    return value != 0;
}

/* Small-icon size at the taskbar's DPI, which can change without sign-out. */
static int tray_icon_size(void) {
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    UINT dpi = taskbar ? GetDpiForWindow(taskbar) : 0;
    return dpi ? GetSystemMetricsForDpi(SM_CXSMICON, dpi) : GetSystemMetrics(SM_CXSMICON);
}

static void init_icon_data(NOTIFYICONDATAW *data) {
    *data = (NOTIFYICONDATAW){ 0 };
    data->cbSize = sizeof *data;
    data->hWnd = window;
    data->uID = TRAY_ICON_ID;
}

/* Draws the icon for the current size, theme and pause state, then adds it
   (NIM_ADD) or swaps it in (NIM_MODIFY). */
static bool show_icon(DWORD action) {
    icon_size = tray_icon_size();
    icon_light = light_taskbar();
    bool paused = hook_paused();
    HICON next = glyph_create_icon(icon_size, icon_light, paused);
    if (!next) next = LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, icon_size, icon_size, 0);

    NOTIFYICONDATAW data;
    init_icon_data(&data);
    data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
    data.uCallbackMessage = WM_APP_TRAY;
    data.hIcon = next;
    lstrcpynW(data.szTip, str(paused ? STR_TIP_PAUSED : STR_TIP_ACTIVE), ARRAYSIZE(data.szTip));
    bool ok = Shell_NotifyIconW(action, &data) != FALSE;
    if (ok && action == NIM_ADD) {
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
    }
    HICON old = ok ? icon : next;
    if (ok) icon = next;
    if (old) DestroyIcon(old);
    return ok;
}

/* Explorer may not be ready yet when started at sign-in: retry until it is. */
static void add_icon(void) {
    NOTIFYICONDATAW data;
    init_icon_data(&data);
    Shell_NotifyIconW(NIM_DELETE, &data); /* a stale icon would make NIM_ADD fail */
    icon_added = show_icon(NIM_ADD);
    if (icon_added) KillTimer(window, TIMER_ADD_ICON);
    else SetTimer(window, TIMER_ADD_ICON, ADD_ICON_RETRY_MS, NULL);
}

static void remove_icon(void) {
    NOTIFYICONDATAW data;
    init_icon_data(&data);
    if (icon_added) Shell_NotifyIconW(NIM_DELETE, &data);
    icon_added = false;
    if (icon) DestroyIcon(icon);
    icon = NULL;
}

static void refresh_icon_if_changed(void) {
    if (icon_added && (tray_icon_size() != icon_size || light_taskbar() != icon_light)) show_icon(NIM_MODIFY);
}

static void toggle_pause(void) {
    hook_set_paused(!hook_paused());
    if (hook_paused()) KillTimer(window, TIMER_SWITCH);
    if (icon_added) show_icon(NIM_MODIFY);
}

static void show_about(void) {
    if (about_open) return;
    about_open = true;
    MessageBoxW(window, str(STR_ABOUT), str(STR_TITLE), MB_OK | MB_ICONINFORMATION);
    about_open = false;
}

static void show_menu(int x, int y) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, str(STR_MENU_HEADER));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING | (hook_paused() ? MF_CHECKED : 0), CMD_PAUSE, str(STR_MENU_PAUSE));
    AppendMenuW(menu, MF_STRING, CMD_ABOUT, str(STR_MENU_ABOUT));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, CMD_EXIT, str(STR_MENU_EXIT));
    /* Without the foreground the menu would not close on an outside click. */
    SetForegroundWindow(window);
    UINT align = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    int command = TrackPopupMenuEx(menu, align | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        x, y, window, NULL);
    PostMessageW(window, WM_NULL, 0, 0);
    DestroyMenu(menu);
    switch (command) {
    case CMD_PAUSE: toggle_pause(); break;
    case CMD_ABOUT: show_about(); break;
    case CMD_EXIT: DestroyWindow(window); break;
    }
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_APP_TRAY:
        /* NOTIFYICON_VERSION_4: the event is in LOWORD(lparam), the anchor in wparam. */
        switch (LOWORD(lparam)) {
        case WM_CONTEXTMENU: show_menu((short)LOWORD(wparam), (short)HIWORD(wparam)); break;
        case WM_LBUTTONDBLCLK: toggle_pause(); break;
        }
        return 0;
    case WM_APP_SWITCH:
        hook_switch_language(hwnd, TIMER_SWITCH);
        return 0;
    case WM_TIMER:
        if (wparam == TIMER_SWITCH) hook_on_timer(hwnd, TIMER_SWITCH);
        else if (wparam == TIMER_ADD_ICON) add_icon();
        return 0;
    case WM_SETTINGCHANGE: /* theme switch arrives as "ImmersiveColorSet" */
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        refresh_icon_if_changed();
        break;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_SWITCH);
        KillTimer(hwnd, TIMER_ADD_ICON);
        remove_icon();
        if (running) PostQuitMessage(0);
        running = false;
        return 0;
    default:
        if (message == taskbar_created && taskbar_created) {
            add_icon(); /* Explorer restarted */
            return 0;
        }
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

HWND tray_create(HINSTANCE app) {
    instance = app;
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = window_class;
    if (!RegisterClassW(&wc)) return NULL;
    /* A hidden top-level window: message-only windows miss the broadcasts
       (TaskbarCreated, WM_SETTINGCHANGE) the tray icon depends on. */
    window = CreateWindowExW(WS_EX_TOOLWINDOW, window_class, str(STR_TITLE), WS_POPUP, 0, 0, 0, 0,
        NULL, NULL, instance, NULL);
    if (!window) return NULL;
    taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    if (taskbar_created) ChangeWindowMessageFilterEx(window, taskbar_created, MSGFLT_ALLOW, NULL);
    if (!hook_start(window, WM_APP_SWITCH)) {
        DestroyWindow(window);
        return NULL;
    }
    running = true;
    add_icon();
    return window;
}

#include "hook.h"
#include "keystate.h"

#define SWITCH_RETRY_MS 15
#define SWITCH_WAIT_MS 2000
#define KEY_DOWN_BIT 0x8000

static ks_state state;
static HHOOK hook;
static HANDLE thread;
static DWORD thread_id;
static HANDLE ready;
static volatile bool installed;
static HWND notify_window;
static UINT notify_message;
static ULONGLONG switch_deadline;

static bool key_down(int key) {
    return (GetAsyncKeyState(key) & KEY_DOWN_BIT) != 0;
}

static bool modifier_down(void) {
    for (int i = 0; i < KS_PHYSICAL_MODIFIER_COUNT; i++)
        if (key_down(ks_physical_modifiers[i])) return true;
    return false;
}

static LRESULT CALLBACK on_key(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION) {
        const KBDLLHOOKSTRUCT *key = (const KBDLLHOOKSTRUCT *)data;
        bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
        if (key->dwExtraInfo != HOOK_OWN_MARKER && (down || up)) {
            ks_action action = ks_process(&state, (int)key->vkCode, down);
            if (action == KS_SWITCH) PostMessageW(notify_window, notify_message, 0, 0);
            if (action != KS_PASS) return 1;
        }
    }
    return CallNextHookEx(hook, code, message, data);
}

static DWORD WINAPI hook_thread(LPVOID unused) {
    (void)unused;
    MSG message;
    PeekMessageW(&message, NULL, 0, 0, PM_NOREMOVE); /* create the message queue */
    for (int i = 0; i < KS_PHYSICAL_MODIFIER_COUNT; i++)
        if (key_down(ks_physical_modifiers[i])) ks_process(&state, ks_physical_modifiers[i], true);
    hook = SetWindowsHookExW(WH_KEYBOARD_LL, on_key, GetModuleHandleW(NULL), 0);
    installed = hook != NULL;
    SetEvent(ready);
    if (!hook) return 1;
    while (GetMessageW(&message, NULL, 0, 0) > 0) DispatchMessageW(&message);
    UnhookWindowsHookEx(hook);
    hook = NULL;
    return 0;
}

bool hook_start(HWND window, UINT switch_message) {
    notify_window = window;
    notify_message = switch_message;
    ks_init(&state, key_down);
    ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!ready) return false;
    thread = CreateThread(NULL, 0, hook_thread, NULL, 0, &thread_id);
    if (thread) {
        WaitForSingleObject(ready, INFINITE);
        if (!installed) {
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
            thread = NULL;
        }
    }
    CloseHandle(ready);
    ready = NULL;
    return thread != NULL;
}

void hook_stop(void) {
    if (!thread) return;
    PostThreadMessageW(thread_id, WM_QUIT, 0, 0);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    thread = NULL;
}

void hook_set_paused(bool paused) {
    state.enabled = !paused;
}

bool hook_paused(void) {
    return !state.enabled;
}

static void key_input(INPUT *input, WORD key, bool up) {
    input->type = INPUT_KEYBOARD;
    input->ki.wVk = key;
    input->ki.wScan = 0;
    input->ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0) | (key == VK_LWIN ? KEYEVENTF_EXTENDEDKEY : 0);
    input->ki.time = 0;
    input->ki.dwExtraInfo = HOOK_OWN_MARKER;
}

/* Win+Space selects the next input language. */
static void send_switch(void) {
    INPUT inputs[4];
    key_input(&inputs[0], VK_LWIN, false);
    key_input(&inputs[1], VK_SPACE, false);
    key_input(&inputs[2], VK_SPACE, true);
    key_input(&inputs[3], VK_LWIN, true);
    SendInput(4, inputs, sizeof inputs[0]);
}

void hook_switch_language(HWND window, UINT_PTR timer_id) {
    if (hook_paused()) return;
    if (!modifier_down()) {
        KillTimer(window, timer_id);
        send_switch();
        return;
    }
    switch_deadline = GetTickCount64() + SWITCH_WAIT_MS;
    SetTimer(window, timer_id, SWITCH_RETRY_MS, NULL);
}

void hook_on_timer(HWND window, UINT_PTR timer_id) {
    if (hook_paused() || GetTickCount64() >= switch_deadline) {
        KillTimer(window, timer_id);
    } else if (!modifier_down()) {
        KillTimer(window, timer_id);
        send_switch();
    }
}

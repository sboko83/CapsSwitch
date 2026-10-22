/* Entry point without the C runtime: single instance, --stop, message loop. */

#include <windows.h>
#include "hook.h"
#include "strings.h"
#include "tray.h"

/* Named objects are scoped to the current Windows session. */
#define INSTANCE_NAME L"Local\\CapsSwitch.Instance"
#define STOP_NAME L"Local\\CapsSwitch.Stop"
#define STOP_WAIT_MS 5000

/* The compiler may emit calls to these for struct initialisation and copies.
   volatile keeps the loops from being turned back into memset/memcpy calls. */
#pragma function(memset, memcpy)
void *memset(void *destination, int value, size_t size) {
    volatile unsigned char *p = destination;
    while (size--) *p++ = (unsigned char)value;
    return destination;
}

void *memcpy(void *destination, const void *source, size_t size) {
    volatile unsigned char *d = destination;
    const unsigned char *s = source;
    while (size--) *d++ = *s++;
    return destination;
}

/* The command line after the program name, without leading blanks. */
static const wchar_t *arguments(void) {
    const wchar_t *p = GetCommandLineW();
    if (*p == L'"') {
        for (p++; *p && *p != L'"'; p++) {}
        if (*p) p++;
    } else {
        while (*p && *p != L' ' && *p != L'\t') p++;
    }
    while (*p == L' ' || *p == L'\t') p++;
    return p;
}

/* True if the arguments are exactly this option, ignoring trailing blanks. */
static bool is_option(const wchar_t *args, const wchar_t *option) {
    while (*option && *args == *option) {
        args++;
        option++;
    }
    if (*option) return false;
    while (*args == L' ' || *args == L'\t') args++;
    return *args == 0;
}

/* Signals the running instance and waits until it has exited, so the EXE
   can be replaced right after --stop returns. */
static int stop_running_instance(void) {
    HANDLE stop = OpenEventW(EVENT_MODIFY_STATE, FALSE, STOP_NAME);
    if (!stop) return 0;
    HANDLE instance = OpenMutexW(SYNCHRONIZE, FALSE, INSTANCE_NAME);
    SetEvent(stop);
    CloseHandle(stop);
    if (instance) {
        DWORD result = WaitForSingleObject(instance, STOP_WAIT_MS);
        if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) ReleaseMutex(instance);
        CloseHandle(instance);
    }
    return 0;
}

/* Waits for window messages and the stop event without timers or polling. */
static int message_loop(HWND window, HANDLE stop) {
    for (;;) {
        DWORD result = MsgWaitForMultipleObjects(1, &stop, FALSE, INFINITE, QS_ALLINPUT);
        if (result == WAIT_OBJECT_0) DestroyWindow(window);
        else if (result == WAIT_FAILED) return 1;
        MSG message;
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) return (int)message.wParam;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

static int run(void) {
    const wchar_t *args = arguments();
    if (is_option(args, L"--stop")) return stop_running_instance();
    /* The elevated relaunch: replace the running instance, then start as usual. */
    if (is_option(args, L"--restart")) stop_running_instance();

    HANDLE instance = CreateMutexW(NULL, TRUE, INSTANCE_NAME);
    if (!instance) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(instance);
        return 0;
    }
    int code = 1;
    HANDLE stop = CreateEventW(NULL, FALSE, FALSE, STOP_NAME);
    HWND window = stop ? tray_create(GetModuleHandleW(NULL)) : NULL;
    if (window) {
        code = message_loop(window, stop);
        hook_stop();
    } else {
        MessageBoxW(NULL, str(STR_START_FAILED), str(STR_TITLE), MB_OK | MB_ICONERROR);
    }
    if (stop) CloseHandle(stop);
    ReleaseMutex(instance);
    CloseHandle(instance);
    return code;
}

void __stdcall entry(void) {
    ExitProcess((UINT)run());
}

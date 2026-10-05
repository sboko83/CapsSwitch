/* Console test runner, built with the regular CRT: build.ps1 test */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/glyph.h"
#include "../src/hook.h"
#include "../src/keystate.h"

static int checks, failures;

static void check(bool ok, const char *label, int size) {
    checks++;
    if (ok) return;
    failures++;
    fprintf(stderr, "FAIL %s (%d px)\n", label, size);
}

static const char *action_name(ks_action action) {
    switch (action) {
    case KS_PASS: return "Pass";
    case KS_SUPPRESS: return "Suppress";
    case KS_SWITCH: return "Switch";
    }
    return "?";
}

static void expect(ks_action actual, ks_action expected, const char *label) {
    checks++;
    if (actual == expected) return;
    failures++;
    fprintf(stderr, "FAIL %s: %s, expected %s\n", label, action_name(actual), action_name(expected));
}

static void test_caps_presses(void) {
    ks_state s;
    ks_init(&s, NULL);
    expect(ks_process(&s, 0x41, true), KS_PASS, "letter unchanged");
    expect(ks_process(&s, KS_VK_CAPITAL, false), KS_PASS, "orphan release");
    expect(ks_process(&s, KS_VK_CAPITAL, true), KS_SWITCH, "first press");
    for (int i = 0; i < 100; i++) expect(ks_process(&s, KS_VK_CAPITAL, true), KS_SUPPRESS, "held key");
    expect(ks_process(&s, KS_VK_CAPITAL, false), KS_SUPPRESS, "release blocked");
    expect(ks_process(&s, KS_VK_CAPITAL, true), KS_SWITCH, "second press");
    s.enabled = false;
    expect(ks_process(&s, KS_VK_CAPITAL, false), KS_SUPPRESS, "pause during press");
    expect(ks_process(&s, KS_VK_CAPITAL, true), KS_PASS, "paused");
    s.enabled = true;
    expect(ks_process(&s, KS_VK_CAPITAL, true), KS_PASS, "resume during passed press");
    expect(ks_process(&s, KS_VK_CAPITAL, false), KS_PASS, "release passed press");
}

static void test_modifiers(void) {
    ks_state s;
    ks_init(&s, NULL);
    for (int i = 0; i < KS_PHYSICAL_MODIFIER_COUNT; i++) {
        int mod = ks_physical_modifiers[i];
        expect(ks_process(&s, mod, true), KS_PASS, "modifier down");
        expect(ks_process(&s, KS_VK_CAPITAL, true), KS_PASS, "modified Caps passes");
        expect(ks_process(&s, mod, false), KS_PASS, "modifier up");
        expect(ks_process(&s, KS_VK_CAPITAL, true), KS_PASS, "modified repeat passes");
        expect(ks_process(&s, KS_VK_CAPITAL, false), KS_PASS, "modified release passes");
        expect(ks_process(&s, KS_VK_CAPITAL, true), KS_SWITCH, "normal Caps restored");
        expect(ks_process(&s, KS_VK_CAPITAL, false), KS_SUPPRESS, "normal release");
    }
    expect(ks_process(&s, KS_VK_LSHIFT, true), KS_PASS, "left shift");
    expect(ks_process(&s, KS_VK_RSHIFT, true), KS_PASS, "right shift");
    expect(ks_process(&s, KS_VK_LSHIFT, false), KS_PASS, "left shift released");
    expect(ks_process(&s, KS_VK_CAPITAL, true), KS_PASS, "right shift still held");
    expect(ks_process(&s, KS_VK_CAPITAL, false), KS_PASS, "release");
    expect(ks_process(&s, KS_VK_RSHIFT, false), KS_PASS, "right shift released");
    expect(ks_process(&s, KS_VK_CAPITAL, true), KS_SWITCH, "both shifts released");
    expect(ks_process(&s, KS_VK_CAPITAL, false), KS_SUPPRESS, "release");
    expect(ks_process(&s, KS_VK_SHIFT, true), KS_PASS, "generic shift");
    expect(ks_process(&s, KS_VK_CAPITAL, true), KS_PASS, "generic shift held");
    expect(ks_process(&s, KS_VK_CAPITAL, false), KS_PASS, "release");
    expect(ks_process(&s, KS_VK_SHIFT, false), KS_PASS, "generic shift released");
    expect(ks_process(&s, KS_VK_CAPITAL, true), KS_SWITCH, "generic shift gone");
    expect(ks_process(&s, KS_VK_CAPITAL, false), KS_SUPPRESS, "release");
}

static bool win_down;
static bool live_is_down(int key) { return key == KS_VK_LWIN && win_down; }

static void test_stale_modifiers(void) {
    ks_state live;
    ks_init(&live, live_is_down);
    win_down = true;
    expect(ks_process(&live, KS_VK_LWIN, true), KS_PASS, "win down");
    expect(ks_process(&live, KS_VK_CAPITAL, true), KS_PASS, "win held");
    expect(ks_process(&live, KS_VK_CAPITAL, false), KS_PASS, "release");
    win_down = false; /* Win+L: the release happened on the secure desktop */
    expect(ks_process(&live, KS_VK_CAPITAL, true), KS_SWITCH, "stale win dropped");
    expect(ks_process(&live, KS_VK_CAPITAL, false), KS_SUPPRESS, "release");
}

static uint32_t alpha_at(const uint32_t *pixels, int size, int x, int y) {
    return pixels[y * size + x] >> 24;
}

static void test_glyph_frame(const wchar_t *font, int size, bool light, bool paused) {
    uint32_t *pixels = malloc(sizeof *pixels * size * size);
    uint32_t ink = paused ? (light ? 0x101010 : 0x959595) : (light ? 0x1C1C1C : 0xFFFFFF);
    uint32_t top = paused ? 150 : 255, max_alpha = 0;
    int min_x = size, min_y = size, max_x = -1, max_y = -1;
    double sum = 0, sum_x = 0, sum_y = 0;
    bool same_ink = true;
    check(glyph_render_font(font, size, light, paused, pixels), "glyph rendered", size);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            uint32_t pixel = pixels[y * size + x], alpha = pixel >> 24;
            if (!alpha) continue;
            if (alpha > max_alpha) max_alpha = alpha;
            if ((pixel & 0xFFFFFF) != ink) same_ink = false;
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
            sum += alpha;
            sum_x += alpha * (x + 0.5);
            sum_y += alpha * (y + 0.5);
        }
    }
    check(same_ink, "glyph ink colour", size);
    check(max_alpha == top, "glyph full ink alpha", size);
    check(alpha_at(pixels, size, 0, 0) == 0 && alpha_at(pixels, size, size - 1, 0) == 0 &&
          alpha_at(pixels, size, 0, size - 1) == 0 && alpha_at(pixels, size, size - 1, size - 1) == 0,
          "glyph corners transparent", size);
    /* Centred: the ink box has equal margins and the ink balances in the middle. */
    check(min_x == size - 1 - max_x && min_y == size - 1 - max_y, "glyph ink box centred", size);
    check(sum > 0 && fabs(sum_x / sum - size / 2.0) < 0.1 && fabs(sum_y / sum - size / 2.0) < 0.1,
          "glyph ink centroid centred", size);
    free(pixels);
}

/* Verify embedded artwork through the same loader used by the tray. */
static void test_artwork(void) {
    static const int sizes[] = { 16, 20, 24, 32, 40, 48, 64, 96, 128, 256 };
    for (int i = 0; i < (int)(sizeof sizes / sizeof sizes[0]); i++) {
        int size = sizes[i];
        for (int paused = 0; paused < 2; paused++) {
            HICON icon = glyph_create_icon(size, false, paused != 0);
            ICONINFO info = { 0 };
            BITMAP bitmap = { 0 };
            bool ok = icon && GetIconInfo(icon, &info) &&
                      GetObjectW(info.hbmColor, sizeof bitmap, &bitmap);
            check(ok && bitmap.bmWidth == size && bitmap.bmHeight == size && bitmap.bmBitsPixel == 32,
                  "artwork size and colour depth", size);
            if (ok) {
                BITMAPINFO format = { 0 };
                format.bmiHeader.biSize = sizeof format.bmiHeader;
                format.bmiHeader.biWidth = size;
                format.bmiHeader.biHeight = -size;
                format.bmiHeader.biPlanes = 1;
                format.bmiHeader.biBitCount = 32;
                uint32_t *pixels = calloc((size_t)size * size, sizeof *pixels);
                HDC dc = CreateCompatibleDC(NULL);
                bool read = pixels && dc && GetDIBits(dc, info.hbmColor, 0, (UINT)size,
                    pixels, &format, DIB_RGB_COLORS) == size;
                check(read, "artwork pixels readable", size);
                if (read) {
                    uint32_t max_alpha = 0;
                    bool dark = false, beige = false;
                    for (int p = 0; p < size * size; p++) {
                        uint32_t pixel = pixels[p], alpha = pixel >> 24;
                        if (alpha > max_alpha) max_alpha = alpha;
                        if (alpha < (paused ? 140u : 240u)) continue;
                        uint32_t r = pixel >> 16 & 255, g = pixel >> 8 & 255, b = pixel & 255;
                        dark |= r < 100 && g < 100 && b < 100;
                        beige |= r > b && g > b && r > (paused ? 100u : 180u);
                    }
                    check(max_alpha == (paused ? 150u : 255u), "artwork pause opacity", size);
                    check(dark && beige, "artwork globe and beige key visible", size);
                    check(!alpha_at(pixels, size, 0, 0) && !alpha_at(pixels, size, size - 1, 0) &&
                          !alpha_at(pixels, size, 0, size - 1) &&
                          !alpha_at(pixels, size, size - 1, size - 1), "artwork transparent corners", size);
                }
                if (dc) DeleteDC(dc);
                free(pixels);
            }
            if (info.hbmColor) DeleteObject(info.hbmColor);
            if (info.hbmMask) DeleteObject(info.hbmMask);
            if (icon) DestroyIcon(icon);
        }
    }
}

static void test_glyph(void) {
    static const int sizes[] = { 16, 20, 24, 32, 40, 48 };
    uint32_t probe[16 * 16];
    bool any_font = false;
    for (int f = 0; f < GLYPH_FONT_COUNT; f++) {
        if (!glyph_render_font(glyph_fonts[f], 16, false, false, probe)) {
            printf("skipped: %ls is not installed\n", glyph_fonts[f]);
            continue;
        }
        any_font = true;
        for (int i = 0; i < (int)(sizeof sizes / sizeof sizes[0]); i++)
            for (int light = 0; light < 2; light++)
                for (int paused = 0; paused < 2; paused++) test_glyph_frame(glyph_fonts[f], sizes[i], light, paused);
    }
    check(any_font, "an icon font is installed", 0);
    check(!glyph_render_font(L"No Such Font CapsSwitch", 16, false, false, probe), "missing font detected", 16);

    for (int i = 0; i < (int)(sizeof sizes / sizeof sizes[0]); i++) {
        int size = sizes[i];
        HICON icon = glyph_create_icon(size, false, false);
        ICONINFO info = { 0 };
        BITMAP bitmap;
        bool ok = icon && GetIconInfo(icon, &info);
        if (ok) {
            ok = GetObjectW(info.hbmColor, sizeof bitmap, &bitmap) && bitmap.bmWidth == size &&
                 bitmap.bmHeight == size && bitmap.bmBitsPixel == 32;
            DeleteObject(info.hbmColor);
            DeleteObject(info.hbmMask);
        }
        check(ok, "glyph icon handle", size);
        if (icon) DestroyIcon(icon);
    }
}
/* The hook installs on its own thread and shuts down cleanly, twice. */
static void test_hook_lifecycle(void) {
    for (int round = 0; round < 2; round++) {
        bool started = hook_start(NULL, WM_APP);
        check(started, "hook installed", round);
        hook_set_paused(true);
        check(hook_paused(), "hook paused", round);
        hook_set_paused(false);
        check(!hook_paused(), "hook resumed", round);
        if (started) hook_stop();
    }
}

int main(void) {
    test_caps_presses();
    test_modifiers();
    test_stale_modifiers();
    test_glyph();
    test_artwork();
    test_hook_lifecycle();
    if (failures) {
        fprintf(stderr, "FAILED: %d of %d checks\n", failures, checks);
        return 1;
    }
    printf("PASS: %d checks (keyboard behaviour, tray artwork and glyph fallback, hook lifecycle)\n", checks);
    return 0;
}

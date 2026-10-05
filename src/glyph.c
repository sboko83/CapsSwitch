#include "glyph.h"
#include "tray.h"

/* The glyph comes from the same icon font as the system tray icons, so its
   hinting keeps lines on the pixel grid and the shape centred at every size.
   The font's em square is the icon square: baseline at the bottom edge. */

#define GLOBE 0xE774            /* Globe in both icon fonts */
#define INK_ON_DARK 0xFFFFFFu   /* RGB on a dark taskbar */
#define INK_ON_LIGHT 0x1C1C1Cu  /* RGB on a light taskbar */
#define ALPHA_ACTIVE 255u
/* Pause darkens the ink as well as fading it. */
#define ALPHA_PAUSED 150u
#define PAUSED_INK_ON_DARK 0x959595u
#define PAUSED_INK_ON_LIGHT 0x101010u

const wchar_t *const glyph_fonts[GLYPH_FONT_COUNT] = { L"Segoe Fluent Icons", L"Segoe MDL2 Assets" };

/* Draws the globe in white on the black bitmap selected into dc. */
static bool draw_globe(HDC dc, const wchar_t *face, int size) {
    HFONT font = CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face);
    if (!font) return false;
    HGDIOBJ old = SelectObject(dc, font);
    WCHAR actual[LF_FACESIZE], ch = GLOBE;
    WORD index = 0xFFFF;
    /* GDI silently substitutes a missing font, so check what it picked. */
    bool ok = GetTextFaceW(dc, LF_FACESIZE, actual) && !lstrcmpiW(actual, face) &&
              GetGlyphIndicesW(dc, &ch, 1, &index, GGI_MARK_NONEXISTING_GLYPHS) == 1 && index != 0xFFFF;
    if (ok) {
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkMode(dc, TRANSPARENT);
        SetTextAlign(dc, TA_LEFT | TA_BASELINE);
        ok = TextOutW(dc, 0, size, &ch, 1) != 0;
        GdiFlush();
    }
    SelectObject(dc, old);
    DeleteObject(font);
    return ok;
}

static HBITMAP create_dib(HDC dc, int size, void **bits) {
    BITMAPINFOHEADER header;
    header.biSize = sizeof header;
    header.biWidth = size;
    header.biHeight = -size; /* top-down */
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    header.biSizeImage = 0;
    header.biXPelsPerMeter = 0;
    header.biYPelsPerMeter = 0;
    header.biClrUsed = 0;
    header.biClrImportant = 0;
    *bits = NULL;
    return CreateDIBSection(dc, (BITMAPINFO *)&header, DIB_RGB_COLORS, bits, NULL, 0);
}

bool glyph_render_font(const wchar_t *font, int size, bool light_taskbar, bool paused, uint32_t *pixels) {
    HDC dc = CreateCompatibleDC(NULL);
    if (!dc) return false;
    void *bits;
    HBITMAP bitmap = create_dib(dc, size, &bits);
    bool ok = false;
    if (bitmap) {
        HGDIOBJ old = SelectObject(dc, bitmap);
        PatBlt(dc, 0, 0, size, size, BLACKNESS);
        ok = draw_globe(dc, font, size);
        SelectObject(dc, old);
        if (ok) {
            /* Grayscale antialiasing: any channel is the coverage. */
            const uint32_t *coverage = bits;
            uint32_t rgb = paused ? (light_taskbar ? PAUSED_INK_ON_LIGHT : PAUSED_INK_ON_DARK)
                                  : (light_taskbar ? INK_ON_LIGHT : INK_ON_DARK);
            uint32_t ink_alpha = paused ? ALPHA_PAUSED : ALPHA_ACTIVE;
            for (int i = 0; i < size * size; i++) {
                uint32_t alpha = ((coverage[i] >> 8 & 0xFF) * ink_alpha + 127) / 255;
                pixels[i] = alpha ? alpha << 24 | rgb : 0;
            }
        }
        DeleteObject(bitmap);
    }
    DeleteDC(dc);
    return ok;
}

bool glyph_render(int size, bool light_taskbar, bool paused, uint32_t *pixels) {
    for (int i = 0; i < GLYPH_FONT_COUNT; i++)
        if (glyph_render_font(glyph_fonts[i], size, light_taskbar, paused, pixels)) return true;
    return false;
}

HICON glyph_create_icon(int size, bool light_taskbar, bool paused) {
    if (size < GLYPH_MIN_SIZE) size = GLYPH_MIN_SIZE;
    if (size > GLYPH_MAX_SIZE) size = GLYPH_MAX_SIZE;

    HICON artwork = LoadImageW(GetModuleHandleW(NULL),
        MAKEINTRESOURCEW(paused ? IDI_APP_PAUSED : IDI_APP), IMAGE_ICON, size, size, 0);
    if (artwork) return artwork;

    void *bits;
    HBITMAP color = create_dib(NULL, size, &bits);
    if (!color) return NULL;
    uint32_t *pixels = bits;
    HBITMAP mask = NULL;
    if (glyph_render(size, light_taskbar, paused, pixels)) {
        /* AND mask: set where fully transparent, for consumers that ignore
           alpha. Monochrome rows are padded to 16 bits. */
        int stride = (size + 15) / 16 * 2;
        HANDLE heap = GetProcessHeap();
        BYTE *mask_bits = HeapAlloc(heap, HEAP_ZERO_MEMORY, (SIZE_T)stride * size);
        if (mask_bits) {
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++)
                    if (!pixels[y * size + x]) mask_bits[y * stride + x / 8] |= (BYTE)(0x80 >> x % 8);
            mask = CreateBitmap(size, size, 1, 1, mask_bits);
            HeapFree(heap, 0, mask_bits);
        }
    }

    HICON icon = NULL;
    if (mask) {
        ICONINFO info;
        info.fIcon = TRUE;
        info.xHotspot = 0;
        info.yHotspot = 0;
        info.hbmMask = mask;
        info.hbmColor = color;
        icon = CreateIconIndirect(&info);
        DeleteObject(mask);
    }
    DeleteObject(color);
    return icon;
}

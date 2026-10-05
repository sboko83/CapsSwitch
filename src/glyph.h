#ifndef CAPSSWITCH_GLYPH_H
#define CAPSSWITCH_GLYPH_H

/* Tray artwork: the embedded keycap icon at the exact small-icon size.
   The system Globe font glyph remains a fallback if the resource is missing. */

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

#define GLYPH_MIN_SIZE 8
#define GLYPH_MAX_SIZE 256

/* Icon fonts in order of preference: Windows 11, then Windows 10. */
#define GLYPH_FONT_COUNT 2
extern const wchar_t *const glyph_fonts[GLYPH_FONT_COUNT];

/* Fills size*size top-down pixels as 0xAARRGGBB with straight
   (non-premultiplied) alpha, the layout of a 32-bit icon DIB.
   Returns false if no icon font with the glyph is installed. */
bool glyph_render(int size, bool light_taskbar, bool paused, uint32_t *pixels);

/* Same with one specific font; false if it is missing or lacks the glyph. */
bool glyph_render_font(const wchar_t *font, int size, bool light_taskbar, bool paused, uint32_t *pixels);

/* Loads the active or paused artwork, falling back to the system glyph.
   Returns NULL on failure; the caller destroys the icon with DestroyIcon. */
HICON glyph_create_icon(int size, bool light_taskbar, bool paused);

#endif

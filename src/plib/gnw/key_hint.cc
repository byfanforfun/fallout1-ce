#include "plib/gnw/key_hint.h"

#include <math.h>
#include <string.h>

#include <SDL.h>

#include "plib/color/color.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input_rebind.h"
#include "plib/gnw/kb.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

#define KEY_HINT_SURFACE_WIDTH 64
#define KEY_HINT_SURFACE_HEIGHT 32
#define KEY_HINT_FONT 101

typedef struct GamepadKeyLabel {
    int key;
    const char* label;
} GamepadKeyLabel;

// Windows that handle some actions with raw keys instead of registered buttons
// (list scrolling, pagination, switching stacks). These can't be discovered by
// scanning the button list, so they are listed explicitly per screen. The badge
// is drawn as a small legend along the bottom of the active window in the order
// given here.
#define KEY_HINT_LEGEND_MAX 8
#define KEY_HINT_LEGEND_SPACING 38
#define KEY_HINT_LEGEND_MARGIN_X 24
#define KEY_HINT_LEGEND_MARGIN_Y 18

typedef struct KeyHintLegend {
    int screen;
    int keys[KEY_HINT_LEGEND_MAX];
} KeyHintLegend;

// Actions that are not backed by any button but still need a hint at a fixed
// spot (e.g. the two toggle knobs in the kiosk start message). Coordinates are
// relative to the top-left corner of the active window and denote the center of
// the badge.
typedef struct KeyHintExtra {
    int screen;
    int key;
    int x;
    int y;
} KeyHintExtra;

static const KeyHintExtra key_hint_extras[] = {
    // Kiosk start message: difficulty and language filter knobs.
    { SCREEN_START_MESSAGE, 505, 178, 364 },
    { SCREEN_START_MESSAGE, 506, 178, 428 },
};

static const KeyHintLegend key_hint_legends[] = {
    { SCREEN_INV, { KEY_ARROW_UP, KEY_ARROW_DOWN, KEY_PAGE_UP, KEY_PAGE_DOWN, 0 } },
    { SCREEN_USE_ITEM, { KEY_ARROW_UP, KEY_ARROW_DOWN, KEY_PAGE_UP, KEY_PAGE_DOWN, 0 } },
    { SCREEN_LOOT, { KEY_ARROW_UP, KEY_ARROW_DOWN, KEY_ARROW_LEFT, KEY_ARROW_RIGHT, KEY_PAGE_UP, KEY_PAGE_DOWN, 0 } },
    { SCREEN_BARTER, { KEY_ARROW_UP, KEY_ARROW_DOWN, KEY_PAGE_UP, KEY_PAGE_DOWN, 0 } },
    { SCREEN_LOAD_SAVE, { KEY_ARROW_UP, KEY_ARROW_DOWN, KEY_HOME, KEY_END, 0 } },
    { SCREEN_FILE_LOAD, { KEY_ARROW_UP, KEY_ARROW_DOWN, KEY_HOME, KEY_END, 0 } },
    { SCREEN_FILE_SAVE, { KEY_ARROW_UP, KEY_ARROW_DOWN, KEY_HOME, KEY_END, 0 } },
    { SCREEN_CHAR_SELECT, { KEY_ARROW_LEFT, KEY_ARROW_RIGHT, 0 } },
    { SCREEN_START_MESSAGE, { KEY_ARROW_LEFT, KEY_ARROW_RIGHT, KEY_PAGE_UP, KEY_PAGE_DOWN, 0 } },
};

// Maps game action key codes to the label printed on the corresponding gamepad
// control, assuming the default bindings from gamepad.cc.
static const GamepadKeyLabel gamepad_key_labels[] = {
    { 105, "D^" },
    { 9, "Dv" },
    { 99, "D<" },
    { 112, "D>" },
    { 13, "A" },
    { 32, "B" },
    { 115, "X" },
    { 27, "Y" },
    { 110, "L1" },
    { 97, "R1" },
    { 98, "L2" },
    { 109, "R2" },
    { 327, "L3" },
    { 320, "ST" },
    { 321, "BK" },
    { 390, "GD" },
    { 331, "L<" },
    { 333, "L>" },
    { 328, "L^" },
    { 336, "Lv" },
};

static SDL_Surface* key_hint_surface = NULL;
static int key_hint_surface_failed = 0;
static bool key_hint_visible = false;

static const char* key_hint_label(int key)
{
    for (size_t index = 0; index < sizeof(gamepad_key_labels) / sizeof(gamepad_key_labels[0]); index++) {
        if (gamepad_key_labels[index].key == key) {
            return gamepad_key_labels[index].label;
        }
    }

    return NULL;
}

static SDL_Surface* key_hint_get_surface()
{
    if (key_hint_surface_failed) {
        return NULL;
    }

    if (key_hint_surface == NULL) {
        key_hint_surface = SDL_CreateRGBSurfaceWithFormat(0, KEY_HINT_SURFACE_WIDTH, KEY_HINT_SURFACE_HEIGHT, 8, SDL_PIXELFORMAT_INDEX8);
        if (key_hint_surface == NULL) {
            key_hint_surface_failed = 1;
        }
    }

    return key_hint_surface;
}

static void key_hint_sync_palette()
{
    if (key_hint_surface == NULL
        || key_hint_surface->format->palette == NULL
        || gSdlSurface == NULL
        || gSdlSurface->format->palette == NULL) {
        return;
    }

    SDL_SetPaletteColors(key_hint_surface->format->palette, gSdlSurface->format->palette->colors, 0, gSdlSurface->format->palette->ncolors);
}

static void key_hint_draw_badge(int center_x, int center_y, const char* label)
{
    int text_w = text_width(label);
    int text_h = text_height();

    int diameter = 20;
    if (text_w + 8 > diameter) {
        diameter = text_w + 8;
    }
    if (text_h + 6 > diameter) {
        diameter = text_h + 6;
    }
    if (diameter > KEY_HINT_SURFACE_HEIGHT) {
        diameter = KEY_HINT_SURFACE_HEIGHT;
    }
    if (diameter > KEY_HINT_SURFACE_WIDTH) {
        diameter = KEY_HINT_SURFACE_WIDTH;
    }

    unsigned char* buf = (unsigned char*)key_hint_surface->pixels;
    int pitch = key_hint_surface->pitch;
    memset(buf, 0, pitch * KEY_HINT_SURFACE_HEIGHT);

    int fill_color = colorTable[0x18E3];
    int border_color = colorTable[0x7FFF];
    int text_color = colorTable[0x7FFF];

    int radius = diameter / 2;
    for (int y = 0; y < diameter; y++) {
        int dy = y - radius;
        int dx = (int)sqrt((double)(radius * radius - dy * dy));

        int x = radius - dx;
        int width = dx * 2 + 1;
        if (x < 0) {
            width += x;
            x = 0;
        }
        if (x + width > diameter) {
            width = diameter - x;
        }

        if (width <= 0) {
            continue;
        }

        buf_fill(buf + y * pitch + x, width, 1, pitch, fill_color);
        buf[y * pitch + x] = border_color;
        buf[y * pitch + x + width - 1] = border_color;
    }

    text_to_buf(buf + ((diameter - text_h) / 2) * pitch + (diameter - text_w) / 2, label, diameter, pitch, text_color);

    key_hint_sync_palette();
    SDL_SetColorKey(key_hint_surface, SDL_TRUE, 0);

    SDL_Rect src;
    src.x = 0;
    src.y = 0;
    src.w = diameter;
    src.h = diameter;

    SDL_Rect dst;
    dst.x = center_x - diameter / 2;
    dst.y = center_y - diameter / 2;
    dst.w = diameter;
    dst.h = diameter;

    SDL_BlitSurface(key_hint_surface, &src, gSdlTextureSurface, &dst);
}

static void key_hint_draw_legends(Window* window)
{
    for (size_t legend = 0; legend < sizeof(key_hint_legends) / sizeof(key_hint_legends[0]); legend++) {
        if (key_hint_legends[legend].screen != current_screen) {
            continue;
        }

        int index = 0;
        for (int slot = 0; slot < KEY_HINT_LEGEND_MAX && key_hint_legends[legend].keys[slot] != 0; slot++) {
            const char* label = key_hint_label(get_physical_key(current_screen, key_hint_legends[legend].keys[slot]));
            if (label == NULL) {
                continue;
            }

            int center_x = window->rect.ulx + KEY_HINT_LEGEND_MARGIN_X + index * KEY_HINT_LEGEND_SPACING;
            int center_y = window->rect.lry - KEY_HINT_LEGEND_MARGIN_Y;
            key_hint_draw_badge(center_x, center_y, label);
            index++;
        }

        break;
    }
}

static void key_hint_draw_extras(Window* window)
{
    for (size_t extra = 0; extra < sizeof(key_hint_extras) / sizeof(key_hint_extras[0]); extra++) {
        if (key_hint_extras[extra].screen != current_screen) {
            continue;
        }

        const char* label = key_hint_label(get_physical_key(current_screen, key_hint_extras[extra].key));
        if (label == NULL) {
            continue;
        }

        key_hint_draw_badge(window->rect.ulx + key_hint_extras[extra].x,
            window->rect.uly + key_hint_extras[extra].y,
            label);
    }
}

static Window* key_hint_active_window()
{
    for (int index = win_get_num_windows() - 1; index >= 1; index--) {
        Window* window = win_get_window(index);
        if (window == NULL) {
            continue;
        }

        if ((window->flags & WINDOW_HIDDEN) != 0) {
            continue;
        }

        if (window->buttonListHead != NULL) {
            return window;
        }

        if ((window->flags & WINDOW_MODAL) != 0) {
            return window;
        }
    }

    return NULL;
}

void key_hint_update()
{
    if (!GNW_win_init_flag
        || gSdlSurface == NULL
        || gSdlTextureSurface == NULL) {
        return;
    }

    bool active = kb_is_alt_held();
    if (!active && !key_hint_visible) {
        return;
    }

    // The overlay is drawn directly into the texture surface, which the window
    // manager never clears on its own. Repaint all windows first so hints from
    // the previous frame do not linger once Alt is released or buttons move.
    Rect screen_rect;
    screen_rect.ulx = 0;
    screen_rect.uly = 0;
    screen_rect.lrx = gSdlTextureSurface->w - 1;
    screen_rect.lry = gSdlTextureSurface->h - 1;
    win_refresh_all(&screen_rect);

    if (!active) {
        key_hint_visible = false;
        return;
    }

    key_hint_visible = true;

    Window* window = key_hint_active_window();
    if (window == NULL) {
        return;
    }

    if (key_hint_get_surface() == NULL) {
        return;
    }

    int saved_font = text_curr();
    text_font(KEY_HINT_FONT);

    for (Button* button = window->buttonListHead; button != NULL; button = button->next) {
        if ((button->flags & BUTTON_FLAG_DISABLED) != 0) {
            continue;
        }

        if (button->rect.lrx <= button->rect.ulx || button->rect.lry <= button->rect.uly) {
            continue;
        }

        int code = button->leftMouseUpEventCode;
        if (code < 0) {
            code = button->rightMouseUpEventCode;
        }
        if (code < 0) {
            continue;
        }

        const char* label = key_hint_label(get_physical_key(current_screen, code));
        if (label == NULL) {
            continue;
        }

        int center_x = window->rect.ulx + (button->rect.ulx + button->rect.lrx) / 2;
        int center_y = window->rect.uly + (button->rect.uly + button->rect.lry) / 2;
        key_hint_draw_badge(center_x, center_y, label);
    }

    key_hint_draw_legends(window);
    key_hint_draw_extras(window);

    text_font(saved_font);
}

} // namespace fallout

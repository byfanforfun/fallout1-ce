#include "game/vkb.h"

#include <string.h>

#include "game/art.h"
#include "game/gkioskconf.h"
#include "game/gsound.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/gnw/button.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/kb.h"
#include "plib/gnw/text.h"

namespace fallout {

// =============================================================================
// Numeric keyboard
// =============================================================================

void vkb_numeric_draw(int win, int kbY)
{
    unsigned char* windowBuffer = win_get_buf(win);
    int windowWidth = win_width(win);

    CacheEntry* backgroundHandle;
    int backgroundFid = art_id(OBJ_TYPE_INTERFACE, VKB_NUMERIC_FRM, 0, 0, 0);
    unsigned char* backgroundData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundHandle);
    if (backgroundData != NULL) {
        buf_to_buf(backgroundData, VKB_NUMERIC_FRM_WIDTH, VKB_NUMERIC_FRM_HEIGHT, VKB_NUMERIC_FRM_WIDTH, windowBuffer + kbY * windowWidth, windowWidth);
        art_ptr_unlock(backgroundHandle);
    } else {
        buf_fill(windowBuffer + kbY * windowWidth, VKB_NUMERIC_FRM_WIDTH, VKB_NUMERIC_FRM_HEIGHT, windowWidth, 84);
    }

    CacheEntry* digitHandle;
    int digitFid = art_id(OBJ_TYPE_INTERFACE, VKB_NUMERIC_BIGNUM_FRM, 0, 0, 0);
    unsigned char* digitData = art_ptr_lock_data(digitFid, 0, 0, &digitHandle);
    if (digitData == NULL) {
        return;
    }

    const int rowCenterY[2] = {
        VKB_NUMERIC_ROW1_CENTER_Y,
        VKB_NUMERIC_ROW2_CENTER_Y,
    };
    for (int row = 0; row < 2; row++) {
        for (int index = 0; index < VKB_NUMERIC_DIGIT_COUNT; index++) {
            int digit = row == 0 ? index + 1 : (index < 4 ? index + 6 : 0);
            int centerX = VKB_NUMERIC_COL_START_X + index * VKB_NUMERIC_STEP;
            unsigned char* src = digitData + VKB_NUMERIC_DIGIT_GLYPH_WIDTH * digit;
            buf_to_buf(src, VKB_NUMERIC_DIGIT_GLYPH_WIDTH, VKB_NUMERIC_DIGIT_GLYPH_HEIGHT, 336, windowBuffer + windowWidth * (kbY + rowCenterY[row] - 12) + (centerX - 7), windowWidth);
        }
    }

    art_ptr_unlock(digitHandle);
}

void vkb_numeric_register(int win, int kbY)
{
    for (int row = 0; row < 2; row++) {
        int centerY = row == 0 ? VKB_NUMERIC_ROW1_CENTER_Y : VKB_NUMERIC_ROW2_CENTER_Y;
        for (int index = 0; index < VKB_NUMERIC_DIGIT_COUNT; index++) {
            int digit = row == 0 ? index + 1 : (index < 4 ? index + 6 : 0);
            int centerX = VKB_NUMERIC_COL_START_X + index * VKB_NUMERIC_STEP;
            int btn = win_register_button(win, centerX - 18, kbY + centerY - 15, 36, 30, -1, -1, KEY_0 + digit, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
            }
        }
    }

    int btn = win_register_button(win,
        VKB_NUMERIC_DELETE_ULX,
        kbY + VKB_NUMERIC_DELETE_ULY,
        VKB_NUMERIC_DELETE_LRX - VKB_NUMERIC_DELETE_ULX + 1,
        VKB_NUMERIC_DELETE_LRY - VKB_NUMERIC_DELETE_ULY + 1,
        -1, -1, KEY_BACKSPACE, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }
}

// =============================================================================
// Text keyboard — language pages
// =============================================================================

// CP1251 lowercase letter tables.  Zero means the cell is unused for that
// page.  Layout maps 1:1 to the 3 letter rows (rows 1-3 of the 5-row grid).

static const unsigned char vkb_en_lower[3][12] = {
    { 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', '\\' },
    { 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', 0 },
    { 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0 },
};

static const unsigned char vkb_ru_lower[3][12] = {
    { 0xE9, 0xF6, 0xF3, 0xEA, 0xE5, 0xED, 0xE3, 0xF8, 0xF9, 0xE7, 0xF5, 0xFA }, // й ц у к е н г ш щ з х ъ
    { 0xF4, 0xFB, 0xE2, 0xE0, 0xEF, 0xF0, 0xEE, 0xEB, 0xE4, 0xE6, 0xFD, 0 },     // ф ы в а п р о л д ж э
    { 0xFF, 0xF7, 0xF1, 0xEC, 0xE8, 0xF2, 0xFC, 0xE1, 0xFE, '.', 0, 0 },          // я ч с м и т ь б ю .
};

static bool vkb_text_pages[VKB_TEXT_LANGUAGE_COUNT];
static bool vkb_text_uppercase;
static int vkb_text_active_page;
static bool vkb_text_inited;

static int vkb_text_first_enabled_page()
{
    for (int i = 0; i < VKB_TEXT_LANGUAGE_COUNT; i++) {
        if (vkb_text_pages[i]) return i;
    }
    return VKB_TEXT_LANGUAGE_EN;
}

void vkb_text_init()
{
    if (vkb_text_inited) return;
    vkb_text_inited = true;

    vkb_text_pages[VKB_TEXT_LANGUAGE_EN] = true;
    vkb_text_pages[VKB_TEXT_LANGUAGE_RU] = true;

    // Parse vkb_languages (e.g. "en,ru") from config.
    if (gconfig_virtual_kb_languages != NULL && gconfig_virtual_kb_languages[0] != '\0') {
        vkb_text_pages[VKB_TEXT_LANGUAGE_EN] = false;
        vkb_text_pages[VKB_TEXT_LANGUAGE_RU] = false;

        const char* s = gconfig_virtual_kb_languages;
        while (*s != '\0') {
            // Skip non-alnum separators.
            while (*s != '\0' && !((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))) s++;
            if (*s == '\0') break;

            const char* token = s;
            while (*s != '\0' && ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))) s++;
            int len = (int)(s - token);

            if (len == 2) {
                char t[3] = { (char)(token[0] | 0x20), (char)(token[1] | 0x20), '\0' };
                if (t[0] == 'e' && t[1] == 'n') vkb_text_pages[VKB_TEXT_LANGUAGE_EN] = true;
                else if (t[0] == 'r' && t[1] == 'u') vkb_text_pages[VKB_TEXT_LANGUAGE_RU] = true;
            }
        }
    }

    vkb_text_active_page = vkb_text_first_enabled_page();
    vkb_text_uppercase = false;
}

static int vkb_text_next_page()
{
    int start = vkb_text_active_page;
    for (int i = 1; i <= VKB_TEXT_LANGUAGE_COUNT; i++) {
        int idx = (start + i) % VKB_TEXT_LANGUAGE_COUNT;
        if (vkb_text_pages[idx]) return idx;
    }
    return vkb_text_active_page;
}

static int vkb_text_serial_to_byte(int serial)
{
    if (serial == VKB_TEXT_SERIAL_Ё) {
        if (vkb_text_active_page == VKB_TEXT_LANGUAGE_RU) {
            return vkb_text_uppercase ? 0xA8 : 0xB8;
        }
        return -1;
    }

    int idx = serial - VKB_TEXT_SERIAL_FIRST_LETTER;
    if (idx < 0 || idx >= VKB_TEXT_LETTER_KEY_COUNT) return -1;
    int row = idx / VKB_TEXT_KEYBOARD_COLUMNS;
    int col = idx % VKB_TEXT_KEYBOARD_COLUMNS;
    if (row >= 3) return -1;

    const unsigned char* table = (vkb_text_active_page == VKB_TEXT_LANGUAGE_RU)
        ? vkb_ru_lower[row]
        : vkb_en_lower[row];
    int byte = table[col];
    if (byte == 0) return -1;

    if (vkb_text_uppercase) {
        if (byte >= 'a' && byte <= 'z') byte -= 'a' - 'A';
        else if (byte >= 0xE0 && byte <= 0xFF) byte -= 0x20;
        else if (byte == 0xB8) byte = 0xA8;
    }

    return byte;
}

int vkb_text_handle_key(int keyCode)
{
    if (keyCode == VKB_TEXT_KEY_LANG) {
        vkb_text_active_page = vkb_text_next_page();
        return VKB_TEXT_KEY_CONSUMED;
    }
    if (keyCode == VKB_TEXT_KEY_CASE) {
        vkb_text_uppercase = !vkb_text_uppercase;
        return VKB_TEXT_KEY_CONSUMED;
    }
    if (keyCode >= VKB_TEXT_KEY_SERIAL_BASE && keyCode < VKB_TEXT_KEY_SERIAL_BASE + VKB_TEXT_LETTER_KEY_COUNT + 1) {
        int serial = keyCode - VKB_TEXT_KEY_SERIAL_BASE;
        int byte = vkb_text_serial_to_byte(serial);
        if (byte < 0) return VKB_TEXT_KEY_INACTIVE;
        if (!text_is_glyph(byte)) return VKB_TEXT_KEY_INACTIVE;
        return byte;
    }
    return keyCode;
}

// =============================================================================
// Text keyboard — geometry helpers
// =============================================================================

static int vkb_text_key_w(int windowWidth)
{
    int totalGaps = (VKB_TEXT_KEYBOARD_COLUMNS - 1) * VKB_TEXT_KEYBOARD_GAP;
    return (windowWidth - 2 * VKB_TEXT_KEYBOARD_PAD_X - totalGaps) / VKB_TEXT_KEYBOARD_COLUMNS;
}

static void vkb_text_cell_rect(int row, int col, int span, int windowWidth,
                                int* outX, int* outY, int* outW, int* outH)
{
    int keyW = vkb_text_key_w(windowWidth);
    *outX = VKB_TEXT_KEYBOARD_PAD_X + col * (keyW + VKB_TEXT_KEYBOARD_GAP);
    *outY = VKB_TEXT_KEYBOARD_PAD_Y + row * (VKB_TEXT_KEYBOARD_KEY_HEIGHT + VKB_TEXT_KEYBOARD_ROW_GAP);
    *outW = span * keyW + (span - 1) * VKB_TEXT_KEYBOARD_GAP;
    *outH = VKB_TEXT_KEYBOARD_KEY_HEIGHT;
}

int vkb_text_kb_y(int windowHeight)
{
    return windowHeight - VKB_TEXT_KEYBOARD_HEIGHT;
}

// =============================================================================
// Text keyboard — drawing
// =============================================================================

static void vkb_text_draw_label(unsigned char* windowBuffer, int windowWidth,
                                 int cx, int cy, int cw, int ch,
                                 const char* label, int color)
{
    int labelW = text_width(label);
    int labelX = cx + (cw - labelW) / 2;
    int labelY = cy + (ch - text_height()) / 2;
    text_to_buf(windowBuffer + windowWidth * labelY + labelX, label, windowWidth, windowWidth, color);
}

void vkb_text_draw(int win, int kbY)
{
    unsigned char* windowBuffer = win_get_buf(win);
    int windowWidth = win_width(win);

    int oldFont = text_curr();
    text_font(101);

    // Background: FRM stub or plain fill.
    CacheEntry* backgroundHandle;
    int backgroundFid = art_id(OBJ_TYPE_INTERFACE, VKB_TEXT_FRM, 0, 0, 0);
    unsigned char* backgroundData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundHandle);
    if (backgroundData != NULL) {
        buf_to_buf(backgroundData, windowWidth, VKB_TEXT_KEYBOARD_HEIGHT, windowWidth, windowBuffer + kbY * windowWidth, windowWidth);
        art_ptr_unlock(backgroundHandle);
    } else {
        buf_fill(windowBuffer + kbY * windowWidth, windowWidth, VKB_TEXT_KEYBOARD_HEIGHT, windowWidth, 84);
    }

    char label[4];

    // Row 0 — digits 1..0, Ё, backspace.
    for (int col = 0; col < 12; col++) {
        int x, y, w, h;
        vkb_text_cell_rect(0, col, 1, windowWidth, &x, &y, &w, &h);

        if (col < 10) {
            int digit = (col + 1) % 10;
            label[0] = (char)('0' + digit);
            label[1] = '\0';
            vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, label, colorTable[21091]);
            continue;
        }
        if (col == 10) {
            int byte = vkb_text_serial_to_byte(VKB_TEXT_SERIAL_Ё);
            if (byte > 0 && text_is_glyph(byte)) {
                label[0] = (char)byte;
                label[1] = '\0';
                vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, label, colorTable[21091]);
            }
            continue;
        }
        // Backspace
        label[0] = '<'; label[1] = '\0';
        vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, label, colorTable[21091]);
    }

    // Rows 1..3 — letter grid.
    for (int serial = VKB_TEXT_SERIAL_FIRST_LETTER; serial < VKB_TEXT_SERIAL_FIRST_LETTER + VKB_TEXT_LETTER_KEY_COUNT; serial++) {
        int byte = vkb_text_serial_to_byte(serial);
        int idx = serial - VKB_TEXT_SERIAL_FIRST_LETTER;
        int row = idx / VKB_TEXT_KEYBOARD_COLUMNS + 1;
        int col = idx % VKB_TEXT_KEYBOARD_COLUMNS;

        int x, y, w, h;
        vkb_text_cell_rect(row, col, 1, windowWidth, &x, &y, &w, &h);

        if (byte > 0 && text_is_glyph(byte)) {
            label[0] = (char)byte;
            label[1] = '\0';
            vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, label, colorTable[21091]);
        }
    }

    // Row 4 — space (8 cols), case toggle, lang toggle, enter (2 cols).
    {
        int x, y, w, h;
        // Space: no label, just the empty filled cell.
        // Case toggle
        vkb_text_cell_rect(4, 8, 1, windowWidth, &x, &y, &w, &h);
        label[0] = (vkb_text_uppercase ? 'A' : 'a');
        label[1] = (vkb_text_uppercase ? 'B' : 'b');
        label[2] = (vkb_text_uppercase ? 'C' : 'c');
        label[3] = '\0';
        vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, label, colorTable[21091]);

        // Lang toggle
        vkb_text_cell_rect(4, 9, 1, windowWidth, &x, &y, &w, &h);
        const char* langLabel = (vkb_text_active_page == VKB_TEXT_LANGUAGE_RU) ? "RU" : "EN";
        vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, langLabel, colorTable[21091]);

        // Enter
        vkb_text_cell_rect(4, 10, 2, windowWidth, &x, &y, &w, &h);
        vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, "OK", colorTable[21091]);
    }

    text_font(oldFont);
}

// =============================================================================
// Text keyboard — button registration
// =============================================================================

void vkb_text_register(int win, int kbY)
{
    int windowWidth = win_width(win);

    vkb_text_init();

    // Row 0 — digits and special keys.
    for (int col = 0; col < 12; col++) {
        int x, y, w, h;
        vkb_text_cell_rect(0, col, 1, windowWidth, &x, &y, &w, &h);

        int keyCode;
        if (col < 10) {
            int digit = (col + 1) % 10;
            keyCode = KEY_0 + digit;
        } else if (col == 10) {
            keyCode = VKB_TEXT_KEY_SERIAL_BASE + VKB_TEXT_SERIAL_Ё;
        } else {
            keyCode = KEY_BACKSPACE;
        }

        int btn = win_register_button(win, x, kbY + y, w, h, -1, -1, keyCode, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }
    }

    // Rows 1..3 — letter grid (stable serial codes).
    for (int serial = VKB_TEXT_SERIAL_FIRST_LETTER; serial < VKB_TEXT_SERIAL_FIRST_LETTER + VKB_TEXT_LETTER_KEY_COUNT; serial++) {
        int idx = serial - VKB_TEXT_SERIAL_FIRST_LETTER;
        int row = idx / VKB_TEXT_KEYBOARD_COLUMNS + 1;
        int col = idx % VKB_TEXT_KEYBOARD_COLUMNS;

        int x, y, w, h;
        vkb_text_cell_rect(row, col, 1, windowWidth, &x, &y, &w, &h);

        int btn = win_register_button(win, x, kbY + y, w, h, -1, -1, VKB_TEXT_KEY_SERIAL_BASE + serial, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }
    }

    // Row 4 — space, case, lang, enter.
    {
        int x, y, w, h;

        // Space (8 cols wide)
        vkb_text_cell_rect(4, 0, 8, windowWidth, &x, &y, &w, &h);
        int btn = win_register_button(win, x, kbY + y, w, h, -1, -1, KEY_SPACE, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }

        // Case toggle
        vkb_text_cell_rect(4, 8, 1, windowWidth, &x, &y, &w, &h);
        btn = win_register_button(win, x, kbY + y, w, h, -1, -1, VKB_TEXT_KEY_CASE, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }

        // Lang toggle
        vkb_text_cell_rect(4, 9, 1, windowWidth, &x, &y, &w, &h);
        btn = win_register_button(win, x, kbY + y, w, h, -1, -1, VKB_TEXT_KEY_LANG, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }

        // Enter (2 cols wide)
        vkb_text_cell_rect(4, 10, 2, windowWidth, &x, &y, &w, &h);
        btn = win_register_button(win, x, kbY + y, w, h, -1, -1, KEY_RETURN, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }
    }
}

// =============================================================================
// Text keyboard — split halves (used when the host window has no room below
// it, e.g. the "talk about" box near the bottom of the dialogue screen).
// Each half is drawn in its own window sized for VKB_TEXT_SPLIT_COLUMNS
// columns.  The left half keeps the low columns, the right half the high
// ones, so the serial codes stay the same as in the full keyboard.
// =============================================================================

static int vkb_text_split_cell_w(int windowWidth)
{
    int totalGaps = (VKB_TEXT_SPLIT_COLUMNS - 1) * VKB_TEXT_KEYBOARD_GAP;
    return (windowWidth - 2 * VKB_TEXT_KEYBOARD_PAD_X - totalGaps) / VKB_TEXT_SPLIT_COLUMNS;
}

static void vkb_text_split_cell_rect(int row, int localCol, int span, int windowWidth,
                                      int* outX, int* outY, int* outW, int* outH)
{
    int cellW = vkb_text_split_cell_w(windowWidth);
    *outX = VKB_TEXT_KEYBOARD_PAD_X + localCol * (cellW + VKB_TEXT_KEYBOARD_GAP);
    *outY = VKB_TEXT_KEYBOARD_PAD_Y + row * (VKB_TEXT_KEYBOARD_KEY_HEIGHT + VKB_TEXT_KEYBOARD_ROW_GAP);
    *outW = span * cellW + (span - 1) * VKB_TEXT_KEYBOARD_GAP;
    *outH = VKB_TEXT_KEYBOARD_KEY_HEIGHT;
}

void vkb_text_split_draw(int win, int kbY, bool isLeft)
{
    unsigned char* windowBuffer = win_get_buf(win);
    int windowWidth = win_width(win);

    int oldFont = text_curr();
    text_font(101);

    buf_fill(windowBuffer + kbY * windowWidth, windowWidth, VKB_TEXT_KEYBOARD_HEIGHT, windowWidth, 84);

    char label[4];

    // Row 0 — digits, Ё, backspace (only the cell(s) owned by this half).
    for (int localCol = 0; localCol < VKB_TEXT_SPLIT_COLUMNS; localCol++) {
        int globalCol = isLeft ? localCol : VKB_TEXT_SPLIT_COLUMNS + localCol;
        int x, y, w, h;
        vkb_text_split_cell_rect(0, localCol, 1, windowWidth, &x, &y, &w, &h);

        if (globalCol < 10) {
            int digit = (globalCol + 1) % 10;
            label[0] = (char)('0' + digit);
            label[1] = '\0';
            vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, label, colorTable[21091]);
            continue;
        }
        if (globalCol == 10) {
            int byte = vkb_text_serial_to_byte(VKB_TEXT_SERIAL_Ё);
            if (byte > 0 && text_is_glyph(byte)) {
                label[0] = (char)byte;
                label[1] = '\0';
                vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, label, colorTable[21091]);
            }
            continue;
        }
        label[0] = '<';
        label[1] = '\0';
        vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, label, colorTable[21091]);
    }

    // Rows 1..3 — letter grid (stable serial codes, half of the columns).
    for (int serial = VKB_TEXT_SERIAL_FIRST_LETTER; serial < VKB_TEXT_SERIAL_FIRST_LETTER + VKB_TEXT_LETTER_KEY_COUNT; serial++) {
        int idx = serial - VKB_TEXT_SERIAL_FIRST_LETTER;
        int row = idx / VKB_TEXT_KEYBOARD_COLUMNS + 1;
        int globalCol = idx % VKB_TEXT_KEYBOARD_COLUMNS;
        if ((isLeft && globalCol >= VKB_TEXT_SPLIT_COLUMNS) || (!isLeft && globalCol < VKB_TEXT_SPLIT_COLUMNS)) {
            continue;
        }

        int byte = vkb_text_serial_to_byte(serial);
        int localCol = globalCol - (isLeft ? 0 : VKB_TEXT_SPLIT_COLUMNS);
        int x, y, w, h;
        vkb_text_split_cell_rect(row, localCol, 1, windowWidth, &x, &y, &w, &h);

        if (byte > 0 && text_is_glyph(byte)) {
            label[0] = (char)byte;
            label[1] = '\0';
            vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, label, colorTable[21091]);
        }
    }

    // Row 4 — left: space; right: case, lang, enter.
    {
        int x, y, w, h;
        if (isLeft) {
            vkb_text_split_cell_rect(4, 0, VKB_TEXT_SPLIT_COLUMNS, windowWidth, &x, &y, &w, &h);
            vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, "Space", colorTable[21091]);
        } else {
            vkb_text_split_cell_rect(4, 2, 1, windowWidth, &x, &y, &w, &h);
            label[0] = (vkb_text_uppercase ? 'A' : 'a');
            label[1] = (vkb_text_uppercase ? 'B' : 'b');
            label[2] = (vkb_text_uppercase ? 'C' : 'c');
            label[3] = '\0';
            vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, label, colorTable[21091]);

            vkb_text_split_cell_rect(4, 3, 1, windowWidth, &x, &y, &w, &h);
            const char* langLabel = (vkb_text_active_page == VKB_TEXT_LANGUAGE_RU) ? "RU" : "EN";
            vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, langLabel, colorTable[21091]);

            vkb_text_split_cell_rect(4, 4, 2, windowWidth, &x, &y, &w, &h);
            vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, "OK", colorTable[21091]);
        }
    }

    text_font(oldFont);
}

void vkb_text_split_register(int win, int kbY, bool isLeft)
{
    int windowWidth = win_width(win);

    vkb_text_init();

    // Row 0 — digits and special keys.
    for (int localCol = 0; localCol < VKB_TEXT_SPLIT_COLUMNS; localCol++) {
        int globalCol = isLeft ? localCol : VKB_TEXT_SPLIT_COLUMNS + localCol;
        int x, y, w, h;
        vkb_text_split_cell_rect(0, localCol, 1, windowWidth, &x, &y, &w, &h);

        int keyCode;
        if (globalCol < 10) {
            int digit = (globalCol + 1) % 10;
            keyCode = KEY_0 + digit;
        } else if (globalCol == 10) {
            keyCode = VKB_TEXT_KEY_SERIAL_BASE + VKB_TEXT_SERIAL_Ё;
        } else {
            keyCode = KEY_BACKSPACE;
        }

        int btn = win_register_button(win, x, kbY + y, w, h, -1, -1, keyCode, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }
    }

    // Rows 1..3 — letter grid (stable serial codes).
    for (int serial = VKB_TEXT_SERIAL_FIRST_LETTER; serial < VKB_TEXT_SERIAL_FIRST_LETTER + VKB_TEXT_LETTER_KEY_COUNT; serial++) {
        int idx = serial - VKB_TEXT_SERIAL_FIRST_LETTER;
        int row = idx / VKB_TEXT_KEYBOARD_COLUMNS + 1;
        int globalCol = idx % VKB_TEXT_KEYBOARD_COLUMNS;
        if ((isLeft && globalCol >= VKB_TEXT_SPLIT_COLUMNS) || (!isLeft && globalCol < VKB_TEXT_SPLIT_COLUMNS)) {
            continue;
        }

        int localCol = globalCol - (isLeft ? 0 : VKB_TEXT_SPLIT_COLUMNS);
        int x, y, w, h;
        vkb_text_split_cell_rect(row, localCol, 1, windowWidth, &x, &y, &w, &h);

        int btn = win_register_button(win, x, kbY + y, w, h, -1, -1, VKB_TEXT_KEY_SERIAL_BASE + serial, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }
    }

    // Row 4 — left half: space; right half: case, lang, enter.
    {
        int x, y, w, h;
        if (isLeft) {
            vkb_text_split_cell_rect(4, 0, VKB_TEXT_SPLIT_COLUMNS, windowWidth, &x, &y, &w, &h);
            int btn = win_register_button(win, x, kbY + y, w, h, -1, -1, KEY_SPACE, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
            }
        } else {
            vkb_text_split_cell_rect(4, 2, 1, windowWidth, &x, &y, &w, &h);
            int btn = win_register_button(win, x, kbY + y, w, h, -1, -1, VKB_TEXT_KEY_CASE, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
            }

            vkb_text_split_cell_rect(4, 3, 1, windowWidth, &x, &y, &w, &h);
            btn = win_register_button(win, x, kbY + y, w, h, -1, -1, VKB_TEXT_KEY_LANG, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
            }

            vkb_text_split_cell_rect(4, 4, 2, windowWidth, &x, &y, &w, &h);
            btn = win_register_button(win, x, kbY + y, w, h, -1, -1, KEY_RETURN, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
            }
        }
    }
}

} // namespace fallout

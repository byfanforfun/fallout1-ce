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

    // Temporary background fill until a proper FRM is made (same look as the
    // split text keyboard).
    buf_fill(windowBuffer + kbY * windowWidth, windowWidth, VKB_NUMERIC_FRM_HEIGHT, windowWidth, 84);

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
// Text keyboard — navigation (arrows with wrap-around)
// =============================================================================

static bool vkb_text_split_layout = false;
static int vkb_text_focus_row = 4;
static int vkb_text_focus_col = 10;
static bool vkb_text_nav_active = false;

// Row 4 is not a plain 12-column grid: it holds four grouped keys separated
// by gaps (space, case, lang, OK).  Navigation on that row moves between the
// groups, so every group is always reachable.
enum {
    VKB_TEXT_GROUP_COUNT = 4,
    VKB_TEXT_GROUP_SPACE = 0,
    VKB_TEXT_GROUP_CASE = 1,
    VKB_TEXT_GROUP_LANG = 2,
    VKB_TEXT_GROUP_OK = 3,
};

static int vkb_text_focus_group = VKB_TEXT_GROUP_OK;

void vkb_text_set_split_layout(bool splitLayout)
{
    vkb_text_split_layout = splitLayout;
}

void vkb_text_nav_reset()
{
    vkb_text_focus_row = 4;
    vkb_text_focus_group = VKB_TEXT_GROUP_OK;
    vkb_text_nav_active = true;
}

void vkb_text_nav_disable()
{
    vkb_text_nav_active = false;
}

// Key code produced by the given grid cell, or -1 when the cell is dead on the
// active page.  Only the letter/digit rows (0..3) are a plain grid; row 4 is
// handled by the group helpers below.
static int vkb_text_cell_key(int row, int col)
{
    if (row == 0) {
        if (col < 10) {
            int digit = (col + 1) % 10;
            return KEY_0 + digit;
        }
        if (col == 10) {
            int byte = vkb_text_serial_to_byte(VKB_TEXT_SERIAL_Ё);
            return (byte > 0 && text_is_glyph(byte)) ? byte : -1;
        }
        return KEY_BACKSPACE;
    }

    if (row < 4) {
        int serial = VKB_TEXT_SERIAL_FIRST_LETTER + (row - 1) * VKB_TEXT_KEYBOARD_COLUMNS + col;
        int byte = vkb_text_serial_to_byte(serial);
        return (byte > 0 && text_is_glyph(byte)) ? byte : -1;
    }

    return -1;
}

// Grid column that the group is reached from / lands on when moving between
// row 4 and the letter rows.
static int vkb_text_group_to_col(int group)
{
    switch (group) {
    case VKB_TEXT_GROUP_CASE:
        return 8;
    case VKB_TEXT_GROUP_LANG:
        return 9;
    case VKB_TEXT_GROUP_OK:
        return 10;
    default:
        return 4;
    }
}

// Group that a grid column maps to when crossing from the letter rows down
// into row 4.
static int vkb_text_group_from_col(int col)
{
    if (col < 8) return VKB_TEXT_GROUP_SPACE;
    if (col == 8) return VKB_TEXT_GROUP_CASE;
    if (col == 9) return VKB_TEXT_GROUP_LANG;
    return VKB_TEXT_GROUP_OK;
}

static int vkb_text_group_key(int group)
{
    switch (group) {
    case VKB_TEXT_GROUP_CASE:
        return VKB_TEXT_KEY_CASE;
    case VKB_TEXT_GROUP_LANG:
        return VKB_TEXT_KEY_LANG;
    case VKB_TEXT_GROUP_OK:
        return KEY_RETURN;
    default:
        return KEY_SPACE;
    }
}

void vkb_text_nav_anchor()
{
    if (vkb_text_focus_row == 4) {
        // All four groups are always reachable keys.
        return;
    }

    if (vkb_text_cell_key(vkb_text_focus_row, vkb_text_focus_col) >= 0) {
        return;
    }

    for (int row = 0; row < VKB_TEXT_KEYBOARD_ROWS; row++) {
        if (vkb_text_cell_key(row, vkb_text_focus_col) >= 0) {
            vkb_text_focus_row = row;
            vkb_text_focus_col = 0;
            return;
        }
    }

    for (int row = 0; row < VKB_TEXT_KEYBOARD_ROWS; row++) {
        for (int col = 0; col < VKB_TEXT_KEYBOARD_COLUMNS; col++) {
            if (vkb_text_cell_key(row, col) >= 0) {
                vkb_text_focus_row = row;
                vkb_text_focus_col = col;
                return;
            }
        }
    }
}

static void vkb_text_focus_move_row(int dir)
{
    for (int i = 1; i <= VKB_TEXT_KEYBOARD_ROWS; i++) {
        int row = (vkb_text_focus_row + dir * i) % VKB_TEXT_KEYBOARD_ROWS;
        if (row < 0) {
            row += VKB_TEXT_KEYBOARD_ROWS;
        }
        if (row == 4) {
            vkb_text_focus_row = 4;
            vkb_text_focus_group = vkb_text_group_from_col(vkb_text_focus_col);
            return;
        }
        if (vkb_text_cell_key(row, vkb_text_focus_col) >= 0) {
            vkb_text_focus_row = row;
            return;
        }
    }
}

static void vkb_text_focus_move_col(int dir)
{
    for (int i = 1; i <= VKB_TEXT_KEYBOARD_COLUMNS; i++) {
        int col = (vkb_text_focus_col + dir * i) % VKB_TEXT_KEYBOARD_COLUMNS;
        if (col < 0) {
            col += VKB_TEXT_KEYBOARD_COLUMNS;
        }
        if (vkb_text_cell_key(vkb_text_focus_row, col) >= 0) {
            vkb_text_focus_col = col;
            return;
        }
    }
}

void vkb_text_navigate(int keyCode)
{
    if (!vkb_text_nav_active) {
        return;
    }

    if (vkb_text_focus_row == 4) {
        switch (keyCode) {
        case KEY_LEFT:
            vkb_text_focus_group = (vkb_text_focus_group + VKB_TEXT_GROUP_COUNT - 1) % VKB_TEXT_GROUP_COUNT;
            break;
        case KEY_RIGHT:
            vkb_text_focus_group = (vkb_text_focus_group + 1) % VKB_TEXT_GROUP_COUNT;
            break;
        case KEY_UP:
        case KEY_DOWN: {
            int col = vkb_text_group_to_col(vkb_text_focus_group);
            int dir = (keyCode == KEY_UP) ? -1 : 1;
            for (int i = 1; i <= VKB_TEXT_KEYBOARD_ROWS; i++) {
                int row = (4 + dir * i + VKB_TEXT_KEYBOARD_ROWS) % VKB_TEXT_KEYBOARD_ROWS;
                if (vkb_text_cell_key(row, col) >= 0) {
                    vkb_text_focus_row = row;
                    vkb_text_focus_col = col;
                    return;
                }
            }
            break;
        }
        }
        return;
    }

    switch (keyCode) {
    case KEY_LEFT:
        vkb_text_focus_move_col(-1);
        break;
    case KEY_RIGHT:
        vkb_text_focus_move_col(1);
        break;
    case KEY_UP:
        vkb_text_focus_move_row(-1);
        break;
    case KEY_DOWN:
        vkb_text_focus_move_row(1);
        break;
    }
}

int vkb_text_focus_key()
{
    if (!vkb_text_nav_active) {
        return KEY_RETURN;
    }

    if (vkb_text_focus_row == 4) {
        return vkb_text_group_key(vkb_text_focus_group);
    }

    int key = vkb_text_cell_key(vkb_text_focus_row, vkb_text_focus_col);
    if (key < 0) {
        return KEY_RETURN;
    }
    return key;
}

static void vkb_text_cell_rect(int row, int col, int span, int windowWidth,
    int* outX, int* outY, int* outW, int* outH);
static void vkb_text_split_cell_rect(int row, int localCol, int span, int windowWidth,
    int* outX, int* outY, int* outW, int* outH);
static void vkb_text_row4_rect(int group, int windowWidth, bool isSplit, bool isLeft,
    int* outX, int* outY, int* outW, int* outH);

// Draw a box around the active navigation cell (full or split keyboard).
static void vkb_text_draw_focus(unsigned char* windowBuffer, int windowWidth,
    int kbY, bool isSplit, bool isLeft)
{
    if (!vkb_text_nav_active) {
        return;
    }

    int x, y, w, h;
    if (vkb_text_focus_row == 4) {
        if (isSplit && ((isLeft && vkb_text_focus_group != VKB_TEXT_GROUP_SPACE)
            || (!isLeft && vkb_text_focus_group == VKB_TEXT_GROUP_SPACE))) {
            return;
        }
        vkb_text_row4_rect(vkb_text_focus_group, windowWidth, isSplit, isLeft, &x, &y, &w, &h);
        if (w <= 0) {
            return;
        }
    } else {
        if (isSplit) {
            if ((isLeft && vkb_text_focus_col >= VKB_TEXT_SPLIT_COLUMNS)
                || (!isLeft && vkb_text_focus_col < VKB_TEXT_SPLIT_COLUMNS)) {
                return;
            }
            vkb_text_split_cell_rect(vkb_text_focus_row, vkb_text_focus_col - (isLeft ? 0 : VKB_TEXT_SPLIT_COLUMNS), 1, windowWidth, &x, &y, &w, &h);
        } else {
            vkb_text_cell_rect(vkb_text_focus_row, vkb_text_focus_col, 1, windowWidth, &x, &y, &w, &h);
        }
    }

    draw_shaded_box(windowBuffer, windowWidth, x - 1, y + kbY - 1, x + w, y + kbY + h, colorTable[21091], colorTable[21091]);
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

// Rectangle of one row-4 group key (space / case / lang / OK).  The keys are
// sized on a unit column so the whole row fits the window width exactly and
// each key is visually separated by a full gap:
//   full : space(8u) gap case(1u) gap lang(1u) gap OK(2u)      = 12u + 3 gaps
//   left : space fills the whole half
//   right: case(1u) gap lang(1u) gap OK(2u)                    = 4u + 2 gaps
static void vkb_text_row4_rect(int group, int windowWidth, bool isSplit, bool isLeft,
                                int* outX, int* outY, int* outW, int* outH)
{
    int innerWidth = windowWidth - 2 * VKB_TEXT_KEYBOARD_PAD_X;
    int gap = VKB_TEXT_KEYBOARD_GAP;

    *outY = VKB_TEXT_KEYBOARD_PAD_Y + 4 * (VKB_TEXT_KEYBOARD_KEY_HEIGHT + VKB_TEXT_KEYBOARD_ROW_GAP);
    *outH = VKB_TEXT_KEYBOARD_KEY_HEIGHT;

    if (isSplit && !isLeft) {
        int unitKeyW = (innerWidth - 2 * gap) / 4;
        int x = VKB_TEXT_KEYBOARD_PAD_X;
        if (group == VKB_TEXT_GROUP_CASE) {
            *outX = x;
            *outW = unitKeyW;
        } else if (group == VKB_TEXT_GROUP_LANG) {
            *outX = x + unitKeyW + gap;
            *outW = unitKeyW;
        } else {
            *outX = x + 2 * (unitKeyW + gap);
            *outW = 2 * unitKeyW;
        }
    } else if (isSplit && isLeft) {
        *outX = VKB_TEXT_KEYBOARD_PAD_X;
        *outW = (group == VKB_TEXT_GROUP_SPACE) ? innerWidth : 0;
    } else {
        int unitKeyW = (innerWidth - 3 * gap) / 12;
        int x = VKB_TEXT_KEYBOARD_PAD_X;
        if (group == VKB_TEXT_GROUP_SPACE) {
            *outX = x;
            *outW = 8 * unitKeyW;
        } else if (group == VKB_TEXT_GROUP_CASE) {
            *outX = x + 8 * unitKeyW + gap;
            *outW = unitKeyW;
        } else if (group == VKB_TEXT_GROUP_LANG) {
            *outX = x + 9 * unitKeyW + 2 * gap;
            *outW = unitKeyW;
        } else {
            *outX = x + 10 * unitKeyW + 3 * gap;
            *outW = 2 * unitKeyW;
        }
    }
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

    // Temporary background fill until a proper FRM is made (same look as the
    // split text keyboard).
    buf_fill(windowBuffer + kbY * windowWidth, windowWidth, VKB_TEXT_KEYBOARD_HEIGHT, windowWidth, 84);

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

    // Row 4 — space, case, lang, OK as four grouped keys with gaps.
    {
        int x, y, w, h;

        // Space
        vkb_text_row4_rect(VKB_TEXT_GROUP_SPACE, windowWidth, false, false, &x, &y, &w, &h);
        vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, "Space", colorTable[21091]);

        // Case toggle
        vkb_text_row4_rect(VKB_TEXT_GROUP_CASE, windowWidth, false, false, &x, &y, &w, &h);
        label[0] = (vkb_text_uppercase ? 'A' : 'a');
        label[1] = (vkb_text_uppercase ? 'B' : 'b');
        label[2] = (vkb_text_uppercase ? 'C' : 'c');
        label[3] = '\0';
        vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, label, colorTable[21091]);

        // Lang toggle
        vkb_text_row4_rect(VKB_TEXT_GROUP_LANG, windowWidth, false, false, &x, &y, &w, &h);
        const char* langLabel = (vkb_text_active_page == VKB_TEXT_LANGUAGE_RU) ? "RU" : "EN";
        vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, langLabel, colorTable[21091]);

        // OK
        vkb_text_row4_rect(VKB_TEXT_GROUP_OK, windowWidth, false, false, &x, &y, &w, &h);
        vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, "OK", colorTable[21091]);
    }

    vkb_text_draw_focus(windowBuffer, windowWidth, kbY, false, false);

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

    // Row 4 — space, case, lang, OK as four grouped keys with gaps.
    {
        int x, y, w, h;

        // Space
        vkb_text_row4_rect(VKB_TEXT_GROUP_SPACE, windowWidth, false, false, &x, &y, &w, &h);
        int btn = win_register_button(win, x, kbY + y, w, h, -1, -1, KEY_SPACE, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }

        // Case toggle
        vkb_text_row4_rect(VKB_TEXT_GROUP_CASE, windowWidth, false, false, &x, &y, &w, &h);
        btn = win_register_button(win, x, kbY + y, w, h, -1, -1, VKB_TEXT_KEY_CASE, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }

        // Lang toggle
        vkb_text_row4_rect(VKB_TEXT_GROUP_LANG, windowWidth, false, false, &x, &y, &w, &h);
        btn = win_register_button(win, x, kbY + y, w, h, -1, -1, VKB_TEXT_KEY_LANG, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }

        // OK
        vkb_text_row4_rect(VKB_TEXT_GROUP_OK, windowWidth, false, false, &x, &y, &w, &h);
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

    // Row 4 — left: space; right: case, lang, OK as grouped keys with gaps.
    {
        int x, y, w, h;
        if (isLeft) {
            vkb_text_row4_rect(VKB_TEXT_GROUP_SPACE, windowWidth, true, true, &x, &y, &w, &h);
            vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, "Space", colorTable[21091]);
        } else {
            vkb_text_row4_rect(VKB_TEXT_GROUP_CASE, windowWidth, true, false, &x, &y, &w, &h);
            label[0] = (vkb_text_uppercase ? 'A' : 'a');
            label[1] = (vkb_text_uppercase ? 'B' : 'b');
            label[2] = (vkb_text_uppercase ? 'C' : 'c');
            label[3] = '\0';
            vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, label, colorTable[21091]);

            vkb_text_row4_rect(VKB_TEXT_GROUP_LANG, windowWidth, true, false, &x, &y, &w, &h);
            const char* langLabel = (vkb_text_active_page == VKB_TEXT_LANGUAGE_RU) ? "RU" : "EN";
            vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, langLabel, colorTable[21091]);

            vkb_text_row4_rect(VKB_TEXT_GROUP_OK, windowWidth, true, false, &x, &y, &w, &h);
            vkb_text_draw_label(windowBuffer, windowWidth, x, y + kbY, w, h, "OK", colorTable[21091]);
        }
    }

    vkb_text_draw_focus(windowBuffer, windowWidth, kbY, true, isLeft);

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

    // Row 4 — left half: space; right half: case, lang, OK as grouped keys.
    {
        int x, y, w, h;
        if (isLeft) {
            vkb_text_row4_rect(VKB_TEXT_GROUP_SPACE, windowWidth, true, true, &x, &y, &w, &h);
            int btn = win_register_button(win, x, kbY + y, w, h, -1, -1, KEY_SPACE, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
            }
        } else {
            vkb_text_row4_rect(VKB_TEXT_GROUP_CASE, windowWidth, true, false, &x, &y, &w, &h);
            int btn = win_register_button(win, x, kbY + y, w, h, -1, -1, VKB_TEXT_KEY_CASE, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
            }

            vkb_text_row4_rect(VKB_TEXT_GROUP_LANG, windowWidth, true, false, &x, &y, &w, &h);
            btn = win_register_button(win, x, kbY + y, w, h, -1, -1, VKB_TEXT_KEY_LANG, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
            }

            vkb_text_row4_rect(VKB_TEXT_GROUP_OK, windowWidth, true, false, &x, &y, &w, &h);
            btn = win_register_button(win, x, kbY + y, w, h, -1, -1, KEY_RETURN, -1, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
            }
        }
    }
}

} // namespace fallout

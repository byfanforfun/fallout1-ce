#ifndef FALLOUT_GAME_VKB_H_
#define FALLOUT_GAME_VKB_H_

#include <stdbool.h>

namespace fallout {

// -- Numeric keyboard (move-items / set-timer) --------------------------------
// Drawn inside a host window at the very bottom.  The background is a plain
// temporary fill (see vkb_numeric_draw); these FRM ids are reserved for a real
// background later.  Digit glyphs are rendered with the BIGNUM.frm art.

#define VKB_NUMERIC_FRM 309
#define VKB_NUMERIC_FRM_WIDTH 259
#define VKB_NUMERIC_FRM_HEIGHT 94
#define VKB_NUMERIC_COL_START_X 30
#define VKB_NUMERIC_STEP 40
#define VKB_NUMERIC_ROW1_CENTER_Y 36
#define VKB_NUMERIC_ROW2_CENTER_Y 70
#define VKB_NUMERIC_DELETE_ULX 215
#define VKB_NUMERIC_DELETE_ULY 22
#define VKB_NUMERIC_DELETE_LRX 236
#define VKB_NUMERIC_DELETE_LRY 85
#define VKB_NUMERIC_DIGIT_COUNT 5
#define VKB_NUMERIC_DIGIT_GLYPH_WIDTH 14
#define VKB_NUMERIC_DIGIT_GLYPH_HEIGHT 24
#define VKB_NUMERIC_BIGNUM_FRM 170

// Draw the numeric keypad into the host window buffer starting at row kbY.
void vkb_numeric_draw(int win, int kbY);

// Register the invisible numeric keypad buttons on the host window.
// kbY is the top row of the keyboard area inside the window.
void vkb_numeric_register(int win, int kbY);

// -- Text keyboard (QWERTY / ЙЦУКЕН with number row) -------------------------
//
// Embedded into the bottom of any text-input window when show_virt_kb is on.
// 5 rows × 12 columns:
//   row 0 : 1 2 3 4 5 6 7 8 9 0 Ё ⌫
//   row 1 : q w e r t y u i o p [ \   (EN) / й ц у к е н г ш щ з х ъ (RU)
//   row 2 : a s d f g h j k l ; ' .   (EN) / ф ы в а п р о л д ж э   (RU)
//   row 3 : z x c v b n m , . /       (EN) / я ч с м и т ь б ю .    (RU)
//   row 4 : [ SPACE 8 ] [ abc ] [ lang ] [ OK 2 ]
//
// Toggle actions (LANG / CASE) use dedicated serial event codes and redraw
// the keyboard in place.

// Reserved FRM id for a future keyboard background; the current background is
// a plain temporary fill (see vkb_text_draw).
#define VKB_TEXT_FRM 311
#define VKB_TEXT_KEYBOARD_COLUMNS 12
#define VKB_TEXT_KEYBOARD_ROWS 5
#define VKB_TEXT_KEYBOARD_PAD_X 8
#define VKB_TEXT_KEYBOARD_PAD_Y 6
#define VKB_TEXT_KEYBOARD_GAP 3
#define VKB_TEXT_KEYBOARD_KEY_HEIGHT 30
#define VKB_TEXT_KEYBOARD_ROW_GAP 4

// Columns per half when the keyboard is split left/right of a window that
// has no room below it (e.g. the "talk about" box at the bottom of the
// dialogue screen).
#define VKB_TEXT_SPLIT_COLUMNS 6

// Total height of the text keyboard area (pixels).
#define VKB_TEXT_KEYBOARD_HEIGHT \
    (VKB_TEXT_KEYBOARD_PAD_Y * 2                             \
     + VKB_TEXT_KEYBOARD_ROWS * VKB_TEXT_KEYBOARD_KEY_HEIGHT \
     + (VKB_TEXT_KEYBOARD_ROWS - 1) * VKB_TEXT_KEYBOARD_ROW_GAP)

// Stable serial codes for the letter grid (0..37) and for the toggle keys.
// These are NOT real key codes — they are translated to the active byte by
// vkb_text_handle_key before reaching the caller.
#define VKB_TEXT_SERIAL_Ё 0
#define VKB_TEXT_SERIAL_FIRST_LETTER 1
#define VKB_TEXT_LETTER_KEY_COUNT 37
#define VKB_TEXT_KEY_SERIAL_BASE 0x4000
#define VKB_TEXT_KEY_CASE 0x7C00
#define VKB_TEXT_KEY_LANG 0x7C01

// Returned by vkb_text_handle_key when the code was consumed by the
// keyboard (toggle / redraw) and the caller should continue the loop.
#define VKB_TEXT_KEY_CONSUMED (-1)

// Returned when the serial maps to an inactive key on the current page.
#define VKB_TEXT_KEY_INACTIVE (-2)

// Language identifiers.
#define VKB_TEXT_LANGUAGE_EN 0
#define VKB_TEXT_LANGUAGE_RU 1
#define VKB_TEXT_LANGUAGE_COUNT 2

// Parse the vkb_languages config (once) and set up the enabled[] array.
void vkb_text_init();

// Draw the text keyboard into the host window buffer starting at row kbY.
void vkb_text_draw(int win, int kbY);

// Register the invisible text keyboard buttons on the host window.
void vkb_text_register(int win, int kbY);

// Draw half a text keyboard into a window sized for
// VKB_TEXT_SPLIT_COLUMNS columns.  isLeft selects the left half
// (columns 0..5) or the right half (columns 6..11).  kbY is the top row of
// the keyboard area inside the window buffer.
void vkb_text_split_draw(int win, int kbY, bool isLeft);

// Register the invisible buttons for half a text keyboard.
void vkb_text_split_register(int win, int kbY, bool isLeft);

// Translate a raw get_input() code. Returns:
//   >= 0  — character byte to feed to the text-input field (or a real code
//           such as KEY_RETURN / KEY_BACKSPACE that the caller already
//           handles).
//   VKB_TEXT_KEY_CONSUMED  — toggle action was applied; caller should
//                            redraw the kb area and continue the loop.
//   VKB_TEXT_KEY_INACTIVE  — dead cell on the active page; skip.
int vkb_text_handle_key(int keyCode);

// Returns the top row (inside the window buffer) where the text keyboard
// sits for a given host-window height.
int vkb_text_kb_y(int windowHeight);

} // namespace fallout

#endif /* FALLOUT_GAME_VKB_H_ */

#include "game/hud.h"

#include <stdlib.h>
#include <string.h>

#include "game/art.h"
#include "game/gamconf.h"
#include "game/map.h"
#include "plib/gnw/button.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/gnw_types.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/input_rebind.h"
#include "plib/gnw/mouse.h"
#include "plib/gnw/svga.h"

namespace fallout {

// The `[hud]` overlay consists of two transparent GNW windows: the scroll
// D-pad (8 arrows) on the left edge and the action buttons column on the right
// edge. Both windows only cover their own buttons, so that touches aimed at
// the game world fall through to the display window.

#define HUD_SCROLL_GRID 3
#define HUD_SCROLL_BUTTON_COUNT 8

// Map-scroll directions, one entry per scroll button.
typedef struct ScrollDir {
    int dx;
    int dy;
} ScrollDir;

static int hudScrollWindow = -1;
static int hudActionsWindow = -1;

static int hudScrollButtons[HUD_SCROLL_BUTTON_COUNT];
static const ScrollDir hudScrollDirs[HUD_SCROLL_BUTTON_COUNT] = {
    { -1, -1 }, // NW
    { 0, -1 }, // N
    { 1, -1 }, // NE
    { -1, 0 }, // W
    { 1, 0 }, // E
    { -1, 1 }, // SW
    { 0, 1 }, // S
    { 1, 1 }, // SE
};

// Interface frame numbers for the 3x3 grid. The center cell is empty; the
// entries are NW, N, NE, W, (empty), E, SW, S, SE in scan order. Values map
// to `intrface.list` rows minus one.
static const int hudScrollFrameNums[9] = {
    270,
    271,
    272,
    277,
    0,
    273,
    276,
    275,
    274,
};

static int hudActionButtons[GAM_CONFIG_ACTION_SLOTS];
static unsigned char* hudActionUp[GAM_CONFIG_ACTION_SLOTS];
static unsigned char* hudActionDown[GAM_CONFIG_ACTION_SLOTS];

static void hud_scroll_on_down(int btnId, int keyCode)
{
    for (int index = 0; index < HUD_SCROLL_BUTTON_COUNT; index++) {
        if (hudScrollButtons[index] == btnId) {
            map_scroll(hudScrollDirs[index].dx, hudScrollDirs[index].dy);
            break;
        }
    }
}

// Builds a (size x size) transparent button image from the scaled interface
// frame `frameNum`. The frame is scaled to fit, keeping proportions, and
// centered. The caller owns the returned buffer, or receives NULL on failure.
static unsigned char* hud_make_button_image(int frameNum, int size)
{
    CacheEntry* cacheEntry = NULL;
    int frameWidth;
    int frameHeight;
    unsigned char* frameData = art_lock(art_id(OBJ_TYPE_INTERFACE, frameNum, 0, 0, 0), &cacheEntry, &frameWidth, &frameHeight);
    if (frameData == NULL || cacheEntry == NULL) {
        return NULL;
    }

    int scaledWidth = frameWidth;
    int scaledHeight = frameHeight;
    if (frameWidth > 0 && frameHeight > 0) {
        int maxDimension = frameWidth > frameHeight ? frameWidth : frameHeight;
        double scale = (double)size / maxDimension;
        scaledWidth = (int)(frameWidth * scale);
        scaledHeight = (int)(frameHeight * scale);
        if (scaledWidth <= 0 || scaledHeight <= 0) {
            scaledWidth = frameWidth;
            scaledHeight = frameHeight;
        }
    }

    unsigned char* upBuf = (unsigned char*)calloc(1, size * size);
    if (upBuf == NULL) {
        art_ptr_unlock(cacheEntry);
        return NULL;
    }

    unsigned char* scaled = (unsigned char*)malloc(scaledWidth * scaledHeight);
    if (scaled != NULL) {
        trans_cscale(frameData, frameWidth, frameHeight, frameWidth, scaled, scaledWidth, scaledHeight, scaledWidth);

        int offsetX = (size - scaledWidth) / 2;
        int offsetY = (size - scaledHeight) / 2;
        trans_buf_to_buf(scaled, scaledWidth, scaledHeight, scaledWidth, upBuf + offsetY * size + offsetX, size);

        free(scaled);
    }

    art_ptr_unlock(cacheEntry);

    return upBuf;
}

// Resolves the press/release frame pair for an action slot key.
static void hud_resolve_action_frames(int slotKey, int* frameUpPtr, int* frameDownPtr)
{
    int frameUp;
    int frameDown;

    switch (slotKey) {
    case '\t':
        // Automap.
        frameUp = 13;
        frameDown = 10;
        break;
    case 'a':
        // Attack.
        frameUp = 32;
        frameDown = 31;
        break;
    case 'i':
        // Inventory.
        frameUp = 47;
        frameDown = 46;
        break;
    case 'c':
        // Character.
        frameUp = 57;
        frameDown = 56;
        break;
    case 'p':
        // Pipboy.
        frameUp = 59;
        frameDown = 58;
        break;
    default:
        // Big red button fallback.
        frameUp = 6;
        frameDown = 7;
        break;
    }

    *frameUpPtr = frameUp;
    *frameDownPtr = frameDown;
}

// Applies per-pixel transparency to a button buffer: all nonzero pixels get
// their value scaled so that the whole image fades toward the transparent
// color. `opacityPercent` is 0 (fully transparent)..100 (unchanged).
static void hud_apply_opacity(unsigned char* buf, int size, int opacityPercent)
{
    if (opacityPercent >= 100 || buf == NULL) {
        return;
    }
    if (opacityPercent <= 0) {
        memset(buf, 0, size * size);
        return;
    }

    for (int index = 0; index < size * size; index++) {
        unsigned int value = buf[index];
        if (value != 0) {
            buf[index] = (unsigned char)((value * opacityPercent) / 100);
        }
    }
}

int hud_init()
{
    if (hudScrollWindow != -1 || hudActionsWindow != -1) {
        return -1;
    }

    // Skip both windows entirely when the HUD is disabled.
    if (gconfig_hud_type == 0) {
        return 0;
    }

    double scale = gconfig_hud_scale;
    if (scale <= 0) {
        scale = 1.0;
    }
    int opacity = gconfig_hud_opacity;

    int size = (int)(gconfig_scroll_size * scale);
    if (size <= 0) {
        size = 64;
    }

    if (gconfig_scroll_enabled) {
        int grid = HUD_SCROLL_GRID * size;

        int offsetX = gconfig_scroll_offset_x;
        if (offsetX < 0) {
            offsetX = screenGetWidth() + offsetX - grid;
        }
        int offsetY = gconfig_scroll_offset_y;
        if (offsetY < 0) {
            offsetY = screenGetHeight() + offsetY - grid;
        }

        hudScrollWindow = win_add(offsetX, offsetY, grid, grid, 0, WINDOW_TRANSPARENT | WINDOW_HIDDEN);
        if (hudScrollWindow == -1) {
            return -1;
        }

        int buttonIndex = 0;
        for (int y = 0; y < HUD_SCROLL_GRID; y++) {
            for (int x = 0; x < HUD_SCROLL_GRID; x++) {
                int frameNum = hudScrollFrameNums[y * HUD_SCROLL_GRID + x];
                if (frameNum == 0) {
                    continue;
                }

                unsigned char* up = hud_make_button_image(frameNum, size);
                unsigned char* down = hud_make_button_image(frameNum, size);
                if (up == NULL || down == NULL) {
                    if (up != NULL) {
                        free(up);
                    }
                    if (down != NULL) {
                        free(down);
                    }
                    continue;
                }

                hud_apply_opacity(up, size, opacity);
                hud_apply_opacity(down, size, opacity);

                int btnId = win_register_button(hudScrollWindow,
                    x * size,
                    y * size,
                    size,
                    size,
                    -1,
                    -1,
                    -1,
                    -1,
                    up,
                    down,
                    NULL,
                    BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_GRAPHIC);
                if (btnId == -1) {
                    free(up);
                    free(down);
                    continue;
                }

                win_register_button_func(btnId, NULL, NULL, hud_scroll_on_down, NULL);

                hudScrollButtons[buttonIndex] = btnId;
                buttonIndex++;
            }
        }
    }

    int actionSize = (int)(gconfig_actions_size * scale);
    if (actionSize <= 0) {
        actionSize = 48;
    }
    int gap = gconfig_actions_gap;
    if (gap < 0) {
        gap = 0;
    }

    if (gconfig_actions_enabled) {
        int columnHeight = GAM_CONFIG_ACTION_SLOTS * actionSize + (GAM_CONFIG_ACTION_SLOTS - 1) * gap;

        int actionOffsetX = gconfig_actions_offset_x;
        if (actionOffsetX < 0) {
            actionOffsetX = screenGetWidth() + actionOffsetX - actionSize;
        }
        int actionOffsetY = gconfig_actions_offset_y;
        if (actionOffsetY < 0) {
            actionOffsetY = screenGetHeight() + actionOffsetY - columnHeight;
        }

        hudActionsWindow = win_add(actionOffsetX, actionOffsetY, actionSize, columnHeight, 0, WINDOW_TRANSPARENT | WINDOW_HIDDEN);
        if (hudActionsWindow == -1) {
            win_delete(hudScrollWindow);
            hudScrollWindow = -1;
            return -1;
        }

        for (int slot = 0; slot < GAM_CONFIG_ACTION_SLOTS; slot++) {
            int frameUp;
            int frameDown;
            hud_resolve_action_frames(gconfig_action_slots[slot], &frameUp, &frameDown);

            unsigned char* up = hud_make_button_image(frameUp, actionSize);
            unsigned char* down = hud_make_button_image(frameDown, actionSize);
            if (up == NULL || down == NULL) {
                if (up != NULL) {
                    free(up);
                }
                if (down != NULL) {
                    free(down);
                }
                continue;
            }

            hud_apply_opacity(up, actionSize, opacity);
            hud_apply_opacity(down, actionSize, opacity);

            int slotKey = gconfig_action_slots[slot];

            int btnId = win_register_button(hudActionsWindow,
                0,
                slot * (actionSize + gap),
                actionSize,
                actionSize,
                -1,
                -1,
                slotKey,
                -1,
                up,
                down,
                NULL,
                BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_GRAPHIC);
            if (btnId == -1) {
                free(up);
                free(down);
                continue;
            }

            hudActionButtons[slot] = btnId;
            hudActionUp[slot] = up;
            hudActionDown[slot] = down;
        }
    }

    return 0;
}

void hud_reset()
{
    if (hudScrollWindow != -1) {
        win_hide(hudScrollWindow);
    }
    if (hudActionsWindow != -1) {
        win_hide(hudActionsWindow);
    }
}

// Shows or hides the HUD windows according to the configured type and the
// current screen. Called once per frame from a background process.
void hud_process()
{
    if (hudScrollWindow == -1 && hudActionsWindow == -1) {
        return;
    }

    bool visible = current_screen == SCREEN_GAME;
    if (visible) {
        switch (gconfig_hud_type) {
        case 0:
            visible = false;
            break;
        case 1:
            break;
        case 2:
            // Smart: keep the overlay while the screen is being touched.
            visible = mouse_get_buttons() != 0;
            break;
        }
    }

    Window* scrollWnd = GNW_find(hudScrollWindow);
    if (scrollWnd != NULL) {
        bool shown = (scrollWnd->flags & WINDOW_HIDDEN) == 0;
        if (visible && !shown) {
            win_show(hudScrollWindow);
        } else if (!visible && shown) {
            win_hide(hudScrollWindow);
        }
    }

    Window* actionsWnd = GNW_find(hudActionsWindow);
    if (actionsWnd != NULL) {
        bool shown = (actionsWnd->flags & WINDOW_HIDDEN) == 0;
        if (visible && !shown) {
            win_show(hudActionsWindow);
        } else if (!visible && shown) {
            win_hide(hudActionsWindow);
        }
    }
}

void hud_exit()
{
    if (hudActionsWindow != -1) {
        win_delete(hudActionsWindow);
        hudActionsWindow = -1;
    }

    if (hudScrollWindow != -1) {
        win_delete(hudScrollWindow);
        hudScrollWindow = -1;
    }

    for (int slot = 0; slot < GAM_CONFIG_ACTION_SLOTS; slot++) {
        if (hudActionUp[slot] != NULL) {
            free(hudActionUp[slot]);
            hudActionUp[slot] = NULL;
        }
        if (hudActionDown[slot] != NULL) {
            free(hudActionDown[slot]);
            hudActionDown[slot] = NULL;
        }
        hudActionButtons[slot] = -1;
    }

    memset(hudScrollButtons, 0, sizeof(hudScrollButtons));
}

} // namespace fallout
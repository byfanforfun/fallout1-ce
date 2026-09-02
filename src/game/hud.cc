#include "game/hud.h"

#include <stdio.h>
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
// Arrows only: the D-pad center button is handled separately.
#define HUD_SCROLL_BUTTON_COUNT 8

// Unique event codes assigned to the HUD buttons. They are never handled by
// the game, so a button click is simply swallowed instead of falling through
// to the game world as a mouse click.
#define HUD_BUTTON_EVENT_BASE 0x4000
#define HUD_BUTTON_EVENT_CENTER (HUD_BUTTON_EVENT_BASE + 8)
#define HUD_BUTTON_EVENT_HOVER_BASE (HUD_BUTTON_EVENT_BASE + 0x10)
#define HUD_TOGGLE_EVENT_HOVER (HUD_BUTTON_EVENT_BASE + 0x20)
#define HUD_TOGGLE_EVENT (HUD_BUTTON_EVENT_BASE + 0x21)

// Map-scroll directions, one entry per scroll button.
typedef struct ScrollDir {
    int dx;
    int dy;
} ScrollDir;

static int hudScrollWindow = -1;
static int hudActionsWindow = -1;
static int hudToggleWindow = -1;

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

// Interface frame numbers for the 3x3 grid. The center cell is the "center on
// player" button (ACTPICK frame); the entries are NW, N, NE, W, (center), E,
// SW, S, SE in scan order. Values map to `intrface.list` rows minus one.
static const int hudScrollFrameNums[9] = {
    270,
    271,
    272,
    277,
    283,
    273,
    276,
    275,
    274,
};

static int hudActionButtons[GAM_CONFIG_ACTION_SLOTS];
static unsigned char* hudActionUp[GAM_CONFIG_ACTION_SLOTS];
static unsigned char* hudActionDown[GAM_CONFIG_ACTION_SLOTS];

static int hudToggleButton = -1;
static unsigned char* hudToggleUp = NULL;
static unsigned char* hudToggleDown = NULL;

// When the always-visible HUD (type 1) is collapsed, this flag is active.
static bool hudHidden = false;

static bool hudDebugMode = false;

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
// centered. `trans_cscale` only writes opaque pixels, so the transparent
// background stays transparent as long as the destination buffer is zero
// filled. The caller owns the returned buffer, or receives NULL on failure.
static unsigned char* hud_make_button_image(int frameNum, int size)
{
    CacheEntry* cacheEntry = NULL;
    int frameWidth;
    int frameHeight;
    unsigned char* frameData = art_lock(art_id(OBJ_TYPE_INTERFACE, frameNum, 0, 0, 0), &cacheEntry, &frameWidth, &frameHeight);
    FILE* dbg = fopen("hud_dbg.txt", "a");
    if (dbg != NULL) {
        fprintf(dbg, "  art %d -> %p w=%d h=%d\n", frameNum, (void*)frameData, frameWidth, frameHeight);
        fflush(dbg);
    }
    if (frameData == NULL || cacheEntry == NULL) {
        if (dbg != NULL) {
            fclose(dbg);
        }
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

    // The destination is zero filled so that every pixel the source leaves
    // untouched stays fully transparent.
    unsigned char* upBuf = (unsigned char*)calloc(1, size * size);
    if (upBuf == NULL) {
        art_ptr_unlock(cacheEntry);
        return NULL;
    }

    if (dbg != NULL) {
        fprintf(dbg, "    cscale %d -> %d x %d (src %d x %d)\n", frameNum, scaledWidth, scaledHeight, frameWidth, frameHeight);
        fflush(dbg);
    }
    trans_cscale(frameData,
        frameWidth,
        frameHeight,
        frameWidth,
        upBuf + ((size - scaledHeight) / 2) * size + (size - scaledWidth) / 2,
        scaledWidth,
        scaledHeight,
        size);

    art_ptr_unlock(cacheEntry);
    if (dbg != NULL) {
        fclose(dbg);
    }

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

// Toggles the visibility of the scroll D-pad and the actions column. Called
// when the always-visible HUD button (type 1) is pressed.
static void hud_toggle_on_down(int btnId, int keyCode)
{
    hudHidden = !hudHidden;
}

int hud_init()
{
    static const char* dbgPath = "hud_dbg.txt";
    FILE* dbg = fopen(dbgPath, "a");
    if (dbg != NULL) {
        fprintf(dbg, "hud_init: type=%d scroll=%d actions=%d\n", gconfig_hud_type, gconfig_scroll_enabled, gconfig_actions_enabled);
        fflush(dbg);
    }

    if (hudScrollWindow != -1 || hudActionsWindow != -1) {
        if (dbg != NULL) {
            fclose(dbg);
        }
        return -1;
    }

    hudDebugMode = getenv("FALLOUT_HUD_DEBUG") != NULL;
    if (hudDebugMode) {
        gconfig_hud_type = 1;
    }

    // Skip both windows entirely when the HUD is disabled.
    if (gconfig_hud_type == 0 && !hudDebugMode) {
        if (dbg != NULL) {
            fprintf(dbg, "hud_init: disabled\n");
            fclose(dbg);
        }
        return 0;
    }

    double scale = gconfig_hud_scale;
    if (scale <= 0) {
        scale = 1.0;
    }
    int opacity = gconfig_hud_opacity;
    if (hudDebugMode) {
        opacity = 100;
    }

    int size = (int)(gconfig_scroll_size * scale);
    if (size <= 0) {
        size = 64;
    }

    if (gconfig_scroll_enabled) {
        int screenWidth = screenGetWidth();
        int screenHeight = screenGetHeight();

        // Keep the D-pad within the screen on small displays, otherwise
        // win_add fails for windows larger than the screen.
        int maxGrid = screenWidth < screenHeight ? screenWidth : screenHeight;
        int grid = HUD_SCROLL_GRID * size;
        if (grid > maxGrid) {
            size = maxGrid / HUD_SCROLL_GRID;
            if (size < 16) {
                size = 16;
            }
            grid = HUD_SCROLL_GRID * size;
        }

        int offsetX = gconfig_scroll_offset_x;
        if (offsetX < 0) {
            offsetX = screenWidth + offsetX - grid;
        }
        int offsetY = gconfig_scroll_offset_y;
        if (offsetY < 0) {
            offsetY = screenHeight + offsetY - grid;
        }

        int windowFlags = WINDOW_HIDDEN;
        if (!hudDebugMode) {
            windowFlags |= WINDOW_TRANSPARENT;
        }

        hudScrollWindow = win_add(offsetX, offsetY, grid, grid, 0, windowFlags);
        if (hudScrollWindow == -1) {
            FILE* dbg = fopen("hud_dbg.txt", "a");
            if (dbg != NULL) {
                fprintf(dbg, "scroll window creation failed (%d x %d)\n", grid, grid);
                fclose(dbg);
            }
        } else {

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

                    int btnFlags = BUTTON_FLAG_GRAPHIC | BUTTON_FLAG_0x40;
                    if (!hudDebugMode) {
                        btnFlags |= BUTTON_FLAG_TRANSPARENT;
                    }

                    // Only the sprite pixels are clickable: the mask points at
                    // the up image, whose zero pixels are transparent, so taps
                    // on the transparent parts of a button fall through to the
                    // game world.
                    bool isCenter = (y == 1 && x == 1);
                    int mouseDownCode = isCenter ? KEY_HOME : (HUD_BUTTON_EVENT_BASE + buttonIndex);
                    int mouseUpCode = isCenter ? HUD_BUTTON_EVENT_CENTER : (HUD_BUTTON_EVENT_BASE + buttonIndex);
                    int mouseEnterCode = isCenter ? HUD_BUTTON_EVENT_CENTER : (HUD_BUTTON_EVENT_HOVER_BASE + buttonIndex);

                    int btnId = win_register_button(hudScrollWindow,
                        x * size,
                        y * size,
                        size,
                        size,
                        mouseEnterCode,
                        -1,
                        mouseDownCode,
                        mouseUpCode,
                        up,
                        down,
                        NULL,
                        btnFlags);
                    if (btnId == -1) {
                        free(up);
                        free(down);
                        continue;
                    }

                    win_register_button_mask(btnId, up);

                    if (isCenter) {
                        // The HOME-analog button: no scroll callback, the
                        // engine handles the KEY_HOME event it produces.
                        continue;
                    }

                    win_register_button_func(btnId, NULL, NULL, hud_scroll_on_down, NULL);

                    hudScrollButtons[buttonIndex] = btnId;
                    buttonIndex++;
                }
            }
        }
    }

    // In the always-visible mode (type 1) a persistent toggle button floats at
    // the top center of the screen. It survives the collapse of the rest of
    // the HUD so that the user can restore it.
    if (gconfig_hud_type == 1) {
        int toggleSize = (int)(40 * scale);
        if (toggleSize < 32) {
            toggleSize = 32;
        }

        int screenWidth = screenGetWidth();

        int toggleOffsetX = (screenWidth - toggleSize) / 2;
        // Always keep the toggle at the top center of the screen, no matter
        // where the D-pad itself is positioned.
        int toggleOffsetY = 8;

        int windowFlags = WINDOW_HIDDEN;
        if (!hudDebugMode) {
            windowFlags |= WINDOW_TRANSPARENT;
        }

        hudToggleWindow = win_add(toggleOffsetX, toggleOffsetY, toggleSize, toggleSize, 0, windowFlags);
        if (hudToggleWindow != -1) {
            int frameNum = 6;
            hudToggleUp = hud_make_button_image(frameNum, toggleSize);
            hudToggleDown = hud_make_button_image(7, toggleSize);
            if (hudToggleUp != NULL && hudToggleDown != NULL) {
                hud_apply_opacity(hudToggleUp, toggleSize, opacity);
                hud_apply_opacity(hudToggleDown, toggleSize, opacity);

                int btnFlags = BUTTON_FLAG_GRAPHIC | BUTTON_FLAG_0x40;
                if (!hudDebugMode) {
                    btnFlags |= BUTTON_FLAG_TRANSPARENT;
                }

                hudToggleButton = win_register_button(hudToggleWindow,
                    0,
                    0,
                    toggleSize,
                    toggleSize,
                    HUD_TOGGLE_EVENT_HOVER,
                    -1,
                    HUD_TOGGLE_EVENT,
                    HUD_TOGGLE_EVENT,
                    hudToggleUp,
                    hudToggleDown,
                    NULL,
                    btnFlags);
                if (hudToggleButton == -1) {
                    free(hudToggleUp);
                    hudToggleUp = NULL;
                    free(hudToggleDown);
                    hudToggleDown = NULL;
                } else {
                    win_register_button_mask(hudToggleButton, hudToggleUp);
                    win_register_button_func(hudToggleButton, NULL, NULL, hud_toggle_on_down, NULL);
                }
            } else {
                if (hudToggleUp != NULL) {
                    free(hudToggleUp);
                    hudToggleUp = NULL;
                }
                if (hudToggleDown != NULL) {
                    free(hudToggleDown);
                    hudToggleDown = NULL;
                }
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
        int screenHeight = screenGetHeight();

        // Shrink the action buttons so the column always fits the screen
        // height (windows taller than the screen cause win_add to fail).
        if (columnHeight > screenHeight) {
            if (gap >= screenHeight) {
                gap = 0;
            }
            actionSize = (screenHeight - (GAM_CONFIG_ACTION_SLOTS - 1) * gap) / GAM_CONFIG_ACTION_SLOTS;
            if (actionSize < 16) {
                actionSize = 16;
            }
            columnHeight = GAM_CONFIG_ACTION_SLOTS * actionSize + (GAM_CONFIG_ACTION_SLOTS - 1) * gap;
        }

        int actionOffsetX = gconfig_actions_offset_x;
        if (actionOffsetX < 0) {
            actionOffsetX = screenGetWidth() + actionOffsetX - actionSize;
        }
        int actionOffsetY = gconfig_actions_offset_y;
        if (actionOffsetY < 0) {
            actionOffsetY = screenGetHeight() + actionOffsetY - columnHeight;
        }

        int windowFlags = WINDOW_HIDDEN;
        if (!hudDebugMode) {
            windowFlags |= WINDOW_TRANSPARENT;
        }

        hudActionsWindow = win_add(actionOffsetX, actionOffsetY, actionSize, columnHeight, 0, windowFlags);
        if (hudActionsWindow == -1) {
            FILE* dbg = fopen("hud_dbg.txt", "a");
            if (dbg != NULL) {
                fprintf(dbg, "actions window creation failed (%d x %d)\n", actionSize, columnHeight);
                fclose(dbg);
            }
        }

        for (int slot = 0; slot < GAM_CONFIG_ACTION_SLOTS; slot++) {
            FILE* dbg = fopen("hud_dbg.txt", "a");
            if (dbg != NULL) {
                fprintf(dbg, "actions slot %d key=%d\n", slot, gconfig_action_slots[slot]);
                fflush(dbg);
                fclose(dbg);
            }

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

            int btnFlags = BUTTON_FLAG_GRAPHIC;
            if (!hudDebugMode) {
                btnFlags |= BUTTON_FLAG_TRANSPARENT;
            }

            // The down code is the real game key, which performs the action.
            // The enter/up codes are unique HUD codes that swallow the click
            // so it does not fall through to the game world.
            int btnId = win_register_button(hudActionsWindow,
                0,
                slot * (actionSize + gap),
                actionSize,
                actionSize,
                HUD_BUTTON_EVENT_HOVER_BASE + 0x20 + slot,
                -1,
                slotKey,
                HUD_BUTTON_EVENT_BASE + 0x30 + slot,
                up,
                down,
                NULL,
                btnFlags);
            if (btnId == -1) {
                free(up);
                free(down);
                continue;
            }

            win_register_button_mask(btnId, up);

            hudActionButtons[slot] = btnId;
            hudActionUp[slot] = up;
            hudActionDown[slot] = down;
        }
    }

    if (hudDebugMode && current_screen == SCREEN_GAME) {
        if (hudScrollWindow != -1) {
            win_show(hudScrollWindow);
        }
        if (hudActionsWindow != -1) {
            win_show(hudActionsWindow);
        }
    }

    if (dbg != NULL) {
        fprintf(dbg, "hud_init: done\n");
        fclose(dbg);
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
    if (hudToggleWindow != -1) {
        win_hide(hudToggleWindow);
    }
}

// Shows or hides the HUD windows according to the configured type and the
// current screen. Called once per frame from a background process.
void hud_process()
{
    if (hudScrollWindow == -1 && hudActionsWindow == -1 && hudToggleWindow == -1) {
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

    // The toggle button always stays on top of the always-visible HUD, even
    // when the rest of the overlay is collapsed.
    if (gconfig_hud_type == 1 && hudToggleWindow != -1) {
        Window* toggleWnd = GNW_find(hudToggleWindow);
        if (toggleWnd != NULL) {
            bool shown = (toggleWnd->flags & WINDOW_HIDDEN) == 0;
            bool wantsShown = current_screen == SCREEN_GAME;
            if (wantsShown && !shown) {
                win_show(hudToggleWindow);
            } else if (!wantsShown && shown) {
                win_hide(hudToggleWindow);
            }
        }
    }

    if (hudHidden) {
        visible = false;
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
    if (hudToggleWindow != -1) {
        win_delete(hudToggleWindow);
        hudToggleWindow = -1;
    }

    if (hudToggleUp != NULL) {
        free(hudToggleUp);
        hudToggleUp = NULL;
    }
    if (hudToggleDown != NULL) {
        free(hudToggleDown);
        hudToggleDown = NULL;
    }
    hudToggleButton = -1;

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
    hudHidden = false;
}

} // namespace fallout
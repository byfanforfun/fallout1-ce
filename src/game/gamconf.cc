#include "game/gamconf.h"

#include <string.h>

#include "platform_compat.h"
#include "plib/gnw/svga.h"

namespace fallout {

Config gam_config;

static bool gamconf_initialized = false;
static bool gamconf_needs_defaults = false;
static char gamconf_file_name[COMPAT_MAX_PATH];

int gconfig_hud_type;
double gconfig_hud_scale;
int gconfig_hud_opacity;

int gconfig_scroll_enabled;
int gconfig_scroll_size;
int gconfig_scroll_offset_x;
int gconfig_scroll_offset_y;

int gconfig_actions_enabled;
int gconfig_actions_size;
int gconfig_actions_offset_x;
int gconfig_actions_offset_y;
int gconfig_actions_gap;
int gconfig_action_slots[GAM_CONFIG_ACTION_SLOTS];

bool gamconf_init()
{
    if (gamconf_initialized) {
        return false;
    }

    if (!config_init(&gam_config)) {
        return false;
    }

    config_set_value(&gam_config, GAM_CONFIG_HUD_KEY, GAM_CONFIG_HUD_TYPE_KEY, 2);
    config_set_double(&gam_config, GAM_CONFIG_HUD_KEY, GAM_CONFIG_HUD_SCALE_KEY, 1.0);
    config_set_value(&gam_config, GAM_CONFIG_HUD_KEY, GAM_CONFIG_HUD_OPACITY_KEY, 128);

    config_set_value(&gam_config, GAM_CONFIG_SCROLL_KEY, GAM_CONFIG_SCROLL_ENABLED_KEY, 1);
    config_set_value(&gam_config, GAM_CONFIG_SCROLL_KEY, GAM_CONFIG_SCROLL_SIZE_KEY, 64);
    config_set_value(&gam_config, GAM_CONFIG_SCROLL_KEY, GAM_CONFIG_SCROLL_OFFSET_X_KEY, 8);
    config_set_value(&gam_config, GAM_CONFIG_SCROLL_KEY, GAM_CONFIG_SCROLL_OFFSET_Y_KEY, 0);

    config_set_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_ENABLED_KEY, 1);
    config_set_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_SIZE_KEY, 48);
    config_set_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_OFFSET_X_KEY, -8);
    config_set_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_OFFSET_Y_KEY, 0);
    config_set_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_GAP_KEY, 8);

    const char* actionKeys[GAM_CONFIG_ACTION_SLOTS] = { GAM_CONFIG_ACTION_SLOT_KEYS };
    int actionDefaults[GAM_CONFIG_ACTION_SLOTS] = {
        'a',
        'n',
        'm',
        'b',
        'i',
        'c',
        'p',
        '\x09',
        's',
        '1',
        '2',
        '3',
    };
    for (int index = 0; index < GAM_CONFIG_ACTION_SLOTS; index++) {
        config_set_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, actionKeys[index], actionDefaults[index]);
    }

    strcpy(gamconf_file_name, GAM_CONFIG_FILE_NAME);

    // Remember whether f1_am.ini already exists. If it does not, the layout
    // defaults are derived from the screen resolution on first launch.
    FILE* stream = compat_fopen(gamconf_file_name, "rb");
    if (stream == NULL) {
        gamconf_needs_defaults = true;
    } else {
        fclose(stream);
        config_load(&gam_config, gamconf_file_name, false);
    }

    config_get_value(&gam_config, GAM_CONFIG_HUD_KEY, GAM_CONFIG_HUD_TYPE_KEY, &gconfig_hud_type);
    config_get_double(&gam_config, GAM_CONFIG_HUD_KEY, GAM_CONFIG_HUD_SCALE_KEY, &gconfig_hud_scale);
    config_get_value(&gam_config, GAM_CONFIG_HUD_KEY, GAM_CONFIG_HUD_OPACITY_KEY, &gconfig_hud_opacity);

    config_get_value(&gam_config, GAM_CONFIG_SCROLL_KEY, GAM_CONFIG_SCROLL_ENABLED_KEY, &gconfig_scroll_enabled);
    config_get_value(&gam_config, GAM_CONFIG_SCROLL_KEY, GAM_CONFIG_SCROLL_SIZE_KEY, &gconfig_scroll_size);
    config_get_value(&gam_config, GAM_CONFIG_SCROLL_KEY, GAM_CONFIG_SCROLL_OFFSET_X_KEY, &gconfig_scroll_offset_x);
    config_get_value(&gam_config, GAM_CONFIG_SCROLL_KEY, GAM_CONFIG_SCROLL_OFFSET_Y_KEY, &gconfig_scroll_offset_y);

    config_get_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_ENABLED_KEY, &gconfig_actions_enabled);
    config_get_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_SIZE_KEY, &gconfig_actions_size);
    config_get_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_OFFSET_X_KEY, &gconfig_actions_offset_x);
    config_get_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_OFFSET_Y_KEY, &gconfig_actions_offset_y);
    config_get_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_GAP_KEY, &gconfig_actions_gap);
    for (int index = 0; index < GAM_CONFIG_ACTION_SLOTS; index++) {
        config_get_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, actionKeys[index], &gconfig_action_slots[index]);
    }

    gamconf_initialized = true;

    return true;
}

bool gamconf_save()
{
    if (!gamconf_initialized) {
        return false;
    }

    if (!config_save(&gam_config, gamconf_file_name, false)) {
        return false;
    }

    return true;
}

bool gamconf_apply_defaults_once()
{
    if (!gamconf_initialized || !gamconf_needs_defaults) {
        return false;
    }

    int screenWidth = screenGetWidth();
    int screenHeight = screenGetHeight();
    if (screenWidth <= 0 || screenHeight <= 0) {
        return false;
    }

    // Scroll D-pad: 3x3 grid anchored to the bottom-left corner.
    int scrollSize = 64;
    config_set_value(&gam_config, GAM_CONFIG_SCROLL_KEY, GAM_CONFIG_SCROLL_SIZE_KEY, scrollSize);
    config_set_value(&gam_config, GAM_CONFIG_SCROLL_KEY, GAM_CONFIG_SCROLL_OFFSET_X_KEY, 8);
    config_set_value(&gam_config, GAM_CONFIG_SCROLL_KEY, GAM_CONFIG_SCROLL_OFFSET_Y_KEY, -8);

    // Actions column: 12 buttons anchored to the bottom-right corner. Shrink
    // the buttons so the whole column fits the screen height.
    int actionSize = 48;
    int gap = 8;
    int columnHeight = GAM_CONFIG_ACTION_SLOTS * actionSize + (GAM_CONFIG_ACTION_SLOTS - 1) * gap;
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

    config_set_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_SIZE_KEY, actionSize);
    config_set_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_GAP_KEY, gap);
    config_set_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_OFFSET_X_KEY, -8);
    config_set_value(&gam_config, GAM_CONFIG_ACTIONS_KEY, GAM_CONFIG_ACTIONS_OFFSET_Y_KEY, -8);

    if (!config_save(&gam_config, gamconf_file_name, false)) {
        return false;
    }

    gamconf_needs_defaults = false;
    return true;
}

bool gamconf_exit(bool shouldSave)
{
    if (!gamconf_initialized) {
        return false;
    }

    bool result = true;

    if (shouldSave) {
        if (!config_save(&gam_config, gamconf_file_name, false)) {
            result = false;
        }
    }

    config_exit(&gam_config);

    gamconf_initialized = false;

    return result;
}

} // namespace fallout
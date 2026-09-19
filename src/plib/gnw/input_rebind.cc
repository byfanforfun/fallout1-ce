#include "plib/gnw/input_rebind.h"

#include "game/config.h"
#include "platform_compat.h"
#include <array>
#include <string.h>

namespace fallout {

#define BIND_MAX_BIND SCREEN_MAX
#define BIND_MAX_KEY 512
#define BIND_CONFIG_FILE_NAME "fallout_keys.cfg"

Config bind_config;
static bool bind_config_initialized = false;
static char bind_config_file_name[COMPAT_MAX_PATH];
static int binded_keys[BIND_MAX_BIND][BIND_MAX_KEY];

int current_screen = -1;

constexpr std::array<std::pair<BindScreen, const char*>, BIND_MAX_BIND> BindSectionStrings = { { { SCREEN_MAIN, "main" },
    { SCREEN_GAME, "game" },
    { SCREEN_CHAR, "editor" },
    { SCREEN_INV, "inventory" },
    { SCREEN_PIP, "pip" },
    { SCREEN_INTERFACE, "interface" },
    { SCREEN_USE_ITEM, "use_item" },
    { SCREEN_LOOT, "loot" },
    { SCREEN_BARTER, "barter" },
    { SCREEN_MOVE_ITEMS, "move_items" },
    { SCREEN_SET_TIMER, "set_timer" },
    { SCREEN_DIALOG_OPTIONS, "dialog_options" },
    { SCREEN_DIALOG_REPLY, "dialog_reply" },
    { SCREEN_DIALOG_REVIEW, "dialog_review" },
    { SCREEN_DIALOG, "dialog" },
    { SCREEN_DIALOG_ABOUT, "dialog_about" },
    { SCREEN_LOAD_SAVE, "load_save" },
    { SCREEN_SAVE_COMMENT, "save_comment" },
    { SCREEN_OPTIONS, "options" },
    { SCREEN_PAUSE, "pause" },
    { SCREEN_PREFERENCES, "preferences" },
    { SCREEN_CHAR_SELECT, "char_select" },
    { SCREEN_SKILLDEX, "skilldex" },
    { SCREEN_ELEVATOR, "elevator" },
    { SCREEN_MESSAGE, "message" },
    { SCREEN_FILE_LOAD, "file_load" },
    { SCREEN_FILE_SAVE, "file_save" },
    { SCREEN_HALL_OF_FAME, "hall_of_fame" },
    { SCREEN_HELP, "help" },
    { SCREEN_CALLED_SHOT, "called_shot" },
    { SCREEN_PIP_STATUS, "pip_status" },
    { SCREEN_PIP_AUTOMAPS, "pip_automaps" },
    { SCREEN_PIP_ARCHIVES, "pip_archives" },
    { SCREEN_PIP_ALARM, "pip_alarm" },
    { SCREEN_AUTOMAP, "automap" },
    { SCREEN_WORLD_MAP, "world_map" },
    { SCREEN_TOWN_MAP, "town_map" },
    { SCREEN_START_MESSAGE, "start_message" },
    { SCREEN_EDITOR_NAME, "editor_name" },
    { SCREEN_EDITOR_AGE, "editor_age" },
    { SCREEN_EDITOR_SEX, "editor_sex" },
    { SCREEN_EDITOR_OPTIONS, "editor_options" },
    { SCREEN_PERKS, "perks" } } };

constexpr const char* BindSectionToString(BindScreen section)
{
    for (const auto& [key, value] : BindSectionStrings) {
        if (key == section) return value;
    }

    return BIND_SECTION_NO_SEC;
}

bool bind_config_init();
bool bind_config_exit();

bool bind_init()
{
    if (!bind_config_init())
        return false;

    char ti[6];
    int key;
    for (int i = BindScreen(SCREEN_MAIN); BindScreen(SCREEN_MAX) > i; ++i) {
        for (int ii = 1; BIND_MAX_KEY > ii; ++ii) {
            snprintf(ti, 6, "%d", ii);
            if (config_get_value(&bind_config, BindSectionToString(BindScreen(i)), ti, &key)) {
                binded_keys[i][ii] = key;
            } else {
                binded_keys[i][ii] = -1;
            }
        }
    }

    bind_config_exit();

    return true;
}

bool bind_config_init()
{
    if (bind_config_initialized) {
        return false;
    }

    if (!config_init(&bind_config)) {
        return false;
    }

    strcpy(bind_config_file_name, BIND_CONFIG_FILE_NAME);
    config_load(&bind_config, bind_config_file_name, false);

    bind_config_initialized = true;

    return true;
}

bool bind_config_exit()
{
    if (!bind_config_initialized) {
        return false;
    }

    config_exit(&bind_config);

    bind_config_initialized = false;

    return true;
}

int get_key(int screen, int key)
{
    if (screen < SCREEN_MAIN || screen >= BindScreen(SCREEN_MAX) || key >= BIND_MAX_KEY || 0 > key)
        return key;

    if (binded_keys[screen][key] != -1)
        return binded_keys[screen][key];

    return key;
}

int get_physical_key(int screen, int logical_key)
{
    if (screen < SCREEN_MAIN || screen >= BindScreen(SCREEN_MAX))
        return logical_key;

    for (int i = 1; i < BIND_MAX_KEY; i++) {
        if (binded_keys[screen][i] == logical_key)
            return i;
    }

    return logical_key;
}

}
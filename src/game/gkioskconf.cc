#include "game/gkioskconf.h"

#include <stdio.h>
#include <string.h>

#include "game/gconfig.h"

#include "platform_compat.h"

namespace fallout {

Config kiosk_config;

int _game_difficulty = 0;
int _combat_difficulty = 0;
int _language_filter = 0;

static bool gkioskconf_initialized = false;
static char gkioskconf_file_name[COMPAT_MAX_PATH];
int gconfig_saveload_allowed;
int gconfig_options_allowed;
int gconfig_dialog_exit_0_allowed;
int gconfig_game_exit_allowed;
int gconfig_screensaver_enabled;
int gconfig_screensaver_timeout;
int gconfig_random_locations;

bool gkioskconf_init()
{
    char* sep;

    if (gkioskconf_initialized) {
        return false;
    }

    if (!config_init(&kiosk_config)) {
        return false;
    }

    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_EXP_START_KEY, 0);
    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_ENABLE_SAVELOAD, 0);
    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_ENABLE_OPTIONS, 0);
    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_ENABLE_DIALOG_EXIT_0, 0);
    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_ENABLE_GAME_EXIT, 0);
    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_TIMER_INACTIVE_F, 300);
    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_TIMER_INACTIVE_1, 100);
    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_TIMER_INACTIVE_2, 30);
    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_TIMER_INACTIVE_3, 10);
    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_START_MESSAGE, 1);
    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_SCREENSAVER_ENABLED, 0);
    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_SCREENSAVER_TIMEOUT, 120);
    config_set_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_RANDOM_LOCATIONS, 0);

    config_set_value(&kiosk_config, KIOSK_CONFIG_OVERRIDE_KEY, KIOSK_CONFIG_OVERRIDE_GAME, 0);
    config_set_value(&kiosk_config, KIOSK_CONFIG_OVERRIDE_KEY, KIOSK_CONFIG_OVERRIDE_COMBAT, 0);
    config_set_value(&kiosk_config, KIOSK_CONFIG_OVERRIDE_KEY, KIOSK_CONFIG_OVERRIDE_LFILTER, 0);

    strcpy(gkioskconf_file_name, KIOSK_CONFIG_FILE_NAME);
    config_load(&kiosk_config, gkioskconf_file_name, false);

    //override fallout.cfg
    config_get_value(&kiosk_config, KIOSK_CONFIG_OVERRIDE_KEY, KIOSK_CONFIG_OVERRIDE_GAME, &_game_difficulty);
    config_get_value(&kiosk_config, KIOSK_CONFIG_OVERRIDE_KEY, KIOSK_CONFIG_OVERRIDE_COMBAT, &_combat_difficulty);
    config_get_value(&kiosk_config, KIOSK_CONFIG_OVERRIDE_KEY, KIOSK_CONFIG_OVERRIDE_LFILTER, &_language_filter);

    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, _game_difficulty);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, _combat_difficulty);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, _language_filter);

    //set global vars
    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_ENABLE_SAVELOAD, &gconfig_saveload_allowed);
    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_ENABLE_OPTIONS, &gconfig_options_allowed);
    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_ENABLE_DIALOG_EXIT_0, &gconfig_dialog_exit_0_allowed);
    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_ENABLE_GAME_EXIT, &gconfig_game_exit_allowed);
    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_SCREENSAVER_ENABLED, &gconfig_screensaver_enabled);
    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_SCREENSAVER_TIMEOUT, &gconfig_screensaver_timeout);
    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_RANDOM_LOCATIONS, &gconfig_random_locations);

    gkioskconf_initialized = true;

    return true;
}

bool gkioskconf_save()
{
    if (!gkioskconf_initialized) {
        return false;
    }

    if (!config_save(&kiosk_config, gkioskconf_file_name, false)) {
        return false;
    }

    return true;
}

bool gkioskconf_exit(bool shouldSave)
{
    if (!gkioskconf_initialized) {
        return false;
    }

    bool result = true;

    if (shouldSave) {
        if (!config_save(&kiosk_config, gkioskconf_file_name, false)) {
            result = false;
        }
    }

    config_exit(&kiosk_config);

    gkioskconf_initialized = false;

    return result;
}

} // namespace fallout

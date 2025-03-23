#include "game/gkioskconf.h"

#include <stdio.h>
#include <string.h>

#include "platform_compat.h"

namespace fallout {

Config kiosk_config;
static bool gkioskconf_initialized = false;
static char gkioskconf_file_name[COMPAT_MAX_PATH];

bool gkioskconf_init()
{
    char* sep;

    if (gkioskconf_initialized) {
        return false;
    }

    if (!config_init(&kiosk_config)) {
        return false;
    }
    
    strcpy(gkioskconf_file_name, KIOSK_CONFIG_FILE_NAME);
    config_load(&kiosk_config, gkioskconf_file_name, false);

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

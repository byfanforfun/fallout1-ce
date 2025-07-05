#include "plib/gnw/input_rebind.h"

#include <array>
#include <string.h>
#include "game/config.h"
#include "platform_compat.h"

namespace fallout {

#define BIND_MAX_BIND 8
#define BIND_MAX_KEY 512
#define BIND_CONFIG_FILE_NAME "fallout_keys.cfg"

Config bind_config;
static bool bind_config_initialized = false;
static char bind_config_file_name[COMPAT_MAX_PATH];
static int binded_keys[BIND_MAX_BIND][BIND_MAX_KEY];

int current_screen = -1;

constexpr std::array<std::pair<BindSection, const char*>, BindSection(BindSection_max)> BindSectionStrings = {{
    {BindSection::Main, "main"},
    {BindSection::Game, "game"},
    {BindSection::Editor, "editor"},
    {BindSection::Inv, "inventory"},
    {BindSection::Pip, "pip"}
}};

constexpr const char* BindSectionToString(BindSection section) {
    for (const auto& [key, value] : BindSectionStrings) {
        if (key == section) return value;
    }

    return BIND_SECTION_NO_SEC;
}


bool bind_config_init();
bool bind_config_exit();

bool bind_init() {
    if(!bind_config_init())
        return false;

    char ti[6];
    int key;
    for(int i = BindScreen(SCREEN_MAIN); BindScreen(SCREEN_MAX) >= i; ++i){
        for(int ii = 1; BIND_MAX_KEY >= ii; ++ii){
            snprintf(ti, 6, "%d", ii);
            if(config_get_value(&bind_config, BindSectionToString(BindSection(i)), ti, &key)) {
                binded_keys[i][ii] = key;
            }else{
                binded_keys[i][ii] = -1;
            }
        }
    }

    bind_config_exit();

    return true;
}

bool bind_config_init() {
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

bool bind_config_exit() {
    if (!bind_config_initialized) {
        return false;
    }

    config_exit(&bind_config);

    bind_config_initialized = false;

    return true;
}

int get_key(int screen, int key) {
    if(screen < SCREEN_MAIN || screen >= BindScreen(SCREEN_MAX) || key >= BIND_MAX_KEY || 0 > key)
        return key;

    if(binded_keys[screen][key] != -1)
        return binded_keys[screen][key];

    return key;
}

}
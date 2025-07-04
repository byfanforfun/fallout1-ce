#include "plib/gnw/system_exec.h"
#include <string.h>

#include "game/gconfig.h"

#include "platform_compat.h"

namespace fallout {

#define EXEC_CONFIG_FILE_NAME "kiosk_exec.cfg"
#define EXEC_CONFIG_EXEC_SECTION "exec"

#define EXEC_MAX_LINES 8

Config exec_config;
static bool exec_config_initialized = false;
static char exec_config_file_name[COMPAT_MAX_PATH];

bool exec_config_init();
bool exec_config_exit(bool shouldSave);

bool exec_config_init()
{
    if (exec_config_initialized) {
        return false;
    }

    if (!config_init(&exec_config)) {
        return false;
    }
    
    strcpy(exec_config_file_name, EXEC_CONFIG_FILE_NAME);
    config_load(&exec_config, exec_config_file_name, false);

    exec_config_initialized = true;

    return true;
}

bool exec_config_exit(bool shouldSave)
{
    if (!exec_config_initialized) {
        return false;
    }
    
    bool result = true;

    if (shouldSave) {
        if (!config_save(&exec_config, exec_config_file_name, false)) {
            result = false;
        }
    }

    config_exit(&exec_config);

    exec_config_initialized = false;

    return result;
}

bool system_exec(int line_nums[]) {
    exec_config_init();

    static char* cmd = NULL;
    char ti[4];
    for(int i = 0; EXEC_MAX_LINES > i; ++i){
        snprintf(ti, 4, "%d", i);
        if(config_get_string(&exec_config, EXEC_CONFIG_EXEC_SECTION, ti, &cmd))
        {
            for(int ii = 0; EXEC_MAX_LINES > ii; ++ii){
                if(line_nums[ii] == i)
                    if(compat_exec(cmd) != 0) {
                        printf("executed abnormally: %s\n", cmd);
                    }
            }
        }
    }

    exec_config_exit(false);

    return true;
}
}

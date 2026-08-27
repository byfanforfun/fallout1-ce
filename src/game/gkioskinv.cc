#include "game/gkioskinv.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <random>
#include <vector>

#include "game/item.h"
#include "game/object.h"
#include "game/proto.h"

#include "plib/gnw/debug.h"

#include "platform_compat.h"

namespace fallout {

static void init_cached_item_pids();
static bool is_pid_excluded(int pid);
static bool is_pid_once(int pid);

static char inv_conf_file_name[COMPAT_MAX_PATH];
static bool inv_conf_initialized = false;

static bool rndex_conf_initialized = false;

static std::vector<int> cachedItemPids;
static bool cachedItemsInitialized = false;

static int rndex_exclude_pids[INV_MAX_EXCLUDE];
static int rndex_exclude_count = 0;
static int rndex_keep_pids[INV_MAX_EXCLUDE];
static int rndex_keep_count = 0;
static int rndex_once_pids[INV_MAX_EXCLUDE];
static int rndex_once_count = 0;

Config gkiosk_inv_config;

bool inv_conf_init()
{
    if (inv_conf_initialized) {
        return false;
    }

    if (!config_init(&gkiosk_inv_config)) {
        return false;
    }

    strcpy(inv_conf_file_name, INV_CONFIG_FILE_NAME);
    config_load(&gkiosk_inv_config, inv_conf_file_name, false);

    inv_conf_initialized = true;

    return true;
}

bool inv_conf_exit()
{
    if (!inv_conf_initialized) {
        return false;
    }

    config_exit(&gkiosk_inv_config);
    inv_conf_initialized = false;

    return true;
}

void inv_generate_items_for_value(Object* container, int targetValue)
{
    if (container == NULL || targetValue <= 0) {
        return;
    }

    init_cached_item_pids();

    if (cachedItemPids.empty()) {
        return;
    }

    std::vector<int> itemPids = cachedItemPids;

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(itemPids.begin(), itemPids.end(), g);

    int currentValue = 0;
    int maxItems = 8;
    if (maxItems > (int)itemPids.size()) {
        maxItems = itemPids.size();
    }

    for (int i = 0; i < maxItems && currentValue < targetValue; i++) {
        int pid = itemPids[i];
        Proto* proto;
        if (proto_ptr(pid, &proto) != 0) continue;

        int itemCost = proto->item.cost;
        if (itemCost == 0) continue;

        if (currentValue + itemCost > targetValue * 1.2) {
            continue;
        }

        Object* newItem = NULL;
        if (obj_pid_new(&newItem, pid) == 0 && newItem != NULL) {
            is_pid_once(pid);
            obj_disconnect(newItem, NULL);
            item_add_force(container, newItem, 1);
            currentValue += itemCost;
        }
    }
}

static bool rnd_item_conf_init()
{
    if (rndex_conf_initialized) {
        return true;
    }

    inv_conf_init();

    rndex_exclude_count = 0;
    rndex_keep_count = 0;
    rndex_once_count = 0;
    char key[8];
    int pid = 0;

    for (int i = 0; i < INV_MAX_EXCLUDE; i++) {
        snprintf(key, sizeof(key), "%d", i);

        pid = 0;
        if (config_get_value(&gkiosk_inv_config, INV_CONFIG_EXCLUDE_KEY, key, &pid)) {
            if (pid > 0) {
                rndex_exclude_pids[rndex_exclude_count++] = pid;
            }
        }

        pid = 0;
        if (config_get_value(&gkiosk_inv_config, INV_CONFIG_KEEP_KEY, key, &pid)) {
            if (pid > 0) {
                rndex_keep_pids[rndex_keep_count++] = pid;
            }
        }

        pid = 0;
        if (config_get_value(&gkiosk_inv_config, INV_CONFIG_ONCE_KEY, key, &pid)) {
            if (pid > 0) {
                rndex_once_pids[rndex_once_count++] = pid;
            }
        }
    }

    rndex_conf_initialized = true;
    return true;
}

static int is_pid_in_array(int pid, int pid_count, int aps[])
{
    if (!rndex_conf_initialized) {
        rnd_item_conf_init();
    }

    for (int i = 0; i < pid_count; i++) {
        if (aps[i] == pid) {
            return i;
        }
    }
    return -1;
}

static bool is_pid_excluded(int pid)
{
    return is_pid_in_array(pid, rndex_exclude_count, rndex_exclude_pids) >= 0 ? true : false;
}

bool should_keep_item(int pid)
{
    return is_pid_in_array(pid, rndex_exclude_count, rndex_exclude_pids) >= 0 ? true : false;
}

static bool is_pid_once(int pid)
{
    int index = is_pid_in_array(pid, rndex_once_count, rndex_once_pids);
    if (index < 0) {
        return false;
    }

    std::vector<int>::iterator it = std::find(cachedItemPids.begin(), cachedItemPids.end(), pid);
    cachedItemPids.erase(cachedItemPids.begin() + index);

    rndex_once_pids[index] = 0;

    return true;
}

static void init_cached_item_pids()
{
    if (cachedItemsInitialized) {
        return;
    }

    if (!rndex_conf_initialized) {
        rnd_item_conf_init();
    }

    for (int pid = 0; pid < 10000; pid++) {
        Proto* testProto;
        if (proto_ptr(pid, &testProto) == 0) {
            if (testProto->item.type >= ITEM_TYPE_WEAPON && testProto->item.type <= ITEM_TYPE_MISC) {
                if (testProto->item.cost > 0) {
                    if (!is_pid_excluded(pid)) {
                        cachedItemPids.push_back(pid);
                    }
                }
            }
        }
    }

    cachedItemsInitialized = true;
}

void gkioskinv_reset()
{
    inv_conf_exit();
    rndex_conf_initialized = false;
    cachedItemsInitialized = false;
    cachedItemPids.clear();
    rndex_exclude_count = 0;
    rndex_keep_count = 0;
    rndex_once_count = 0;
}

}

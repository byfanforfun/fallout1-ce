#ifndef FALLOUT_GAME_GKIOSKINV_H_
#define FALLOUT_GAME_GKIOSKINV_H_

#include "game/config.h"
#include "game/object.h"

namespace fallout {

#define INV_CONFIG_FILE_NAME "kiosk_inv.cfg"
#define INV_CONFIG_INVENTORY_KEY "inventory"
#define INV_CONFIG_EXCLUDE_KEY "exclude"
#define INV_CONFIG_KEEP_KEY "keep"
#define INV_CONFIG_ONCE_KEY "once"
#define INV_MAX_PROTO 255
#define INV_MAX_EXCLUDE 256

extern Config gkiosk_inv_config;

bool inv_conf_init();
bool inv_conf_exit();
void inv_generate_items_for_value(Object* container, int targetValue);
bool should_keep_item(int pid);
void gkioskinv_reset();

}

#endif // FALLOUT_GAME_GKIOSKINV_H_

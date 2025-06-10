#include "game/start_inv_v.h"

#include "game/gdialog.h"
#include "game/gkioskconf.h"
#include "game/inventry.h"
#include "game/item.h"
#include "game/palette.h"
#include "game/object.h"

#include "plib/color/color.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"

#include "plib/gnw/debug.h"

#include "platform_compat.h"

namespace fallout {

#define INV_CONFIG_FILE_NAME        "kiosk_inv.cfg"
#define INV_CONFIG_INVENTORY_KEY    "inventory"
#define INV_MAX_PROTO               255

static Object* inventory_objs[INV_MAX_PROTO] = {0};
static Object* inventory_holder = NULL;
static Config inv_config;

static char inv_conf_file_name[COMPAT_MAX_PATH];

static int inventory_proto[INV_MAX_PROTO] = {0};
static int caps_start = 0;
static int barter_mod = 0;

static bool cursorWasHidden = false;
static bool inv_conf_initialized = false;

static int start_init();
static int start_process();
static int start_exit();
static int create_holder();
static bool inv_conf_init();
static int inv_destroy();
static int clear_window(int window);

int start_inventory() {

    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_CAPS_START, &caps_start);
    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_BARTER_MOD, &barter_mod);

    if(1 > caps_start)
        return -1;

    if(start_init() != 0) {
        debug_printf("Failed to init start inventory.\n");
        return -1;
    }

    start_process();

    start_exit();

    return 0;
};

static int start_init() {
    item_caps_adjust(obj_dude, caps_start);

    loadColorTable("color.pal");
    palette_fade_to(cmap);

    cursorWasHidden = mouse_hidden();
    if (cursorWasHidden) {
        mouse_show();
    }

    if(create_holder() != 0)
        return -1;

    if(!inv_conf_init())
        return -1;

    inven_reset_dude();

    return 0;
}

static int start_process() {
    gDialogStartInventory(inventory_holder, barter_mod, DIALOG_BACKGROUND_HEIST);

    return 0;
}

static int start_exit() {
    int caps_total = item_caps_total(obj_dude);
    item_caps_adjust(obj_dude, -(caps_total));

    inv_destroy();

    if (cursorWasHidden) {
        mouse_hide();
    }

    palette_fade_to(black_palette);

    return 0;
}

static int create_holder()
{
    if(obj_pid_new(&inventory_holder, 100) == -1) {
        debug_printf("Failed to create inventory holder\n");
        return -1;
    }

    return 0;
}

static bool inv_conf_init()
{
    if (inv_conf_initialized) {
        return false;
    }

    if (!config_init(&inv_config)) {
        return false;
    }

    strcpy(inv_conf_file_name, INV_CONFIG_FILE_NAME);
    config_load(&inv_config, inv_conf_file_name, false);

    int proto_count = 0;
    char ti[4];
    for(int i = 0; INV_MAX_PROTO > i; ++i){
        snprintf(ti, 4, "%d", i);
        if(config_get_value(&inv_config, INV_CONFIG_INVENTORY_KEY, ti, &proto_count))
        {
            inventory_proto[i] = proto_count;
        }
    }

    Object *to;
    for(int i = 0; INV_MAX_PROTO > i; ++i){
        if(inventory_proto[i] != 0)
            if(obj_pid_new(&to, i) == 0){
                inventory_objs[i] = to;
                obj_disconnect(to, NULL);

                if(item_add_force(inventory_holder, to, inventory_proto[i]) != 0) {
                    obj_erase_object(to, NULL);
                    debug_printf("failed to add proto %d count %d.\n", i, inventory_proto[i]);
                }

                debug_printf("load proto %d count %d.\n", i, inventory_proto[i]);
            }
    }

    inv_conf_initialized = true;

    return true;
}

static int inv_destroy()
{
    for(int i = 0; INV_MAX_PROTO > i; ++i){
        if(inventory_proto[i] != 0)
            item_remove_mult(inventory_holder, inventory_objs[i], inventory_proto[i]);
    }

    if(inventory_holder != NULL) {
        obj_disconnect(inventory_holder, NULL);
        mem_free(inventory_holder);
    }

    return 0;
}


}
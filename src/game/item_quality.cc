#include "game/item_quality.h"

#include <stdlib.h>
#include <string.h>
#include <unordered_map>

#include "game/critter.h"
#include "game/gkioskconf.h"
#include "game/item.h"
#include "game/kiosk_msgfile.h"
#include "game/message.h"
#include "game/object.h"
#include "game/object_types.h"
#include "game/proto.h"
#include "game/proto_types.h"
#include "game/roll.h"
#include "game/skill.h"
#include "game/stat.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/memory.h"

#include "game/bmpdlog.h"
#include "game/map.h"
#include "platform_compat.h"
#include "plib/color/color.h"

namespace fallout {

#define QUALITY_MSG_BASE 1300
#define QUALITY_NAME_ERR "????. "

static bool items_quality_initialized = false;

void items_quality_init()
{
    if (items_quality_initialized) {
        return;
    }
    items_quality_initialized = true;
}

int get_quality_levels()
{
    if (!items_quality_initialized) {
        items_quality_init();
    }
    return gconfig_quality_levels;
}

double get_quality_modifier(int quality)
{
    if (!items_quality_initialized) {
        items_quality_init();
    }

    if (quality == ITEM_QUALITY_DEFAULT) {
        return 1.0;
    }

    if (quality < 0 || quality >= gconfig_quality_levels) {
        return 1.0;
    }

    return gconfig_quality_mods[quality];
}

const char* get_quality_name(int quality)
{
    if (!items_quality_initialized) {
        items_quality_init();
    }

    if (quality == ITEM_QUALITY_DEFAULT || quality < 0 || quality >= gconfig_quality_levels) {
        return "";
    }

    if (!kiosk_msgfile_initialized()) {
        return QUALITY_NAME_ERR;
    }

    MessageListItem mesg;
    mesg.num = QUALITY_MSG_BASE + quality;
    if (getmsg(&kiosk_msgfile, &mesg, mesg.num)) {
        return mesg.text;
    }

    return QUALITY_NAME_ERR;
}

int get_quality_for_object(Object* obj)
{
    if (!items_quality_initialized) {
        items_quality_init();
    }

    if (obj == NULL) {
        return ITEM_QUALITY_DEFAULT;
    }

    if (PID_TYPE(obj->pid) != OBJ_TYPE_ITEM) {
        return ITEM_QUALITY_DEFAULT;
    }

    return obj->quality;
}

int get_quality_for_npc(Object* npc)
{
    if (!items_quality_initialized) {
        items_quality_init();
    }

    if (npc == NULL) {
        return -1;
    }

    if (PID_TYPE(npc->pid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    int hp = critter_get_hits(npc);
    int levels = get_quality_levels();

    for (int i = 0; i < levels; i++) {
        if (hp < gconfig_quality_npc_hp[i]) {
            return i;
        }
    }

    return levels - 1;
}

int get_quality_for_ground(Object* obj)
{
    if (!items_quality_initialized) {
        items_quality_init();
    }

    if (gconfig_quality_levels <= 0) {
        return 2; // Standard
    }

    int roll = roll_random(1, 100);
    int cumulative = 0;

    for (int i = 0; i < gconfig_quality_levels; i++) {
        cumulative += gconfig_quality_ground_chance[i];
        if (roll <= cumulative) {
            return i;
        }
    }

    return gconfig_quality_levels - 1;
}

int get_quality_for_encounter()
{
    if (!items_quality_initialized) {
        items_quality_init();
    }

    if (gconfig_quality_levels <= 0) {
        return 2;
    }

    int roll = roll_random(1, 100);
    int cumulative = 0;

    for (int i = 0; i < gconfig_quality_levels; i++) {
        cumulative += gconfig_quality_encounter_chance[i];
        if (roll <= cumulative) {
            return i;
        }
    }

    return gconfig_quality_levels - 1;
}

int get_quality_cost(int baseCost, int quality)
{
    if (quality <= 0 || quality > gconfig_quality_levels) {
        return baseCost;
    }

    double mod = get_quality_modifier(quality);
    return (int)(baseCost * mod);
}

int get_ammo_quality(Object* weapon)
{
    int inx = (gconfig_quality_default_index < 0 ? ITEM_QUALITY_DEFAULT : gconfig_quality_default_index);

    if (weapon == NULL) {
        return inx;
    }

    if (PID_TYPE(weapon->pid) != OBJ_TYPE_ITEM) {
        return inx;
    }

    Proto* proto;
    if (proto_ptr(weapon->pid, &proto) != 0) {
        return inx;
    }

    if (proto->item.type != ITEM_TYPE_WEAPON) {
        return inx;
    }

    return weapon->data.item.weapon.ammoQuality;
}

void set_ammo_quality(Object* weapon, int quality)
{
    if (weapon == NULL) {
        return;
    }

    if (PID_TYPE(weapon->pid) != OBJ_TYPE_ITEM) {
        return;
    }

    Proto* proto;
    if (proto_ptr(weapon->pid, &proto) != 0) {
        return;
    }

    if (proto->item.type != ITEM_TYPE_WEAPON) {
        return;
    }

    if (quality < 0 || quality > gconfig_quality_levels) {
        quality = gconfig_quality_default_index;
    }

    weapon->data.item.weapon.ammoQuality = quality;
}

static void apply_quality_to_item(Object* item, int quality)
{
    if (item == NULL) return;
    if (quality == ITEM_QUALITY_DEFAULT) return;

    if (PID_TYPE(item->pid) != OBJ_TYPE_ITEM) {
        return;
    }

    Proto* proto;
    if (proto_ptr(item->pid, &proto) != 0) {
        return;
    }

    int item_type = proto->item.type;

    if (item_type != ITEM_TYPE_WEAPON && item_type != ITEM_TYPE_ARMOR && item_type != ITEM_TYPE_AMMO && item_type != ITEM_TYPE_DRUG) {
        return;
    }

    if (quality < 0 || quality >= gconfig_quality_levels) {
        quality = gconfig_quality_default_index;
    }

    item->quality = quality;
}

void apply_quality_to_item_normal(Object* item)
{
    apply_quality_to_item(item, gconfig_quality_default_index);
}

void apply_quality_to_ammo_normal(Object* item)
{
    set_ammo_quality(item, gconfig_quality_default_index);
}

static void process_inventory(Object* container, bool isEncounter)
{
    if (container == NULL) return;

    Inventory* inv = &(container->data.inventory);
    if (inv == NULL || inv->length == 0 || inv->items == NULL) return;

    Object* item = NULL;
    Proto* proto = NULL;

    int ammo_pid = 0;
    int ammo_quality = gconfig_quality_default_index;

    for (int i = 0; i < inv->length; i++) {
        item = inv->items[i].item;
        if (item == NULL) continue;

        if (PID_TYPE(item->pid) != OBJ_TYPE_ITEM) continue;

        if (proto_ptr(item->pid, &proto) != 0) continue;

        int quality = isEncounter ? get_quality_for_encounter() : get_quality_for_ground(NULL);
        int item_type = proto->item.type;

        if (item_type == ITEM_TYPE_WEAPON) {
            apply_quality_to_item(item, quality);

            ammo_pid = item->data.item.weapon.ammoTypePid;
            if (ammo_pid != 0) {
                ammo_quality = quality;
                set_ammo_quality(item, ammo_quality);
            }
        } else if (item_type == ITEM_TYPE_ARMOR || item_type == ITEM_TYPE_AMMO || item_type == ITEM_TYPE_DRUG) {
            apply_quality_to_item(item, quality);
        }
    }
}

void items_apply_quality_on_map()
{
    if (!items_quality_initialized) {
        items_quality_init();
    }

    if (gconfig_quality_levels <= 0) {
        return;
    }

    bool isEncounter = false;
    {
        const char* name = map_data.name;
        if (name != NULL && name[0] != '\0') {
            if (strncmp(name, "DESERT", 6) == 0 || strncmp(name, "MOUNTN", 6) == 0 || strncmp(name, "CITY", 4) == 0 || strncmp(name, "COAST", 5) == 0) {
                isEncounter = true;
            }
        }
    }

    for (Object* obj = obj_find_first(); obj != NULL; obj = obj_find_next()) {
        if (obj == obj_dude) continue;

        if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
            process_inventory(obj, isEncounter);
        } else if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            Proto* proto;
            if (proto_ptr(obj->pid, &proto) != 0) continue;

            if (proto->item.type == ITEM_TYPE_CONTAINER) {
                process_inventory(obj, isEncounter);
            } else {
                Object* owner = obj->owner;
                if (owner == NULL || owner == obj) {
                    int quality = isEncounter ? get_quality_for_encounter() : get_quality_for_ground(NULL);
                    apply_quality_to_item(obj, quality);
                }
            }
        }
    }
}

static char quality_name_buffer[256] = { 0 };

char* items_get_item_name(Object* obj)
{
    if (!items_quality_initialized) {
        items_quality_init();
    }

    if (obj == NULL) {
        return proto_name(-1);
    }

    if (gconfig_quality_levels <= 0) {
        return proto_name(obj->pid);
    }

    Proto* proto;
    if (proto_ptr(obj->pid, &proto) != 0) {
        return proto_name(obj->pid);
    }

    if (proto->item.type != ITEM_TYPE_WEAPON && proto->item.type != ITEM_TYPE_ARMOR && proto->item.type != ITEM_TYPE_AMMO && proto->item.type != ITEM_TYPE_DRUG) {
        return proto_name(obj->pid);
    }

    int quality = obj->quality;

    if (quality == ITEM_QUALITY_DEFAULT || quality < 0 || quality >= gconfig_quality_levels) {
        return proto_name(obj->pid);
    }

    const char* original_name = proto_name(obj->pid);
    if (original_name == NULL) {
        return proto_name(obj->pid);
    }

    const char* prefix = get_quality_name(quality);
    if (prefix == NULL || prefix[0] == '\0') {
        return (char*)original_name;
    }

    snprintf(quality_name_buffer, sizeof(quality_name_buffer), "%s%s", prefix, original_name);
    return quality_name_buffer;
}

char* get_weapon_ammo_name(Object* weapon)
{
    if (!items_quality_initialized) {
        items_quality_init();
    }

    if (weapon == NULL) {
        return (char*)"";
    }

    int ammoPid = item_w_ammo_pid(weapon);
    if (ammoPid == -1 || ammoPid == 0) {
        return (char*)"";
    }

    char* original_name = proto_name(ammoPid);

    if (gconfig_quality_levels <= 0) {
        if (ammoPid == -1 || ammoPid == 0) {
            return (char*)"";
        }
        return original_name;
    }

    int ammoQuality = get_ammo_quality(weapon);
    if (ammoQuality == ITEM_QUALITY_DEFAULT || ammoQuality < 0) {
        if (ammoPid == -1 || ammoPid == 0) {
            return (char*)"";
        }
        return (char*)proto_name(ammoPid);
    }

    const char* prefix = get_quality_name(ammoQuality);
    if (prefix == NULL || prefix[0] == '\0') {
        return (char*)original_name;
    }

    snprintf(quality_name_buffer, sizeof(quality_name_buffer), "%s%s", prefix, original_name);
    return quality_name_buffer;
}

} // namespace fallout

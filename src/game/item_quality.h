#ifndef FALLOUT_GAME_GKIOSKITEM_H_
#define FALLOUT_GAME_GKIOSKITEM_H_

#include "game/object.h"

namespace fallout {

#define ITEM_QUALITY_DEFAULT -1

extern int gconfig_quality_default_index;

void quality_var_init();
void items_quality_init();
int get_quality_levels();
int get_quality_for_object(Object* obj);
int get_quality_for_npc(Object* npc);
int get_quality_for_ground(Object* obj);
double get_quality_modifier(int quality);
const char* get_quality_name(int quality);
int get_quality_cost(int baseCost, int quality);
int get_ammo_quality(Object* weapon);
void set_ammo_quality(Object* weapon, int quality);
char* get_weapon_ammo_name(Object* weapon);
void items_apply_quality_on_map();
char* items_get_item_name(Object* obj);

void apply_quality_to_item_normal(Object* item);
void apply_quality_to_ammo_normal(Object* item);

} // namespace fallout

#endif /* FALLOUT_GAME_GKIOSKITEM_H_ */

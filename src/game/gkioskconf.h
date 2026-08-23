#ifndef FALLOUT_GAME_GKIOSKCONF_H_
#define FALLOUT_GAME_GKIOSKCONF_H_

#include "game/config.h"

namespace fallout {

#define KIOSK_CONFIG_FILE_NAME "kiosk.cfg"

#define KIOSK_CONFIG_GAME_KEY "game"
#define KIOSK_CONFIG_EXP_START_KEY "exp_start"
#define KIOSK_CONFIG_DISABLE_SAVELOAD "disable_saveload"
#define KIOSK_CONFIG_DISABLE_OPTIONS "disable_options"
#define KIOSK_CONFIG_ENABLE_DIALOG_EXIT_0 "dialog_exit_0"
#define KIOSK_CONFIG_ENABLE_GAME_EXIT "game_exit"
#define KIOSK_CONFIG_TIMER_INACTIVE_1 "inact1"
#define KIOSK_CONFIG_TIMER_INACTIVE_2 "inact2"
#define KIOSK_CONFIG_TIMER_INACTIVE_3 "inact3"
#define KIOSK_CONFIG_TIMER_INACTIVE_F "inact"
#define KIOSK_CONFIG_START_MESSAGE "start_message"
#define KIOSK_CONFIG_OVERRIDE_KEY "overrides"
#define KIOSK_CONFIG_OVERRIDE_GAME "game_difficulty"
#define KIOSK_CONFIG_OVERRIDE_COMBAT "combat_difficulty"
#define KIOSK_CONFIG_OVERRIDE_LFILTER "language_filter"
#define KIOSK_CONFIG_CAPS_START "caps_start"
#define KIOSK_CONFIG_BARTER_MOD "barter_mod"
#define KIOSK_CONFIG_SCREENSAVER_ENABLED "screensaver_enabled"
#define KIOSK_CONFIG_SCREENSAVER_TIMEOUT "screensaver_timeout"
#define KIOSK_CONFIG_RANDOM_LOCATIONS "random_locations"
#define KIOSK_CONFIG_RANDOM_CONTAINERS "random_containers"
#define KIOSK_CONFIG_RANDOM_CONTAINERS_BASE_CHANCE "random_containers_base_chance"
#define KIOSK_CONFIG_RANDOM_CONTAINERS_LUCK_FACTOR "random_containers_luck_factor"
#define KIOSK_CONFIG_RANDOM_CONTAINERS_BARTER_FACTOR "random_containers_barter_factor"
#define KIOSK_CONFIG_QUALITY_ITEM_MOD "quality_total"
#define KIOSK_CONFIG_QUALITY_MOD_BASE "quality_mod_"
#define KIOSK_CONFIG_QUALITY_NPC_HP_BASE "quality_npc_hp_"
#define KIOSK_CONFIG_QUALITY_GROUND_BASE "quality_ground_"
#define KIOSK_CONFIG_QUALITY_ENCOUNTER_BASE "quality_encounter_"
#define KIOSK_CONFIG_DEBUG_SPAWN_DRUGS "debug_spawn_drugs"
#define MAX_QUALITY_LEVELS 10

extern Config kiosk_config;
extern int gconfig_exp_start;
extern int gconfig_caps_start;
extern int gconfig_saveload_disabled;
extern int gconfig_options_disabled;
extern int gconfig_dialog_exit_0_allowed;
extern int gconfig_game_exit_allowed;
extern int gconfig_screensaver_enabled;
extern int gconfig_screensaver_timeout;
extern int gconfig_random_locations;
extern int gconfig_random_containers;
extern int gconfig_random_containers_base_chance;
extern int gconfig_random_containers_luck_factor;
extern int gconfig_random_containers_barter_factor;
extern int gconfig_quality_levels;
extern int gconfig_quality_default_index;
extern double gconfig_quality_mods[MAX_QUALITY_LEVELS + 1];
extern int gconfig_quality_npc_hp[MAX_QUALITY_LEVELS + 1];
extern int gconfig_quality_ground_chance[MAX_QUALITY_LEVELS + 1];
extern int gconfig_quality_encounter_chance[MAX_QUALITY_LEVELS + 1];
extern int gconfig_debug_spawn_drugs;

bool gkioskconf_init();
bool gkioskconf_save();
bool gkioskconf_exit(bool shouldSave);

} // namespace fallout

#endif /* FALLOUT_GAME_GKIOSKCONF_H_ */

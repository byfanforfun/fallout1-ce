#ifndef FALLOUT_GAME_GAMCONF_H_
#define FALLOUT_GAME_GAMCONF_H_

#include "game/config.h"

namespace fallout {

// The file name of the Android mod config file. Stored next to `fallout.cfg`
// and `f1_res.ini`.
#define GAM_CONFIG_FILE_NAME "f1_am.ini"

// Keys of the `[hud]` section.
#define GAM_CONFIG_HUD_KEY "hud"
#define GAM_CONFIG_HUD_TYPE_KEY "type"
#define GAM_CONFIG_HUD_SCALE_KEY "scale"
#define GAM_CONFIG_HUD_OPACITY_KEY "opacity"

// Keys of the `[scroll]` section.
#define GAM_CONFIG_SCROLL_KEY "scroll"
#define GAM_CONFIG_SCROLL_ENABLED_KEY "enabled"
#define GAM_CONFIG_SCROLL_SIZE_KEY "size"
#define GAM_CONFIG_SCROLL_OFFSET_X_KEY "offset_x"
#define GAM_CONFIG_SCROLL_OFFSET_Y_KEY "offset_y"

// Keys of the `[actions]` section.
#define GAM_CONFIG_ACTIONS_KEY "actions"
#define GAM_CONFIG_ACTIONS_ENABLED_KEY "enabled"
#define GAM_CONFIG_ACTIONS_SIZE_KEY "size"
#define GAM_CONFIG_ACTIONS_OFFSET_X_KEY "offset_x"
#define GAM_CONFIG_ACTIONS_OFFSET_Y_KEY "offset_y"
#define GAM_CONFIG_ACTIONS_GAP_KEY "gap"

// Number of configurable action slots on the right side of the screen.
#define GAM_CONFIG_ACTION_SLOTS 12

// Keys of the `[actions]` section, per-slot keycodes. Corresponding actions:
// 'a' attack, 'n' cycle attack mode, 'm' change cursor, 'b' switch hand,
// 'i' inventory, 'c' character, 'p' pipboy, TAB automap, 's' skilldex,
// '1'-'8' skill slots.
#define GAM_CONFIG_ACTION_SLOT_KEYS                       \
    "slot0", "slot1", "slot2", "slot3", "slot4", "slot5", \
        "slot6", "slot7", "slot8", "slot9", "slot10", "slot11"

extern Config gam_config;

// HUD visibility mode: 0 - disabled, 1 - always visible, 2 - smart
// (overlay fades out when untouched).
extern int gconfig_hud_type;
extern double gconfig_hud_scale;
extern int gconfig_hud_opacity;

extern int gconfig_scroll_enabled;
extern int gconfig_scroll_size;
extern int gconfig_scroll_offset_x;
extern int gconfig_scroll_offset_y;

extern int gconfig_actions_enabled;
extern int gconfig_actions_size;
extern int gconfig_actions_offset_x;
extern int gconfig_actions_offset_y;
extern int gconfig_actions_gap;
extern int gconfig_action_slots[GAM_CONFIG_ACTION_SLOTS];

bool gamconf_init();
bool gamconf_save();
bool gamconf_exit(bool shouldSave);

} // namespace fallout

#endif /* FALLOUT_GAME_GAMCONF_H_ */
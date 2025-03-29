#ifndef FALLOUT_GAME_CHARDUMP_H_
#define FALLOUT_GAME_CHARDUMP_H_

//#include <stdio.h>
//#include <stddef.h>

//#include "platform_compat.h"
//#include "game/object.h"

namespace fallout {

#define CHAR_MAX_DATE                       16
#define CHAR_MAX_PERKS                      512
#define CHAR_MAX_KILLS                      512

#define CHAR_CONFIG_DIR                     "chars/"

#define CHAR_CONFIG_CHAR_KEY	            "char"
#define CHAR_CONFIG_CHAR_NAME_KEY           "name"
#define CHAR_CONFIG_CHAR_STATUS_KEY         "status"
#define CHAR_CONFIG_CHAR_KARMA_KEY          "karma"
#define CHAR_CONFIG_CHAR_LVL_KEY            "level"
#define CHAR_CONFIG_CHAR_EXP_KEY            "exp"
#define CHAR_CONFIG_CHAR_KILLER_KEY         "death_cause"
#define CHAR_CONFIG_CHAR_LIFETIME_KEY       "lifetime"
#define CHAR_CONFIG_CHAR_TRAIT_1_KEY        "trait1"
#define CHAR_CONFIG_CHAR_TRAIT_2_KEY        "trait2"

#define CHAR_CONFIG_QUEST_KEY               "quest"
#define CHAR_CONFIG_QUEST_CHIP_KEY          "found_chip"
#define CHAR_CONFIG_QUEST_CATH_KEY          "destroy_cathedral"
#define CHAR_CONFIG_QUEST_BASE_KEY          "destroy_base"
#define CHAR_CONFIG_QUEST_ARMY_KEY          "join_master"

#define CHAR_CONFIG_STAT_KEY	            "stat"
#define CHAR_CONFIG_STAT_STR_KEY            "str"
#define CHAR_CONFIG_STAT_PER_KEY            "per"
#define CHAR_CONFIG_STAT_END_KEY            "end"
#define CHAR_CONFIG_STAT_CHA_KEY            "cha"
#define CHAR_CONFIG_STAT_INT_KEY            "int"
#define CHAR_CONFIG_STAT_AGL_KEY            "agl"
#define CHAR_CONFIG_STAT_LUK_KEY            "luk"

#define CHAR_CONFIG_STAT_STR_B_KEY          "strb"
#define CHAR_CONFIG_STAT_PER_B_KEY          "perb"
#define CHAR_CONFIG_STAT_END_B_KEY          "endb"
#define CHAR_CONFIG_STAT_CHA_B_KEY          "chab"
#define CHAR_CONFIG_STAT_INT_B_KEY          "intb"
#define CHAR_CONFIG_STAT_AGL_B_KEY          "aglb"
#define CHAR_CONFIG_STAT_LUK_B_KEY          "lukb"

#define CHAR_CONFIG_TAGS_KEY                "tags"
#define CHAR_CONFIG_TAGS_TAG_1_KEY          "tag1"
#define CHAR_CONFIG_TAGS_TAG_2_KEY          "tag2"
#define CHAR_CONFIG_TAGS_TAG_3_KEY          "tag3"
#define CHAR_CONFIG_TAGS_TAG_4_KEY          "tag4"

#define CHAR_CONFIG_PERKS_KEY               "perks"
#define CHAR_CONFIG_PERKS_GAINED_KEY        "gained"
#define CHAR_CONFIG_PERKS_KARMA_KEY         "karma"

#define CHAR_CONFIG_OTHER_KEY               "other"
#define CHAR_CONFIG_CHAR_KILLED_KEY         "killed"

#define CHAR_CONFIG_INV_KEY                 "inv"
#define CHAR_CONFIG_INV_ARMOR_KEY           "armor"
#define CHAR_CONFIG_INV_HAND_1_KEY          "hand1"
#define CHAR_CONFIG_INV_HAND_2_KEY          "hand2"

#define CHAR_CONFIG_QUEST_KEY               "quest"
#define CHAR_CONFIG_QUEST_FOUND_CHIP_KEY    "chip"
#define CHAR_CONFIG_QUEST_KILL_MASTER_KEY   "master"
#define CHAR_CONFIG_QUEST_DESTROY_VATS_KEY  "vats"
#define CHAR_CONFIG_QUEST_JOIN_MASTER_KEY   "join"


int char_dir_create();
int char_dump();
}

#endif /* FALLOUT_GAME_CHARDUMP_H_ */
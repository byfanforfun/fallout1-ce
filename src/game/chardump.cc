#include "game/chardump.h"

#include <ctime>
#include <stdio.h>
#include <string.h>

#include "platform_compat.h"
#include "game/config.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/editor.h"
#include "game/game.h"
#include "game/game_vars.h"
#include "game/gmovie.h"
#include "game/inventry.h"
#include "game/item.h"
#include "game/kiosk_msgfile.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/proto_types.h"
#include "game/scripts.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/trait.h"

#include "plib/gnw/hash_fnv-1a.h"
#include "plib/gnw/system_exec.h"

namespace fallout {

unsigned int last_char_hash = 0;

unsigned int get_last_char_hash() {
    return last_char_hash;
}

unsigned int set_last_char_hash(unsigned int hash) {
    last_char_hash = hash;
    return last_char_hash;
}

unsigned int flush_last_char_hash() {
    set_last_char_hash(0);
    return last_char_hash;
}

int char_dir_create()
{
    if (compat_stat(CHAR_CONFIG_DIR) == -1) {
        return compat_mkdir(CHAR_CONFIG_DIR);
    }

    return -1;
}

int char_dump()
{
    int exp = stat_pc_get(PC_STAT_EXPERIENCE);
    if(exp == 0)
        return -2;

    int l[2] = {0, 1};
    system_exec(l);

    Config char_config;
    static MessageListItem mesg;

    const char* char_name = critter_name(obj_dude);
    char file_name[COMPAT_MAX_PATH];

    snprintf(file_name, COMPAT_MAX_PATH, "%s%lu%s%s%s", CHAR_CONFIG_DIR, (unsigned long)time(NULL), "-", char_name, ".char");

    if (!config_init(&char_config)) {
        return -1;
    }

    set_last_char_hash(fnv1a_hash(file_name, strlen(file_name)));

    //quests
    int gvar_value = 0;
    gvar_value = (game_get_global_var(GVAR_MASTER_DEAD) + game_get_global_var(GVAR_MASTER_BLOWN));
    if(gvar_value > 0)
        config_set_value(&char_config, CHAR_CONFIG_QUEST_KEY, CHAR_CONFIG_QUEST_KILL_MASTER_KEY, 1);

    gvar_value = (game_get_global_var(GVAR_VATS_STATUS) + game_get_global_var(GVAR_VATS_BLOWN));
    if(gvar_value > 0)
        config_set_value(&char_config, CHAR_CONFIG_QUEST_KEY, CHAR_CONFIG_QUEST_DESTROY_VATS_KEY, 1);

    //2 == took to the vault
    gvar_value = game_get_global_var(GVAR_FIND_WATER_CHIP);
    if(gvar_value > 1)
        config_set_value(&char_config, CHAR_CONFIG_QUEST_KEY, CHAR_CONFIG_QUEST_FOUND_CHIP_KEY, 1);


    gvar_value = game_get_global_var(GVAR_VAULT_WATER);
    //gmovie defined global variable
    if(1 >= gvar_value || dude_end_story_status == 1)
        death_cause = getmsg(&kiosk_msgfile, &mesg, 1220);

    if(dude_end_story_status == 2)
        death_cause = getmsg(&kiosk_msgfile, &mesg, 1221);

    if(dude_end_story_status == 3){
        config_set_value(&char_config, CHAR_CONFIG_QUEST_KEY, CHAR_CONFIG_QUEST_JOIN_MASTER_KEY, 1);
        death_cause = getmsg(&kiosk_msgfile, &mesg, 1222);
    }

    //char
    config_set_string(&char_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_NAME_KEY, char_name);
    config_set_value(&char_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_LVL_KEY, stat_pc_get(PC_STAT_LEVEL));
    config_set_value(&char_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_EXP_KEY, exp);

    gvar_value = game_get_global_var(GVAR_PLAYER_REPUATION);
    config_set_value(&char_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_KARMA_KEY, gvar_value);

    int trait_1, trait_2  = 0;
    trait_get(&trait_1, &trait_2);

    if(trait_1 != 0)
        config_set_string(&char_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_TRAIT_1_KEY, trait_name(trait_1));
    if(trait_2 != 0)
        config_set_string(&char_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_TRAIT_2_KEY, trait_name(trait_2));

    if(strlen(death_cause) > 0)
        config_set_string(&char_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_KILLER_KEY, death_cause);

    int day;
    int month;
    int year;

    game_time_date(&month, &day, &year);

    //2161 12 5

    day -= 5;
    month -= 12;
    year -= 2161;

    char date[CHAR_MAX_DATE];
    snprintf(date, CHAR_MAX_DATE, "%d %d %d", year, month, day);
    config_set_string(&char_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_LIFETIME_KEY, date);

    //inv
    Object* item = NULL;

    item = inven_worn(obj_dude);
    if(item != NULL)
        config_set_string(&char_config, CHAR_CONFIG_INV_KEY, CHAR_CONFIG_INV_ARMOR_KEY, item_name(item));

    item = inven_left_hand(obj_dude);
    if(item != NULL)
        config_set_string(&char_config, CHAR_CONFIG_INV_KEY, CHAR_CONFIG_INV_HAND_1_KEY, item_name(item));

    item = inven_right_hand(obj_dude);
    if(item != NULL)
        config_set_string(&char_config, CHAR_CONFIG_INV_KEY, CHAR_CONFIG_INV_HAND_2_KEY, item_name(item));

    //stat
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_STR_KEY, stat_get_base(obj_dude, STAT_STRENGTH));
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_PER_KEY, stat_get_base(obj_dude, STAT_PERCEPTION));
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_END_KEY, stat_get_base(obj_dude, STAT_ENDURANCE));
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_CHA_KEY, stat_get_base(obj_dude, STAT_CHARISMA));
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_INT_KEY, stat_get_base(obj_dude, STAT_INTELLIGENCE));
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_AGL_KEY, stat_get_base(obj_dude, STAT_AGILITY));
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_LUK_KEY, stat_get_base(obj_dude, STAT_LUCK));

    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_STR_B_KEY, stat_get_bonus(obj_dude, STAT_STRENGTH));
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_PER_B_KEY, stat_get_bonus(obj_dude, STAT_PERCEPTION));
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_END_B_KEY, stat_get_bonus(obj_dude, STAT_ENDURANCE));
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_CHA_B_KEY, stat_get_bonus(obj_dude, STAT_CHARISMA));
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_INT_B_KEY, stat_get_bonus(obj_dude, STAT_INTELLIGENCE));
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_AGL_B_KEY, stat_get_bonus(obj_dude, STAT_AGILITY));
    config_set_value(&char_config, CHAR_CONFIG_STAT_KEY, CHAR_CONFIG_STAT_LUK_B_KEY, stat_get_bonus(obj_dude, STAT_LUCK));

    //tags
    int skills[NUM_TAGGED_SKILLS];
    skill_get_tags(skills, NUM_TAGGED_SKILLS);

    config_set_string(&char_config, CHAR_CONFIG_TAGS_KEY, CHAR_CONFIG_TAGS_TAG_1_KEY, skill_name(skills[0]));
    config_set_string(&char_config, CHAR_CONFIG_TAGS_KEY, CHAR_CONFIG_TAGS_TAG_2_KEY, skill_name(skills[1]));
    config_set_string(&char_config, CHAR_CONFIG_TAGS_KEY, CHAR_CONFIG_TAGS_TAG_3_KEY, skill_name(skills[2]));
    if(skills[3] != 0)
        config_set_string(&char_config, CHAR_CONFIG_TAGS_KEY, CHAR_CONFIG_TAGS_TAG_4_KEY, skill_name(skills[3]));

    //perks
    int t_count = 0;
    char* t_name;
    char names[CHAR_MAX_PERKS];
    char buffer[CHAR_MAX_PERKS];

    static MessageList editor_message_file;
    char path[COMPAT_MAX_PATH];

    memset(&names[0], 0, sizeof(names));
    if(message_init(&editor_message_file)) {
        snprintf(path, sizeof(path), "%s%s", msg_path, "editor.msg");

        if (message_load(&editor_message_file, path)) {
            char *t_sex = getmsg(&editor_message_file, &mesg, 107 + stat_level(obj_dude, STAT_GENDER));
            config_set_string(&char_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_SEX_KEY, t_sex);

            for(int index = 0; index < 9; index++) {
                if (game_get_global_var(karma_var_table[index]) > 0) {
                    t_name = getmsg(&editor_message_file, &mesg, 1001 + index);
                    snprintf(buffer, CHAR_MAX_PERKS, "%s\n%s", names, t_name);
                    strcpy(names, buffer);
                }
            }
        }

        message_exit(&editor_message_file);
    }

    if(strlen(names) > 0)
        config_set_string(&char_config, CHAR_CONFIG_PERKS_KEY, CHAR_CONFIG_PERKS_KARMA_KEY, names);

    memset(&names[0], 0, sizeof(names));
    for(int i = 0; PERK_COUNT > i; ++i){
        t_count = perk_level(i);
        if(t_count > 0){
            t_name = perk_name(i);
            snprintf(buffer, CHAR_MAX_PERKS, "%s\n%s (%d)", names, t_name, t_count);
            strcpy(names, buffer);
        }
    }

    if(strlen(names) > 0)
        config_set_string(&char_config, CHAR_CONFIG_PERKS_KEY, CHAR_CONFIG_PERKS_GAINED_KEY, names);

    //killed
    memset(&names[0], 0, sizeof(names));
    memset(&buffer[0], 0, sizeof(buffer));
    for(int i = 0; KILL_TYPE_COUNT > i; ++i){
        t_count = critter_kill_count(i);
        if(t_count > 0){
            t_name = critter_kill_name(i);
            snprintf(buffer, CHAR_MAX_PERKS, "%s\n%s (%d)", names, t_name, t_count);
            strcpy(names, buffer);
        }
    }

    if(strlen(names) > 0)
        config_set_string(&char_config, CHAR_CONFIG_OTHER_KEY, CHAR_CONFIG_CHAR_KILLED_KEY, names);

    if (!config_save(&char_config, static_cast<char*>(file_name), false)) {
        return -2;
    }

    return 0;
}

} // namespace fallout

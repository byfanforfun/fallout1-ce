#include "game/timer_inactive.h"

#include <string.h>
#include "platform_compat.h"
#include "game/bmpdlog.h"
#include "game/critter.h"
#include "game/chardump.h"
#include "game/game.h"
#include "game/gkioskconf.h"
#include "game/kiosk_msgfile.h"
#include "game/object.h"
#include "game/tile.h"

#include "plib/color/color.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/timer.h"

namespace fallout {

#define KIOSK_MSG_TIME_BEFORE_END       999
#define KIOSK_MSG_ATTENTION_1ST_START   1000
#define KIOSK_MSG_ATTENTION_1ST_END     1020
#define KIOSK_MSG_ATTENTION_2ND_START   1050
#define KIOSK_MSG_ATTENTION_2ND_END     1070
#define KIOSK_MSG_ATTENTION_3RD_START   1100
#define KIOSK_MSG_ATTENTION_3RD_END     1120

static MessageList km;
static MessageListItem kmsg;

static int kiosk_msg_reinit = 0;

int timer_death = 0;
int timer_att_1 = 0;
int timer_att_2 = 0;
int timer_att_3 = 0;

int inactive_death(int a1, int a2, int a3)
{
    death_cause = "AFK";
    char_dump();
    main_show_death_scene = 1;
    game_user_wants_to_quit = 2;

    return 0;
}

int inactive_attention(int a1, int a2, int a3)
{
    if(kiosk_msg_reinit == 0){
        kiosk_load_msg();
        kiosk_msg_reinit = 1;
    }

    if(!kiosk_msgfile_initialized()){
        debug_printf("not initialized msg kiosk");
        return -1;
    }

    tile_set_center(obj_dude->tile, TILE_SET_CENTER_REFRESH_WINDOW);

    char* tm;
    char dm[160];
    char msg[260];

    tm = getmsg(&kiosk_msgfile, &kmsg, KIOSK_MSG_TIME_BEFORE_END);
    snprintf(dm, 160, tm, a1, "");

    int index = a2 + rand() % (a3-a2+1);
    if(index > a3 || a2 > index)
        index = a2;

    tm = getmsg(&kiosk_msgfile, &kmsg, index);
    strcpy(msg, tm);

    const char* a[] = {  dm };
    dialog_out(msg, a, 1, 169, 116, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_LARGE);

    return 0;
}

int timer_inactive_init()
{
    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_TIMER_INACTIVE_F, &timer_death);
    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_TIMER_INACTIVE_1, &timer_att_1);
    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_TIMER_INACTIVE_2, &timer_att_2);
    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_TIMER_INACTIVE_3, &timer_att_3);

    if(1 > timer_death)
        return 0;

    timer_create(TIMER_INACTIVE_DEATH, &inactive_death, timer_death, 0, 0, 0);
    timer_create(TIMER_INACTIVE_ATT_1, &inactive_attention, timer_death-timer_att_1, timer_att_1, KIOSK_MSG_ATTENTION_1ST_START, KIOSK_MSG_ATTENTION_1ST_END);
    timer_create(TIMER_INACTIVE_ATT_2, &inactive_attention, timer_death-timer_att_2, timer_att_2, KIOSK_MSG_ATTENTION_2ND_START, KIOSK_MSG_ATTENTION_2ND_END);
    timer_create(TIMER_INACTIVE_ATT_3, &inactive_attention, timer_death-timer_att_3, timer_att_3, KIOSK_MSG_ATTENTION_3RD_START, KIOSK_MSG_ATTENTION_3RD_END);

    return 0;
}

}
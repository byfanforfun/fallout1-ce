#include "game/timer_inactive.h"

#include <string.h>

#include "game/bmpdlog.h"
#include "game/critter.h"
#include "game/chardump.h"
#include "game/game.h"
#include "game/gkioskconf.h"
#include "game/object.h"
#include "game/tile.h"

#include "plib/color/color.h"
#include "plib/gnw/timer.h"

#include "platform_compat.h"

namespace fallout {

static MessageList kiosk_msg;
static MessageListItem msg;

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
    char msg[160];

    tile_set_center(obj_dude->tile, TILE_SET_CENTER_REFRESH_WINDOW);

    snprintf(msg, 160, "%s %d seconds!\n%s", "I will die in", a1, "Save me! Do something!");
    dialog_out(msg, 0, 0, 169, 117, colorTable[32328], NULL, colorTable[32328], 0);

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
    timer_create(TIMER_INACTIVE_ATT_1, &inactive_attention, timer_death-timer_att_1, timer_att_1, 1000, 21);
    timer_create(TIMER_INACTIVE_ATT_2, &inactive_attention, timer_death-timer_att_2, timer_att_2, 1050, 21);
    timer_create(TIMER_INACTIVE_ATT_3, &inactive_attention, timer_death-timer_att_3, timer_att_3, 1100, 21);

    return 0;
}

}
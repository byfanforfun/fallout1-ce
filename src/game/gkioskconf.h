#ifndef FALLOUT_GAME_GKIOSKCONF_H_
#define FALLOUT_GAME_GKIOSKCONF_H_

#include "game/config.h"

namespace fallout {

#define KIOSK_CONFIG_FILE_NAME "kiosk.cfg"

#define KIOSK_CONFIG_GAME_KEY	"game"
#define KIOSK_CONFIG_EXP_START_KEY "exp_start"
#define KIOSK_CONFIG_ENABLE_SAVELOAD   "saveload"
#define KIOSK_CONFIG_ENABLE_OPTIONS   "options"
#define KIOSK_CONFIG_TIMER_INACTIVE_1   "inact1"
#define KIOSK_CONFIG_TIMER_INACTIVE_2   "inact2"
#define KIOSK_CONFIG_TIMER_INACTIVE_3   "inact3"
#define KIOSK_CONFIG_TIMER_INACTIVE_F   "inact"

extern Config kiosk_config;
extern int gconfig_saveload_allowed;
extern int gconfig_options_allowed;

bool gkioskconf_init();
bool gkioskconf_save();
bool gkioskconf_exit(bool shouldSave);

} // namespace fallout

#endif /* FALLOUT_GAME_GKIOSKCONF_H_ */

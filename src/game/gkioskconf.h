#ifndef FALLOUT_GAME_GKIOSKCONF_H_
#define FALLOUT_GAME_GKIOSKCONF_H_

#include "game/config.h"

namespace fallout {

#define KIOSK_CONFIG_FILE_NAME "kiosk.cfg"

#define KIOSK_CONFIG_GAME_KEY	"game"
#define KIOSK_CONFIG_EXP_START_KEY "exp_start"

extern Config kiosk_config;

bool gkioskconf_init();
bool gkioskconf_save();
bool gkioskconf_exit(bool shouldSave);

} // namespace fallout

#endif /* FALLOUT_GAME_GKIOSKCONF_H_ */

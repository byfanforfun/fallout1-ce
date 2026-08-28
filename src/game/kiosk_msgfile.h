#include "game/message.h"

namespace fallout {

// continue-play main menu labels (kiosk.msg).
#define KIOSK_MSG_MENU_BEST 300
#define KIOSK_MSG_MENU_CONTINUE 301

// save name short skill names (kiosk.msg), one per skill, SKILL_0..SKILL_17.
#define KIOSK_MSG_SAVE_SKILL_FIRST 310
// save name level word (kiosk.msg).
#define KIOSK_MSG_SAVE_LEVEL_WORD 318

extern MessageList kiosk_msgfile;

bool kiosk_msgfile_initialized();
int kiosk_load_msg();
void kiosk_msgfile_exit();
}
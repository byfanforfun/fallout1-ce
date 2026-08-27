#include "game/message.h"

namespace fallout {

// continue-play main menu labels (kiosk.msg).
#define KIOSK_MSG_MENU_BEST 300
#define KIOSK_MSG_MENU_CONTINUE 301

extern MessageList kiosk_msgfile;

bool kiosk_msgfile_initialized();
int kiosk_load_msg();
void kiosk_msgfile_exit();
}
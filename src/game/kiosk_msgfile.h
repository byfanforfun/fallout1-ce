#include "game/message.h"

namespace fallout {
extern MessageList kiosk_msgfile;

bool kiosk_msgfile_initialized();
int kiosk_load_msg();
void kiosk_msgfile_exit();
}
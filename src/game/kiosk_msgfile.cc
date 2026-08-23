#include "game/kiosk_msgfile.h"

#include "game/game.h"
#include "game/message.h"

#include "platform_compat.h"

namespace fallout {

MessageList kiosk_msgfile;
int kiosk_msgfile_init = 0;

int kiosk_load_msg()
{
    if (!message_init(&kiosk_msgfile))
        return -1;

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", msg_path, "kiosk.msg");

    if (!message_load(&kiosk_msgfile, path))
        return -1;

    kiosk_msgfile_init = 1;
    return 0;
}

void kiosk_msgfile_exit()
{
    message_exit(&kiosk_msgfile);
    kiosk_msgfile_init = 0;
}

bool kiosk_msgfile_initialized()
{
    return kiosk_msgfile_init > 0;
}
}
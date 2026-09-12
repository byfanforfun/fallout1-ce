#ifndef FALLOUT_PLIB_GNW_GAMEPAD_H_
#define FALLOUT_PLIB_GNW_GAMEPAD_H_

#include <SDL.h>

namespace fallout {

bool gamepad_init();
void gamepad_exit();
void gamepad_process_event(SDL_Event* event);
int gamepad_get_connected();

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_GAMEPAD_H_ */
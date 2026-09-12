#ifndef FALLOUT_PLIB_GNW_GAMEPAD_H_
#define FALLOUT_PLIB_GNW_GAMEPAD_H_

#include <SDL.h>

namespace fallout {

bool gamepad_init();
void gamepad_exit();
void gamepad_process_event(SDL_Event* event);
void gamepad_poll();
bool gamepad_mouse_button_pressed();
void gamepad_mouse_get_movement(int* dx, int* dy);
void gamepad_set_pointer_slow(bool slow);
int gamepad_get_connected();

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_GAMEPAD_H_ */
#include "plib/gnw/gamepad.h"

#include <stdio.h>

#include "plib/gnw/input.h"

namespace fallout {

#define GAMEPAD_AXIS_DEADZONE_PRESS 24000
#define GAMEPAD_AXIS_DEADZONE_RELEASE 8000

typedef struct gamepad_button_bind_t {
    SDL_GameControllerButton button;
    SDL_Scancode scancode;
} gamepad_button_bind_t;

typedef struct gamepad_axis_bind_t {
    SDL_GameControllerAxis axis;
    SDL_Scancode negativeScancode;
    SDL_Scancode positiveScancode;
    bool negativePressed;
    bool positivePressed;
} gamepad_axis_bind_t;

static gamepad_button_bind_t gamepad_button_binds[] = {
    // Movement: D-pad acts as arrow keys.
    { SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_SCANCODE_UP },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_SCANCODE_DOWN },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_SCANCODE_LEFT },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_SCANCODE_RIGHT },
    // Face buttons: confirm / cancel / use / pip-boy.
    { SDL_CONTROLLER_BUTTON_A, SDL_SCANCODE_RETURN },
    { SDL_CONTROLLER_BUTTON_B, SDL_SCANCODE_ESCAPE },
    { SDL_CONTROLLER_BUTTON_X, SDL_SCANCODE_SPACE },
    { SDL_CONTROLLER_BUTTON_Y, SDL_SCANCODE_TAB },
    // Shoulders: list scrolling.
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_SCANCODE_PAGEUP },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_SCANCODE_PAGEDOWN },
    // Stick clicks: system shortcuts.
    { SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_SCANCODE_F1 },
    { SDL_CONTROLLER_BUTTON_RIGHTSTICK, SDL_SCANCODE_F2 },
    // Back / Start: quick load / quick save.
    { SDL_CONTROLLER_BUTTON_BACK, SDL_SCANCODE_F7 },
    { SDL_CONTROLLER_BUTTON_START, SDL_SCANCODE_F6 },
    // Guide button: return to main menu.
    { SDL_CONTROLLER_BUTTON_GUIDE, SDL_SCANCODE_F12 },
    { SDL_CONTROLLER_BUTTON_INVALID, SDL_SCANCODE_UNKNOWN },
};

static gamepad_axis_bind_t gamepad_axis_binds[] = {
    { SDL_CONTROLLER_AXIS_LEFTX, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, false, false },
    { SDL_CONTROLLER_AXIS_LEFTY, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, false, false },
    // Triggers act as attack / run while held past the deadzone.
    { SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_LCTRL, false, false },
    { SDL_CONTROLLER_AXIS_TRIGGERRIGHT, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_LSHIFT, false, false },
    { SDL_CONTROLLER_AXIS_INVALID, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, false, false },
};

static int gamepad_connected = 0;

static SDL_Scancode gamepad_button_to_scancode(int button)
{
    for (int index = 0; gamepad_button_binds[index].button != SDL_CONTROLLER_BUTTON_INVALID; index++) {
        if (gamepad_button_binds[index].button == button) {
            return gamepad_button_binds[index].scancode;
        }
    }

    return SDL_SCANCODE_UNKNOWN;
}

static void gamepad_post_key(SDL_Scancode scancode, bool down)
{
    KeyboardData keyboardData;
    keyboardData.key = scancode;
    keyboardData.down = down ? 1 : 0;
    GNW95_process_key(&keyboardData);
}

static void gamepad_process_axis(int axis, Sint16 value)
{
    for (int index = 0; gamepad_axis_binds[index].axis != SDL_CONTROLLER_AXIS_INVALID; index++) {
        gamepad_axis_bind_t* bind = &(gamepad_axis_binds[index]);
        if (bind->axis != axis) {
            continue;
        }

        bool negativePressed = value < -GAMEPAD_AXIS_DEADZONE_PRESS;
        bool positivePressed = value > GAMEPAD_AXIS_DEADZONE_PRESS;

        if (negativePressed && !bind->negativePressed) {
            bind->negativePressed = true;
            gamepad_post_key(bind->negativeScancode, true);
        } else if (!negativePressed && bind->negativePressed && value > -GAMEPAD_AXIS_DEADZONE_RELEASE) {
            bind->negativePressed = false;
            gamepad_post_key(bind->negativeScancode, false);
        }

        if (positivePressed && !bind->positivePressed) {
            bind->positivePressed = true;
            gamepad_post_key(bind->positiveScancode, true);
        } else if (!positivePressed && bind->positivePressed && value < GAMEPAD_AXIS_DEADZONE_RELEASE) {
            bind->positivePressed = false;
            gamepad_post_key(bind->positiveScancode, false);
        }
    }
}

static void gamepad_open(int index)
{
    if (!SDL_IsGameController(index)) {
        return;
    }

    SDL_GameController* controller = SDL_GameControllerOpen(index);
    if (controller != NULL) {
        gamepad_connected++;
        fprintf(stderr, "gamepad: opened '%s'\n", SDL_GameControllerName(controller));
    }
}

bool gamepad_init()
{
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "gamepad: SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) failed: %s\n", SDL_GetError());
        return false;
    }

    for (int index = 0; index < SDL_NumJoysticks(); index++) {
        gamepad_open(index);
    }

    return true;
}

void gamepad_exit()
{
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    gamepad_connected = 0;
}

void gamepad_process_event(SDL_Event* event)
{
    switch (event->type) {
    case SDL_CONTROLLERDEVICEADDED:
        gamepad_open(event->cdevice.which);
        break;
    case SDL_CONTROLLERDEVICEREMOVED: {
        SDL_GameController* controller = SDL_GameControllerFromInstanceID(event->cdevice.which);
        if (controller != NULL) {
            fprintf(stderr, "gamepad: closed '%s'\n", SDL_GameControllerName(controller));
            SDL_GameControllerClose(controller);
            if (gamepad_connected > 0) {
                gamepad_connected--;
            }
        }
        break;
    }
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
        if (kb_is_disabled()) {
            break;
        }

        SDL_GameController* controller = SDL_GameControllerFromInstanceID(event->cbutton.which);
        if (controller == NULL) {
            break;
        }

        SDL_Scancode scancode = gamepad_button_to_scancode(event->cbutton.button);
        if (scancode != SDL_SCANCODE_UNKNOWN) {
            gamepad_post_key(scancode, event->cbutton.state == SDL_PRESSED);
        }
        break;
    }
    case SDL_CONTROLLERAXISMOTION:
        if (!kb_is_disabled()) {
            SDL_GameController* controller = SDL_GameControllerFromInstanceID(event->caxis.which);
            if (controller != NULL) {
                gamepad_process_axis(event->caxis.axis, event->caxis.value);
            }
        }
        break;
    }
}

int gamepad_get_connected()
{
    return gamepad_connected;
}

} // namespace fallout
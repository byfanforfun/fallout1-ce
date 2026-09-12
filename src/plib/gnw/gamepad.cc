#include "plib/gnw/gamepad.h"

#include <stdio.h>
#include <string.h>

#include "game/config.h"
#include "platform_compat.h"
#include "plib/gnw/input.h"
#include "plib/gnw/mouse.h"

namespace fallout {

#define GAMEPAD_CONFIG_FILE_NAME "gamepad.cfg"

// The maximum distance analog stick can travel (SDL2 reports -32768..32767).
#define GAMEPAD_AXIS_RANGE 32768

// Press and release thresholds for digital (key) axis mapping. Release is
// lower than press to add hysteresis and prevent jitter around the threshold.
#define GAMEPAD_AXIS_DEADZONE_PRESS 24000
#define GAMEPAD_AXIS_DEADZONE_RELEASE 8000

// Right stick is ignored while its value stays below this threshold.
#define GAMEPAD_AXIS_DEADZONE_MOUSE 2000

// Mouse pointer speed in pixels per frame at full stick deflection.
#define GAMEPAD_MOUSE_SPEED 12

// Right stick press is a click if released within this time, otherwise it
// becomes a right-button hold (context menu).
#define GAMEPAD_CLICK_HOLD_MS 350

typedef enum GamepadBindType {
    GAMEPAD_BIND_NONE,
    GAMEPAD_BIND_KEY,
    GAMEPAD_BIND_MOUSE_MOVE,
    GAMEPAD_BIND_MOUSE_CLICK,
} GamepadBindType;

typedef struct GamepadButtonBind {
    GamepadBindType type;
    SDL_Scancode scancode;
} GamepadButtonBind;

typedef struct GamepadAxisBind {
    GamepadBindType type;
    SDL_Scancode negativeScancode;
    SDL_Scancode positiveScancode;
    bool negativePressed;
    bool positivePressed;
} GamepadAxisBind;

static GamepadButtonBind gamepad_button_binds[SDL_CONTROLLER_BUTTON_MAX];
static GamepadAxisBind gamepad_axis_binds[SDL_CONTROLLER_AXIS_MAX];

static SDL_GameController* gamepad_controller = NULL;
static int gamepad_connected = 0;

// Right stick press state for click / context-menu hold logic.
static bool gamepad_rstick_pressed = false;
static unsigned int gamepad_rstick_press_time = 0;
static bool gamepad_rstick_context = false;

// Left stick press state for the stick-click key polling.
static bool gamepad_lstick_held = false;

static void gamepad_parse_bind(const char* value, GamepadButtonBind* out)
{
    out->type = GAMEPAD_BIND_NONE;
    out->scancode = SDL_SCANCODE_UNKNOWN;

    if (value == NULL || strcmp(value, "none") == 0) {
        return;
    }

    if (strcmp(value, "mouse") == 0 || strcmp(value, "click") == 0) {
        out->type = GAMEPAD_BIND_MOUSE_CLICK;
        return;
    }

    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    if (strcmp(value, "esc") == 0 || strcmp(value, "escape") == 0) {
        scancode = SDL_SCANCODE_ESCAPE;
    } else if (strcmp(value, "return") == 0 || strcmp(value, "enter") == 0) {
        scancode = SDL_SCANCODE_RETURN;
    } else if (strcmp(value, "space") == 0) {
        scancode = SDL_SCANCODE_SPACE;
    } else if (strcmp(value, "tab") == 0) {
        scancode = SDL_SCANCODE_TAB;
    } else if (strcmp(value, "backspace") == 0) {
        scancode = SDL_SCANCODE_BACKSPACE;
    } else if (strcmp(value, "up") == 0) {
        scancode = SDL_SCANCODE_UP;
    } else if (strcmp(value, "down") == 0) {
        scancode = SDL_SCANCODE_DOWN;
    } else if (strcmp(value, "left") == 0) {
        scancode = SDL_SCANCODE_LEFT;
    } else if (strcmp(value, "right") == 0) {
        scancode = SDL_SCANCODE_RIGHT;
    } else if (strcmp(value, "home") == 0) {
        scancode = SDL_SCANCODE_HOME;
    } else if (strcmp(value, "end") == 0) {
        scancode = SDL_SCANCODE_END;
    } else if (strcmp(value, "pageup") == 0) {
        scancode = SDL_SCANCODE_PAGEUP;
    } else if (strcmp(value, "pagedown") == 0) {
        scancode = SDL_SCANCODE_PAGEDOWN;
    } else if (strcmp(value, "ctrl") == 0) {
        scancode = SDL_SCANCODE_LCTRL;
    } else if (strcmp(value, "shift") == 0) {
        scancode = SDL_SCANCODE_LSHIFT;
    } else if (strcmp(value, "capslock") == 0) {
        scancode = SDL_SCANCODE_CAPSLOCK;
    } else if (strlen(value) == 2 && value[0] == 'f' && value[1] >= '1' && value[1] <= '9') {
        scancode = (SDL_Scancode)(SDL_SCANCODE_F1 + value[1] - '1');
    } else if (strlen(value) == 3 && value[0] == 'f' && value[1] == '1' && value[2] >= '0' && value[2] <= '2') {
        scancode = (SDL_Scancode)(SDL_SCANCODE_F9 + value[2] - '0');
    } else if (strlen(value) == 1 && value[0] >= 'a' && value[0] <= 'z') {
        scancode = (SDL_Scancode)(SDL_SCANCODE_A + value[0] - 'a');
    } else if (strlen(value) == 1 && value[0] >= '0' && value[0] <= '9') {
        scancode = (SDL_Scancode)(SDL_SCANCODE_1 + value[0] - '1');
    }

    if (scancode != SDL_SCANCODE_UNKNOWN) {
        out->type = GAMEPAD_BIND_KEY;
        out->scancode = scancode;
    } else {
        fprintf(stderr, "gamepad: unrecognized binding '%s'\n", value);
    }
}

static void gamepad_set_button_default(SDL_GameControllerButton button, const char* value)
{
    if (button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX) {
        return;
    }

    gamepad_parse_bind(value, &(gamepad_button_binds[button]));
}

static void gamepad_set_axis_default(SDL_GameControllerAxis axis, const char* value)
{
    if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX) {
        return;
    }

    GamepadAxisBind* bind = &(gamepad_axis_binds[axis]);
    bind->type = GAMEPAD_BIND_NONE;
    bind->negativeScancode = SDL_SCANCODE_UNKNOWN;
    bind->positiveScancode = SDL_SCANCODE_UNKNOWN;
    bind->negativePressed = false;
    bind->positivePressed = false;

    if (value == NULL || strcmp(value, "none") == 0) {
        return;
    }

    if (strcmp(value, "mouse") == 0) {
        bind->type = GAMEPAD_BIND_MOUSE_MOVE;
        return;
    }

    // "neg,pos" or a single key (used for one-way axes like triggers).
    char buffer[64];
    strncpy(buffer, value, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* positive = strchr(buffer, ',');
    if (positive != NULL) {
        *positive++ = '\0';
    }

    GamepadButtonBind parsed;
    gamepad_parse_bind(buffer, &parsed);
    if (parsed.type == GAMEPAD_BIND_KEY) {
        bind->type = GAMEPAD_BIND_KEY;
        bind->negativeScancode = parsed.scancode;
    }

    if (positive != NULL && positive[0] != '\0') {
        gamepad_parse_bind(positive, &parsed);
        if (parsed.type == GAMEPAD_BIND_KEY) {
            bind->type = GAMEPAD_BIND_KEY;
            bind->positiveScancode = parsed.scancode;
        }
    }
}

static const char* gamepad_button_config_key(SDL_GameControllerButton button)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        return "btn_dpad_up";
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return "btn_dpad_down";
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return "btn_dpad_left";
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return "btn_dpad_right";
    case SDL_CONTROLLER_BUTTON_A:
        return "btn_a";
    case SDL_CONTROLLER_BUTTON_B:
        return "btn_b";
    case SDL_CONTROLLER_BUTTON_X:
        return "btn_x";
    case SDL_CONTROLLER_BUTTON_Y:
        return "btn_y";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        return "btn_l1";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        return "btn_r1";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
        return "btn_l3";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        return "btn_r3";
    case SDL_CONTROLLER_BUTTON_START:
        return "btn_start";
    case SDL_CONTROLLER_BUTTON_BACK:
        return "btn_back";
    case SDL_CONTROLLER_BUTTON_GUIDE:
        return "btn_guide";
    default:
        return NULL;
    }
}

static const char* gamepad_axis_config_key(SDL_GameControllerAxis axis)
{
    switch (axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        return "axis_leftx";
    case SDL_CONTROLLER_AXIS_LEFTY:
        return "axis_lefty";
    case SDL_CONTROLLER_AXIS_RIGHTX:
        return "axis_rightx";
    case SDL_CONTROLLER_AXIS_RIGHTY:
        return "axis_righty";
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        return "axis_lefttrigger";
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        return "axis_righttrigger";
    default:
        return NULL;
    }
}

static const char* gamepad_button_default(SDL_GameControllerButton button)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        return "i";
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return "tab";
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return "c";
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return "p";
    case SDL_CONTROLLER_BUTTON_A:
        return "return";
    case SDL_CONTROLLER_BUTTON_B:
        return "space";
    case SDL_CONTROLLER_BUTTON_X:
        return "s";
    case SDL_CONTROLLER_BUTTON_Y:
        return "esc";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        return "n";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        return "a";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
        return "home";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        return "mouse";
    case SDL_CONTROLLER_BUTTON_START:
        return "f6";
    case SDL_CONTROLLER_BUTTON_BACK:
        return "f7";
    case SDL_CONTROLLER_BUTTON_GUIDE:
        return "f12";
    default:
        return NULL;
    }
}

static const char* gamepad_axis_default(SDL_GameControllerAxis axis)
{
    switch (axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        return "left,right";
    case SDL_CONTROLLER_AXIS_LEFTY:
        return "up,down";
    case SDL_CONTROLLER_AXIS_RIGHTX:
        return "mouse";
    case SDL_CONTROLLER_AXIS_RIGHTY:
        return "mouse";
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        return "b";
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        return "m";
    default:
        return NULL;
    }
}

static bool gamepad_load_bindings()
{
    Config config;
    if (!config_init(&config)) {
        return false;
    }

    const char* section = "gamepad";

    for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; button++) {
        const char* key = gamepad_button_config_key((SDL_GameControllerButton)button);
        const char* value = gamepad_button_default((SDL_GameControllerButton)button);
        if (key != NULL && value != NULL) {
            config_set_string(&config, section, key, value);
        }
    }

    for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
        const char* key = gamepad_axis_config_key((SDL_GameControllerAxis)axis);
        const char* value = gamepad_axis_default((SDL_GameControllerAxis)axis);
        if (key != NULL && value != NULL) {
            config_set_string(&config, section, key, value);
        }
    }

    // Write the file with the default bindings on the first run so the user
    // has a reference to edit.
    FILE* probe = compat_fopen(GAMEPAD_CONFIG_FILE_NAME, "rt");
    if (probe == NULL) {
        config_save(&config, GAMEPAD_CONFIG_FILE_NAME, false);
    } else {
        fclose(probe);
    }

    config_load(&config, GAMEPAD_CONFIG_FILE_NAME, false);

    for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; button++) {
        const char* key = gamepad_button_config_key((SDL_GameControllerButton)button);
        if (key != NULL) {
            char* value = NULL;
            if (config_get_string(&config, section, key, &value) && value != NULL) {
                gamepad_set_button_default((SDL_GameControllerButton)button, value);
            }
        }
    }

    for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
        const char* key = gamepad_axis_config_key((SDL_GameControllerAxis)axis);
        if (key != NULL) {
            char* value = NULL;
            if (config_get_string(&config, section, key, &value) && value != NULL) {
                gamepad_set_axis_default((SDL_GameControllerAxis)axis, value);
            }
        }
    }

    config_exit(&config);

    return true;
}

static void gamepad_post_key(SDL_Scancode scancode, bool down)
{
    KeyboardData keyboardData;
    keyboardData.key = scancode;
    keyboardData.down = down ? 1 : 0;
    GNW95_process_key(&keyboardData);
}

static void gamepad_process_axis_event(int axis, Sint16 value)
{
    if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX) {
        return;
    }

    GamepadAxisBind* bind = &(gamepad_axis_binds[axis]);
    if (bind->type != GAMEPAD_BIND_KEY) {
        return;
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

static void gamepad_rstick_down()
{
    gamepad_rstick_pressed = true;
    gamepad_rstick_press_time = get_time();
    gamepad_rstick_context = false;

    // The game samples mouse buttons once per frame after the event pump, so
    // the DOWN must survive for at least a frame. Emit it here and clear it on
    // the release edge (gamepad_rstick_up), mirroring the touch tap path.
    mouse_simulate_input(0, 0, MOUSE_STATE_LEFT_BUTTON_DOWN);
}

static void gamepad_rstick_up()
{
    if (!gamepad_rstick_pressed) {
        return;
    }

    gamepad_rstick_pressed = false;
    gamepad_rstick_context = false;
    mouse_simulate_input(0, 0, 0);
}

// Called once per frame from GNW95_process_message. Right stick drives the
// emulated mouse pointer; a long press of the right stick becomes the
// right-button hold that opens the context menu.
static void gamepad_poll_mouse()
{
    if (gamepad_controller == NULL) {
        return;
    }

    int dx = 0;
    int dy = 0;

    if (gamepad_axis_binds[SDL_CONTROLLER_AXIS_RIGHTX].type == GAMEPAD_BIND_MOUSE_MOVE) {
        Sint16 value = SDL_GameControllerGetAxis(gamepad_controller, SDL_CONTROLLER_AXIS_RIGHTX);
        if (value > GAMEPAD_AXIS_DEADZONE_MOUSE || value < -GAMEPAD_AXIS_DEADZONE_MOUSE) {
            dx = (value * GAMEPAD_MOUSE_SPEED) / GAMEPAD_AXIS_RANGE;
        }
    }

    if (gamepad_axis_binds[SDL_CONTROLLER_AXIS_RIGHTY].type == GAMEPAD_BIND_MOUSE_MOVE) {
        Sint16 value = SDL_GameControllerGetAxis(gamepad_controller, SDL_CONTROLLER_AXIS_RIGHTY);
        if (value > GAMEPAD_AXIS_DEADZONE_MOUSE || value < -GAMEPAD_AXIS_DEADZONE_MOUSE) {
            dy = (value * GAMEPAD_MOUSE_SPEED) / GAMEPAD_AXIS_RANGE;
        }
    }

    int buttons = gamepad_rstick_context ? MOUSE_STATE_RIGHT_BUTTON_DOWN : 0;
    if (dx != 0 || dy != 0 || buttons != 0) {
        mouse_simulate_input(dx, dy, buttons);
    }

    if (gamepad_rstick_pressed && !gamepad_rstick_context
        && elapsed_time(gamepad_rstick_press_time) >= GAMEPAD_CLICK_HOLD_MS) {
        gamepad_rstick_context = true;
        mouse_simulate_input(0, 0, 0);
        mouse_simulate_input(0, 0, MOUSE_STATE_RIGHT_BUTTON_DOWN);
    }
}

// Reports the current physical state of a stick-click button. Polling is used
// because some controllers/mappings do not deliver SDL_CONTROLLERBUTTONDOWN
// for the stick clicks (they arrive only as raw joystick buttons), so the
// event path alone would silently swallow them.
static bool gamepad_button_held(SDL_GameControllerButton button)
{
    if (gamepad_controller == NULL) {
        return false;
    }

    if (SDL_GameControllerGetButton(gamepad_controller, button) == SDL_PRESSED) {
        return true;
    }

    // Fallback: probe the raw joystick at the conventional L3/R3 positions.
    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(gamepad_controller);
    if (joystick == NULL) {
        return false;
    }

    int rawButton = button == SDL_CONTROLLER_BUTTON_RIGHTSTICK ? 9 : 8;
    return rawButton < SDL_JoystickNumButtons(joystick) && SDL_JoystickGetButton(joystick, rawButton) != 0;
}

static void gamepad_poll_button(SDL_GameControllerButton button, bool* held)
{
    GamepadButtonBind* bind = &(gamepad_button_binds[button]);
    bool down = gamepad_button_held(button);

    if (bind->type == GAMEPAD_BIND_KEY) {
        if (down != *held) {
            *held = down;
            if (!kb_is_disabled()) {
                gamepad_post_key(bind->scancode, down);
            }
        }
    } else if (bind->type == GAMEPAD_BIND_MOUSE_CLICK) {
        if (down && !*held) {
            *held = true;
            gamepad_rstick_down();
        } else if (!down && *held) {
            *held = false;
            gamepad_rstick_up();
        }
    }
}

// Polled per frame from gamepad_poll(); stick clicks are read by state rather
// than by event (see gamepad_button_held for details).
static void gamepad_poll_stick_buttons()
{
    gamepad_poll_button(SDL_CONTROLLER_BUTTON_LEFTSTICK, &gamepad_lstick_held);
    gamepad_poll_button(SDL_CONTROLLER_BUTTON_RIGHTSTICK, &gamepad_rstick_pressed);
}

static void gamepad_open(int index)
{
    if (!SDL_IsGameController(index)) {
        return;
    }

    SDL_GameController* controller = SDL_GameControllerOpen(index);
    if (controller != NULL) {
        gamepad_controller = controller;
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

    gamepad_load_bindings();

    for (int index = 0; index < SDL_NumJoysticks(); index++) {
        gamepad_open(index);
    }

    return true;
}

void gamepad_exit()
{
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    gamepad_controller = NULL;
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

            if (gamepad_controller == controller) {
                gamepad_controller = NULL;
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

        int button = event->cbutton.button;
        if (button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX) {
            break;
        }

        // Stick clicks are polled per-frame; ignore events to avoid double posts.
        if (button == SDL_CONTROLLER_BUTTON_LEFTSTICK || button == SDL_CONTROLLER_BUTTON_RIGHTSTICK) {
            break;
        }

        GamepadButtonBind* bind = &(gamepad_button_binds[button]);
        bool down = event->cbutton.state == SDL_PRESSED;

        if (bind->type == GAMEPAD_BIND_KEY) {
            gamepad_post_key(bind->scancode, down);
        } else if (bind->type == GAMEPAD_BIND_MOUSE_CLICK) {
            if (down) {
                gamepad_rstick_down();
            } else {
                gamepad_rstick_up();
            }
        }
        break;
    }
    case SDL_CONTROLLERAXISMOTION:
        if (!kb_is_disabled()) {
            gamepad_process_axis_event(event->caxis.axis, event->caxis.value);
        }
        break;
    }
}

void gamepad_poll()
{
    gamepad_poll_stick_buttons();
    gamepad_poll_mouse();
}

int gamepad_get_connected()
{
    return gamepad_connected;
}

} // namespace fallout
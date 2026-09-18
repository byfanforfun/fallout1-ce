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

// Triggers are digital (no analog pressure), so a much lower press threshold
// is enough to register a pull.
#define GAMEPAD_TRIGGER_DEADZONE_PRESS 10000
#define GAMEPAD_TRIGGER_DEADZONE_RELEASE 3000

// Default mouse pointer speed in pixels per frame at full stick deflection.
#define GAMEPAD_MOUSE_SPEED 12

// Pointer speed divisor while the pointer slow mode (context menu) is active.
#define GAMEPAD_POINTER_SLOW_DIV 5

// Consecutive frames a stick-click button must report as released before the
// emulated left button is actually released. Guards against a single trans-
// ient poll miss dropping an in-progress drag or hold.
#define GAMEPAD_BUTTON_RELEASE_DEBOUNCE 3

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

// Right stick press state. While pressed it holds the left mouse button, which
// the game turns into the actions (context) menu after ~250ms in arrow mode.
static bool gamepad_rstick_pressed = false;

// Consecutive not-pressed frames of the right stick before releasing.
static int gamepad_rstick_release_counter = 0;

// Right stick pointer movement accumulated per frame; consumed and emitted by
// mouse_info()'s single mouse-state update.
static int gamepad_mouse_dx = 0;
static int gamepad_mouse_dy = 0;

// Left stick press state for the stick-click key polling.
static bool gamepad_lstick_held = false;

// Mouse pointer speed in pixels per frame at full stick deflection.
static int gamepad_mouse_speed = GAMEPAD_MOUSE_SPEED;

// When set, the right stick pointer is slowed down (used while the in-game
// context/actions menu is open, where items sit close together).
static bool gamepad_pointer_slow = false;

// Per-axis inversion flags (negate the incoming axis value).
static bool gamepad_axis_inverted[SDL_CONTROLLER_AXIS_MAX];

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
    } else if (strcmp(value, "alt") == 0 || strcmp(value, "lalt") == 0) {
        scancode = SDL_SCANCODE_LALT;
    } else if (strcmp(value, "ralt") == 0) {
        scancode = SDL_SCANCODE_RALT;
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

    // "neg,pos" or a single key (used for one-way axes like triggers). A
    // single key is assigned to the positive side: trigger axes report only
    // positive values, so posting from the negative side would never fire.
    char buffer[64];
    strncpy(buffer, value, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* positive = strchr(buffer, ',');
    if (positive != NULL) {
        *positive++ = '\0';
    }

    GamepadButtonBind parsed;
    if (positive == NULL) {
        gamepad_parse_bind(buffer, &parsed);
        if (parsed.type == GAMEPAD_BIND_KEY) {
            bind->type = GAMEPAD_BIND_KEY;
            bind->positiveScancode = parsed.scancode;
        }
        return;
    }

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
        return "btn_l2";
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        return "btn_r2";
    default:
        return NULL;
    }
}

static const char* gamepad_axis_invert_config_key(SDL_GameControllerAxis axis)
{
    switch (axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        return "invert_leftx";
    case SDL_CONTROLLER_AXIS_LEFTY:
        return "invert_lefty";
    case SDL_CONTROLLER_AXIS_RIGHTX:
        return "invert_rightx";
    case SDL_CONTROLLER_AXIS_RIGHTY:
        return "invert_righty";
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        return "invert_lefttrigger";
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        return "invert_righttrigger";
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

    config_set_string(&config, section, "mouse_speed", "12");

    for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
        const char* key = gamepad_axis_invert_config_key((SDL_GameControllerAxis)axis);
        if (key != NULL) {
            config_set_string(&config, section, key, "0");
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

    int mouseSpeed = 0;
    if (config_get_value(&config, section, "mouse_speed", &mouseSpeed) && mouseSpeed > 0) {
        gamepad_mouse_speed = mouseSpeed;
    }

    for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
        const char* key = gamepad_axis_invert_config_key((SDL_GameControllerAxis)axis);
        if (key != NULL) {
            int invert = 0;
            if (config_get_value(&config, section, key, &invert)) {
                gamepad_axis_inverted[axis] = invert != 0;
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

    if (gamepad_axis_inverted[axis]) {
        value = -value;
    }

    // Triggers are digital on/off controls in this game, so make them far more
    // sensitive than the analog sticks.
    int pressThreshold = (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
        ? GAMEPAD_TRIGGER_DEADZONE_PRESS
        : GAMEPAD_AXIS_DEADZONE_PRESS;
    int releaseThreshold = (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
        ? GAMEPAD_TRIGGER_DEADZONE_RELEASE
        : GAMEPAD_AXIS_DEADZONE_RELEASE;

    bool negativePressed = value < -pressThreshold;
    bool positivePressed = value > pressThreshold;

    if (negativePressed && !bind->negativePressed) {
        bind->negativePressed = true;
        gamepad_post_key(bind->negativeScancode, true);
    } else if (!negativePressed && bind->negativePressed && value > -releaseThreshold) {
        bind->negativePressed = false;
        gamepad_post_key(bind->negativeScancode, false);
    }

    if (positivePressed && !bind->positivePressed) {
        bind->positivePressed = true;
        gamepad_post_key(bind->positiveScancode, true);
    } else if (!positivePressed && bind->positivePressed && value < releaseThreshold) {
        bind->positivePressed = false;
        gamepad_post_key(bind->positiveScancode, false);
    }
}

static void gamepad_rstick_down()
{
    gamepad_rstick_pressed = true;
}

static void gamepad_rstick_up()
{
    gamepad_rstick_pressed = false;
}

// Called once per frame from GNW95_process_message. The right stick drives the
// emulated mouse pointer; while the right stick is pressed the left button is
// held, which the game turns into the actions (context) menu after ~250ms in
// arrow mode. Movement and buttons are only applied by mouse_info()'s single
// per-frame state update, so the emulated input is never split across two
// events in the same frame (that would re-arm the DOWN edge before the window
// system samples it and drop menu clicks and drags).
static void gamepad_poll_mouse()
{
    gamepad_mouse_dx = 0;
    gamepad_mouse_dy = 0;

    if (gamepad_controller == NULL) {
        return;
    }

    int speed = gamepad_mouse_speed;
    if (gamepad_pointer_slow) {
        speed /= GAMEPAD_POINTER_SLOW_DIV;
    }

    if (gamepad_axis_binds[SDL_CONTROLLER_AXIS_RIGHTX].type == GAMEPAD_BIND_MOUSE_MOVE) {
        Sint16 value = SDL_GameControllerGetAxis(gamepad_controller, SDL_CONTROLLER_AXIS_RIGHTX);
        if (gamepad_axis_inverted[SDL_CONTROLLER_AXIS_RIGHTX]) {
            value = -value;
        }
        if (value > GAMEPAD_AXIS_DEADZONE_MOUSE || value < -GAMEPAD_AXIS_DEADZONE_MOUSE) {
            gamepad_mouse_dx = (value * speed) / GAMEPAD_AXIS_RANGE;
        }
    }

    if (gamepad_axis_binds[SDL_CONTROLLER_AXIS_RIGHTY].type == GAMEPAD_BIND_MOUSE_MOVE) {
        Sint16 value = SDL_GameControllerGetAxis(gamepad_controller, SDL_CONTROLLER_AXIS_RIGHTY);
        if (gamepad_axis_inverted[SDL_CONTROLLER_AXIS_RIGHTY]) {
            value = -value;
        }
        if (value > GAMEPAD_AXIS_DEADZONE_MOUSE || value < -GAMEPAD_AXIS_DEADZONE_MOUSE) {
            gamepad_mouse_dy = (value * speed) / GAMEPAD_AXIS_RANGE;
        }
    }
}

// Slows the right stick pointer down while the in-game context menu is open so
// its items (spaced ~10px apart) can be selected precisely.
void gamepad_set_pointer_slow(bool slow)
{
    gamepad_pointer_slow = slow;
}

// Returns whether the right stick currently emulates a left-button hold.
// mouse_info() folds this into its single per-frame mouse state update so the
// emulated button behaves like a real mouse button (sustained DOWN|REPEAT,
// no per-frame edge re-generation that would break drags and the hold menu).
bool gamepad_mouse_button_pressed()
{
    return gamepad_rstick_pressed;
}

// Returns the right stick pointer movement. Called by mouse_info() so movement
// and buttons reach the game in one consistent state update.
void gamepad_mouse_get_movement(int* dx, int* dy)
{
    *dx = gamepad_mouse_dx;
    *dy = gamepad_mouse_dy;
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

    // Trust SDL's own mapping first. The raw-index fallback below must never
    // run for a button SDL knows about: guessing flat indices here can cross-
    // match another physical button (e.g. read L3's press as R3).
    SDL_GameControllerButtonBind bind = SDL_GameControllerGetBindForButton(gamepad_controller, button);
    if (bind.bindType != SDL_CONTROLLER_BINDTYPE_NONE) {
        return SDL_GameControllerGetButton(gamepad_controller, button) == SDL_PRESSED;
    }

    // Fallback for controllers/mappings without stick buttons: probe the raw
    // joystick at the conventional L3/R3 positions.
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
        if (down) {
            if (!*held) {
                *held = true;
                gamepad_rstick_down();
            }
            gamepad_rstick_release_counter = 0;
        } else if (*held && ++gamepad_rstick_release_counter >= GAMEPAD_BUTTON_RELEASE_DEBOUNCE) {
            *held = false;
            gamepad_rstick_release_counter = 0;
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

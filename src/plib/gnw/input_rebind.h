#ifndef FALLOUT_PLIB_GNW_INPUT_REBIND_H_
#define FALLOUT_PLIB_GNW_INPUT_REBIND_H_

namespace fallout {

#define BIND_SECTION_NO_SEC    "nosec"

typedef enum BindScreen {
    SCREEN_MAIN,
    SCREEN_GAME,
    SCREEN_CHAR,
    SCREEN_INV,
    SCREEN_PIP,
    SCREEN_MAX
} BindScreen;

typedef enum BindSection {
    Main,
    Game,
    Editor,
    Inv,
    Pip,
    BindSection_max
} BindSection;

/*
#define BIND_SCREEN_MAIN    0
#define BIND_SCREEN_GAME    1
#define BIND_SCREEN_CHAR    2
#define BIND_SCREEN_INV     3
#define BIND_SCREEN_PIP     4

#define BIND_SECTION_MAIN    "main"
#define BIND_SECTION_GAME    "game"
#define BIND_SECTION_CHAR    "char"
#define BIND_SECTION_INV     "inv"
#define BIND_SECTION_PIP     "pip"
*/

extern int current_screen;

bool bind_init();
int get_key(int screen, int key);
int get_physical_key(int screen, int logical_key);

}

#endif // FALLOUT_PLIB_GNW_INPUT_REBIND_H_

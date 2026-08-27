#ifndef FALLOUT_GAME_LOADSAVE_H_
#define FALLOUT_GAME_LOADSAVE_H_

#include "game/art.h"
#include "game/message.h"
#include "plib/db/db.h"
#include "plib/gnw/rect.h"

namespace fallout {

typedef enum LoadSaveMode {
    // Special case - loading game from main menu.
    LOAD_SAVE_MODE_FROM_MAIN_MENU,

    // Normal (full-screen) save/load screen.
    LOAD_SAVE_MODE_NORMAL,

    // Quick load/save.
    LOAD_SAVE_MODE_QUICK,

    // Pick a slot for a new continues-play game (returns 1-based slot).
    LOAD_SAVE_MODE_PICK_SLOT,
} LoadSaveMode;

void InitLoadSave();
void ResetLoadSave();
int SaveGame(int mode);
int LoadGame(int mode);
int isLoadingGame();
void KillOldMaps();
int MapDirErase(const char* path, const char* a2);
int MapDirEraseFile(const char* a1, const char* a2);

// Continues-play (one life, one slot) support.
void kiosk_continues_set_slot(int slot);
int kiosk_continues_get_slot();
void kiosk_continues_erase_slot();
int kiosk_continues_autosave();

extern bool loadingFromSave;

} // namespace fallout

#endif /* FALLOUT_GAME_LOADSAVE_H_ */

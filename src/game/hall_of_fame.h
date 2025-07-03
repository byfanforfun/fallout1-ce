#ifndef FALLOUT_GAME_HALL_OF_FAME_H_
#define FALLOUT_GAME_HALL_OF_FAME_H_

namespace fallout {

#define HOF_STAR_SIGN "★"
#define HOF_MAX_PAGES 99
#define HOF_ENTRIES_PER_PAGE 13
#define HOF_MAX_ENTRIES HOF_MAX_PAGES * HOF_ENTRIES_PER_PAGE
#define HOF_MAX_RAWS 8
#define HOF_LINE_OFFSET 25

#define HOF_WINDOW_WIDTH 640
#define HOF_WINDOW_HEIGHT 480

#define ANIMATE 0x01
#define RED_NUMBERS 0x02
#define BIG_NUM_WIDTH 14
#define BIG_NUM_HEIGHT 24
#define BIG_NUM_ANIMATION_DELAY 123
#define EDITOR_GRAPHIC_BIG_NUMBERS 0
#define EDITOR_GRAPHIC_COUNT 50

#define HOF_MSG_NUM 1200
#define HOF_MSG_NAME 1201
#define HOF_MSG_LEVEL 1202
#define HOF_MSG_EXP 1203
#define HOF_MSG_CHIP 1204
#define HOF_MSG_CATH 1205
#define HOF_MSG_MASTER 1206
#define HOF_MSG_DC 1207
#define HOF_MSG_YES 1210
#define HOF_MSG_NO 1211

typedef enum {
    HOF_SORT_RANK,
    HOF_SORT_NAME,
    HOF_SORT_LEVEL,
    HOF_SORT_EXP,
    HOF_SORT_WATER_CHIP,
    HOF_SORT_CATHEDRAL,
    HOF_SORT_MASTER,
    HOF_SORT_DEATH_CAUSE,
    HOF_SORT_TIME
} HofSortColumn;

typedef struct HallOfFameEntry {
    char name[32];
    int level;
    int exp;
    bool water_chip_found;
    bool cathedral_destroyed;
    bool master_destroyed;
    char death_cause[64];
    int days;
    int hours;
    int minutes;
    int victory_score;
    int permanent_rank;
} HallOfFameEntry;

typedef struct HallOfFameWindow {
    HallOfFameEntry entries[HOF_MAX_ENTRIES];
    int num_entries;
    int current_page;
    HofSortColumn sort_column;
    bool sort_ascending;
    int window_id;
    int column_buttons[HOF_MAX_RAWS]; // ID кнопок для сортировки
} HallOfFameWindow;

int game_handle_hof();

}

#endif //FALLOUT_GAME_HALL_OF_FAME_H_
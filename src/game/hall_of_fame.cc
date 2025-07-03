#include "game/hall_of_fame.h"

#include "game/art.h"

#include "game/chardump.h"

#include "game/gconfig.h"
#include "game/gsound.h"

#include "game/palette.h"

#include "game/kiosk_msgfile.h"

#include "plib/color/color.h"

#include "plib/gnw/gnw.h"
#include "plib/gnw/gnw_types.h"

#include "plib/gnw/button.h"
#include "plib/gnw/input.h"
#include "plib/gnw/hash_fnv-1a.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/text.h"
#include "plib/gnw/svga.h"

#include "plib/db/db.h"

#include "platform_compat.h"

namespace fallout {

static HallOfFameWindow hof_window;

int hof_handle_input(int key);
void hof_load_entries();
void hof_create_window();
void hof_redraw();
void hof_destroy_window();
void hof_process_file(const char* path, int index);
void hof_increase_vic_point(HallOfFameEntry *entry);
static void hof_sort_entries();
static int hof_compare_entries(const void* a, const void* b);
void hof_draw_victory_icon(int x, int y, unsigned char* buf, int pitch);
void hof_column_click(int btn_id, int key_code);
int hof_draw_back();
static void hof_perform_sort();
int button_init();
void PrintBigNum(int x, int y, int flags, int value, int previousValue, int windowHandle);
static int hof_msg_load();
unsigned int hof_hash_path(const char* path);

static Size GInfoBigNum;

static int prev_button = -1;
static unsigned char* prev_button_up;
static unsigned char* prev_button_down;
static CacheEntry* prev_button_up_key = NULL;
static CacheEntry* prev_button_down_key = NULL;

static int next_button = -1;
static unsigned char* next_button_up;
static unsigned char* next_button_down;
static CacheEntry* next_button_up_key = NULL;
static CacheEntry* next_button_down_key = NULL;

static int back_button = -1;
static unsigned char* back_button_up;
static unsigned char* back_button_down;
static CacheEntry* back_button_up_key = NULL;
static CacheEntry* back_button_down_key = NULL;

static unsigned char* bignum;

static char* messageItemNum;
static char* messageItemName;
static char* messageItemSex;
static char* messageItemLevel;
static char* messageItemExp;
static char* messageItemChip;
static char* messageItemCath;
static char* messageItemMaster;
static char* messageItemDC;

static char* messageItemYes;
static char* messageItemNo;

static CacheEntry* bignum_key;

static MessageListItem mesg;

static int fontsave;
static int old_page;

static unsigned int frame_time;

static bool initial_sort = false;

int game_handle_hof() {
    if(hof_msg_load() != 0)
        return -1;

    hof_create_window();
    button_init();

    int key = 0;
    while (hof_window.window_id != -1) {
        sharedFpsLimiter.mark();

        key = get_input();
        if(hof_handle_input(key) != 0)
            return -1;

        hof_redraw();

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    flush_last_char_hash();

    return 0;
}

int hof_init() {
    memset(&hof_window, 0, sizeof(HallOfFameWindow));
    hof_window.window_id = -1;
    for (int i = 0; i < 9; i++) {
        hof_window.column_buttons[i] = -1;
    }
    hof_window.sort_column = HOF_SORT_RANK;
    hof_window.sort_ascending = false;

    //170 - BIG NUMS
    int fid = art_id(OBJ_TYPE_INTERFACE, 170, 0, 0, 0);
    bignum = art_lock(fid, &bignum_key, &(GInfoBigNum.width), &(GInfoBigNum.height));
    if (bignum == NULL) {
        return -1;
    }

    old_page = 0;

    return 1;
}

int button_init() {
    if(hof_window.window_id == -1)
        return -1;

    int fid;

    int width = win_width(hof_window.window_id);
    int height = win_height(hof_window.window_id);

    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    next_button_up = art_ptr_lock_data(fid, 0, 0, &next_button_up_key);
    if (next_button_up == NULL) {
        return -1;
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    next_button_down = art_ptr_lock_data(fid, 0, 0, &next_button_down_key);
    if (next_button_down == NULL) {
        return -1;
    }

    next_button = win_register_button(hof_window.window_id,
        146,
        HOF_WINDOW_HEIGHT-34,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_ARROW_LEFT,
        next_button_up,
        next_button_down,
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (next_button == -1) {
        return -1;
    }

    win_register_button_sound_func(next_button, gsound_red_butt_press, gsound_red_butt_release);

    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    prev_button_up = art_ptr_lock_data(fid, 0, 0, &prev_button_up_key);
    if (prev_button_up == NULL) {
        return -1;
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    prev_button_down = art_ptr_lock_data(fid, 0, 0, &prev_button_down_key);
    if (prev_button_down == NULL) {
        return -1;
    }

    prev_button = win_register_button(hof_window.window_id,
        239,
        HOF_WINDOW_HEIGHT-34,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_ARROW_RIGHT,
        prev_button_up,
        prev_button_down,
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (prev_button == -1) {
        return -1;
    }

    win_register_button_sound_func(prev_button, gsound_red_butt_press, gsound_red_butt_release);

    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    back_button_up = art_ptr_lock_data(fid, 0, 0, &back_button_up_key);
    if (back_button_up == NULL) {
        return -1;
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    back_button_down = art_ptr_lock_data(fid, 0, 0, &back_button_down_key);
    if (back_button_down == NULL) {
        return -1;
    }

    back_button = win_register_button(hof_window.window_id,
        530,
        HOF_WINDOW_HEIGHT-34,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        back_button_up,
        back_button_down,
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (back_button == -1) {
        return -1;
    }

    win_register_button_sound_func(back_button, gsound_red_butt_press, gsound_red_butt_release);


    return 0;
}

int hof_msg_load()
{
    if (!kiosk_msgfile_initialized())
        return -1;

    messageItemNum = getmsg(&kiosk_msgfile, &mesg, HOF_MSG_NUM);
    messageItemName = getmsg(&kiosk_msgfile, &mesg, HOF_MSG_NAME);
    messageItemSex = getmsg(&kiosk_msgfile, &mesg, HOF_MSG_SEX);
    messageItemLevel = getmsg(&kiosk_msgfile, &mesg, HOF_MSG_LEVEL);
    messageItemExp = getmsg(&kiosk_msgfile, &mesg, HOF_MSG_EXP);
    messageItemChip = getmsg(&kiosk_msgfile, &mesg, HOF_MSG_CHIP);
    messageItemCath = getmsg(&kiosk_msgfile, &mesg, HOF_MSG_CATH);
    messageItemMaster = getmsg(&kiosk_msgfile, &mesg, HOF_MSG_MASTER);
    messageItemDC = getmsg(&kiosk_msgfile, &mesg, HOF_MSG_DC);

    messageItemYes = getmsg(&kiosk_msgfile, &mesg, HOF_MSG_YES);
    messageItemNo = getmsg(&kiosk_msgfile, &mesg, HOF_MSG_NO);

    return 0;
}

#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

void hof_load_entries() {
    char pattern[COMPAT_MAX_PATH];
    snprintf(pattern, sizeof(pattern), "chars/*.char");
    int ii = 0;

#ifdef _WIN32
    struct _finddata_t fileinfo;
    intptr_t handle = _findfirst(pattern, &fileinfo);
    if (handle == -1L) return;

    do {
        char filepath[COMPAT_MAX_PATH];
        snprintf(filepath, sizeof(filepath), "chars/%s", fileinfo.name);
        hof_process_file(filepath, ii);
        ++ii;
    } while (_findnext(handle, &fileinfo) == 0);

    _findclose(handle);
#else
    DIR *dir = opendir("chars");
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strstr(ent->d_name, ".char")) {
            char filepath[COMPAT_MAX_PATH];
            snprintf(filepath, sizeof(filepath), "chars/%s", ent->d_name);
            hof_process_file(filepath, ii);
            ++ii;
        }
    }
    closedir(dir);
#endif

    hof_window.num_entries = ii;

    hof_window.sort_column = HOF_SORT_RANK;
    hof_window.sort_ascending = false;
    hof_perform_sort();

    int last_char_page = 0;
    unsigned int last_char_hash = get_last_char_hash();

    // Фиксируем ранги после первоначальной сортировки
    for (int i = 0; i < hof_window.num_entries; i++) {
        hof_window.entries[i].permanent_rank = i + 1;
        if(last_char_hash == hof_window.entries[i].hash)
            last_char_page = (i+1) / HOF_ENTRIES_PER_PAGE;
    }

    if(last_char_page != 0)
        hof_window.current_page = last_char_page;
}

unsigned int hof_hash_path(const char* path) {
    return fnv1a_hash(path, strlen(path));
}

void hof_process_file(const char* path, int index) {
    Config hof_config;
    if (config_init(&hof_config)) {
        if (config_load(&hof_config, path, false)) {
            HallOfFameEntry* entry = &hof_window.entries[index];
            char *str;
            config_get_string(&hof_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_NAME_KEY, &str);
            memcpy(&entry->name, str, strlen(str)*sizeof(char));

            config_get_string(&hof_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_SEX_KEY, &str);
            memcpy(&entry->sex, str, strlen(str)*sizeof(char));

            config_get_string(&hof_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_KILLER_KEY, &str);
            memcpy(&entry->death_cause, str, strlen(str)*sizeof(char));

            config_get_value(&hof_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_LVL_KEY, &entry->level);
            config_get_value(&hof_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_EXP_KEY, &entry->exp);

            config_get_value(&hof_config, CHAR_CONFIG_QUEST_KEY, CHAR_CONFIG_QUEST_FOUND_CHIP_KEY, (int*)&entry->water_chip_found);
            config_get_value(&hof_config, CHAR_CONFIG_QUEST_KEY, CHAR_CONFIG_QUEST_DESTROY_VATS_KEY, (int*)&entry->cathedral_destroyed);
            config_get_value(&hof_config, CHAR_CONFIG_QUEST_KEY, CHAR_CONFIG_QUEST_KILL_MASTER_KEY, (int*)&entry->master_destroyed);

            config_get_value(&hof_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_LIFETIME_KEY, &entry->days);
            config_get_value(&hof_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_LIFETIME_KEY, &entry->hours);
            config_get_value(&hof_config, CHAR_CONFIG_CHAR_KEY, CHAR_CONFIG_CHAR_LIFETIME_KEY, &entry->minutes);

            hof_increase_vic_point(entry);

            entry->hash = hof_hash_path(path);
        }

        config_exit(&hof_config);
    }
}

//only first sorting
static void hof_perform_sort() {
    if (hof_window.num_entries <= 1) return;

    bool swapped;
    int n = hof_window.num_entries;

    do {
        swapped = false;
        for (int i = 1; i < n; i++) {
            HallOfFameEntry* a = &hof_window.entries[i-1];
            HallOfFameEntry* b = &hof_window.entries[i];
            bool need_swap = false;

            //if (hof_window.sort_column == HOF_SORT_RANK) {
                // Специальная логика для ранжирования
                int cmp = b->victory_score - a->victory_score;
                if (cmp == 0) {
                    if (b->master_destroyed != a->master_destroyed) {
                        cmp = b->master_destroyed ? 1 : -1;
                    } else if (b->cathedral_destroyed != a->cathedral_destroyed) {
                        cmp = b->cathedral_destroyed ? 1 : -1;
                    } else if (b->water_chip_found != a->water_chip_found) {
                        cmp = b->water_chip_found ? 1 : -1;
                    } else {
                        cmp = b->exp - a->exp;
                    }
                }
                need_swap = hof_window.sort_ascending ? (cmp < 0) : (cmp > 0);

            if (need_swap) {
                HallOfFameEntry tmp = *a;
                *a = *b;
                *b = tmp;
                swapped = true;
            }
        }
        n--;
    } while (swapped);
}

void hof_increase_vic_point(HallOfFameEntry *entry) {

    entry->victory_score = 0;
    if (entry->master_destroyed) entry->victory_score += 100000;
    if (entry->cathedral_destroyed) entry->victory_score += 10000;
    if (entry->water_chip_found) entry->victory_score += 1000;

    entry->victory_score += entry->exp / 100;

    return;
}

static int hof_compare_entries(const void* a, const void* b) {
    const HallOfFameEntry* entry_a = (const HallOfFameEntry*)a;
    const HallOfFameEntry* entry_b = (const HallOfFameEntry*)b;
    int result = 0;

    switch (hof_window.sort_column) {
    case HOF_SORT_RANK:
        result = entry_a->victory_score - entry_b->victory_score;
        if (result == 0) {
            if (entry_a->master_destroyed != entry_b->master_destroyed) {
                result = entry_a->master_destroyed ? 1 : -1;
            }
            else if (entry_a->cathedral_destroyed != entry_b->cathedral_destroyed) {
                result = entry_a->cathedral_destroyed ? 1 : -1;
            }
            else if (entry_a->water_chip_found != entry_b->water_chip_found) {
                result = entry_a->water_chip_found ? 1 : -1;
            }
            else {
                result = entry_a->exp - entry_b->exp;
            }
        }
        break;

    case HOF_SORT_NAME:
        result = strcmp(entry_a->name, entry_b->name);
        break;

    case HOF_SORT_SEX:
        result = strcmp(entry_a->sex, entry_b->sex);
        break;

    case HOF_SORT_LEVEL:
        result = entry_a->level - entry_b->level;
        break;

    case HOF_SORT_EXP:
        result = entry_a->exp - entry_b->exp;
        break;

    case HOF_SORT_WATER_CHIP:
        result = (entry_a->water_chip_found ? 1 : 0) - (entry_b->water_chip_found ? 1 : 0);
        break;

    case HOF_SORT_CATHEDRAL:
        result = (entry_a->cathedral_destroyed ? 1 : 0) - (entry_b->cathedral_destroyed ? 1 : 0);
        break;

    case HOF_SORT_MASTER:
        result = (entry_a->master_destroyed ? 1 : 0) - (entry_b->master_destroyed ? 1 : 0);
        break;

    case HOF_SORT_DEATH_CAUSE:
        result = strcmp(entry_a->death_cause, entry_b->death_cause);
        break;

    case HOF_SORT_TIME:
        result = (entry_a->days * 1440 + entry_a->hours * 60 + entry_a->minutes) -
            (entry_b->days * 1440 + entry_b->hours * 60 + entry_b->minutes);
        break;
    }

    return hof_window.sort_ascending ? result : -result;
}

static void hof_sort_entries() {
    if (hof_window.num_entries > 1) {
        qsort(hof_window.entries, hof_window.num_entries, sizeof(HallOfFameEntry), hof_compare_entries);
    }
}
/*
void hof_draw_victory_icon(int x, int y, unsigned char* buf, int pitch) {
    // Загружаем арт звезды (предполагая, что он есть в game/art/)
    int star_fid = art_id(OBJ_TYPE_INTERFACE, 339, 0, 0, 0); // ID арта
    CacheEntry* star_handle;
    Art* star_art = art_ptr_lock(star_fid, &star_handle);

    if (star_art) {
        unsigned char* star_pixels = art_frame_data(star_art, 0, 0);
        if (star_pixels) {
            // Рисуем звезду в буфере
            buf_to_buf(star_pixels,
                art_frame_width(star_art, 0, 0),
                art_frame_length(star_art, 0, 0),
                art_frame_width(star_art, 0, 0),
                buf + pitch * y + x,
                pitch);
        }
        art_ptr_unlock(star_handle);
    } else {
        // Fallback: рисуем текстовую звезду, если арта нет
        text_to_buf(buf + pitch * y + x, "*", pitch, pitch, colorTable[15855]);
    }
}
*/
// ================== ОТОБРАЖЕНИЕ ==================
void hof_redraw() {
    if (hof_window.window_id == -1) return;

    unsigned char* buf = win_get_buf(hof_window.window_id);
    int width = win_width(hof_window.window_id);
    int height = win_height(hof_window.window_id);

    int base_offset = HOF_LINE_OFFSET;

    // Очистка фона
    //buf_fill(buf, width, height, width, colorTable[0x323232]);

    hof_draw_back();

    // Заголовки столбцов
    const char* headers[] = {messageItemNum, messageItemName, messageItemSex,messageItemLevel, messageItemExp, messageItemChip, messageItemCath, messageItemMaster, messageItemDC};
    int columns[] = {20, 50, 155, 195, 225, 275, 325, 375, 430};
    int column_widths[] = {15, 45, 30, 30, 50, 30, 30, 30, 120};

    for (int i = 0; i < HOF_MAX_RAWS; i++) {
        // Текст заголовка
        text_to_buf(
            buf + width * 50 + columns[i] + base_offset,
            headers[i],
            width,
            width,
            (hof_window.sort_column == i) ? colorTable[15855] : colorTable[17969]
        );

        // Индикатор сортировки
        if (hof_window.sort_column == i) {
            //const char* arrow = hof_window.sort_ascending ? "↑" : "↓";
            const char* arrow = "";
            text_to_buf(
                buf + width * 50 + columns[i] + column_widths[i] - 15 + base_offset,
                arrow,
                width,
                width,
                colorTable[15855]
            );
        }

        // Кнопки для сортировки
        if (hof_window.column_buttons[i] == -1) {
            hof_window.column_buttons[i] = win_register_button(
                hof_window.window_id,
                columns[i] + base_offset, 50, column_widths[i], 20,
                -1, -1, -1, -1,
                NULL, NULL, NULL, 0
            );
            win_register_button_func(
                hof_window.column_buttons[i],
                NULL, NULL, NULL,
                hof_column_click
            );
        }
    }

    // Записи
    int start = hof_window.current_page * HOF_ENTRIES_PER_PAGE;
    char lvl[2];
    char exp[6];
    unsigned int last_char_hash = get_last_char_hash();

    for (int i = 0; i < HOF_ENTRIES_PER_PAGE; i++) {
        int entry_idx = start + i;
        if (entry_idx >= hof_window.num_entries) break;

        HallOfFameEntry* entry = &hof_window.entries[entry_idx];
        sprintf(lvl, "%d", entry->level);
        sprintf(exp, "%d", entry->exp);

        bool is_hero = entry->water_chip_found && entry->cathedral_destroyed && entry->master_destroyed;
        int y = 80 + i * 25;

        //star
        //if(is_hero)
        //    hof_draw_victory_icon(base_offset, 0, buf, y);

        // Порядковый номер
        char num_buf[8];
        //snprintf(num_buf, sizeof(num_buf), "%d", entry_idx + 1);
        snprintf(num_buf, sizeof(num_buf), "%d", entry->permanent_rank);
        text_to_buf(
            buf + width * y + columns[0] + base_offset,
            num_buf,
            width,
            width,
            (last_char_hash == entry->hash ? colorTable[32747] : colorTable[14723])
        );

        // Имя
        text_to_buf(
            buf + width * y + columns[1] + base_offset,
            entry->name,
            width,
            width,
            (last_char_hash == entry->hash ? colorTable[32747] : colorTable[14723])
        );

        // Пол
        text_to_buf(
            buf + width * y + columns[2] + base_offset,
            entry->sex,
            width,
            width,
            (last_char_hash == entry->hash ? colorTable[32747] : colorTable[14723])
        );

        // Уровень
        text_to_buf(
            buf + width * y + columns[3] + base_offset,
            lvl,
            width,
            width,
            (last_char_hash == entry->hash ? colorTable[32747] : colorTable[14723])
        );

        // Опыт
        text_to_buf(
            buf + width * y + columns[4] + base_offset,
            exp,
            width,
            width,
            (last_char_hash == entry->hash ? colorTable[32747] : colorTable[14723])
        );

        // Водный чип
        text_to_buf(
            buf + width * y + columns[5] + base_offset,
            entry->water_chip_found ? messageItemYes : messageItemNo,
            width,
            width,
            entry->water_chip_found ? colorTable[16191] : colorTable[15855]
        );

        // Собор
        text_to_buf(
            buf + width * y + columns[6] + base_offset,
            entry->cathedral_destroyed ? messageItemYes : messageItemNo,
            width,
            width,
            entry->cathedral_destroyed ? colorTable[16191] : colorTable[15855]
        );

        // Мастер
        text_to_buf(
            buf + width * y + columns[7] + base_offset,
            entry->master_destroyed ? messageItemYes : messageItemNo,
            width,
            width,
            entry->master_destroyed ? colorTable[16191] : colorTable[15855]
        );

        // Причина смерти
        text_to_buf(
            buf + width * y + columns[8] + base_offset,
            entry->death_cause,
            width,
            width,
            (last_char_hash == entry->hash ? colorTable[32747] : colorTable[14723])
        );

        // Звезда для победителей
        if (entry->master_destroyed && entry->cathedral_destroyed && entry->water_chip_found) {
            text_to_buf(
                buf + width * y + 5 + base_offset,
                //"★",
                "*",
                width,
                width,
                colorTable[32747]
            );
        }
    }

    initial_sort = true;

    // Пагинация
    char page_str[32];

    PrintBigNum(185, height-38, ANIMATE, hof_window.current_page+1, old_page+1, hof_window.window_id);

    win_draw(hof_window.window_id);
}

// ================== ОБРАБОТКА ВВОДА ==================
void hof_column_click(int btn_id, int key_code) {
    for (int i = 0; i < HOF_MAX_RAWS; i++) {
        if (hof_window.column_buttons[i] == btn_id) {
            if (hof_window.sort_column == i) {
                hof_window.sort_ascending = !hof_window.sort_ascending;
            } else {
                hof_window.sort_column = HofSortColumn(i);
                if(hof_window.sort_column == HOF_SORT_NAME)
                    hof_window.sort_ascending = true;
                else
                    hof_window.sort_ascending = false;
            }
            hof_sort_entries();
            hof_redraw();
            break;
        }
    }
}

int hof_handle_input(int key) {
    if (hof_window.window_id == -1) return -1;

    old_page = hof_window.current_page;
    switch (key) {
    case KEY_ARROW_LEFT:
        if (hof_window.current_page > 0) {
            hof_window.current_page--;
            hof_redraw();
        }
        break;

    case KEY_ARROW_RIGHT:
        if ((hof_window.current_page + 1) * HOF_ENTRIES_PER_PAGE < hof_window.num_entries) {
            hof_window.current_page++;
            hof_redraw();
        }
        break;

    case KEY_ESCAPE:
        hof_destroy_window();
        break;
    }

    return 0;
}

// ================== СОЗДАНИЕ/ЗАКРЫТИЕ ОКНА ==================
void hof_create_window() {
    loadColorTable("color.pal");
    palette_fade_to(cmap);

    hof_init();
    hof_load_entries();

    hof_window.window_id = win_add(
        (screenGetWidth() - HOF_WINDOW_WIDTH) / 2,
        (screenGetHeight() - HOF_WINDOW_HEIGHT) / 2,
        HOF_WINDOW_WIDTH,
        HOF_WINDOW_HEIGHT,
        256,
        WINDOW_DONT_MOVE_TOP
    );

    if (hof_window.window_id == -1) return;

    // Кнопка закрытия
    win_register_button(
        hof_window.window_id,
        580, 360, 50, 30,
        -1, -1, -1, KEY_ESCAPE,
        NULL, NULL, NULL, 0
    );

    bool cursorWasHidden = mouse_hidden();
    if (cursorWasHidden) {
        mouse_show();
    }

    hof_redraw();
}

int hof_draw_back() {
    CacheEntry* backgroundFrmHandle;

    // hof.frm - hall of fame screen background
    int fid = art_id(OBJ_TYPE_INTERFACE, 338, 0, 0, 0);
    unsigned char* backgroundFrmData = art_ptr_lock_data(fid, 0, 0, &backgroundFrmHandle);
    if (backgroundFrmData == NULL) {
        return -1;
    }

    //int windowWidth = HOF_WINDOW_WIDTH;
    unsigned char* windowBuffer = win_get_buf(hof_window.window_id);
    buf_to_buf(backgroundFrmData, HOF_WINDOW_WIDTH, HOF_WINDOW_HEIGHT, HOF_WINDOW_WIDTH, windowBuffer, HOF_WINDOW_WIDTH);
    art_ptr_unlock(backgroundFrmHandle);

    return 0;
}

//copied from editor.cc
void PrintBigNum(int x, int y, int flags, int value, int previousValue, int windowHandle)
{
    Rect rect;
    int windowWidth;
    unsigned char* windowBuf;
    int tens;
    int ones;
    unsigned char* tensBufferPtr;
    unsigned char* onesBufferPtr;
    unsigned char* numbersGraphicBufferPtr;

    windowWidth = win_width(windowHandle);
    windowBuf = win_get_buf(windowHandle);

    rect.ulx = x;
    rect.uly = y;
    rect.lrx = x + BIG_NUM_WIDTH * 2;
    rect.lry = y + BIG_NUM_HEIGHT;

    numbersGraphicBufferPtr = bignum;

    if (flags & RED_NUMBERS) {
        // First half of the bignum.frm is white,
        // second half is red.
        numbersGraphicBufferPtr += GInfoBigNum.width / 2;
    }

    tensBufferPtr = windowBuf + windowWidth * y + x;
    onesBufferPtr = tensBufferPtr + BIG_NUM_WIDTH;

    if (value >= 0 && value <= 99 && previousValue >= 0 && previousValue <= 99) {
        tens = value / 10;
        ones = value % 10;

        if (flags & ANIMATE) {
            if (previousValue % 10 != ones) {
                frame_time = get_time();
                buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * 11,
                    BIG_NUM_WIDTH,
                    BIG_NUM_HEIGHT,
                    GInfoBigNum.width,
                    onesBufferPtr,
                    windowWidth);
                win_draw_rect(windowHandle, &rect);
                renderPresent();
                while (elapsed_time(frame_time) < BIG_NUM_ANIMATION_DELAY)
                    ;
            }

            buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * ones,
                BIG_NUM_WIDTH,
                BIG_NUM_HEIGHT,
                GInfoBigNum.width,
                onesBufferPtr,
                windowWidth);
            win_draw_rect(windowHandle, &rect);
            renderPresent();

            if (previousValue / 10 != tens) {
                frame_time = get_time();
                buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * 11,
                    BIG_NUM_WIDTH,
                    BIG_NUM_HEIGHT,
                    GInfoBigNum.width,
                    tensBufferPtr,
                    windowWidth);
                win_draw_rect(windowHandle, &rect);
                while (elapsed_time(frame_time) < BIG_NUM_ANIMATION_DELAY)
                    ;
            }

            buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * tens,
                BIG_NUM_WIDTH,
                BIG_NUM_HEIGHT,
                GInfoBigNum.width,
                tensBufferPtr,
                windowWidth);
            win_draw_rect(windowHandle, &rect);
            renderPresent();
        } else {
            buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * tens,
                BIG_NUM_WIDTH,
                BIG_NUM_HEIGHT,
                GInfoBigNum.width,
                tensBufferPtr,
                windowWidth);
            buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * ones,
                BIG_NUM_WIDTH,
                BIG_NUM_HEIGHT,
                GInfoBigNum.width,
                onesBufferPtr,
                windowWidth);
        }
    } else {

        buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * 9,
            BIG_NUM_WIDTH,
            BIG_NUM_HEIGHT,
            GInfoBigNum.width,
            tensBufferPtr,
            windowWidth);
        buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * 9,
            BIG_NUM_WIDTH,
            BIG_NUM_HEIGHT,
            GInfoBigNum.width,
            onesBufferPtr,
            windowWidth);
    }
}

void hof_destroy_window() {
    palette_fade_to(black_palette);

    if (hof_window.window_id != -1) {
        for (int i = 0; i < HOF_MAX_RAWS; i++) {
            if (hof_window.column_buttons[i] != -1) {
                win_delete_button(hof_window.column_buttons[i]);
            }
        }

        if (prev_button != -1) {
            win_delete_button(prev_button);
            prev_button = -1;
        }

        if (prev_button_down != NULL) {
            art_ptr_unlock(prev_button_down_key);
            prev_button_down_key = NULL;
            prev_button_down = NULL;
        }

        if (prev_button_up != NULL) {
            art_ptr_unlock(prev_button_up_key);
            prev_button_up_key = NULL;
            prev_button_up = NULL;
        }

        if (next_button != -1) {
            win_delete_button(next_button);
            next_button = -1;
        }

        if (next_button_down != NULL) {
            art_ptr_unlock(next_button_down_key);
            next_button_down_key = NULL;
            next_button_down = NULL;
        }

        if (next_button_up != NULL) {
            art_ptr_unlock(next_button_up_key);
            next_button_up_key = NULL;
            next_button_up = NULL;
        }

        if (back_button != -1) {
            win_delete_button(back_button);
            back_button = -1;
        }

        if (back_button_down != NULL) {
            art_ptr_unlock(back_button_down_key);
            back_button_down_key = NULL;
            back_button_down = NULL;
        }

        if (back_button_up != NULL) {
            art_ptr_unlock(back_button_up_key);
            back_button_up_key = NULL;
            back_button_up = NULL;
        }

        if (bignum != NULL) {
            art_ptr_unlock(bignum_key);
            bignum_key = NULL;
            bignum = NULL;
        }

        win_delete(hof_window.window_id);
        hof_window.window_id = -1;
    }
}


}
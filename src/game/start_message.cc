#include "game/start_message.h"

#include "game/art.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gkioskconf.h"
#include "game/gsound.h"
#include "game/message.h"
#include "game/palette.h"

#include "plib/color/color.h"
#include "plib/gnw/button.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"
#include "platform_compat.h"

namespace fallout {

#define SM_WINDOW_WIDTH 640
#define SM_WINDOW_HEIGHT 480
#define SM_WINDOW_BACKGROUND_X 40
#define SM_WINDOW_BACKGROUND_Y 30
#define SM_WINDOW_BACKGROUND_WIDTH 560
#define SM_WINDOW_BACKGROUND_HEIGHT 300

#define SM_WINDOW_NEXT_BUTTON_X 435
#define SM_WINDOW_NEXT_BUTTON_Y 320

#define SM_WINDOW_BACK_BUTTON_X 435
#define SM_WINDOW_BACK_BUTTON_Y 425

#define SM_KNOB_WIDTH 22
#define SM_KNOB_HEIGHT 25

#define SM_KNOB_DIFFICULTY_X 167
#define SM_KNOB_DIFFICULTY_Y 352

#define SM_KNOB_LFILTER_X 167
#define SM_KNOB_LFILTER_Y 416

static int fontsave = 0;

int start_message_window_id = -1;
static unsigned char* start_message_window_buffer = NULL;
static unsigned char* monitor = NULL;
static Rect monitor_rect = { 40, 30, 599, 329 };

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

static int difficulty_knob_button = -1;
static unsigned char* difficulty_knob;
static CacheEntry* difficulty_knob_key = NULL;

static int language_knob_button = -1;
static unsigned char* language_knob;
static CacheEntry* language_knob_key = NULL;

static MessageList options_msg_file;
static MessageList kiosk_msg_file;
static MessageListItem mesg;

int exp_start = 0;
int t_difficulty = 0;
int t_lfilter = 0;
int game_difficulty = 0;
int combat_difficulty = 0;
int language_filter = 0;

bool needsRefresh = false;

static bool start_message_fatal_error(bool rc);
static void start_message_exit();
int start_message_init();
int start_message_msg_load();
int print_display_data();
int start_message_knob_init(unsigned char** knob, int* button, CacheEntry** key, int kx, int ky, int fy, int default_v, int msg_title, int msg_off, int msg_on);
int start_message_knob_set(unsigned char* knob, int* status, int kx, int ky);

int start_message()
{
    int rc = 0;

    if(start_message_init() != 0)
        return -1;

    bool cursorWasHidden = mouse_hidden();
    if (cursorWasHidden) {
        mouse_show();
    }

    loadColorTable("color.pal");
    palette_fade_to(cmap);

    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        needsRefresh = false;

        int keyCode = get_input();
        switch (keyCode) {
        case KEY_RETURN:
            rc = 2;
            done = true;
            break;
        case KEY_ESCAPE:
            rc = 1;
            done = true;
            break;
        default:
            break;
        }

        start_message_knob_set(difficulty_knob, &t_difficulty, SM_KNOB_DIFFICULTY_X, SM_KNOB_DIFFICULTY_Y);
        start_message_knob_set(language_knob, &t_lfilter, SM_KNOB_LFILTER_X, SM_KNOB_LFILTER_Y);

        if(needsRefresh){
            win_draw(start_message_window_id);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    game_difficulty = (t_difficulty > 0 ? 2 : game_difficulty);
    combat_difficulty = (t_difficulty > 0 ? 2 : combat_difficulty);
    language_filter = t_lfilter;

    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, game_difficulty);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, combat_difficulty);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, language_filter);

    char gconfig_file_name[COMPAT_MAX_PATH];

    strcpy(gconfig_file_name, GAME_CONFIG_FILE_NAME);
    if (!config_save(&game_config, gconfig_file_name, false)) {
        return -1;
    }

    config_load(&game_config, gconfig_file_name, false);

    if (cursorWasHidden) {
        mouse_hide();
    }

    palette_fade_to(black_palette);
    start_message_exit();

    return rc;
}

int start_message_init()
{
    int backgroundFid;
    unsigned char* backgroundFrmData;

    if(start_message_window_id != -1){
        return -1;
    }

    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_EXP_START_KEY, &exp_start);

    config_get_value(&kiosk_config, KIOSK_CONFIG_OVERRIDE_KEY, KIOSK_CONFIG_OVERRIDE_GAME, &game_difficulty);
    config_get_value(&kiosk_config, KIOSK_CONFIG_OVERRIDE_KEY, KIOSK_CONFIG_OVERRIDE_COMBAT, &combat_difficulty);
    config_get_value(&kiosk_config, KIOSK_CONFIG_OVERRIDE_KEY, KIOSK_CONFIG_OVERRIDE_LFILTER, &language_filter);

    int startMessageWindowX = (screenGetWidth() - SM_WINDOW_WIDTH) / 2;
    int startMessageWindowY = (screenGetHeight() - SM_WINDOW_HEIGHT) / 2;
    start_message_window_id = win_add(startMessageWindowX, startMessageWindowY, SM_WINDOW_WIDTH, SM_WINDOW_HEIGHT, colorTable[0], 0);

    if(start_message_window_id == -1){
        return start_message_fatal_error(false);
    }

    start_message_window_buffer = win_get_buf(start_message_window_id);
    if (start_message_window_buffer == NULL) {
        return start_message_fatal_error(false);
    }

    CacheEntry* backgroundFrmHandle;
    backgroundFid = art_id(OBJ_TYPE_INTERFACE, 336, 0, 0, 0);
    backgroundFrmData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundFrmHandle);
    if (backgroundFrmData == NULL) {
        return start_message_fatal_error(false);
    }

    buf_to_buf(backgroundFrmData,
               SM_WINDOW_WIDTH,
               SM_WINDOW_HEIGHT,
               SM_WINDOW_WIDTH,
               start_message_window_buffer,
               SM_WINDOW_WIDTH);

    monitor = (unsigned char*)mem_malloc(SM_WINDOW_BACKGROUND_WIDTH * SM_WINDOW_BACKGROUND_HEIGHT);
    if (monitor == NULL)
        return start_message_fatal_error(false);

    buf_to_buf(backgroundFrmData + SM_WINDOW_WIDTH * SM_WINDOW_BACKGROUND_Y + SM_WINDOW_BACKGROUND_X,
               SM_WINDOW_BACKGROUND_WIDTH,
               SM_WINDOW_BACKGROUND_HEIGHT,
               SM_WINDOW_WIDTH,
               monitor,
               SM_WINDOW_BACKGROUND_WIDTH);

    art_ptr_unlock(backgroundFrmHandle);

    int fid;

    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    next_button_up = art_ptr_lock_data(fid, 0, 0, &next_button_up_key);
    if (next_button_up == NULL) {
        return start_message_fatal_error(false);
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    next_button_down = art_ptr_lock_data(fid, 0, 0, &next_button_down_key);
    if (next_button_down == NULL) {
        return start_message_fatal_error(false);
    }

    next_button = win_register_button(start_message_window_id,
                                      SM_WINDOW_NEXT_BUTTON_X,
                                      SM_WINDOW_NEXT_BUTTON_Y,
                                      15,
                                      16,
                                      -1,
                                      -1,
                                      -1,
                                      KEY_RETURN,
                                      next_button_up,
                                      next_button_down,
                                      NULL,
                                      BUTTON_FLAG_TRANSPARENT);
    if (next_button == -1) {
        return start_message_fatal_error(false);
    }

    win_register_button_sound_func(next_button, gsound_red_butt_press, gsound_red_butt_release);

    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    back_button_up = art_ptr_lock_data(fid, 0, 0, &back_button_up_key);
    if (back_button_up == NULL) {
        return start_message_fatal_error(false);
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    back_button_down = art_ptr_lock_data(fid, 0, 0, &back_button_down_key);
    if (back_button_down == NULL) {
        return start_message_fatal_error(false);
    }

    back_button = win_register_button(start_message_window_id,
                                      SM_WINDOW_BACK_BUTTON_X,
                                      SM_WINDOW_BACK_BUTTON_Y,
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
        return start_message_fatal_error(false);
    }

    win_register_button_sound_func(back_button, gsound_red_butt_press, gsound_red_butt_release);

    if(start_message_msg_load() != 0)
        return -1;

    print_display_data();
    start_message_knob_init(&difficulty_knob, &difficulty_knob_button, &difficulty_knob_key, SM_KNOB_DIFFICULTY_X, SM_KNOB_DIFFICULTY_Y, 0, t_difficulty, 101, 203+game_difficulty, 205);
    start_message_knob_init(&language_knob, &language_knob_button, &language_knob_key, SM_KNOB_LFILTER_X, SM_KNOB_LFILTER_Y, 0, t_lfilter, 108, 202, 201);

    win_draw(start_message_window_id);
    win_draw_rect(start_message_window_id, &monitor_rect);

    return 0;
}

int start_message_knob_init(unsigned char** knob, int* button, CacheEntry** key, int kx, int ky, int fy, int default_v, int msg_title, int msg_off, int msg_on)
{
    int fid = 0;
    int x = SM_KNOB_WIDTH;
    int y = fy;

    fid = art_id(OBJ_TYPE_INTERFACE, 243, 0, 0, 0);
    *knob = art_lock(fid, &*key, &x, &y);

    if (*knob == NULL) {
        art_ptr_unlock(*key);
        return -1;
    }

    buf_to_buf(*knob + 640 * 19 + 251, 113, 34, 640, start_message_window_buffer + 640 * 19 + 251, 640);
    trans_buf_to_buf(*knob + (SM_KNOB_WIDTH * SM_KNOB_HEIGHT) * default_v, SM_KNOB_WIDTH, SM_KNOB_HEIGHT, SM_KNOB_WIDTH, start_message_window_buffer + 640 * ky + kx, 640);

    char* messageItemText;

    start_message_window_buffer = win_get_buf(start_message_window_id);

    fontsave = text_curr();
    text_font(103);

    messageItemText = getmsg(&options_msg_file, &mesg, msg_title);
    text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH * (ky - 28) + (kx - 85), messageItemText, SM_WINDOW_WIDTH, SM_WINDOW_WIDTH, colorTable[18979]);

    text_font(100);

    messageItemText = getmsg(&options_msg_file, &mesg, msg_off);
    text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH * (ky - 7) + (kx - 35), messageItemText, SM_WINDOW_WIDTH, SM_WINDOW_WIDTH, colorTable[18979]);

    messageItemText = getmsg(&options_msg_file, &mesg, msg_on);
    text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH * (ky - 7) + (kx + 21), messageItemText, SM_WINDOW_WIDTH, SM_WINDOW_WIDTH, colorTable[18979]);

    text_font(fontsave);

    //x = minX = 0, y = knobY - 5, width = maxX(0) - x
    int b = win_register_button(
            start_message_window_id,
            0,
            ky - 5,
            0,
            28,
            -1,
            -1,
            -1,
            505,
        NULL,
            NULL,
            NULL,
            32
        );
    *button = b;

    return 0;
}

int start_message_knob_set(unsigned char* knob, int* status, int kx, int ky)
{
    if(knob == NULL)
        return -1;

    int* valuePtr = status;
    bool valueChanged = false;

    int v1 = kx + 11;
    int v2 = ky + 12;
    int minX = 0;
    int maxX = 0;

    int x;
    int y;
    if (!(mouse_get_buttons() & 0x10)) {return -1;}
    mouseGetPositionInWindow(start_message_window_id, &x, &y);

    if (sqrt(pow((double)x - (double)v1, 2) + pow((double)y - (double)v2, 2)) > 10.0) {
        int v23 = ky - 5;
        if (y >= v23 && y <= v23 + text_height() + 2) {
            //minx = 0
            if (x >= minX && x <= kx) {
                *valuePtr = 0;
                valueChanged = true;
            //maxX = 0
            } else if (x >= kx + 22.0 && x <= maxX) {
                *valuePtr = 1;
                valueChanged = true;
            }
        }
    } else {
        *valuePtr ^= 1;
        valueChanged = true;
    }

    if (valueChanged) {
        buf_to_buf(knob + 640 * 19 + 251, 113, 34, 640, start_message_window_buffer + 640 * 19 + 251, 640);
        trans_buf_to_buf(knob + (22 * 25) * *valuePtr, 22, 25, 22, start_message_window_buffer + 640 * ky + kx, 640);

        gsound_play_sfx_file("ib2p1xx1");
        block_for_tocks(70);
        gsound_play_sfx_file("ib2lu1x1");

        needsRefresh = true;
    }

    return 0;
}

int start_message_msg_load()
{
    char path[COMPAT_MAX_PATH];

    if (!message_init(&kiosk_msg_file)) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", msg_path, "kiosk.msg");

    if (!message_load(&kiosk_msg_file, path)) {
        return -1;
    }

    if (!message_init(&options_msg_file)) {
        return -1;
    }

    memset(path, 0, sizeof(char) * COMPAT_MAX_PATH);
    snprintf(path, sizeof(path), "%s%s", msg_path, "options.msg");
    if (!message_load(&options_msg_file, path)) {
        return -1;
    }

    return 0;
}

static void start_message_exit()
{
    message_exit(&kiosk_msg_file);

    if(start_message_window_id != -1) {

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

        if (monitor != NULL) {
            mem_free(monitor);
            monitor = NULL;
        }

        win_delete(start_message_window_id);
        start_message_window_id = -1;
    }
}

int print_display_data()
{
    char* str;
    char tstr[200];
    int length = 0;

    fontsave = text_curr();

    int level = round(sqrt(exp_start / 1000 * 2 + 1));

    str = getmsg(&kiosk_msg_file, &mesg, 100);
    strcpy(tstr, str);

    text_font(101);

    text_char_width(0x20);
    length = text_width(tstr);
    text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH*64 + SM_WINDOW_WIDTH / 2 - length / 2, tstr, 640, SM_WINDOW_WIDTH, colorTable[992]);

    str = getmsg(&kiosk_msg_file, &mesg, 101);
    strcpy(tstr, str);

    length = text_width(tstr);

    text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH*255 + SM_WINDOW_WIDTH / 2 - length / 2, tstr, 640, SM_WINDOW_WIDTH, colorTable[992]);

    for(int i = 0; 11 > i; ++i){
        str = getmsg(&kiosk_msg_file, &mesg, 200+i);
        if(i == 7)
            snprintf(tstr, 200, "%s %d", str, level);
        else
            snprintf(tstr, 200, "%s %s", str, "");

        text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH*(80 + 16 * i) + 50, tstr, 640, SM_WINDOW_WIDTH, colorTable[992]);
    }

    for(int i = 0; 10 > i; ++i){
        str = getmsg(&kiosk_msg_file, &mesg, 400+i);
        text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH*(80 + 16 * i) + 420, str, 640, SM_WINDOW_WIDTH, colorTable[992]);
    }

    return 0;
}

static bool start_message_fatal_error(bool rc)
{
    start_message_exit();
    return rc;
}

}
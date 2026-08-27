#include "game/start_message.h"

#include "game/art.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gkioskconf.h"
#include "game/gsound.h"
#include "game/kiosk_msgfile.h"
#include "game/message.h"
#include "game/palette.h"
#include "game/wordwrap.h"

#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/gnw/button.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

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

#define SM_OPTIONS_PER_PAGE 10
#define SM_OPTIONS_Y 80
#define SM_LINE_HEIGHT 16
#define SM_OPTIONS_TEXT_X 50
#define SM_OPTIONS_VALUE_X 420
#define SM_OPTIONS_AREA_WIDTH 170
// #define SM_OPTIONS_AREA_WIDTH 170
#define SM_OPTIONS_AREA_HEIGHT ((SM_OPTIONS_PER_PAGE + 1) * SM_LINE_HEIGHT)
#define SM_OPTIONS_VALUE_FIRST 400
#define SM_OPTIONS_VALUE_COUNT 10
#define SM_PAGINATION_Y 255
#define SM_PAGINATION_CENTER_X ((SM_WINDOW_BACKGROUND_X + SM_OPTIONS_VALUE_X) / 2)
#define SM_PAGINATION_PREV_X (SM_PAGINATION_CENTER_X - 120)
#define SM_PAGINATION_NEXT_X (SM_PAGINATION_CENTER_X - 75)
#define SM_PAGINATION_FONT 101
#define SM_PAGINATION_ACTIVE_COLOR SM_GREEN_COLOR
#define SM_PAGINATION_INACTIVE_COLOR (SM_GREEN_COLOR - 1)
#define SM_TITLE_Y 64
#define SM_SUBTITLE_Y 255
#define SM_TEXT_COLOR 992
#define SM_GREEN_COLOR SM_TEXT_COLOR

typedef struct {
    int msgId;
    int* configPtr;
} KioskOptionDef;

static KioskOptionDef kioskOptions[] = {
    { 200, NULL },
    { 201, NULL },
    { 202, &gconfig_saveload_disabled },
    { 203, &gconfig_options_disabled },
    { 204, &gconfig_exp_start },
    { 205, &gconfig_caps_start },
    { 206, &gconfig_dialog_exit_0_allowed },
    { 207, &gconfig_game_exit_allowed },
    { 208, &gconfig_screensaver_enabled },
    { 209, &gconfig_random_locations },
    { 210, &gconfig_random_containers },
    { 211, &gconfig_quality_levels },
};

#define MSGID_START_LEVEL 204

#define NUM_KIOSK_OPTIONS (sizeof(kioskOptions) / sizeof(kioskOptions[0]))
#define SM_LEFT_MSG_FIRST 200
#define SM_LEFT_MSG_LAST 211
#define SM_LEFT_MSG_COUNT (SM_LEFT_MSG_LAST - SM_LEFT_MSG_FIRST + 1)

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
static MessageListItem mesg;

int exp_start = 0;
int t_difficulty = 0;
int t_lfilter = 0;
int game_difficulty = 0;
int combat_difficulty = 0;
int language_filter = 0;

bool needsRefresh = false;

static int sm_enabled_opts[SM_LEFT_MSG_COUNT];
static int sm_enabled_count = 0;
static int sm_current_page = 0;
static int sm_max_page = 0;

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

    if (start_message_init() != 0)
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

        // Pagination click handling
        if (mouse_get_buttons() & 0x10) {
            int mx, my;
            mouseGetPositionInWindow(start_message_window_id, &mx, &my);
            if (my >= SM_PAGINATION_Y && my < SM_PAGINATION_Y + text_height()) {
                if (sm_current_page > 0 && mx >= SM_PAGINATION_PREV_X && mx < SM_PAGINATION_PREV_X + 20) {
                    sm_current_page--;
                    needsRefresh = true;
                }
                if (sm_current_page < sm_max_page && mx >= SM_PAGINATION_NEXT_X && mx < SM_PAGINATION_NEXT_X + 20) {
                    sm_current_page++;
                    needsRefresh = true;
                }
            }
        }

        if (needsRefresh) {
            print_display_data();
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

    if (start_message_window_id != -1) {
        return -1;
    }

    config_get_value(&kiosk_config, KIOSK_CONFIG_GAME_KEY, KIOSK_CONFIG_EXP_START_KEY, &exp_start);

    config_get_value(&kiosk_config, KIOSK_CONFIG_OVERRIDE_KEY, KIOSK_CONFIG_OVERRIDE_GAME, &game_difficulty);
    config_get_value(&kiosk_config, KIOSK_CONFIG_OVERRIDE_KEY, KIOSK_CONFIG_OVERRIDE_COMBAT, &combat_difficulty);
    config_get_value(&kiosk_config, KIOSK_CONFIG_OVERRIDE_KEY, KIOSK_CONFIG_OVERRIDE_LFILTER, &language_filter);

    int startMessageWindowX = (screenGetWidth() - SM_WINDOW_WIDTH) / 2;
    int startMessageWindowY = (screenGetHeight() - SM_WINDOW_HEIGHT) / 2;
    start_message_window_id = win_add(startMessageWindowX, startMessageWindowY, SM_WINDOW_WIDTH, SM_WINDOW_HEIGHT, colorTable[0], 0);

    if (start_message_window_id == -1) {
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

    if (start_message_msg_load() != 0)
        return -1;

    print_display_data();
    start_message_knob_init(&difficulty_knob, &difficulty_knob_button, &difficulty_knob_key, SM_KNOB_DIFFICULTY_X, SM_KNOB_DIFFICULTY_Y, 0, t_difficulty, 101, 203 + game_difficulty, 205);
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

    // x = minX = 0, y = knobY - 5, width = maxX(0) - x
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
        32);
    *button = b;

    return 0;
}

int start_message_knob_set(unsigned char* knob, int* status, int kx, int ky)
{
    if (knob == NULL)
        return -1;

    int* valuePtr = status;
    bool valueChanged = false;

    int v1 = kx + 11;
    int v2 = ky + 12;
    int minX = 0;
    int maxX = 0;

    int x;
    int y;
    if (!(mouse_get_buttons() & 0x10)) {
        return -1;
    }
    mouseGetPositionInWindow(start_message_window_id, &x, &y);

    if (sqrt(pow((double)x - (double)v1, 2) + pow((double)y - (double)v2, 2)) > 10.0) {
        int v23 = ky - 5;
        if (y >= v23 && y <= v23 + text_height() + 2) {
            // minx = 0
            if (x >= minX && x <= kx) {
                *valuePtr = 0;
                valueChanged = true;
                // maxX = 0
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
    if (!kiosk_msgfile_initialized())
        return -1;

    char path[COMPAT_MAX_PATH];

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
    if (start_message_window_id != -1) {

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

static void sm_format_option_text(char* buf, size_t bufSize, int msgId, int level)
{
    const char* src = getmsg(&kiosk_msgfile, &mesg, msgId);
    if (msgId == MSGID_START_LEVEL) {
        snprintf(buf, bufSize, src, level);
    } else {
        snprintf(buf, bufSize, "%s", src);
    }
}

int print_display_data()
{
    char* str;
    char tstr[200];
    int length = 0;

    fontsave = text_curr();

    int level = round(sqrt(exp_start / 1000 * 2 + 1));

    str = getmsg(&kiosk_msgfile, &mesg, 100);
    strcpy(tstr, str);

    text_font(101);

    text_char_width(0x20);
    length = text_width(tstr);
    text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH * SM_TITLE_Y + SM_WINDOW_WIDTH / 2 - length / 2, tstr, 640, SM_WINDOW_WIDTH, colorTable[SM_TEXT_COLOR]);

    str = getmsg(&kiosk_msgfile, &mesg, 101);
    strcpy(tstr, str);

    length = text_width(tstr);

    text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH * SM_SUBTITLE_Y + SM_WINDOW_WIDTH / 2 - length / 2, tstr, 640, SM_WINDOW_WIDTH, colorTable[SM_TEXT_COLOR]);

    // Build list of enabled options
    sm_enabled_count = 0;
    for (int msgId = SM_LEFT_MSG_FIRST; msgId <= SM_LEFT_MSG_LAST; msgId++) {
        const char* msgStr = getmsg(&kiosk_msgfile, &mesg, msgId);
        if (msgStr == NULL || msgStr[0] == '\0') {
            continue;
        }
        KioskOptionDef* opt = NULL;
        for (int i = 0; i < (int)NUM_KIOSK_OPTIONS; i++) {
            if (kioskOptions[i].msgId == msgId) {
                opt = &kioskOptions[i];
                break;
            }
        }
        if (opt == NULL) {
            continue;
        }
        if (opt->configPtr == NULL) {
            sm_enabled_opts[sm_enabled_count++] = msgId;
            continue;
        }
        if (*opt->configPtr <= 0) {
            continue;
        }
        sm_enabled_opts[sm_enabled_count++] = msgId;
    }

    // Restore options area from monitor
    buf_to_buf(monitor + (SM_OPTIONS_Y - SM_WINDOW_BACKGROUND_Y) * SM_WINDOW_BACKGROUND_WIDTH,
        SM_WINDOW_BACKGROUND_WIDTH, SM_OPTIONS_AREA_HEIGHT,
        SM_WINDOW_BACKGROUND_WIDTH,
        start_message_window_buffer + SM_WINDOW_WIDTH * SM_OPTIONS_Y + SM_WINDOW_BACKGROUND_X,
        SM_WINDOW_WIDTH);

    // Calculate word-wrapped line count per option and total lines
    text_font(101);
    int lineHeight = text_height();
    int indentWidth = text_width("  ");
    int linesPerPage = SM_OPTIONS_AREA_HEIGHT / lineHeight;
    short wrapBuf[WORD_WRAP_MAX_COUNT];
    short wrapCount;
    int lineCounts[SM_LEFT_MSG_COUNT];
    int totalLines = 0;

    for (int i = 0; i < sm_enabled_count; i++) {
        sm_format_option_text(tstr, sizeof(tstr), sm_enabled_opts[i], level);

        if (word_wrap(tstr, SM_OPTIONS_AREA_WIDTH - indentWidth, wrapBuf, &wrapCount) == 0) {
            lineCounts[i] = wrapCount - 1;
        } else {
            lineCounts[i] = 1;
        }
        totalLines += lineCounts[i];
    }

    sm_max_page = (totalLines > 0) ? ((totalLines - 1) / linesPerPage) : 0;
    if (sm_current_page > sm_max_page) {
        sm_current_page = 0;
    }

    // Draw left column — word_wrap + vertical flow pagination
    int yy = SM_OPTIONS_Y;
    int currentGlobalLine = 0;
    int pageStartLine = sm_current_page * linesPerPage;
    int pageEndLine = pageStartLine + linesPerPage;

    for (int i = 0; i < sm_enabled_count && currentGlobalLine < pageEndLine; i++) {
        int msgId = sm_enabled_opts[i];

        if (currentGlobalLine + lineCounts[i] <= pageStartLine) {
            currentGlobalLine += lineCounts[i];
            continue;
        }

        sm_format_option_text(tstr, sizeof(tstr), msgId, level);

        char lineBuf[256];
        short breakpoints[WORD_WRAP_MAX_COUNT];
        short count;
        if (word_wrap(tstr, SM_OPTIONS_AREA_WIDTH - indentWidth, breakpoints, &count) == 0) {
            int startLine = (currentGlobalLine < pageStartLine) ? (pageStartLine - currentGlobalLine) : 0;

            for (int li = startLine; li < count - 1 && yy < SM_OPTIONS_Y + SM_OPTIONS_AREA_HEIGHT; li++) {
                char saved = tstr[breakpoints[li + 1]];
                tstr[breakpoints[li + 1]] = '\0';

                if (li == 0) {
                    snprintf(lineBuf, sizeof(lineBuf), "%s", tstr + breakpoints[li]);
                } else {
                    snprintf(lineBuf, sizeof(lineBuf), "  %s", tstr + breakpoints[li]);
                }
                text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH * yy + SM_OPTIONS_TEXT_X,
                    lineBuf, SM_OPTIONS_AREA_WIDTH, SM_WINDOW_WIDTH,
                    colorTable[SM_TEXT_COLOR]);

                tstr[breakpoints[li + 1]] = saved;
                yy += lineHeight;
            }
        }
        currentGlobalLine += lineCounts[i];
    }

    // Draw right column — all values, unchanged
    for (int i = 0; i < SM_OPTIONS_VALUE_COUNT; i++) {
        str = getmsg(&kiosk_msgfile, &mesg, SM_OPTIONS_VALUE_FIRST + i);
        text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH * (SM_OPTIONS_Y + SM_LINE_HEIGHT * i) + SM_OPTIONS_VALUE_X,
            str, 640, SM_WINDOW_WIDTH, colorTable[SM_TEXT_COLOR]);
    }

    // Pagination buttons
    text_font(SM_PAGINATION_FONT);
    if (sm_max_page > 0) {
        int prevColor = (sm_current_page > 0) ? SM_PAGINATION_ACTIVE_COLOR : SM_PAGINATION_INACTIVE_COLOR;
        int nextColor = (sm_current_page < sm_max_page) ? SM_PAGINATION_ACTIVE_COLOR : SM_PAGINATION_INACTIVE_COLOR;

        strcpy(tstr, "<");
        text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH * SM_PAGINATION_Y + SM_PAGINATION_PREV_X,
            tstr, 640, SM_WINDOW_WIDTH, colorTable[prevColor]);

        strcpy(tstr, ">");
        text_to_buf(start_message_window_buffer + SM_WINDOW_WIDTH * SM_PAGINATION_Y + SM_PAGINATION_NEXT_X,
            tstr, 640, SM_WINDOW_WIDTH, colorTable[nextColor]);
    }
    text_font(fontsave);

    return 0;
}

static bool start_message_fatal_error(bool rc)
{
    start_message_exit();
    return rc;
}

}

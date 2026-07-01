#include "game/screen_help.h"

#include <stdio.h>
#include <string.h>

#include "game/art.h"
#include "game/cycle.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/mainmenu.h"
#include "game/map.h"
#include "game/message.h"
#include "game/palette.h"
#include "plib/color/color.h"
#include "plib/gnw/button.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/input_rebind.h"
#include "plib/gnw/kb.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"
#include "plib/gnw/timer.h"
#include "platform_compat.h"

namespace fallout {

// CE: Help screen message IDs (800+)
#define MSG_HELP_MAIN_TITLE   800
#define MSG_HELP_GAME_TITLE   801
#define MSG_HELP_NEW_GAME     802
#define MSG_HELP_LOAD_GAME    803
#define MSG_HELP_CREDITS      804
#define MSG_HELP_EXIT         805
#define MSG_HELP_INTRO        806
#define MSG_HELP_GAMMA_UP     807
#define MSG_HELP_GAMMA_DOWN   808
#define MSG_HELP_SCREENSAVER  809
#define MSG_HELP_F2_HELP      810
#define MSG_HELP_VOL_DOWN     811
#define MSG_HELP_VOL_UP       812
#define MSG_HELP_SAVE_GAME    813
#define MSG_HELP_MAIN_MENU    814
#define MSG_HELP_AUTOMAP      815
#define MSG_HELP_COMBAT       816
#define MSG_HELP_INVENTORY    817
#define MSG_HELP_CHARACTER    818
#define MSG_HELP_PIPBOY       819
#define MSG_HELP_SKILLDEX     820
#define MSG_HELP_CHANGE_HAND  821
#define MSG_HELP_CENTER_VIEW  822
#define MSG_HELP_SCROLL_LEFT  823
#define MSG_HELP_SCROLL_RIGHT 824
#define MSG_HELP_SCROLL_UP    825
#define MSG_HELP_SCROLL_DOWN  826
#define MSG_HELP_ROTATE_LEFT  827
#define MSG_HELP_ROTATE_RIGHT 828
#define MSG_HELP_QUICK_SAVE   829
#define MSG_HELP_QUICK_LOAD   830
#define MSG_HELP_FOOTER       831
#define MSG_HELP_NAV_TITLE    833
#define MSG_HELP_CHAR_TITLE   834
#define MSG_HELP_SYS_TITLE    835
#define MSG_HELP_SKILL_BTN1   836
#define MSG_HELP_SKILL_BTN2   837
#define MSG_HELP_SKILL_BTN3   838
#define MSG_HELP_SKILL_BTN4   839
#define MSG_HELP_SKILL_BTN5   840
#define MSG_HELP_SKILL_BTN6   841
#define MSG_HELP_SKILL_BTN7   842
#define MSG_HELP_SKILL_BTN8   843
#define MSG_HELP_HEADER       866
#define MSG_HELP_BOOT_TITLE   860
#define MSG_HELP_BOOT_COPY    861
#define MSG_HELP_BOOT_CPU     862
#define MSG_HELP_BOOT_MEM     863
#define MSG_HELP_BOOT_CHIP    864
#define MSG_HELP_BOOT_TERM    865
#define MSG_HELP_CMD          867
#define MSG_HELP_END_TURN     868
#define MSG_HELP_END_COMBAT   869
#define MSG_HELP_REST         870
#define MSG_HELP_OPTIONS      871
#define MSG_HELP_CLOSE        872
#define MSG_HELP_TOGGLE_MOUSE 873
#define MSG_HELP_TOGGLE_MODE  874
#define MSG_HELP_MOVE_UP      875
#define MSG_HELP_MOVE_DOWN    876
#define MSG_HELP_PAGE_UP      877
#define MSG_HELP_PAGE_DOWN    878
#define MSG_HELP_MOVE_TOP     879
#define MSG_HELP_MOVE_BOTTOM  880
#define MSG_HELP_INV_TITLE    844
#define MSG_HELP_TAB_NAV      900
#define MSG_HELP_TAB_CHR      901
#define MSG_HELP_TAB_INV      902
#define MSG_HELP_TAB_SYS      903

// CE: Help screen message list and item
static MessageList help_msg;
static MessageListItem help_msg_item;

// CE: Convert key code to display name, considering user rebinding
static const char* key_display_name(int key_code)
{
    switch (key_code) {
    case KEY_ESCAPE:   return "[ESC]";
    case KEY_F1:       return "[F1]";
    case KEY_F2:       return "[F2]";
    case KEY_F3:       return "[F3]";
    case KEY_F4:       return "[F4]";
    case KEY_F5:       return "[F5]";
    case KEY_F6:       return "[F6]";
    case KEY_F7:       return "[F7]";
    case KEY_F8:       return "[F8]";
    case KEY_F9:       return "[F9]";
    case KEY_F10:      return "[F10]";
    case KEY_F11:      return "[F11]";
    case KEY_F12:      return "[F12]";
    case KEY_LEFT:     return "[LEFT]";
    case KEY_RIGHT:    return "[RIGHT]";
    case KEY_UP:       return "[UP]";
    case KEY_DOWN:     return "[DOWN]";
    case KEY_HOME:     return "[HOME]";
    case KEY_END:      return "[END]";
    case KEY_PAGE_UP:  return "[PGUP]";
    case KEY_PAGE_DOWN: return "[PGDN]";
    case KEY_TAB:      return "[TAB]";
    case KEY_ENTER:    return "[ENTER]";
    case KEY_BACKSPACE: return "[BS]";
    case KEY_DELETE:   return "[DEL]";
    case KEY_INSERT:   return "[INS]";
    default:           return NULL;
    }
}

// 0x43D130
int game_help()
{
    bool isoWasEnabled = map_disable_bk_processes();
    gmouse_3d_off();

    static CacheEntry* help_button_up_key = NULL;
    static CacheEntry* help_button_down_key = NULL;

    int saved_cursor = gmouse_get_cursor();
    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    bool colorCycleWasEnabled = cycle_is_enabled();
    cycle_disable();

    int overlay = win_add(0, 0, screenGetWidth(), screenGetHeight(), 0, WINDOW_HIDDEN | WINDOW_MOVE_ON_TOP | WINDOW_MODAL);

    int helpWindowX = (screenGetWidth() - HELP_SCREEN_WIDTH) / 2;
    int helpWindowY = (screenGetHeight() - HELP_SCREEN_HEIGHT) / 2;
    int win = win_add(helpWindowX, helpWindowY, HELP_SCREEN_WIDTH, HELP_SCREEN_HEIGHT, 0, WINDOW_HIDDEN | WINDOW_MOVE_ON_TOP | WINDOW_MODAL);
    if (win != -1) {
        unsigned char* windowBuffer = win_get_buf(win);
        if (windowBuffer != NULL) {
            static bool help_inited = false;
            if (!help_inited) {
                char path[COMPAT_MAX_PATH];
                snprintf(path, sizeof(path), "%s%s", msg_path, "help_screen.msg");
                help_inited = message_init(&help_msg) && message_load(&help_msg, path);
            }
            loadColorTable("color.pal");
            palette_set_to(cmap);

            buf_fill(win_get_buf(overlay),
                screenGetWidth(),
                screenGetHeight(),
                screenGetWidth(),
                intensityColorTable[colorTable[0]][0]);

            win_show(overlay);

            // CE: Load FRM 343 as background (keep locked for tab redraws)
            CacheEntry* bgKey;
            unsigned char* bgData = NULL;
            int bgW = 0;
            int bgH = 0;
            {
                int bgFid = art_id(OBJ_TYPE_INTERFACE, 344, 0, 0, 0);
                Art* bgArt = art_ptr_lock(bgFid, &bgKey);
                if (bgArt != NULL) {
                    bgW = art_frame_width(bgArt, 0, 0);
                    bgH = art_frame_length(bgArt, 0, 0);
                    bgData = art_frame_data(bgArt, 0, 0);
                    if (bgData != NULL && bgW > 0 && bgH > 0) {
                        // Fill window with dark background before blitting FRM
                        buf_fill(windowBuffer, HELP_SCREEN_WIDTH, HELP_SCREEN_HEIGHT, HELP_SCREEN_WIDTH, intensityColorTable[colorTable[0]][0]);
                        buf_to_buf(bgData, bgW, bgH, bgW, windowBuffer, HELP_SCREEN_WIDTH);
                    }
                }
            }

            // CE: Load done button FRMs
            int doneUpFid = art_id(OBJ_TYPE_INTERFACE, 91, 0, 0, 0);
            CacheEntry* doneUpKey;
            Art* doneUpArt = art_ptr_lock(doneUpFid, &doneUpKey);
            unsigned char* doneUpData = art_frame_data(doneUpArt, 0, 0);
            int doneW = art_frame_width(doneUpArt, 0, 0);
            int doneH = art_frame_length(doneUpArt, 0, 0);

            int doneDownFid = art_id(OBJ_TYPE_INTERFACE, 92, 0, 0, 0);
            CacheEntry* doneDownKey;
            Art* doneDownArt = art_ptr_lock(doneDownFid, &doneDownKey);
            unsigned char* doneDownData = art_frame_data(doneDownArt, 0, 0);

            // CE: Load tab button FRMs (lilredup/down.frm)
            int btnUpFid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
            CacheEntry* btnUpKey;
            unsigned char* btnUpData = art_ptr_lock_data(btnUpFid, 0, 0, &btnUpKey);

            int btnDownFid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
            CacheEntry* btnDownKey;
            unsigned char* btnDownData = art_ptr_lock_data(btnDownFid, 0, 0, &btnDownKey);

            // CE: Register done button
            int doneBtnId = -1;
            if (doneUpData != NULL && doneDownData != NULL) {
                doneBtnId = win_register_button(win,
                    498, 398, doneW, doneH,
                    -1, -1, -1, 500,
                    doneUpData, doneDownData, NULL,
                    BUTTON_FLAG_TRANSPARENT);
                if (doneBtnId != -1) {
                    win_register_button_sound_func(doneBtnId, gsound_red_butt_press, gsound_red_butt_release);
                }
            }

            // CE: Tab buttons (NAV, CHAR, INV, SYS)
            int tab_btn_ids[4] = {-1, -1, -1, -1};
            int current_tab = 0;
            if (btnUpData != NULL && btnDownData != NULL) {
                for (int i = 0; i < 4; i++) {
                    tab_btn_ids[i] = win_register_button(win,
                        585, 148 + i * 22, 15, 16,
                        -1, -1, -1, 501 + i,
                        btnUpData, btnDownData, NULL,
                        BUTTON_FLAG_TRANSPARENT);
                }
                text_font(101);
                win_register_button_image(tab_btn_ids[0], btnDownData, btnDownData, NULL, false);
            }

            win_show(win);
            text_font(101);

            // CE: Boot log / header
            {
                const int lx = 102;
                int y = 110;

                win_print(win, getmsg(&help_msg, &help_msg_item, MSG_HELP_HEADER), 0, lx, y, colorTable[992] | 0x2000000);
                y += 12;
                win_print(win, getmsg(&help_msg, &help_msg_item, MSG_HELP_BOOT_TITLE), 0, lx, y, colorTable[992] | 0x2000000);
                y += 11;
                win_print(win, getmsg(&help_msg, &help_msg_item, MSG_HELP_BOOT_COPY), 0, lx, y, colorTable[992] | 0x2000000);
                y += 11;
                win_print(win, getmsg(&help_msg, &help_msg_item, MSG_HELP_BOOT_CPU), 0, lx, y, colorTable[992] | 0x2000000);
                y += 11;
                win_print(win, getmsg(&help_msg, &help_msg_item, MSG_HELP_BOOT_MEM), 0, lx, y, colorTable[992] | 0x2000000);
                y += 11;
                win_print(win, getmsg(&help_msg, &help_msg_item, MSG_HELP_BOOT_CHIP), 0, lx, y, colorTable[992] | 0x2000000);
                y += 11;
                win_print(win, getmsg(&help_msg, &help_msg_item, MSG_HELP_BOOT_TERM), 0, lx, y, colorTable[992] | 0x2000000);
                y += 11;
                win_print(win, getmsg(&help_msg, &help_msg_item, MSG_HELP_CMD), 0, lx, y, colorTable[992] | 0x2000000);
            }

            // CE: Tab-based hotkey display
            const int C1 = 102;
            const int C1K = 180;
            const int ls = 11;

            auto key_name = [](int kc) -> const char* {
                kc = get_physical_key(SCREEN_GAME, kc);
                const char* n = key_display_name(kc);
                if (n != NULL) return n;
                static char buf[8];
                snprintf(buf, sizeof(buf), "[%c]", (char)kc);
                return buf;
            };

            auto draw_tab_content = [&](int t) {
                int y = 215;
                if (bgData != NULL && bgH > y) {
                    int rh = 200;
                    if (y + rh > bgH) rh = bgH - y;
                    buf_to_buf(bgData + y * bgW, bgW, rh, bgW, windowBuffer + y * HELP_SCREEN_WIDTH, HELP_SCREEN_WIDTH);
                }
                struct TabRow { int kc; int msg; };
                TabRow p[16];
                int n = 0;
                const char* title = "";

                if (t == 0) {
                    title = getmsg(&help_msg, &help_msg_item, MSG_HELP_NAV_TITLE);
                    n = 10;
                    p[0] = {KEY_ARROW_LEFT,  MSG_HELP_SCROLL_LEFT};
                    p[1] = {KEY_ARROW_RIGHT, MSG_HELP_SCROLL_RIGHT};
                    p[2] = {KEY_ARROW_UP,    MSG_HELP_SCROLL_UP};
                    p[3] = {KEY_ARROW_DOWN,  MSG_HELP_SCROLL_DOWN};
                    p[4] = {KEY_COMMA,       MSG_HELP_ROTATE_LEFT};
                    p[5] = {KEY_DOT,         MSG_HELP_ROTATE_RIGHT};
                    p[6] = {KEY_HOME,        MSG_HELP_CENTER_VIEW};
                    p[7] = {KEY_TAB,         MSG_HELP_AUTOMAP};
                    p[8] = {KEY_SPACE,       MSG_HELP_END_TURN};
                    p[9] = {KEY_RETURN,      MSG_HELP_END_COMBAT};
                } else if (t == 1) {
                    title = getmsg(&help_msg, &help_msg_item, MSG_HELP_CHAR_TITLE);
                    n = 15;
                    p[0]  = {KEY_1,           MSG_HELP_SKILL_BTN1};
                    p[1]  = {KEY_2,           MSG_HELP_SKILL_BTN2};
                    p[2]  = {KEY_3,           MSG_HELP_SKILL_BTN3};
                    p[3]  = {KEY_4,           MSG_HELP_SKILL_BTN4};
                    p[4]  = {KEY_5,           MSG_HELP_SKILL_BTN5};
                    p[5]  = {KEY_6,           MSG_HELP_SKILL_BTN6};
                    p[6]  = {KEY_7,           MSG_HELP_SKILL_BTN7};
                    p[7]  = {KEY_8,           MSG_HELP_SKILL_BTN8};
                    p[8]  = {KEY_UPPERCASE_C, MSG_HELP_CHARACTER};
                    p[9]  = {KEY_UPPERCASE_I, MSG_HELP_INVENTORY};
                    p[10] = {KEY_UPPERCASE_P, MSG_HELP_PIPBOY};
                    p[11] = {KEY_UPPERCASE_S, MSG_HELP_SKILLDEX};
                    p[12] = {KEY_UPPERCASE_B, MSG_HELP_CHANGE_HAND};
                    p[13] = {KEY_UPPERCASE_M, MSG_HELP_TOGGLE_MOUSE};
                    p[14] = {KEY_UPPERCASE_N, MSG_HELP_TOGGLE_MODE};
                } else if (t == 2) {
                    title = getmsg(&help_msg, &help_msg_item, MSG_HELP_INV_TITLE);
                    n = 6;
                    p[0] = {KEY_UP,      MSG_HELP_MOVE_UP};
                    p[1] = {KEY_DOWN,    MSG_HELP_MOVE_DOWN};
                    p[2] = {KEY_PAGE_UP, MSG_HELP_PAGE_UP};
                    p[3] = {KEY_PAGE_DOWN, MSG_HELP_PAGE_DOWN};
                    p[4] = {KEY_HOME,    MSG_HELP_MOVE_TOP};
                    p[5] = {KEY_END,     MSG_HELP_MOVE_BOTTOM};
                } else {
                    title = getmsg(&help_msg, &help_msg_item, MSG_HELP_SYS_TITLE);
                    n = 15;
                    p[0]  = {KEY_F4,          MSG_HELP_SAVE_GAME};
                    p[1]  = {KEY_F5,          MSG_HELP_LOAD_GAME};
                    p[2]  = {KEY_F6,          MSG_HELP_QUICK_SAVE};
                    p[3]  = {KEY_F7,          MSG_HELP_QUICK_LOAD};
                    p[4]  = {KEY_F12,         MSG_HELP_MAIN_MENU};
                    p[5]  = {KEY_UPPERCASE_Z, MSG_HELP_REST};
                    p[6]  = {KEY_UPPERCASE_O, MSG_HELP_OPTIONS};
                    p[7]  = {KEY_ESCAPE,      MSG_HELP_CLOSE};
                    p[8]  = {KEY_UPPERCASE_D, MSG_HELP_SCREENSAVER};
                    p[9]  = {KEY_F2,          MSG_HELP_F2_HELP};
                    p[10] = {KEY_UPPERCASE_H, MSG_HELP_F2_HELP};
                    p[11] = {KEY_PLUS,        MSG_HELP_GAMMA_UP};
                    p[12] = {KEY_MINUS,       MSG_HELP_GAMMA_DOWN};
                    p[13] = {KEY_F3,          MSG_HELP_VOL_UP};
                    p[14] = {KEY_F1,          MSG_HELP_VOL_DOWN};
                }

                win_print(win, title, 0, C1, y, colorTable[992] | 0x2000000);
                y += 14;

                {
                    buf_fill(windowBuffer + y * HELP_SCREEN_WIDTH + C1, 206, 1, HELP_SCREEN_WIDTH, colorTable[992] | 0x2000000);
                    y += 4;
                }

                for (int i = 0; i < n; i++) {
                    if (p[i].kc == 0) {
                        buf_fill(windowBuffer + y * HELP_SCREEN_WIDTH + C1, 206, 1, HELP_SCREEN_WIDTH, colorTable[992] | 0x2000000);
                        y += 4;
                    } else {
                        win_print(win, key_name(p[i].kc), 0, C1, y, colorTable[992] | 0x2000000);
                        win_print(win, getmsg(&help_msg, &help_msg_item, p[i].msg), 0, C1K, y, colorTable[992] | 0x2000000);
                        y += ls;
                    }
                }

                text_font(101);
                {
                    const int msgs[4] = {MSG_HELP_TAB_NAV, MSG_HELP_TAB_CHR, MSG_HELP_TAB_INV, MSG_HELP_TAB_SYS};
                    for (int i = 0; i < 4; i++) {
                        int lx = 582 - text_width(getmsg(&help_msg, &help_msg_item, msgs[i]));
                        win_print(win, getmsg(&help_msg, &help_msg_item, msgs[i]), 0, lx, 151 + i * 22, colorTable[21091] | 0x2000000);
                    }
                }
            };

            draw_tab_content(current_tab);

            // CE: Flush window buffer changes to screen display
            win_refresh_all(&scr_size);

            // CE: Event loop with tab switching support
            {
                int help_rc = MAIN_MENU_EXIT;

                while (game_user_wants_to_quit == 0) {
                    sharedFpsLimiter.mark();
                    renderPresent();
                    sharedFpsLimiter.throttle();

                    int keyCode = get_input();
                    if (keyCode == -1) {
                        continue;
                    }

                    if (keyCode >= 501 && keyCode <= 504) {
                        int new_tab = keyCode - 501;
                        if (new_tab != current_tab) {
                            win_register_button_image(tab_btn_ids[current_tab], btnUpData, btnDownData, NULL, false);
                            win_register_button_image(tab_btn_ids[new_tab], btnDownData, btnDownData, NULL, true);
                            current_tab = new_tab;
                            draw_tab_content(current_tab);
                            win_refresh_all(&scr_size);
                        }
                        continue;
                    }

                    if (keyCode == 500 || keyCode == KEY_ESCAPE || game_user_wants_to_quit == 3) {
                        main_menu_play_sound("nmselec1");
                        help_rc = MAIN_MENU_EXIT;
                        break;
                    }
                }
            }

            // CE: Handle mouse click.
            int rc = MAIN_MENU_EXIT;
            while (mouse_get_buttons() != 0) {
                sharedFpsLimiter.mark();

                int keyCode = get_input();
                if (keyCode != -1) {
                    if (keyCode == KEY_ESCAPE || game_user_wants_to_quit == 3) {
                        rc = MAIN_MENU_EXIT;
                        main_menu_play_sound("nmselec1");
                        break;
                    }
                }

                renderPresent();
                sharedFpsLimiter.throttle();
            }

            // CE: Cleanup buttons
            for (int i = 0; i < 4; i++) {
                if (tab_btn_ids[i] != -1) {
                    win_delete_button(tab_btn_ids[i]);
                }
            }
            if (doneBtnId != -1) {
                win_delete_button(doneBtnId);
            }
            if (bgData != NULL) {
                art_ptr_unlock(bgKey);
            }
            if (doneUpData != NULL) {
                art_ptr_unlock(doneUpKey);
            }
            if (doneDownData != NULL) {
                art_ptr_unlock(doneDownKey);
            }
            if (btnUpData != NULL) {
                art_ptr_unlock(btnUpKey);
            }
            if (btnDownData != NULL) {
                art_ptr_unlock(btnDownKey);
            }

            palette_set_to(black_palette);
        }

        win_delete(overlay);
        win_delete(win);
        loadColorTable("color.pal");
        palette_set_to(cmap);
    }

    if (colorCycleWasEnabled) {
        cycle_enable();
    }

    gmouse_set_cursor(saved_cursor);
    gmouse_3d_on();

    if (isoWasEnabled) {
        map_enable_bk_processes();
    }

    return MAIN_MENU_EXIT;
}

} // namespace fallout

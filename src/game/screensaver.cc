#include "game/screensaver.h"

#include <cstring>

#include "game/art.h"
#include "game/gconfig.h"
#include "game/gkioskconf.h"
#include "game/palette.h"
#include "game/roll.h"

#include "plib/color/color.h"

#include "plib/gnw/grbuf.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/hash_fnv-1a.h"
#include "plib/gnw/input.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/gnw.h"

namespace fallout {

bool Screensaver::initialized = false;
unsigned int Screensaver::timeout = 120;
unsigned int Screensaver::bombFid = 0;
int Screensaver::bombWidth = 0;
int Screensaver::bombHeight = 0;
unsigned char* Screensaver::bombArt = nullptr;
CacheEntry* Screensaver::bombKey = nullptr;

void Screensaver::Init()
{
    if (initialized) {
        return;
    }
    
    bombFid = art_id(OBJ_TYPE_INTERFACE, 226, 0, 0, 0);
    
    bombArt = art_lock(bombFid, &bombKey, &bombWidth, &bombHeight);
    
    timeout = 120;
    initialized = true;
}

void Screensaver::Shutdown()
{
    if (!initialized) {
        return;
    }
    
    if (bombArt != nullptr && bombKey != nullptr) {
        art_ptr_unlock(bombKey);
        bombArt = nullptr;
        bombKey = nullptr;
    }
    
    initialized = false;
}

void Screensaver::SetTimeout(unsigned int seconds)
{
    timeout = seconds;
}

unsigned int Screensaver::GetTimeout()
{
    return timeout;
}

bool Screensaver::ShouldStart(unsigned int lastActivityTime)
{
    if (!initialized) {
        Init();
    }
    
    unsigned int currentTime = get_time() / 1000;
    unsigned int elapsed = currentTime - lastActivityTime;
    
    return elapsed >= (timeout * 1000);
}

bool Screensaver::Play()
{
    if (!initialized) {
        Init();
    }
    
    if (bombArt == nullptr) {
        return false;
    }

    palette_fade_to(black_palette);
    mouse_hide();

    int screenWidth = screenGetWidth();
    int screenHeight = screenGetHeight();

    int screensaverWin = win_add(0, 0, screenWidth, screenHeight, 0, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);

    unsigned char* screenBuffer = win_get_buf(screensaverWin);
    if (screenBuffer == nullptr) {
        return false;
    }

    win_draw(screensaverWin);
    palette_fade_to(cmap);

    ScreensaverBomb bombs[SCREENSAVER_BOMB_COUNT];
    memset(bombs, 0, sizeof(bombs));
    
    int oldMouseX = 0;
    int oldMouseY = 0;
    mouse_get_position(&oldMouseX, &oldMouseY);
    
    unsigned int frameTime = get_time();
    bool firstFrame = true;

    int srcX = 0;
    int srcY = 0;
    int srcWidth = 0;
    int srcHeight = 0;
    int destX = 0;
    int destY = 0;

    int mouseX = oldMouseX;
    int mouseY = oldMouseY;

    unsigned int currentTime = 0;
    
    while (true) {
        sharedFpsLimiter.mark();

        buf_fill(screenBuffer, screenWidth, screenHeight, screenWidth, 0);

        mouse_get_position(&mouseX, &mouseY);
        
        if (get_input() != -1 || oldMouseX != mouseX || oldMouseY != mouseY) {
            break;
        }

        if (roll_random(0, 10000) < 3000) {
            for (int i = 0; i < SCREENSAVER_BOMB_COUNT; i++) {
                if (!bombs[i].active) {
                    bombs[i].x = roll_random(0, screenWidth * 2);
                    bombs[i].y = -bombHeight;
                    bombs[i].speed = (float)(roll_random(0, 10000) / 10000.0 * 2.0 + 0.5);
                    bombs[i].offset = 0.0f;
                    bombs[i].active = true;
                    break;
                }
            }
        }
        
        if (!firstFrame) {
            for (int i = 0; i < SCREENSAVER_BOMB_COUNT; i++) {
                if (bombs[i].active) {
                    srcX = 0;
                    srcY = 0;
                    srcWidth = bombWidth;
                    srcHeight = bombHeight;
                    destX = bombs[i].x;
                    destY = bombs[i].y;
                    
                    if (destX < 0) {
                        srcX = -destX;
                        srcWidth = bombWidth - srcX;
                        destX = 0;
                    }
                    
                    if (destX + srcWidth > screenWidth) {
                        srcWidth = screenWidth - destX;
                    }
                    
                    if (destY < 0) {
                        srcY = -destY;
                        srcHeight = bombHeight - srcY;
                        destY = 0;
                    }
                    
                    if (destY + srcHeight > screenHeight) {
                        srcHeight = screenHeight - destY;
                    }
                    
                    if (srcWidth > 0 && srcHeight > 0) {
                        trans_buf_to_buf(
                            bombArt + bombWidth * srcY + srcX,
                            srcWidth,
                            srcHeight,
                            bombWidth,
                            screenBuffer + screenWidth * destY + destX,
                            screenWidth);
                    }
                    
                    bombs[i].offset += bombs[i].speed;
                    if (bombs[i].offset >= 1.0f) {
                        bombs[i].x = (int)((float)bombs[i].x - bombs[i].offset);
                        bombs[i].y = (int)((float)bombs[i].y + bombs[i].offset);
                        bombs[i].offset = 0.0f;
                    }
                    
                    if (bombs[i].y > screenHeight) {
                        bombs[i].active = false;
                    }
                }
            }
        }
        
        if (firstFrame) {
            firstFrame = false;
        }
        
        win_draw(screensaverWin);
        
        while (elapsed_time(frameTime) < 33) {
        }
        frameTime = get_time();
        
        renderPresent();
        sharedFpsLimiter.throttle();
    }

    palette_fade_to(black_palette);
    win_delete(screensaverWin);
    palette_fade_to(cmap);

    mouse_show();
    
    return true;
}

} // namespace fallout

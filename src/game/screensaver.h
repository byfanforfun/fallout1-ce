#ifndef FALLOUT_GAME_SCREENSAVER_H_
#define FALLOUT_GAME_SCREENSAVER_H_

#include <ctime>

#include "game/cache.h"

namespace fallout {

#define SCREENSAVER_BOMB_COUNT 32

struct ScreensaverBomb {
    int x;
    int y;
    float speed;
    float offset;
    bool active;
};

class Screensaver {
public:
    static void Init();
    static void Shutdown();
    static bool Play();
    static void SetTimeout(unsigned int seconds);
    static unsigned int GetTimeout();
    static bool ShouldStart(unsigned int lastActivityTime);
    
private:
    static bool initialized;
    static unsigned int timeout;
    static unsigned int bombFid;
    static int bombWidth;
    static int bombHeight;
    static unsigned char* bombArt;
    static CacheEntry* bombKey;
};

} // namespace fallout

#endif /* FALLOUT_GAME_SCREENSAVER_H_ */

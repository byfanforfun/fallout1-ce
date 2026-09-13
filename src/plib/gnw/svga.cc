#include "plib/gnw/svga.h"

#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/mouse.h"
#include "plib/gnw/timer.h"
#include "plib/gnw/winmain.h"

namespace fallout {

static bool createRenderer(int width, int height);
static void destroyRenderer();

// screen rect
Rect scr_size;

// 0x6ACA18
ScreenBlitFunc* scr_blit = GNW95_ShowRect;

SDL_Window* gSdlWindow = NULL;
SDL_Surface* gSdlSurface = NULL;
SDL_Renderer* gSdlRenderer = NULL;
SDL_Texture* gSdlTexture = NULL;
SDL_Surface* gSdlTextureSurface = NULL;

// TODO: Remove once migration to update-render cycle is completed.
FpsLimiter sharedFpsLimiter;

// 0x4CB310
void GNW95_SetPaletteEntries(unsigned char* palette, int start, int count)
{
    if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
        SDL_Color colors[256];

        if (count != 0) {
            for (int index = 0; index < count; index++) {
                colors[index].r = palette[index * 3] << 2;
                colors[index].g = palette[index * 3 + 1] << 2;
                colors[index].b = palette[index * 3 + 2] << 2;
                colors[index].a = 255;
            }
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, start, count);
        SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);
    }
}

// 0x4CB568
void GNW95_SetPalette(unsigned char* palette)
{
    if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
        SDL_Color colors[256];

        for (int index = 0; index < 256; index++) {
            colors[index].r = palette[index * 3] << 2;
            colors[index].g = palette[index * 3 + 1] << 2;
            colors[index].b = palette[index * 3 + 2] << 2;
            colors[index].a = 255;
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);
        SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);
    }
}

// 0x4CB850
void GNW95_ShowRect(unsigned char* src, unsigned int srcPitch, unsigned int a3, unsigned int srcX, unsigned int srcY, unsigned int srcWidth, unsigned int srcHeight, unsigned int destX, unsigned int destY)
{
    buf_to_buf(src + srcPitch * srcY + srcX, srcWidth, srcHeight, srcPitch, (unsigned char*)gSdlSurface->pixels + gSdlSurface->pitch * destY + destX, gSdlSurface->pitch);

    SDL_Rect srcRect;
    srcRect.x = destX;
    srcRect.y = destY;
    srcRect.w = srcWidth;
    srcRect.h = srcHeight;

    SDL_Rect destRect;
    destRect.x = destX;
    destRect.y = destY;
    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlTextureSurface, &destRect);
}

bool svga_init(VideoOptions* video_options)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "svga: SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
        return false;
    }

    // Force an OpenGL ES context. SDL's default GL profile is desktop
    // OpenGL (SDL_VIDEO_OPENGL wins over SDL_VIDEO_OPENGL_ES2 at compile
    // time), but the DRM targets are GLES-only: on panfrost (Mali)
    // SDL_EGL_ChooseConfig then requests EGL_OPENGL_BIT, finds no config and
    // fails with "Can't window GBM/EGL surfaces on window creation.", while
    // the non-ES profile also makes SDL bootstrap EGL through libGL.so.1
    // (gl4es on EmuELEC). Pinning ES 2.0 selects a valid GLES config and
    // routes the EGL/GLES library loading to Mesa. Only the KMSDRM driver
    // (headless DRM targets) gets this; desktop GLX drivers do not support
    // an ES profile.
    const char* currentVideoDriver = SDL_GetCurrentVideoDriver();
    if (currentVideoDriver != NULL && SDL_strcasecmp(currentVideoDriver, "kmsdrm") == 0) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    }

    Uint32 windowFlags = SDL_WINDOW_ALLOW_HIGHDPI;

    if (video_options->fullscreen) {
        if (video_options->exclusive)
            windowFlags |= SDL_WINDOW_FULLSCREEN;
        else
            windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    gSdlWindow = SDL_CreateWindow(GNW95_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        video_options->width * video_options->scale,
        video_options->height * video_options->scale,
        windowFlags);
    if (gSdlWindow == NULL) {
        fprintf(stderr, "svga: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    if (!createRenderer(video_options->width, video_options->height)) {
        fprintf(stderr, "svga: createRenderer failed: %s\n", SDL_GetError());
        destroyRenderer();

        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = NULL;

        return false;
    }

    gSdlSurface = SDL_CreateRGBSurface(0,
        video_options->width,
        video_options->height,
        8,
        0,
        0,
        0,
        0);
    if (gSdlSurface == NULL) {
        destroyRenderer();

        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = NULL;
    }

    SDL_Color colors[256];
    for (int index = 0; index < 256; index++) {
        colors[index].r = index;
        colors[index].g = index;
        colors[index].b = index;
        colors[index].a = 255;
    }

    SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);

    scr_size.ulx = 0;
    scr_size.uly = 0;
    scr_size.lrx = video_options->width - 1;
    scr_size.lry = video_options->height - 1;

    mouse_blit_trans = NULL;
    scr_blit = GNW95_ShowRect;
    mouse_blit = GNW95_ShowRect;

    return true;
}

void svga_exit()
{
    destroyRenderer();

    if (gSdlWindow != NULL) {
        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = NULL;
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

int screenGetWidth()
{
    // TODO: Make it on par with _xres;
    return rectGetWidth(&scr_size);
}

int screenGetHeight()
{
    // TODO: Make it on par with _yres.
    return rectGetHeight(&scr_size);
}

static bool createRenderer(int width, int height)
{
    // KMSDRM windows run on a GLES-only stack (panfrost/Mali on the kiosk
    // device), so the "opengl" renderer (SDL_render_gl.c, desktop
    // fixed-function GL) is useless there: its calls do not exist in an
    // ES 2.0 context and every frame fails. Use the dedicated GLES2
    // renderer. Note there is no "gpu" renderer in SDL 2.x (that name is
    // SDL3), and an unknown hint makes SDL_CreateRenderer silently pick the
    // first driver (desktop GL), so never list it here.
    const char* currentVideoDriver = SDL_GetCurrentVideoDriver();
    const bool isKmsdrm = currentVideoDriver != NULL && SDL_strcasecmp(currentVideoDriver, "kmsdrm") == 0;
    static const char* const kmsdrmDrivers[] = { "opengles2", "software" };
    static const char* const defaultDrivers[] = { "opengl", "software" };
    const char* const* drivers = isKmsdrm ? kmsdrmDrivers : defaultDrivers;
    unsigned int driverCount = isKmsdrm ? SDL_arraysize(kmsdrmDrivers) : SDL_arraysize(defaultDrivers);

    for (unsigned int i = 0; i < driverCount; i++) {
        SDL_ClearError();
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, drivers[i]);
        gSdlRenderer = SDL_CreateRenderer(gSdlWindow, -1, 0);
        if (gSdlRenderer != NULL) {
            SDL_RendererInfo info;
            SDL_GetRendererInfo(gSdlRenderer, &info);
            fprintf(stderr, "svga: renderer=%s\n", info.name);
            break;
        }
        fprintf(stderr, "svga: renderer %s unavailable: %s\n", drivers[i], SDL_GetError());
    }
    if (gSdlRenderer == NULL) {
        return false;
    }

    if (SDL_RenderSetLogicalSize(gSdlRenderer, width, height) != 0) {
        return false;
    }

    gSdlTexture = SDL_CreateTexture(gSdlRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (gSdlTexture == NULL) {
        return false;
    }

    Uint32 format;
    if (SDL_QueryTexture(gSdlTexture, &format, NULL, NULL, NULL) != 0) {
        return false;
    }

    gSdlTextureSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height, SDL_BITSPERPIXEL(format), format);
    if (gSdlTextureSurface == NULL) {
        return false;
    }

    return true;
}

static void destroyRenderer()
{
    if (gSdlTextureSurface != NULL) {
        SDL_FreeSurface(gSdlTextureSurface);
        gSdlTextureSurface = NULL;
    }

    if (gSdlTexture != NULL) {
        SDL_DestroyTexture(gSdlTexture);
        gSdlTexture = NULL;
    }

    if (gSdlRenderer != NULL) {
        SDL_DestroyRenderer(gSdlRenderer);
        gSdlRenderer = NULL;
    }
}

void handleWindowSizeChanged()
{
    destroyRenderer();
    createRenderer(screenGetWidth(), screenGetHeight());
}

void renderPresent()
{
    SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
    SDL_RenderClear(gSdlRenderer);
    SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
    SDL_RenderPresent(gSdlRenderer);

    timer_tick();
}

} // namespace fallout

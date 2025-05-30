#ifndef PLIB_GNW_VULKAN_RENDERER_H_
#define PLIB_GNW_VULKAN_RENDERER_H_

// Forward declare SDL_Window from SDL2/SDL.h to avoid including SDL.h in this header
struct SDL_Window; 
// Forward declare VkInstance from vulkan/vulkan.h
typedef struct VkInstance_T* VkInstance;


namespace fallout {
namespace plib {
namespace gnw {

bool vulkan_renderer_init(SDL_Window* sdlWindow, VkInstance instance, int screenWidth, int screenHeight);
void vulkan_renderer_shutdown();
void vulkan_renderer_draw_frame();

} // namespace gnw
} // namespace plib
} // namespace fallout

#endif // PLIB_GNW_VULKAN_RENDERER_H_

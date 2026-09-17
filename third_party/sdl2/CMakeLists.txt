# Build static lib only
set(BUILD_SHARED_LIBS OFF)
set(SDL_SHARED OFF)
set(SDL_STATIC ON)

# Fixes _ftol2_sse already defined
set(SDL_LIBC ON)

if (ANDROID)
    set(SDL_STATIC_PIC ON)
endif()

include(FetchContent)

FetchContent_Declare(sdl2
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL"
    GIT_TAG "release-2.26.1"
)

FetchContent_GetProperties(sdl2)
if (NOT sdl2_POPULATED)
    FetchContent_Populate(sdl2)
endif()

add_subdirectory(${sdl2_SOURCE_DIR} ${sdl2_BINARY_DIR} EXCLUDE_FROM_ALL)

# SDL 2.26's atomic/spinlock code (SDL_atomic.c, SDL_spinlock.c) has no
# GNU-asm fallback for aarch64: it depends on HAVE_GCC_ATOMICS, which the SDL
# configure-time check_c_source_compiles probes determine. When that probe is
# skipped or fails under a cross sysroot, SDL_spinlock.c hits the
# "#error Please implement for your platform." on __atomic_lock_test_and_set.
# GCC/Clang expose the __sync_* builtins everywhere we build (aarch64/x86),
# so pin the macro on directly; SDL_config.h only comments the #undef out, so
# an explicit -D also survives builds where detection did run. MSVC is
# excluded: it has its own Interlocked* paths.
if(NOT MSVC AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_definitions(SDL2-static PUBLIC HAVE_GCC_ATOMICS=1)
endif()

set(SDL2_INCLUDE_DIRS ${sdl2_SOURCE_DIR} ${sdl2_BINARY_DIR} PARENT_SCOPE)
set(SDL2_LIBRARIES SDL2-static SDL2::SDL2main PARENT_SCOPE)

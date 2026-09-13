# Cross-compilation toolchain for GNU/Linux on aarch64 (ARMv8-A).
#
# Target: STB / TV-box / Raspberry Pi-class single board computers.
#
# Requirements on the host (Debian/Ubuntu):
#   sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
#        libsdl2-dev:arm64
#
# The SDL2 development package must be installed into the sysroot that CMake
# searches. With a foreign-architecture package (`libsdl2-dev:arm64`) run under
# the default x86_64 host, point CMAKE_FIND_ROOT_PATH at the multiarch include
# tree, e.g.:
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
#         -DCMAKE_FIND_ROOT_PATH=/usr/aarch64-linux-gnu \
#         -DCMAKE_PREFIX_PATH=/usr/aarch64-linux-gnu \
#         -DFALLOUT_RETROARCH=ON ..
#
# If you build inside a full aarch64 rootfs (e.g. a chroot mounted under
# /opt/aarch64-rootfs), pass that path via CMAKE_FIND_ROOT_PATH instead.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
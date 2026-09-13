# Cross-compilation toolchain for GNU/Linux on aarch64 (ARMv8-A).
#
# Target: STB / TV-box / Raspberry Pi-class single board computers (the kiosk
# build platform).
#
# Requirements on the host (Debian/Ubuntu):
#   sudo dpkg --add-architecture arm64
#   sudo apt update
#   sudo apt install crossbuild-essential-arm64 libsdl2-dev:arm64 zlib1g-dev:arm64
#
# Ubuntu's multiarch layout keeps the target headers in the shared /usr/include
# tree while the libraries live under /usr/aarch64-linux-gnu, so the include
# search must walk both (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH). The library
# search stays confined to the target directory.
#
# For a full aarch64 rootfs (e.g. a chroot mounted under /opt/aarch64-rootfs)
# pass that path via CMAKE_FIND_ROOT_PATH; the shared-include caveat above
# does not apply there.
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/aarch64-linux-gnu.cmake \
#         -DCMAKE_FIND_ROOT_PATH=/usr/aarch64-linux-gnu \
#         -DCMAKE_PREFIX_PATH=/usr/aarch64-linux-gnu \
#         -DFALLOUT_RETROARCH=ON ..

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Target sysroot. The aarch64-linux-gnu-gcc driver defaults to an empty
# /usr/aarch64-linux-gnu/sys-root (stub shipped by the Fedora binutils
# package), while the actual target runtime lives elsewhere, so locate it
# explicitly. Fedora's cross sysroot packages install the runtime under
# /usr/aarch64-redhat-linux/sys-root/<releasever>; Debian/Ubuntu multiarch
# uses /usr/aarch64-linux-gnu directly. A manually supplied -DCMAKE_SYSROOT
# (e.g. a Fedora aarch64 installroot) always wins.
if(NOT CMAKE_SYSROOT)
    foreach(candidate IN ITEMS
            "/usr/aarch64-redhat-linux/sys-root/fc43"
            "/usr/aarch64-linux-gnu")
        if(EXISTS "${candidate}/usr/lib64/crt1.o"
           OR EXISTS "${candidate}/usr/lib/crt1.o")
            set(CMAKE_SYSROOT "${candidate}")
            break()
        endif()
    endforeach()
endif()

if(CMAKE_SYSROOT)
    message(STATUS "aarch64 sysroot: ${CMAKE_SYSROOT}")
else()
    message(FATAL_ERROR "aarch64 sysroot not found; install "
        "sysroot-aarch64-fc43-glibc (Fedora) or libc6-dev-arm64-cross "
        "(Debian/Ubuntu) or pass -DCMAKE_SYSROOT=...")
endif()
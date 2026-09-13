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
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib64/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")

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

# libc startup objects (crt1.o, crti.o, crtn.o) and the libc.so linker script
# live in the multiarch directory (/usr/lib/aarch64-linux-gnu) on Debian and
# under /usr/lib64 on Fedora. The compiler driver only searches its own
# Fedora paths, so the linker must be told about the sysroot libdirs. The -L
# goes into the language flags as well: try_compile tests (link-time checks
# at configure) do not receive CMAKE_EXE_LINKER_FLAGS, and gcc forwards -L to
# the real link line anyway.
# Missing directories are harmless for -L, so no existence checks here.
set(_sysroot_link_dirs
    "usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu"
    "lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu"
    "usr/lib/${CMAKE_SYSTEM_PROCESSOR}-redhat-linux"
    "usr/lib64"
    "usr/lib")
foreach(_candidate_sysroot IN LISTS _sysroot_link_dirs)
    string(APPEND CMAKE_C_FLAGS " -L${CMAKE_SYSROOT}/${_candidate_sysroot}")
    string(APPEND CMAKE_CXX_FLAGS " -L${CMAKE_SYSROOT}/${_candidate_sysroot}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -L${CMAKE_SYSROOT}/${_candidate_sysroot}")
    if(EXISTS "${CMAKE_SYSROOT}/${_candidate_sysroot}")
        list(APPEND CMAKE_SYSTEM_LIBRARY_PATH
            "${CMAKE_SYSROOT}/${_candidate_sysroot}")
    endif()
endforeach()

# Redirect pkg-config at the target root: otherwise the host pkg-config wins
# and its modules (EGL, GL, dbus, wayland, ...) resolve to host include and
# library paths, polluting every compile test (e.g. HAVE_PTHREADS ends up
# including the host /usr/include). If the target has no .pc files at all,
# the module checks simply fail and the build stays minimal.

# The Fedora cross gcc-c++-aarch64-linux-gnu package ships no C++ headers
# (and no target libstdc++), so point g++ at the standard headers living in
# the target root (Fedora installs them under /usr/include/c++/<ver> with
# the arch and backward subdirectories).
if(EXISTS "${CMAKE_SYSROOT}/usr/include/c++")
    file(GLOB _cxx_ver_dirs RELATIVE "${CMAKE_SYSROOT}/usr/include/c++"
        "${CMAKE_SYSROOT}/usr/include/c++/*")
    foreach(_ver IN LISTS _cxx_ver_dirs)
        if(NOT IS_DIRECTORY "${CMAKE_SYSROOT}/usr/include/c++/${_ver}")
            continue()
        endif()
        string(APPEND CMAKE_CXX_FLAGS
            " -isystem ${CMAKE_SYSROOT}/usr/include/c++/${_ver}")
        foreach(_cxx_sub IN ITEMS backward
                "${CMAKE_SYSTEM_PROCESSOR}-redhat-linux"
                "${CMAKE_SYSTEM_PROCESSOR}-linux-gnu")
            if(EXISTS "${CMAKE_SYSROOT}/usr/include/c++/${_ver}/${_cxx_sub}")
                string(APPEND CMAKE_CXX_FLAGS
                    " -isystem ${CMAKE_SYSROOT}/usr/include/c++/${_ver}/${_cxx_sub}")
            endif()
        endforeach()
        # Debian/Ubuntu multiarch keeps the arch-specific headers outside the
        # version dir: /usr/include/<tuple>/c++/<ver> (bits/c++config.h).
        foreach(_cxx_tuple IN ITEMS
                "${CMAKE_SYSTEM_PROCESSOR}-linux-gnu"
                "${CMAKE_SYSTEM_PROCESSOR}-redhat-linux")
            if(EXISTS
               "${CMAKE_SYSROOT}/usr/include/${_cxx_tuple}/c++/${_ver}")
                string(APPEND CMAKE_CXX_FLAGS
                    " -isystem ${CMAKE_SYSROOT}/usr/include/${_cxx_tuple}/c++/${_ver}")
            endif()
        endforeach()
    endforeach()
endif()
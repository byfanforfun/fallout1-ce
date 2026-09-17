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
# Use the HOST pkg-config binary: a pkg-config found inside the sysroot is an
# aarch64 ELF that cannot execute on the build host (qemu lacks the target
# dynamic loader), which silently breaks every pkg-config module check. Its
# search paths are redirected below to the target .pc files. find_program is
# avoided here: it still resolves into the sysroot via the root search.
set(_fallout1_host_pkg_config "")
foreach(_pc_candidate IN ITEMS
        /usr/bin/pkg-config /usr/local/bin/pkg-config /bin/pkg-config
        /usr/bin/pkgconf /usr/local/bin/pkgconf)
    if(EXISTS "${_pc_candidate}")
        set(_fallout1_host_pkg_config "${_pc_candidate}")
        break()
    endif()
endforeach()
if(_fallout1_host_pkg_config)
    set(PKG_CONFIG_EXECUTABLE "${_fallout1_host_pkg_config}"
        CACHE FILEPATH "Host pkg-config (a sysroot one cannot run on the host)" FORCE)
    set(ENV{PKG_CONFIG_EXECUTABLE} "${_fallout1_host_pkg_config}")
endif()
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib64/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig:${CMAKE_SYSROOT}/usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu/pkgconfig")
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

# The target C++ runtime (libstdc++.a, needed by -static-libstdc++) lives in
# the compiler version directory of the sysroot's own toolchain
# (usr/lib/gcc/<tuple>/<version>), which the Fedora cross driver never
# searches. Add every such version directory that exists.
file(GLOB _sysroot_gcc_version_dirs RELATIVE "${CMAKE_SYSROOT}"
    "${CMAKE_SYSROOT}/usr/lib/gcc/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu/*")
foreach(_gcc_version IN LISTS _sysroot_gcc_version_dirs)
        if(IS_DIRECTORY "${CMAKE_SYSROOT}/${_gcc_version}")
        list(APPEND _sysroot_link_dirs "${_gcc_version}")
    endif()
endforeach()
foreach(_candidate_sysroot IN LISTS _sysroot_link_dirs)
    string(APPEND CMAKE_C_FLAGS " -L${CMAKE_SYSROOT}/${_candidate_sysroot}")
    string(APPEND CMAKE_CXX_FLAGS " -L${CMAKE_SYSROOT}/${_candidate_sysroot}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -L${CMAKE_SYSROOT}/${_candidate_sysroot}")
    # check_c_source_compiles and similar try_compile macros use only
    # CMAKE_REQUIRED_FLAGS for linking and do NOT inherit CMAKE_C_FLAGS.
    string(APPEND CMAKE_REQUIRED_FLAGS " -L${CMAKE_SYSROOT}/${_candidate_sysroot}")
    if(EXISTS "${CMAKE_SYSROOT}/${_candidate_sysroot}")
        list(APPEND CMAKE_SYSTEM_LIBRARY_PATH
            "${CMAKE_SYSROOT}/${_candidate_sysroot}")
    endif()
endforeach()

# Anchor every compile/link on the sysroot regardless of the host driver's
# defaults: some distributions' cross gcc (e.g. Ubuntu's multiarch
# aarch64-linux-gnu-gcc) keep resolving glibc headers like features.h or
# stdio.h from the HOST /usr/aarch64-linux-gnu/include even when CMAKE_SYSROOT
# is set, which mixed with the rootfs' own glibc (a bookworm chroot vs. the
# Ubuntu cross libc) breaks the headers. The explicit --sysroot is duplicated
# in CMAKE_REQUIRED_FLAGS so configure-time check_* probe compiles use it too.
if(CMAKE_SYSROOT)
    string(APPEND CMAKE_C_FLAGS " --sysroot=${CMAKE_SYSROOT}")
    string(APPEND CMAKE_CXX_FLAGS " --sysroot=${CMAKE_SYSROOT}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " --sysroot=${CMAKE_SYSROOT}")
    string(APPEND CMAKE_REQUIRED_FLAGS " --sysroot=${CMAKE_SYSROOT}")
endif()

# Debian/Ubuntu multiarch keeps the arch-specific glibc headers (bits/
# wordsize.h, asm/...) in /usr/include/<tuple>. The Fedora cross driver does
# not add that directory, so expose it (harmless where it does not exist).
foreach(_arch_inc IN ITEMS
        "usr/include/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu"
        "usr/include/${CMAKE_SYSTEM_PROCESSOR}-redhat-linux")
    if(EXISTS "${CMAKE_SYSROOT}/${_arch_inc}")
        string(APPEND CMAKE_C_FLAGS " -isystem ${CMAKE_SYSROOT}/${_arch_inc}")
        string(APPEND CMAKE_CXX_FLAGS " -isystem ${CMAKE_SYSROOT}/${_arch_inc}")
        string(APPEND CMAKE_REQUIRED_FLAGS " -isystem ${CMAKE_SYSROOT}/${_arch_inc}")
    endif()
endforeach()

# glibc's top-level headers (stdio.h, features.h, stdlib.h, ...) live in
# usr/include of a full rootfs sysroot. A host/host-cross driver that ignores
# --sysroot would otherwise resolve those from its own /usr/include, which is
# a different glibc generation than the rootfs and breaks system headers when
# mixed with the rootfs' bits/ headers. Expose usr/include as -isystem; it is
# searched before the compiler's implicit system dirs, so the rootfs wins, and
# mirrors the standard Debian layout. Guarded by EXISTS so a multiarch
# sysroot (which shares the host /usr/include) stays untouched. The CXX append
# happens last on purpose: libstdc++'s cstdlib does #include_next <stdlib.h>
# and the search must find the sysroot copy right after the C++ include dirs,
# not a host one.
if(EXISTS "${CMAKE_SYSROOT}/usr/include")
    string(APPEND CMAKE_C_FLAGS " -isystem ${CMAKE_SYSROOT}/usr/include")
    string(APPEND CMAKE_REQUIRED_FLAGS " -isystem ${CMAKE_SYSROOT}/usr/include")
endif()

# -static-libgcc makes the driver resolve -lgcc inside the sysroot, i.e. the
# root's own gcc <version> directory, whose libgcc.a carries no exception
# unwinder. The unwind runtime (_Unwind_Resume) lives in the HOST driver's
# libgcc.a, a path --sysroot rewrites out of existence. Link that archive at
# the END of the command line, after the objects and libstdc++: static
# archives only contribute members for symbols undefined at their position in
# the link order. CMAKE_EXE_LINKER_FLAGS is emitted first and must not be
# used here; CMAKE_CXX_STANDARD_LIBRARIES is appended last.
execute_process(
    COMMAND "${CMAKE_C_COMPILER}" "-print-file-name=libgcc.a"
    OUTPUT_VARIABLE _host_libgcc_archive
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(EXISTS "${_host_libgcc_archive}")
    string(APPEND CMAKE_CXX_STANDARD_LIBRARIES " ${_host_libgcc_archive}")
    string(APPEND CMAKE_REQUIRED_LIBRARIES " ${_host_libgcc_archive}")
endif()

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

# libstdc++'s cstdlib uses #include_next <stdlib.h>, which only continues past
# the include directories that precede usr/include, so the sysroot's top-level
# glibc headers must be appended after the C++ search dirs above (see the
# usr/include -isystem block earlier for why they stay in the sysroot).
if(EXISTS "${CMAKE_SYSROOT}/usr/include")
    string(APPEND CMAKE_CXX_FLAGS " -isystem ${CMAKE_SYSROOT}/usr/include")
endif()
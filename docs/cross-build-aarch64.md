# Cross-building Fallout CE for aarch64 (kiosk / handhelds)

Reproducible guide to producing the `FALLOUT_RETROARCH=ON` ("kiosk") build for
an ARMv8-A target from an x86_64 host. It is the result of real debugging, so
every pitfall below has a matching row in the
[troubleshooting matrix](#troubleshooting-matrix) together with the rationale.

## Target and the one hard constraint

The kiosk build runs on headless DRM boxes (STBs, Raspberry Pi-class SBCs)
and on ARM handhelds such as the **R36S** (RK3326) running **EmuELEC** with
the EmulationStation front end launched via RetroArch.

The single non-negotiable constraint: **the maximum `GLIBC_*` symbol version
the binary references must be at or below the glibc version on the device.**
Check the box first:

```console
$ ldd --version   # on the device
```
The R36S with EmuELEC reports glibc 2.36. A binary built against a newer
glibc fails at startup on the box with `GLIBC_2.38 not found` (or similar).

Two build paths are documented:

* [Path A — Fedora aarch64 rootfs](#path-a--fedora-aarch64-rootfs) — use when
  the box runs a glibc as new as the host (SDK style, full rootfs).
* [Path B — Debian bookworm chroot](#path-b--debian-bookworm-chroot-glibc-matched)
  — *recommended for handhelds*: build against a Debian 12 chroot whose glibc
  (2.36) matches the device exactly.

Both use the same toolchain file `cmake/toolchain/aarch64-linux-gnu.cmake`,
which makes several target-layout quirks transparent (see
[what the toolchain does automatically](#what-the-toolchain-does-automatically)).

## Host prerequisites

The cross compiler is the host distro's package (`gcc-c++-aarch64-linux-gnu`
on Fedora, `crossbuild-essential-arm64` on Debian/Ubuntu). The Fedora driver
ships *only* the compiler binaries — no target libstdc++, no C++ headers, and
an empty stub sysroot — so the runtime must come from a rootfs/chroot.

```console
# Fedora 43 (paths below use this release)
$ sudo dnf install gcc-c++-aarch64-linux-gnu debootstrap qemu-user-static
# Debian/Ubuntu
$ sudo dpkg --add-architecture arm64
$ sudo apt update
$ sudo apt install crossbuild-essential-arm64 qemu-user-static
```

## Path A — Fedora aarch64 rootfs

Create a full aarch64 sysroot with the runtime/dev packages. This rootfs has
the host distro's glibc (2.42 on Fedora 43), so it is only correct for boxes
running a distro of the same vintage.

```console
$ sudo dnf --installroot=/opt/aarch64-rootfs --releasever=43 --forcearch=aarch64 \
      install glibc-devel glibc-static libstdc++-devel libstdc++-static \
      libX11-devel libXext-devel
```

Fedora also hides two linker-visible files from the default search dirs
(`=/usr/lib64`, `=/usr/lib` under the sysroot): copy `libstdc++.a` and create
the `libstdc++.so` dev symlink in `/usr/lib64`:

```console
$ sudo cp -av /opt/aarch64-rootfs/usr/lib/gcc/aarch64-redhat-linux/15/libstdc++.a \
              /opt/aarch64-rootfs/usr/lib64/
$ sudo ln -sfv libstdc++.so.6 /opt/aarch64-rootfs/usr/lib64/libstdc++.so
```

## Path B — Debian bookworm chroot (glibc-matched)

Recommended when the device glibc is older than the host's. Matches by
construction:

| Rootfs            | glibc  |
|-------------------|--------|
| Debian 11 bullseye| 2.31   |
| Ubuntu 22.04      | 2.35   |
| **Debian 12 bookworm** | **2.36** |
| Ubuntu 24.04      | 2.39   |

```console
$ sudo debootstrap --arch=arm64 --foreign bookworm /opt/aarch64-bk12 \
      http://deb.debian.org/debian
$ sudo cp /usr/bin/qemu-aarch64-static /opt/aarch64-bk12/usr/bin/
$ sudo chroot /opt/aarch64-bk12 /debootstrap/debootstrap --second-stage
$ sudo chroot /opt/aarch64-bk12 bash -c 'apt-get update && apt-get install -y \
      libc6-dev libstdc++-12-dev zlib1g-dev \
      libdrm-dev libgbm-dev libudev-dev libasound2-dev'
```

`libdrm-dev`/`libgbm-dev` enable SDL's KMSDRM video driver, `libudev-dev`
enables udev hotplug (gamepads also work without it through SDL's evdev
backend) and `libasound2-dev` enables the ALSA audio driver.

### Chroot surgery: default-search symlinks

Three groups of files live outside GNU ld's default sysroot search
(`=/usr/lib64`, `=/usr/lib`) on the Debian multiarch tree and are *not*
found otherwise:

1. **libc startup objects.** `crt1.o`, `crti.o`, `crtn.o` and the `libc.so`
   linker script live in `/usr/lib/aarch64-linux-gnu`. Bare object file
   arguments (unlike `-lname`) are searched **only** in the default
   `SEARCH_DIR`s — `-L` does *not* apply to them.
2. **`libpthread.so` dev symlink.** Debian ships `libpthread.so.0` and
   `libpthread.a` but no `libpthread.so`, so `-lpthread` (used by SDL's
   `HAVE_PTHREADS` check) fails at link time.
3. **Static `libstdc++.a`.** Needed because the kiosk build links the C++
   runtime statically (`-static-libstdc++`); it is installed only inside
   `/usr/lib/gcc/aarch64-linux-gnu/12`, which is deliberately *not* in the
   linker search — see the note on `_Unwind_*` in the troubleshooting matrix.

Create the search dir and add all three groups:

```console
$ sudo mkdir -p /opt/aarch64-bk12/usr/lib64
$ sudo ln -sfv /opt/aarch64-bk12/usr/lib/aarch64-linux-gnu/{crt1.o,Scrt1.o,crti.o,crtn.o} \
      /opt/aarch64-bk12/usr/lib64/
$ sudo ln -sfv libpthread.so.0 /opt/aarch64-bk12/usr/lib64/libpthread.so
$ sudo ln -sfv /opt/aarch64-bk12/usr/lib/gcc/aarch64-linux-gnu/12/libstdc++.a \
      /opt/aarch64-bk12/usr/lib64/libstdc++.a
```

## Build

```console
$ cmake -S . -B build-aarch64 \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/aarch64-linux-gnu.cmake \
        -DCMAKE_SYSROOT=/opt/aarch64-bk12 \
        -DCMAKE_FIND_ROOT_PATH=/opt/aarch64-bk12 \
        -DCMAKE_PREFIX_PATH=/opt/aarch64-bk12/usr \
        -DFALLOUT_RETROARCH=ON
$ cmake --build build-aarch64 -j$(nproc)
```

For Path A replace the root path with `/opt/aarch64-rootfs` and drop the
`CMAKE_PREFIX_PATH` line if desired. The toolchain auto-detects the sysroot if
`-DCMAKE_SYSROOT` is omitted (Fedora `/usr/aarch64-redhat-linux/sys-root/fc43`
or Debian `/usr/aarch64-linux-gnu`).

Do **not** pass `-DFALLOUT_STATIC_GLIBC=ON` for a handheld — a fully static
binary cannot `dlopen()`, so SDL's KMSDRM, udev and ALSA drivers (loaded as
DSOs) all fail at runtime and the box goes to a black screen. The default
"safe static" mode embeds SDL2, adecode, fpattern and the C++ runtime while
leaving glibc dynamic, which keeps the dlopen drivers alive. The only runtime
`NEEDED` entries are then `libc.so.6`, `libpthread.so.0`, `ld-linux-*`.

### Exit hook (optional)

What happens when the player exits the game is decided by two separate
knobs. The `system_exec` on an in-game "Exit" is part of the kiosk build; the
*kiosk_exec.cfg* it reads, however, is only written on first run when the
command is explicitly configured:

```console
# no exit hook (default): kiosk_exec.cfg is never generated/forced
$ cmake ... -DFALLOUT_RETROARCH=ON

# chain into RetroArch's menu when the game exits
$ cmake ... -DFALLOUT_RETROARCH=ON -DFALLOUT_EXIT_EXEC="retroarch --menu"
```

`FALLOUT_EXIT_EXEC` holds the whole command line and is written verbatim as
line `0=` of `[exec]`; an existing `kiosk_exec.cfg` (yours or generated by a
previous run) is never overwritten.

## What the toolchain does automatically

`cmake/toolchain/aarch64-linux-gnu.cmake` hides the following so a repeat
build needs no extra flags:

* **Host pkg-config.** A `pkg-config` inside the sysroot is an aarch64 ELF
  that cannot run on the host; `find_program` would still resolve into the
  sysroot. The toolchain pins `/usr/bin/pkg-config` (or `/usr/bin/pkgconf`)
  and redirects its search with `PKG_CONFIG_LIBDIR` (including the multiarch
  `.../aarch64-linux-gnu/pkgconfig` dir) and `PKG_CONFIG_SYSROOT_DIR`.
* **Sysroot libdirs.** `usr/lib/aarch64-linux-gnu`, `lib/aarch64-linux-gnu`,
  `usr/lib64`, `usr/lib` are added as `-L` to the C, C++ and linker flags
  *and* to `CMAKE_REQUIRED_FLAGS` — configure-time probes such as SDL's
  `check_c_source_compiles` link only with `CMAKE_REQUIRED_FLAGS` and never
  inherit `CMAKE_C_FLAGS` or `CMAKE_EXE_LINKER_FLAGS`.
* **Arch-specific glibc headers.** On Debian `bits/wordsize.h` etc. live in
  `/usr/include/aarch64-linux-gnu`; they are exposed via `-isystem` (both
  `-linux-gnu` and `-redhat-linux` tuples, void where absent).
* **C++ headers.** Fedora ships none with the compiler, so
  `/usr/include/c++/<ver>` plus `backward`, the arch dir and the multiarch
  `include/<tuple>/c++/<ver>` are appended to the C++ include path.
* **libgcc/unwinding source.** The Debian gcc dir is deliberately *not* in
  the search path; `-lgcc` resolves to the host cross-compiler's libgcc,
  which embeds the unwinder (`_Unwind_*`). See troubleshooting below.

## Verify the binary

```console
$ file build-aarch64/fallout-ce
# ELF 64-bit LSB executable, ARM aarch64

$ readelf -d build-aarch64/fallout-ce | grep NEEDED
# expect only: libc.so.6, libpthread.so.0, ld-linux-aarch64.so.1
# (libstdc++, SDL2, adecode, fpattern are statically embedded)

$ objdump -T build-aarch64/fallout-ce | grep -oE 'GLIBC_[0-9.]+' | sort -Vu | tail -3
# the newest GLIBC_x.y must be <= the device glibc (2.36 for EmuELEC/R36S)

# SDL drivers compiled in:
$ nm build-aarch64/fallout-ce | grep -cE 'KMSDRM|drmMode'   # > 0  (KMSDRM)
$ nm build-aarch64/fallout-ce | grep -c  'snd_pcm_'          # > 0  (ALSA)
$ nm build-aarch64/fallout-ce | grep -E 'joystick_udev_callback'  # udev hotplug
```

## Troubleshooting matrix

| Symptom | Root cause | Fix |
|---|---|---|
| Startup on box: `GLIBC_2.38 not found` | Binary built against a newer glibc than the device ships (e.g. the Fedora rootfs with glibc 2.42 vs device 2.36) | Rebuild against a rootfs/chroot with the device's glibc — use Path B. Verify with `objdump -T` (above). |
| `bits/wordsize.h: No such file` | Debian multiarch stores the arch-specific glibc headers in `/usr/include/<tuple>`; the Fedora cross driver does not add it | Toolchain injects `-isystem $SYSROOT/usr/include/aarch64-linux-gnu`. |
| Configure: `Performing Test HAVE_PTHREADS - Failed` / SDL `ERROR: Threads are needed` | Two independent causes: (a) no `libpthread.so` dev symlink (`-lpthread` fails), (b) `check_c_source_compiles` links with only `CMAKE_REQUIRED_FLAGS` and does *not* see `CMAKE_C_FLAGS`/linker flags | (a) create the symlink in `/usr/lib64`; (b) toolchain mirrors the `-L` dirs into `CMAKE_REQUIRED_FLAGS`. |
| Configure: `-lstdc++: cannot find` | Fedora cross compiler ships no libstdc++; static `libstdc++.a` not in the linker's default search dirs | Copy/symlink `libstdc++.a` into `/usr/lib64` (Paths A and B). |
| Link: undefined `_Unwind_Resume`, `_Unwind_GetRegionStart`, ... | `-L` pointed at the Debian gcc dir, so `-lgcc` picked Debian's `libgcc.a`, which has no unwinder (Fedora's compiler would add it only to its *own* libgcc). The driver does not emit `-lgcc_eh` | Keep the gcc dir out of the search. `-lgcc` must resolve to the *host* cross-compiler's libgcc (which embeds `_Unwind_*`). |
| `ld: cannot find crt1.o` | Debian keeps the startup objects in `/usr/lib/aarch64-linux-gnu`; bare object names are only found in GNU ld's default `SEARCH_DIR`s, `-L` does not help | Symlink `crt1.o`, `Scrt1.o`, `crti.o`, `crtn.o` into `/usr/lib64`. |
| SDL `SDL_KMSDRM` etc. report `(Wanted: ON): OFF`, binary has only dummy/offscreen video | `libdrm-dev`/`libgbm-dev` absent from the rootfs; SDL has no DRM headers | Install them in the chroot/rootfs (Path B package list), reconfigure, check the SDL options in the configure log. |
| On the box: `SDL_CreateWindow failed: Can't window GBM/EGL surfaces on window creation.` | SDL was built with desktop GL compiled in (`SDL_VIDEO_OPENGL`), so the window's default GL profile is *desktop* OpenGL; EGL then asks for `EGL_OPENGL_BIT` configs and the GLES-only driver of the target (panfrost = Mali GLES only) returns none. The non-ES profile also makes SDL bootstrap EGL through `libGL.so.1`, which is gl4es on EmuELEC. | The game forces ES 2.0 (`SDL_GL_CONTEXT_PROFILE_ES`, major 2, minor 0) for the `kmsdrm` driver before creating the window, so `SDL_EGL_ChooseConfig` picks a valid GLES config and libraries load from Mesa. |
| Window created, but movies/menu render black and the game loops; log repeats `LIBGL: Error while gathering supported extension (eglInitialize: EGL_BAD_DISPLAY)` | Two linked causes: (a) the renderer fallback tried `"gpu"` first — there is no such renderer in SDL 2.x (SDL3 name), so `SDL_CreateRenderer` silently chose the first driver, the desktop-GL `opengl` renderer whose fixed-function calls (`glBegin`/`glVertex*`) do not exist in the now-ES context; (b) the bootstrapped libGL.so.1 is gl4es, whose own EGL init cannot find a display in a DRM-only environment. | The game uses the dedicated `opengles2` renderer (then `software`) on `kmsdrm`; with the ES profile SDL stops loading libGL.so.1 entirely and gl4es is not touched. |
| Fully static build: no video/audio on the box | `-DFALLOUT_STATIC_GLIBC=ON` → `-static` → `dlopen()` cannot load DSOs → KMSDRM, udev, ALSA all fail | Do not use the fully-static option for handhelds; keep the default safe-static mode. |
| pkg-config probes always fail or resolve to host modules (EGL, dbus, wayland...) | The pkg-config found by `find_program` is an aarch64 ELF that cannot run, or the host pkg-config keeps its own search paths | Toolchain pins the host binary and redirects via `PKG_CONFIG_LIBDIR` + `PKG_CONFIG_SYSROOT_DIR` (multiarch dir included). |
| C++ include errors after switching rootfs layout | Fedora (sysroot) and Debian (multiarch) place the C++ headers in different trees | Toolchain auto-adds both `/usr/include/c++/<ver>[/backward|<tuple>]` and `include/<tuple>/c++/<ver>`. |

## Deploying

Copy `build-aarch64/fallout-ce` to the box and run it under EmulationStation /
RetroArch. Full deployment notes (launcher contract, environment) live in the
main [README deployment section](../README.md#deploying-on-the-target-emuelec--retroarch--emulationstation)
(header "## Deploying on the target (EmuELEC / RetroArch / EmulationStation)").
# Fallout Community Edition

Fallout Community Edition is a fully working re-implementation of Fallout, with the same original gameplay, engine bugfixes, and some quality of life improvements, that works (mostly) hassle-free on multiple platforms.

There is also [Fallout 2 Community Edition](https://github.com/alexbatalov/fallout2-ce).

## Installation

You must own the game to play. Purchase your copy on [GOG](https://www.gog.com/game/fallout) or [Steam](https://store.steampowered.com/app/38400). Download latest [release](https://github.com/alexbatalov/fallout1-ce/releases) or build from source. You can also check latest [debug](https://github.com/alexbatalov/fallout1-ce/actions) build intended for testers.

### Windows

Download and copy `fallout-ce.exe` to your `Fallout` folder. It serves as a drop-in replacement for `falloutw.exe`.

### Linux

- Use Windows installation as a base - it contains data assets needed to play. Copy `Fallout` folder somewhere, for example `/home/john/Desktop/Fallout`.

- Alternatively you can extract the needed files from the GoG installer:

```console
$ sudo apt install innoextract
$ innoextract ~/Downloads/setup_fallout_2.1.0.18.exe -I app
$ mv app Fallout
```

- Download and copy `fallout-ce` to this folder.

- Install [SDL2](https://libsdl.org/download-2.0.php):

```console
$ sudo apt install libsdl2-2.0-0
```

- Run `./fallout-ce`.

### Linux (aarch64, cross-compilation)

On the host install the cross-compilers and the development files for the
target architecture. Debian/Ubuntu:

```console
$ sudo dpkg --add-architecture arm64
$ sudo apt update
$ sudo apt install crossbuild-essential-arm64 zlib1g-dev:arm64
```

Fedora (the toolchain also needs the static libstdc++, and `glibc-static`
if using `-DFALLOUT_STATIC_GLIBC=ON`):

```console
$ sudo dnf install gcc-aarch64-linux-gnu gcc-c++-aarch64-linux-gnu \
      binutils-aarch64-linux-gnu libstdc++-static glibc-static
```

Build with the toolchain:

```console
$ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/aarch64-linux-gnu.cmake \
        -DCMAKE_FIND_ROOT_PATH=/usr/aarch64-linux-gnu \
        -DCMAKE_PREFIX_PATH=/usr/aarch64-linux-gnu \
        -DFALLOUT_RETROARCH=ON ..
$ make
```

The kiosk build (`FALLOUT_RETROARCH=ON`) is statically linked: SDL2
(compiled from `third_party/sdl2`), adecode, fpattern and the C++ runtime
(`-static-libstdc++ -static-libgcc`) are embedded, so the only runtime
dependencies are glibc and the system audio/video libraries, which keeps
SDL's dlopen-based drivers working. Pass `-DFALLOUT_STATIC_GLIBC=ON` for a
fully static binary (maximum portability, but SDL drivers that rely on
dlopen will not load). SDL X11, pipewire and hidapi support are off by
default in this mode (`-DSDL_X11=ON` etc. re-enable) since the kiosk
targets headless DRM boxes; gamepads are read through SDL's evdev backend.

Copy the resulting `fallout-ce` to the aarch64 device along with the game
assets (see [Linux](#linux)).

### macOS

> **NOTE**: macOS 10.11 (El Capitan) or higher is required. Runs natively on Intel-based Macs and Apple Silicon.

- Use Windows installation as a base - it contains data assets needed to play. Copy `Fallout` folder somewhere, for example `/Applications/Fallout`.

- Alternatively you can use Fallout from MacPlay/The Omni Group as a base - you need to extract game assets from the original bundle. Mount CD/DMG, right click `Fallout` -> `Show Package Contents`, navigate to `Contents/Resources`. Copy `GameData` folder somewhere, for example `/Applications/Fallout`.

- Or if you're a Terminal user and have Homebrew installed you can extract the needed files from the GoG installer:

```console
$ brew install innoextract
$ innoextract ~/Downloads/setup_fallout_2.1.0.18.exe -I app
$ mv app /Applications/Fallout
```

- Download and copy `fallout-ce.app` to this folder.

- Run `fallout-ce.app`.

### Android

> **NOTE**: Fallout was designed with mouse in mind. There are many controls that require precise cursor positioning, which is not possible with fingers. Current control scheme resembles trackpad usage:
> - One finger moves mouse cursor around.
> - Tap one finger for left mouse click.
> - Tap two fingers for right mouse click (switches mouse cursor mode).
> - Move two fingers to scroll current view (map view, worldmap view, inventory scrollers).

> **NOTE**: From Android standpoint release and debug builds are different apps. Both apps require their own copy of game assets and have their own savegames. This is intentional. As a gamer just stick with release version and check for updates.

- Use Windows installation as a base - it contains data assets needed to play. Copy `Fallout` folder to your device, for example to `Downloads`. You need `master.dat`, `critter.dat`, and `data` folder. Watch for file names - keep (or make) them lowercased (see [Configuration](#configuration)).

- Download `fallout-ce.apk` and copy it to your device. Open it with file explorer, follow instructions (install from unknown source).

- When you run the game for the first time it will immediately present file picker. Select the folder from the first step. Wait until this data is copied. A loading dialog will appear, just wait for about 30 seconds. The game will start automatically.

### iOS

> **NOTE**: See Android note on controls.

- Download `fallout-ce.ipa`. Use sideloading applications ([AltStore](https://altstore.io/) or [Sideloadly](https://sideloadly.io/)) to install it to your device. Alternatively you can always build from source with your own signing certificate.

- Run the game once. You'll see error message saying "Could not find the master datafile...". This step is needed for iOS to expose the game via File Sharing feature.

- Use Finder (macOS Catalina and later) or iTunes (Windows and macOS Mojave or earlier) to copy `master.dat`, `critter.dat`, and `data` folder to "Fallout" app ([how-to](https://support.apple.com/HT210598)). Watch for file names - keep (or make) them lowercased (see [Configuration](#configuration)).

## Configuration

The main configuration file is `fallout.cfg`. There are several important settings you might need to adjust for your installation. Depending on your Fallout distribution main game assets `master.dat`, `critter.dat`, and `data` folder might be either all lowercased, or all uppercased. You can either update `master_dat`, `critter_dat`, `master_patches` and `critter_patches` settings to match your file names, or rename files to match entries in your `fallout.cfg`.

The `sound` folder (with `music` folder inside) might be located either in `data` folder, or be in the Fallout folder. Update `music_path1` setting to match your hierarchy, usually it's `data/sound/music/` or `sound/music/`. Make sure it match your path exactly (so it might be `SOUND/MUSIC/` if you've installed Fallout from CD). Music files themselves (with `ACM` extension) should be all uppercased, regardless of `sound` and `music` folders.

The second configuration file is `f1_res.ini`. Use it to change game window size and enable/disable fullscreen mode.

```ini
[MAIN]
SCR_WIDTH=1280
SCR_HEIGHT=720
WINDOWED=1
```

Recommendations:
- **Desktops**: Use any size you see fit.
- **Tablets**: Set these values to logical resolution of your device, for example iPad Pro 11 is 1668x2388 (pixels), but it's logical resolution is 834x1194 (points).
- **Mobile phones**: Set height to 480, calculate width according to your device screen (aspect) ratio, for example Samsung S21 is 20:9 device, so the width should be 480 * 20 / 9 = 1067.

In time this stuff will receive in-game interface, right now you have to do it manually.

## Contributing

Here is a couple of current goals. Open up an issue if you have suggestion or feature request.

- **Update to v1.2**. This project is based on Reference Edition which implements v1.1 released in November 1997. There is a newer v1.2 released in March 1998 which at least contains important multilingual support.

- **Backport some Fallout 2 features**. Fallout 2 (with some Sfall additions) added many great improvements and quality of life enhancements to the original Fallout engine. Many deserve to be backported to Fallout 1. Keep in mind this is a different game, with slightly different gameplay balance (which is a fragile thing on its own).

## Kiosk

Use asset files from [fallout1-kiosk-assets](https://github.com/byfanforfun/fallout1-kiosk-assets)

Custom config `kiosk.cfg`

```
[game]
;barter modifier for start inventory trade
barter_mod=0
;how much caps add to player for start inventory trade
caps_start=2000
;how much experience add at game start
exp_start=1000
;in how much seconds player inactivity dweller will die
inact=120
;first attention dialog box
inact1=100
;second
inact2=30
;last
inact3=20
;allow exit from game :)
game_exit=1
;run under a game frontend (see "Frontend integration"); allows returning to it
launcher_enabled=0
launcher_name=esde
launcher_return_on_exit=1
;allow player interact with options menu
disable_options=0
;allow player to save/load game
disable_saveload=0
;"one live - one save" gamemode
continues_play=1
;show kiosk screen message
start_message=1
;enable bomb screensaver
screensaver_enabled=1
;screensaver idle timeout
screensaver_timeout=15
;randomize locations. just shift each other
random_locations=1
;randomize containers items
random_containers=1
;how much quality levels is, 0 for disable
quality_total=6
;quality modifiers
quality_mod_0=0.75
quality_mod_1=0.90
quality_mod_2=1.00
quality_mod_3=1.25
quality_mod_4=1.50
quality_mod_5=2.50
;npc hp step for each quality level
quality_npc_hp_0=10
quality_npc_hp_1=25
quality_npc_hp_2=50
quality_npc_hp_3=100
quality_npc_hp_4=150
quality_npc_hp_5=250
;chance for items on ground by quality level
quality_ground_0=20
quality_ground_1=20
quality_ground_2=30
quality_ground_3=15
quality_ground_4=10
quality_ground_5=1

;override original config options
[overrides]
combat_difficulty=0
game_difficulty=0
language_filter=0


```


Start inventory and random containers config `kiosk_inv.cfg`
```
;start trade inventory
[inventory]
PID=COUNT

;this items not been added to generation stack 
[exclude]
LINE_NUM=PID

;this items will be keeped in container 
[keep]
LINE_NUM=PID

;this items will be spawned only once per game 
[once]
LINE_NUM=PID
```

Max lines is 256 for each sections

Example:
```
[inventory]
111=1
231=34
10=3
11=64
55=1
43=32
23=22

[exclude]
0=58
1=164
2=190
3=191
4=195
5=196
6=215
7=238
8=55
9=114

[keep]
0=127

[once]
0=216
1=192
2=193
3=194
4=217

```

Exec config `kiosk_exec.cfg`
**BEWARE! Lines will be system executed!**
```
[exec]
LINE_NUM=CMD
```

Example:
```
[exec]
0=touch /tmp/approach-apocalipse
```

With `FALLOUT_RETROARCH` the file is generated on the first run if missing:
```
[exec]
0=retroarch --menu
```
so RetroArch opens its menu on the way back to the frontend. The file you
provide is never overwritten.

Max 8 lines

Frontend integration (RetroArch / Emustation)

The kiosk build can run as an ordinary "game" of a game frontend
(EmulationStation/ES-DE/Emustation family). The frontend starts the game and
regains control when the game process exits.

- Build with the compatibility layer: `-DFALLOUT_RETROARCH=ON`.
- Launch contract: `fallout-ce --launcher=<name>` (e.g. `esde`, `emustation`).
  It records the frontend name in `launcher_name` and enables the launcher mode.
- In launcher mode, the "Exit" item of the main menu always returns to the
  frontend (clean process exit) as long as `launcher_return_on_exit=1`, even if
  `game_exit=0`. Keep `launcher_enabled=1` in `kiosk.cfg` to run frontend-style
  without passing the command line flag.
- With `FALLOUT_RETROARCH`, the commands of `kiosk_exec.cfg` are executed when
  the player confirms "Exit" in the in-game menu (instead of on character
  death). Use them to chain the next content or hand control further on the
  way back to the frontend.

Gamepad layout (`gamepad.cfg`)

The gamepad maps buttons and axes to the same game keys the keyboard uses. The
config file is written with the defaults on first run and can be edited to
rebind. Keys use the same names as `fallout_keys.cfg` values (`return`, `esc`,
`i`, `tab`, `home`, `f6`, `f7`, ...) or `mouse` for mouse actions.

```
[gamepad]
btn_dpad_up=i        ; Inventory
btn_dpad_down=tab    ; Automap
btn_dpad_left=c      ; Character
btn_dpad_right=p     ; PIP-Boy
btn_a=return         ; confirm / use in arrow mode
btn_b=space          ; interact with the current selection
btn_x=s              ; Skilldex
btn_y=esc            ; back / options menu (ESC)
btn_l1=n             ; toggle item mode (hands/use)
btn_r1=a             ; combat mode
btn_l3=home          ; center view on the player
btn_r3=mouse         ; mouse button: click menus / hold = actions menu
btn_start=f6         ; quick save
btn_back=f7          ; quick load
btn_guide=f12        ; screenshot
btn_l2=b             ; switch active hand
btn_r2=m             ; toggle 3D mouse mode

; left stick moves the player (arrow keys)
axis_leftx=left,right
axis_lefty=up,down
; right stick moves the mouse pointer
axis_rightx=mouse
axis_righty=mouse

mouse_speed=12       ; pointer speed in pixels per frame at full deflection
invert_leftx=0       ; invert analog axes (0/1)
invert_lefty=0
invert_rightx=0
invert_righty=0
invert_lefttrigger=0
invert_righttrigger=0
```

While the in-game actions (context) menu is open the right stick pointer moves
5x slower for precise item selection. The game fully owns the gamepad while it
runs; when you quit back to the frontend, the frontend takes over again.

Key rebind config `fallout_keys.cfg`
```
[main]
[game]
;set 'a' as 'b'
97=98 
[editor]
[inventory]
[pip]
```
## License

The source code is this repository is available under the [Sustainable Use License](LICENSE.md).

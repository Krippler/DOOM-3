# Building and running DOOM 3 on modern 64-bit systems

This tree builds DOOM 3 and Resurrection of Evil as native 64-bit programs
with SDL2 (window, OpenGL context, input and audio) and CMake. It needs the
game data from a retail, Steam or GOG copy of DOOM 3; no data is included.

## What changed compared to the 2011 source release

* **64-bit**: the code assumed `sizeof(long) == sizeof(void *) == 4` in many
  places. Pointers passed through the event system and script VM, DLL and
  thread handles, GUI expressions, save game/demo fields, file headers (DDS,
  BMP, fonts), checksums and bit-twiddling math macros were fixed for both
  LP64 (Linux) and LLP64 (Windows). The 32-bit x86 inline assembly is
  replaced with portable code.
* **SDL2 platform layer** (`neo/sys/sdl`): replaces X11, DirectInput,
  DirectSound/OSS and wgl/glX. Works with X11, Wayland (XWayland), Windows
  and any audio backend SDL supports.
* **Game libraries** are named after their game directory (`base`, `d3xp`)
  and loaded from next to the executable, so the original 32-bit `gamex86`
  libraries inside the paks are never used.
* **No CD key check** (the Steam and GOG versions have no keys).

## Widescreen, ultrawide and 4K

| cvar | default | meaning |
| --- | --- | --- |
| `r_mode` | `-2` | `-2` = desktop resolution, `-1` = `r_customWidth` x `r_customHeight`, `0`-`26` = presets (`listModes`) |
| `r_fullscreen` | `1` | fullscreen at the desktop resolution is a borderless window, other sizes change the display mode |
| `r_aspectRatio` | `-1` | `-1` = automatic from the window size, `0` = 4:3, `1` = 16:9, `2` = 16:10, `3` = 21:9, `4` = 32:9 |
| `r_scaleMenusTo43` | `1` | draw the menus, PDA and loading screens in a centered 4:3 area instead of stretching them |
| `g_hudScaleTo43` | `0` | also draw the HUD in a centered 4:3 area |
| `g_crosshairScaleTo43` | `1` | keep the crosshair round on wide screens |
| `r_display` | `0` | monitor to use |
| `r_allowHighDPI` | `1` | render at native pixels on high DPI (Retina, scaled Windows) displays |
| `r_swapInterval` | `0` | `1` = vsync, `-1` = adaptive vsync |

The field of view is "Hor+": `g_fov` is the horizontal FOV of a 4:3 screen,
wider screens keep the same vertical FOV and see more to the sides.

Resolution presets added to `listModes`: 1280x720, 1366x768, 1440x900,
1600x900, 1680x1050, 1920x1080, 1920x1200, 2560x1440, 2560x1600, 3200x1800,
3840x2160 (4K UHD), 2560x1080, 3440x1440, 3840x1600, 5120x2160 (21:9),
3840x1080, 5120x1440 (32:9) and 4096x2160. Any other size works with
`r_mode -1`, for example:

```
seta r_mode -1
seta r_customWidth 3440
seta r_customHeight 1440
vid_restart
```

Alt+Enter toggles fullscreen. Old `DoomConfig.cfg` files from the original
game set `r_mode 3` (640x480); delete the file or `seta r_mode -2`.

## Prebuilt Linux binaries and releases

GitHub Actions (`.github/workflows/build-linux.yml`) builds a ready to run
package, `doom3-linux-x86_64.tar.gz`, on every push and pull request, and
checks that the code also builds on Arch Linux. Download it from the
*Actions* tab (artifact `doom3-linux-x86_64`).

To cut a release, push a version tag; the package is attached to a GitHub
release with the notes from `dist/RELEASE_NOTES.md`:

```sh
git tag v1.0.0
git push origin v1.0.0
```

Or run the workflow by hand from the *Actions* tab and fill in
`release_tag` (and the commit or branch it should point at). The archive
contains `doom3`, `base.so`, `d3xp.so`, `README.txt` (from
`dist/README-linux.txt`) and empty `base/` and `d3xp/` folders for the paks.

## Building on Linux

```sh
# Debian / Ubuntu
sudo apt install build-essential cmake libsdl2-dev libgl-dev libopenal-dev
# Fedora:      sudo dnf install gcc-c++ cmake SDL2-devel mesa-libGL-devel openal-soft-devel
# CachyOS/Arch: sudo pacman -S --needed base-devel cmake sdl2 mesa openal

cmake -S neo -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix ~/games/doom3
```

This produces `doom3`, `base.so` and `d3xp.so`. Copy the paks of your copy
of the game into `~/games/doom3/base` (and `d3xp`) and run
`~/games/doom3/doom3`.

## Building for Windows (64-bit)

The MinGW-w64 build is the tested one (cross compiled from Linux, see
below). Visual Studio 2019/2022 (x64) should work with the same CMake
project but has not been tested yet. Get the SDL2 development package from
https://github.com/libsdl-org/SDL/releases (2.x):

```bat
cmake -S neo -B build -A x64 -DCMAKE_PREFIX_PATH=C:\path\to\SDL2-2.30.9\cmake
cmake --build build --config Release
```

Cross compiling from Linux with MinGW-w64:

```sh
cmake -S neo -B build-win64 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=neo/cmake/mingw-w64-x86_64.cmake \
      -DCMAKE_PREFIX_PATH=/path/to/SDL2-2.30.9/x86_64-w64-mingw32
cmake --build build-win64 -j
```

This produces `doom3.exe`, `base.dll` and `d3xp.dll`. Copy `SDL2.dll` next
to them.

## Installing

Put the executable, the game libraries (and `SDL2.dll` on Windows) into the
DOOM 3 installation directory, next to the `base` folder (and `d3xp` for
Resurrection of Evil), then run:

```sh
./doom3                      # DOOM 3
./doom3 +set fs_game d3xp    # Resurrection of Evil
```

Or keep them anywhere and point the engine at the data:
`./doom3 +set fs_basepath /path/to/doom3`.

The game must be patched to 1.3.1 (Steam and GOG versions are).

## Notes and limitations

* Multiplayer is not compatible with original 1.3.1 servers or clients
  (the game code checksums differ).
* Save games of the original 32-bit game can't be loaded, and the 64-bit
  saves are not readable by the original game.
* EAX / OpenAL hardware sound is not used; the engine's software mixer
  plays through SDL (stereo or 5.1 with `s_numberOfSpeakers 6`).
* The MFC based editors (Radiant, GUI editor, ...) are not built.
* The Visual Studio project files and SCons scripts in `neo/` are the
  original 32-bit build and are not maintained.
* macOS has not been tested with this build.

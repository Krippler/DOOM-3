Native 64-bit Linux build of the DOOM 3 1.3.1 GPL source, ported to SDL2.

## Install on CachyOS / Arch Linux
1. Download `doom3-gpl-<version>-1-x86_64.pkg.tar.zst` and install it: `sudo pacman -U doom3-gpl-*.pkg.tar.zst`
2. Copy the `base/` folder of your copy of DOOM 3 (`pak000.pk4` ... `pak008.pk4`) to `~/.local/share/doom3/base` (and the Resurrection of Evil `d3xp/` folder to `~/.local/share/doom3/d3xp`). A Steam copy of DOOM 3 is found automatically.
3. Start "DOOM 3" or "DOOM 3: Resurrection of Evil" from your application menu, or run `doom3`.

## Install anywhere else
1. Install SDL2 and an OpenGL driver. On CachyOS / Arch: `sudo pacman -S --needed sdl2 mesa`
2. Download and extract `doom3-linux-x86_64.tar.gz`.
3. Copy `pak000.pk4` ... `pak008.pk4` from your copy of DOOM 3 into its `base/` folder (and the Resurrection of Evil paks into `d3xp/`), or run it with `+set fs_basepath /path/to/doom3`.
4. Run `./doom3` (or `./doom3 +set fs_game d3xp` for Resurrection of Evil).

The game data is not included. No CD key is needed.

## Highlights
- Runs natively on X11 and Wayland, with PulseAudio / PipeWire sound.
- Starts fullscreen at your desktop resolution.
- Wide screens show more at the sides instead of a zoomed-in view (`r_aspectRatio 0` for the original behavior), and the menus, PDA and loading screens keep their proportions instead of being stretched (`r_scaleMenusTo43 0` to stretch them).
- 16:9, 16:10, 21:9, 32:9 and 4K resolution presets; `listModes` lists them, `r_mode -1` with `r_customWidth` / `r_customHeight` sets any size, `vid_restart` applies changes, and Alt+Enter toggles fullscreen.
- Includes the native 64-bit game code for DOOM 3 (`base.so`) and Resurrection of Evil (`d3xp.so`).

See `README.txt` in the archive for details.

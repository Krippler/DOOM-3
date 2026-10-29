DOOM 3 - native 64 bit Linux build
==================================

This is the id Software GPL source release (1.3.1) ported to 64 bit Linux
with SDL2, so it runs natively on modern distributions (Arch, CachyOS,
Fedora, Ubuntu, ...) on X11 and Wayland, with PulseAudio / PipeWire sound.

Contents
--------
  doom3      the game
  base.so    DOOM 3 game code
  d3xp.so    Resurrection of Evil game code

Game data
---------
The game data is NOT included, you need a copy of DOOM 3 (retail, Steam or
GOG, patched to 1.3.1). Copy the pak000.pk4 ... pak008.pk4 files from your
copy's base directory into the base directory next to doom3.  For
Resurrection of Evil also copy its pak000.pk4 ... pak001.pk4 into d3xp.

Alternatively leave the data where it is and point the game at it:

  ./doom3 +set fs_basepath /path/to/doom3

You need SDL2 and an OpenGL driver, normally already installed:

  sudo pacman -S --needed sdl2 mesa          (CachyOS / Arch)
  sudo apt install libsdl2-2.0-0             (Debian / Ubuntu)
  sudo dnf install SDL2                      (Fedora)

Then run:

  ./doom3                         DOOM 3
  ./doom3 +set fs_game d3xp       Resurrection of Evil

No CD key is needed.

Arch Linux / CachyOS package
----------------------------
The doom3-gpl-*.pkg.tar.zst package from the release installs the same
engine system wide ( sudo pacman -U doom3-gpl-*.pkg.tar.zst ), with
application menu entries for DOOM 3 and Resurrection of Evil.  Its doom3
command looks for the game data in, in this order:

  $DOOM3_DATA
  ~/.local/share/doom3          copy your base/ ( and d3xp/ ) folders here
  ~/.doom3
  the Steam copy of DOOM 3      found automatically
  /usr/share/doom3, /usr/share/games/doom3

Remove it with: sudo pacman -R doom3-gpl

Resolutions
-----------
By default the game starts fullscreen at your desktop resolution
(r_mode -2), as a borderless window.

From the console (Ctrl+Alt+~ or ~):

  r_mode -2              desktop resolution
  r_mode 14              1920x1080, see "listModes" for all modes
  r_mode -1              custom size, set r_customWidth / r_customHeight
  r_fullscreen 0 / 1     windowed / fullscreen
  vid_restart            apply the changes

Presets include 16:9, 16:10, 21:9 and 32:9 ultrawide and 4K modes.
Alt+Enter toggles fullscreen.  r_swapInterval 1 enables vsync.

On wide screens the 3D view keeps the vertical field of view of a 4:3
screen and shows more at the sides ("Hor+").  r_aspectRatio -1 (default)
picks this up from the window size; 0 restores the original 4:3 behavior.
The menus, PDA and loading screens keep their 4:3 proportions and are
centered instead of stretched (r_scaleMenusTo43 0 stretches them).
g_hudScaleTo43 1 does the same for the HUD.

Notes
-----
An old DoomConfig.cfg from the original game sets r_mode 3 (640x480);
delete it or run "seta r_mode -2".  Save games and multiplayer are not
compatible with the original 32 bit game.

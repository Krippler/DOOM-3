/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/

/*
	Windows pieces of the SDL build that used to live in the DirectX / wgl back end.
*/

#include "../../idlib/precompiled.h"
#include "../../sound/snd_local.h"
#pragma hdrstop

// the OpenAL entry points the sound code references. They are never loaded:
// the SDL build always uses the software mixer ( see Sys_LoadOpenAL in sdl_sound.cpp )
#include "../../openal/idal.cpp"

/*
==================
MapKey

only used by the dedicated console window, which needs the history keys.
game input goes through SDL
==================
*/
int MapKey( int key ) {
	int scanCode = ( key >> 16 ) & 0xFF;
	bool extended = ( key & ( 1 << 24 ) ) != 0;

	switch ( scanCode ) {
		case 0x48:	return extended ? K_UPARROW : K_KP_UPARROW;
		case 0x50:	return extended ? K_DOWNARROW : K_KP_DOWNARROW;
		case 0x1C:	return extended ? K_KP_ENTER : K_ENTER;
		case 0x01:	return K_ESCAPE;
		case 0x0F:	return K_TAB;
		case 0x0E:	return K_BACKSPACE;
		default:	return 0;
	}
}

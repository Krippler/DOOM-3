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
===============================================================================

	SDL2 keyboard, mouse and window event handling.
	Shared by all platforms ( replaces the X11, DirectInput and Cocoa code ).

===============================================================================
*/

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include <SDL.h>

#include "sdl_local.h"

static idCVar in_nograb( "in_nograb", "0", CVAR_SYSTEM | CVAR_NOCHEAT | CVAR_BOOL, "prevents input grabbing" );
static idCVar in_grabKeyboard( "in_grabKeyboard", "0", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_NOCHEAT | CVAR_BOOL, "grab the keyboard while the game has the mouse ( blocks alt-tab and similar )" );

// console input from the terminal / dedicated console window
#ifdef _WIN32
char *		Sys_ConsoleInput( void );
#else
char *		Posix_ConsoleInput( void );
#endif

/*
===============================================================================

	event queue

===============================================================================
*/

#define	MAX_QUED_EVENTS		256
#define	MASK_QUED_EVENTS	( MAX_QUED_EVENTS - 1 )

static sysEvent_t	eventQue[ MAX_QUED_EVENTS ];
static int			eventHead = 0;
static int			eventTail = 0;

/*
================
SDL_QueEvent

ptr should either be null, or point to a block of data that can be freed later
================
*/
static void SDL_QueEvent( sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) {
	sysEvent_t *ev;

	ev = &eventQue[ eventHead & MASK_QUED_EVENTS ];
	if ( eventHead - eventTail >= MAX_QUED_EVENTS ) {
		common->Printf( "SDL_QueEvent: overflow\n" );
		// we are discarding an event, but don't leak memory
		if ( ev->evPtr ) {
			Mem_Free( ev->evPtr );
			ev->evPtr = NULL;
		}
		eventTail++;
	}

	eventHead++;

	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;
}

#ifdef _WIN32
// the win32 dedicated console window queues its commands through this
void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) {
	SDL_QueEvent( type, value, value2, ptrLength, ptr );
}
#else
// the posix code still queues console events through this
void Posix_QueEvent( sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) {
	SDL_QueEvent( type, value, value2, ptrLength, ptr );
}
#endif

/*
===============================================================================

	polled input, used by the user command generator

===============================================================================
*/

struct pollEvent_t {
	int		key;		// key number or mouse action
	int		value;		// key state or mouse value
};

#define MAX_POLL_EVENTS		256

static pollEvent_t	keyPollEvents[ MAX_POLL_EVENTS ];
static int			numKeyPollEvents = 0;
static int			numKeyPollEventsReturned = 0;
static pollEvent_t	mousePollEvents[ MAX_POLL_EVENTS ];
static int			numMousePollEvents = 0;
static int			numMousePollEventsReturned = 0;

static void AddKeyboardPollEvent( int key, bool state ) {
	if ( numKeyPollEvents < MAX_POLL_EVENTS ) {
		keyPollEvents[ numKeyPollEvents ].key = key;
		keyPollEvents[ numKeyPollEvents ].value = state;
		numKeyPollEvents++;
	}
}

static void AddMousePollEvent( int action, int value ) {
	if ( numMousePollEvents < MAX_POLL_EVENTS ) {
		mousePollEvents[ numMousePollEvents ].key = action;
		mousePollEvents[ numMousePollEvents ].value = value;
		numMousePollEvents++;
	}
}

int Sys_PollKeyboardInputEvents( void ) {
	return numKeyPollEvents;
}

int Sys_ReturnKeyboardInputEvent( const int n, int &key, bool &state ) {
	if ( n >= numKeyPollEvents ) {
		return 0;
	}
	key = keyPollEvents[ n ].key;
	state = keyPollEvents[ n ].value != 0;
	return 1;
}

void Sys_EndKeyboardInputEvents( void ) {
	numKeyPollEvents = 0;
}

int Sys_PollMouseInputEvents( void ) {
	return numMousePollEvents;
}

int Sys_ReturnMouseInputEvent( const int n, int &action, int &value ) {
	if ( n >= numMousePollEvents ) {
		return 0;
	}
	action = mousePollEvents[ n ].key;
	value = mousePollEvents[ n ].value;
	return 1;
}

void Sys_EndMouseInputEvents( void ) {
	numMousePollEvents = 0;
}

/*
===============================================================================

	key mapping

===============================================================================
*/

static unsigned char s_scantochar[ 128 ];

/*
================
MapKey

translates an SDL key to a DOOM key number, 0 if it has no mapping
================
*/
static int MapKey( const SDL_Keysym &sym ) {
	// the console key is always the key below escape, whatever the keyboard layout is
	if ( sym.scancode == SDL_SCANCODE_GRAVE ) {
		return Sys_GetConsoleKey( false );
	}

	switch ( sym.sym ) {
		case SDLK_TAB:			return K_TAB;
		case SDLK_RETURN:		return K_ENTER;
		case SDLK_ESCAPE:		return K_ESCAPE;
		case SDLK_SPACE:		return K_SPACE;
		case SDLK_BACKSPACE:	return K_BACKSPACE;
		case SDLK_CAPSLOCK:		return K_CAPSLOCK;
		case SDLK_SCROLLLOCK:	return K_SCROLL;
		case SDLK_POWER:		return K_POWER;
		case SDLK_PAUSE:		return K_PAUSE;
		case SDLK_UP:			return K_UPARROW;
		case SDLK_DOWN:			return K_DOWNARROW;
		case SDLK_LEFT:			return K_LEFTARROW;
		case SDLK_RIGHT:		return K_RIGHTARROW;
		case SDLK_LGUI:			return K_LWIN;
		case SDLK_RGUI:			return K_RWIN;
		case SDLK_MENU:			return K_MENU;
		case SDLK_LALT:
		case SDLK_RALT:			return K_ALT;
		case SDLK_LCTRL:
		case SDLK_RCTRL:		return K_CTRL;
		case SDLK_LSHIFT:
		case SDLK_RSHIFT:		return K_SHIFT;
		case SDLK_INSERT:		return K_INS;
		case SDLK_DELETE:		return K_DEL;
		case SDLK_PAGEDOWN:		return K_PGDN;
		case SDLK_PAGEUP:		return K_PGUP;
		case SDLK_HOME:			return K_HOME;
		case SDLK_END:			return K_END;
		case SDLK_F1:			return K_F1;
		case SDLK_F2:			return K_F2;
		case SDLK_F3:			return K_F3;
		case SDLK_F4:			return K_F4;
		case SDLK_F5:			return K_F5;
		case SDLK_F6:			return K_F6;
		case SDLK_F7:			return K_F7;
		case SDLK_F8:			return K_F8;
		case SDLK_F9:			return K_F9;
		case SDLK_F10:			return K_F10;
		case SDLK_F11:			return K_F11;
		case SDLK_F12:			return K_F12;
		case SDLK_F13:			return K_F13;
		case SDLK_F14:			return K_F14;
		case SDLK_F15:			return K_F15;
		case SDLK_KP_7:			return K_KP_HOME;
		case SDLK_KP_8:			return K_KP_UPARROW;
		case SDLK_KP_9:			return K_KP_PGUP;
		case SDLK_KP_4:			return K_KP_LEFTARROW;
		case SDLK_KP_5:			return K_KP_5;
		case SDLK_KP_6:			return K_KP_RIGHTARROW;
		case SDLK_KP_1:			return K_KP_END;
		case SDLK_KP_2:			return K_KP_DOWNARROW;
		case SDLK_KP_3:			return K_KP_PGDN;
		case SDLK_KP_ENTER:		return K_KP_ENTER;
		case SDLK_KP_0:			return K_KP_INS;
		case SDLK_KP_PERIOD:	return K_KP_DEL;
		case SDLK_KP_DIVIDE:	return K_KP_SLASH;
		case SDLK_KP_MINUS:		return K_KP_MINUS;
		case SDLK_KP_PLUS:		return K_KP_PLUS;
		case SDLK_NUMLOCKCLEAR:	return K_KP_NUMLOCK;
		case SDLK_KP_MULTIPLY:	return K_KP_STAR;
		case SDLK_KP_EQUALS:	return K_KP_EQUALS;
		case SDLK_PRINTSCREEN:	return K_PRINT_SCR;
		case SDLK_MODE:			return K_RIGHT_ALT;
		default:
			break;
	}

	// printable keys use their ( lower case ) character as key number
	if ( sym.sym >= 32 && sym.sym < 127 ) {
		return idStr::ToLower( (char)sym.sym );
	}
	// latin-1 characters of european layouts
	if ( sym.sym >= 160 && sym.sym < 256 ) {
		return sym.sym;
	}

	return 0;
}

/*
===========
Sys_InitScanTable
===========
*/
void Sys_InitScanTable( void ) {
	for ( int i = 0; i < 128; i++ ) {
		s_scantochar[ i ] = (unsigned char)i;
	}
}

/*
===========
Sys_GetScanTable
===========
*/
const unsigned char *Sys_GetScanTable( void ) {
	return s_scantochar;
}

/*
===============
Sys_GetConsoleKey
===============
*/
unsigned char Sys_GetConsoleKey( bool shifted ) {
	return shifted ? '~' : '`';
}

/*
===============
Sys_MapCharForKey
===============
*/
unsigned char Sys_MapCharForKey( int key ) {
	return (unsigned char)key;
}

/*
===============================================================================

	mouse grabbing

===============================================================================
*/

static bool		grabWanted = false;
static bool		grabActive = false;

/*
================
UpdateGrab
================
*/
static void UpdateGrab( void ) {
	SDL_Window *window = GLimp_GetWindow();
	if ( !window ) {
		return;
	}

	bool focused = ( SDL_GetWindowFlags( window ) & SDL_WINDOW_INPUT_FOCUS ) != 0;
	bool grab = grabWanted && focused && !in_nograb.GetBool();

	if ( grab == grabActive ) {
		return;
	}
	grabActive = grab;

	SDL_ShowCursor( grab ? SDL_DISABLE : SDL_ENABLE );
	SDL_SetRelativeMouseMode( grab ? SDL_TRUE : SDL_FALSE );
	SDL_SetWindowGrab( window, grab ? SDL_TRUE : SDL_FALSE );
	if ( in_grabKeyboard.GetBool() ) {
		SDL_SetWindowKeyboardGrab( window, grab ? SDL_TRUE : SDL_FALSE );
	}
}

/*
===============
Sys_GrabMouseCursor
===============
*/
void Sys_GrabMouseCursor( bool grabIt ) {
	grabWanted = grabIt;
	UpdateGrab();
}

/*
=================
Sys_InitInput
=================
*/
void Sys_InitInput( void ) {
	common->Printf( "\n------- Input Initialization -------\n" );
	Sys_InitScanTable();
	SDL_SetHint( SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0" );
	SDL_StartTextInput();
	numKeyPollEvents = 0;
	numMousePollEvents = 0;
	grabActive = false;
	common->Printf( "------------------------------------\n" );
}

/*
=================
Sys_ShutdownInput
=================
*/
void Sys_ShutdownInput( void ) {
	grabWanted = false;
	UpdateGrab();
	SDL_StopTextInput();
}

/*
===============================================================================

	SDL event pumping

===============================================================================
*/

static void HandleKey( const SDL_KeyboardEvent &ev ) {
	int key = MapKey( ev.keysym );
	bool down = ( ev.state == SDL_PRESSED );

	// alt+enter toggles fullscreen
	if ( down && ev.keysym.sym == SDLK_RETURN && ( ev.keysym.mod & KMOD_ALT ) && !ev.repeat ) {
		cvarSystem->SetCVarBool( "r_fullscreen", !cvarSystem->GetCVarBool( "r_fullscreen" ) );
		cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "vid_restart\n" );
		return;
	}

	if ( !key ) {
		return;
	}

	SDL_QueEvent( SE_KEY, key, down, 0, NULL );

	// text editing keys, the printable characters arrive through SDL_TEXTINPUT
	if ( down ) {
		switch ( key ) {
			case K_ENTER:
			case K_KP_ENTER:
				SDL_QueEvent( SE_CHAR, '\r', 0, 0, NULL );
				break;
			case K_BACKSPACE:
				SDL_QueEvent( SE_CHAR, '\b', 0, 0, NULL );
				break;
			case K_TAB:
				SDL_QueEvent( SE_CHAR, '\t', 0, 0, NULL );
				break;
			default:
				// ctrl+key shortcuts ( ctrl-v paste, ctrl-c, ... ) are delivered as control chars
				if ( ( ev.keysym.mod & KMOD_CTRL ) && key >= 'a' && key <= 'z' ) {
					SDL_QueEvent( SE_CHAR, key - 'a' + 1, 0, 0, NULL );
				}
				break;
		}
	}

	if ( !ev.repeat ) {
		AddKeyboardPollEvent( key, down );
	}
}

static void HandleText( const char *text ) {
	// convert UTF-8 to latin-1, which is what the DOOM 3 fonts can display
	const unsigned char *s = (const unsigned char *)text;
	while ( *s ) {
		int c;
		if ( *s < 0x80 ) {
			c = *s++;
		} else if ( ( *s & 0xE0 ) == 0xC0 && s[1] ) {
			c = ( ( s[0] & 0x1F ) << 6 ) | ( s[1] & 0x3F );
			s += 2;
		} else {
			// outside of latin-1, skip the whole sequence
			s++;
			while ( ( *s & 0xC0 ) == 0x80 ) {
				s++;
			}
			continue;
		}
		if ( c == Sys_GetConsoleKey( false ) || c == Sys_GetConsoleKey( true ) ) {
			// don't type the console toggle character
			continue;
		}
		if ( c >= 32 && c < 256 && c != 127 ) {
			SDL_QueEvent( SE_CHAR, c, 0, 0, NULL );
		}
	}
}

static void HandleMouseButton( const SDL_MouseButtonEvent &ev ) {
	int b;
	switch ( ev.button ) {
		case SDL_BUTTON_LEFT:	b = 0; break;
		case SDL_BUTTON_RIGHT:	b = 1; break;
		case SDL_BUTTON_MIDDLE:	b = 2; break;
		case SDL_BUTTON_X1:		b = 3; break;
		case SDL_BUTTON_X2:		b = 4; break;
		default:
			if ( ev.button > SDL_BUTTON_X2 && ev.button <= 8 ) {
				b = ev.button - 1;
				break;
			}
			return;
	}
	bool down = ( ev.state == SDL_PRESSED );
	SDL_QueEvent( SE_KEY, K_MOUSE1 + b, down, 0, NULL );
	AddMousePollEvent( M_ACTION1 + b, down );
}

static void HandleWheel( const SDL_MouseWheelEvent &ev ) {
	int y = ev.y;
	if ( ev.direction == SDL_MOUSEWHEEL_FLIPPED ) {
		y = -y;
	}
	while ( y != 0 ) {
		int key = ( y > 0 ) ? K_MWHEELUP : K_MWHEELDOWN;
		SDL_QueEvent( SE_KEY, key, true, 0, NULL );
		SDL_QueEvent( SE_KEY, key, false, 0, NULL );
		AddMousePollEvent( M_DELTAZ, ( y > 0 ) ? 1 : -1 );
		y += ( y > 0 ) ? -1 : 1;
	}
}

static void HandleWindowEvent( const SDL_WindowEvent &ev ) {
	switch ( ev.event ) {
		case SDL_WINDOWEVENT_FOCUS_GAINED:
		case SDL_WINDOWEVENT_FOCUS_LOST:
			UpdateGrab();
			break;

		case SDL_WINDOWEVENT_SIZE_CHANGED:
			GLimp_WindowResized( ev.data1, ev.data2 );
			break;

		default:
			break;
	}
}

/*
================
FlushMouseMotion
================
*/
static void FlushMouseMotion( int &motionX, int &motionY ) {
	if ( motionX || motionY ) {
		SDL_QueEvent( SE_MOUSE, motionX, motionY, 0, NULL );
		AddMousePollEvent( M_DELTAX, motionX );
		AddMousePollEvent( M_DELTAY, motionY );
		motionX = motionY = 0;
	}
}

/*
================
Sys_PumpSDLEvents
================
*/
static void Sys_PumpSDLEvents( void ) {
	SDL_Event ev;
	int motionX = 0, motionY = 0;

	while ( SDL_PollEvent( &ev ) ) {
		switch ( ev.type ) {
			case SDL_QUIT:
				cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
				break;

			case SDL_WINDOWEVENT:
				HandleWindowEvent( ev.window );
				break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
				HandleKey( ev.key );
				break;

			case SDL_TEXTINPUT:
				HandleText( ev.text.text );
				break;

			case SDL_MOUSEMOTION:
				if ( grabActive && ( ev.motion.xrel || ev.motion.yrel ) ) {
					// high polling rate mice send thousands of events per second,
					// queue the sum instead of overflowing the event buffers
					motionX += ev.motion.xrel;
					motionY += ev.motion.yrel;
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				FlushMouseMotion( motionX, motionY );
				HandleMouseButton( ev.button );
				break;

			case SDL_MOUSEWHEEL:
				FlushMouseMotion( motionX, motionY );
				HandleWheel( ev.wheel );
				break;

			default:
				break;
		}
	}
	FlushMouseMotion( motionX, motionY );
}

/*
================
Sys_GenerateEvents

called during frame loops, pacifier updates etc.
================
*/
void Sys_GenerateEvents( void ) {
	static bool entered = false;
	if ( entered ) {
		return;
	}
	entered = true;

	if ( SDL_WasInit( SDL_INIT_VIDEO ) ) {
		Sys_PumpSDLEvents();
	}

#ifdef _WIN32
	char *s = Sys_ConsoleInput();
#else
	char *s = Posix_ConsoleInput();
#endif
	if ( s ) {
		int len = strlen( s ) + 1;
		char *b = (char *)Mem_Alloc( len );
		strcpy( b, s );
		SDL_QueEvent( SE_CONSOLE, 0, 0, len, b );
	}

	entered = false;
}

/*
================
Sys_GetEvent
================
*/
sysEvent_t Sys_GetEvent( void ) {
	static sysEvent_t ev;

	// return if we have data
	if ( eventHead > eventTail ) {
		eventTail++;
		return eventQue[ ( eventTail - 1 ) & MASK_QUED_EVENTS ];
	}

	// return the empty event
	memset( &ev, 0, sizeof( ev ) );
	return ev;
}

/*
================
Sys_ClearEvents
================
*/
void Sys_ClearEvents( void ) {
	eventHead = eventTail = 0;
	numKeyPollEvents = 0;
	numMousePollEvents = 0;
}

/*
================
Sys_GetClipboardData
================
*/
char *Sys_GetClipboardData( void ) {
	if ( !SDL_HasClipboardText() ) {
		return NULL;
	}
	char *text = SDL_GetClipboardText();
	if ( !text ) {
		return NULL;
	}
	// the caller frees with Mem_Free
	int len = strlen( text ) + 1;
	char *data = (char *)Mem_Alloc( len );
	strcpy( data, text );
	SDL_free( text );
	return data;
}

/*
================
Sys_SetClipboardData
================
*/
void Sys_SetClipboardData( const char *string ) {
	SDL_SetClipboardText( string );
}

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

	SDL2 OpenGL window and context management.
	Shared by all platforms ( replaces wgl, glX and NSOpenGL code ).

	Fullscreen at the desktop resolution uses a borderless desktop window,
	which is what modern compositors and multi monitor setups expect.
	Other fullscreen resolutions switch the display mode.

===============================================================================
*/

#include "../../idlib/precompiled.h"
#include "../../renderer/tr_local.h"
#pragma hdrstop

#include <SDL.h>

#include "sdl_local.h"

static idCVar r_display( "r_display", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "index of the monitor the game runs on, needs vid_restart" );
static idCVar r_allowHighDPI( "r_allowHighDPI", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "render at the native pixel resolution on high DPI displays" );

static SDL_Window *		window = NULL;
static SDL_GLContext	context = NULL;
static int				currentSwapInterval = -999;

static Uint16			savedGamma[ 3 ][ 256 ];
static bool				gammaSaved = false;

/*
================
GLimp_GetWindow
================
*/
SDL_Window *GLimp_GetWindow( void ) {
	return window;
}

/*
================
GLimp_InitVideo
================
*/
static bool GLimp_InitVideo( void ) {
	if ( SDL_WasInit( SDL_INIT_VIDEO ) ) {
		return true;
	}
	if ( SDL_InitSubSystem( SDL_INIT_VIDEO ) != 0 ) {
		common->Printf( "SDL_Init( SDL_INIT_VIDEO ) failed: %s\n", SDL_GetError() );
		return false;
	}
	return true;
}

/*
================
GLimp_DisplayIndex
================
*/
static int GLimp_DisplayIndex( void ) {
	int num = SDL_GetNumVideoDisplays();
	int display = r_display.GetInteger();
	if ( display < 0 || display >= num ) {
		display = 0;
	}
	return display;
}

/*
================
GLimp_GetDesktopSize

returns the resolution of the monitor the game is running on
================
*/
bool GLimp_GetDesktopSize( int *width, int *height ) {
	SDL_DisplayMode mode;

	if ( !GLimp_InitVideo() ) {
		return false;
	}
	if ( SDL_GetDesktopDisplayMode( GLimp_DisplayIndex(), &mode ) != 0 ) {
		common->Printf( "SDL_GetDesktopDisplayMode failed: %s\n", SDL_GetError() );
		return false;
	}
	*width = mode.w;
	*height = mode.h;
	return true;
}

/*
================
GLimp_UpdateDrawableSize

the drawable can be larger than the window size on high DPI displays
================
*/
static void GLimp_UpdateDrawableSize( void ) {
	int w, h;
	SDL_GL_GetDrawableSize( window, &w, &h );
	if ( w > 0 && h > 0 ) {
		glConfig.vidWidth = w;
		glConfig.vidHeight = h;
	}
}

/*
================
GLimp_WindowResized
================
*/
void GLimp_WindowResized( int width, int height ) {
	if ( !window ) {
		return;
	}
	GLimp_UpdateDrawableSize();
}

/*
================
GLimp_SetFullscreenMode

configures the window for the requested fullscreen / windowed state
================
*/
static bool GLimp_SetFullscreenMode( glimpParms_t parms ) {
	int display = GLimp_DisplayIndex();

	if ( !parms.fullScreen ) {
		if ( SDL_SetWindowFullscreen( window, 0 ) != 0 ) {
			common->Printf( "couldn't leave fullscreen: %s\n", SDL_GetError() );
			return false;
		}
		SDL_SetWindowSize( window, parms.width, parms.height );
		SDL_SetWindowPosition( window, SDL_WINDOWPOS_CENTERED_DISPLAY( display ), SDL_WINDOWPOS_CENTERED_DISPLAY( display ) );
		return true;
	}

	int desktopWidth, desktopHeight;
	if ( GLimp_GetDesktopSize( &desktopWidth, &desktopHeight ) &&
			desktopWidth == parms.width && desktopHeight == parms.height ) {
		// borderless fullscreen window at the native resolution
		if ( SDL_SetWindowFullscreen( window, SDL_WINDOW_FULLSCREEN_DESKTOP ) != 0 ) {
			common->Printf( "couldn't set desktop fullscreen: %s\n", SDL_GetError() );
			return false;
		}
		common->Printf( "...using borderless desktop fullscreen\n" );
		return true;
	}

	// exclusive fullscreen with a display mode change
	SDL_DisplayMode want, closest;
	memset( &want, 0, sizeof( want ) );
	want.w = parms.width;
	want.h = parms.height;
	want.refresh_rate = parms.displayHz;
	if ( !SDL_GetClosestDisplayMode( display, &want, &closest ) ) {
		common->Printf( "no display mode close to %dx%d: %s\n", parms.width, parms.height, SDL_GetError() );
		return false;
	}
	if ( SDL_SetWindowDisplayMode( window, &closest ) != 0 ) {
		common->Printf( "couldn't set display mode: %s\n", SDL_GetError() );
		return false;
	}
	if ( SDL_SetWindowFullscreen( window, SDL_WINDOW_FULLSCREEN ) != 0 ) {
		common->Printf( "couldn't set fullscreen: %s\n", SDL_GetError() );
		return false;
	}
	common->Printf( "...display mode %dx%d @ %d Hz\n", closest.w, closest.h, closest.refresh_rate );
	return true;
}

/*
================
GLimp_UpdateSwapInterval
================
*/
static void GLimp_UpdateSwapInterval( void ) {
	int interval = r_swapInterval.GetInteger();
	if ( interval == currentSwapInterval ) {
		return;
	}
	currentSwapInterval = interval;
	if ( SDL_GL_SetSwapInterval( interval ) != 0 && interval < 0 ) {
		// adaptive vsync is not supported everywhere, fall back to normal vsync
		SDL_GL_SetSwapInterval( 1 );
	}
}

/*
================
GLimp_Init
================
*/
bool GLimp_Init( glimpParms_t parms ) {
	common->Printf( "Initializing OpenGL subsystem through SDL %d.%d.%d\n", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL );

	if ( !GLimp_InitVideo() ) {
		return false;
	}

	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_STEREO, parms.stereo ? 1 : 0 );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, parms.multiSamples > 1 ? 1 : 0 );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, parms.multiSamples > 1 ? parms.multiSamples : 0 );

	int display = GLimp_DisplayIndex();
	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;
	if ( r_allowHighDPI.GetBool() ) {
		flags |= SDL_WINDOW_ALLOW_HIGHDPI;
	}

	window = SDL_CreateWindow( GAME_NAME,
		SDL_WINDOWPOS_CENTERED_DISPLAY( display ), SDL_WINDOWPOS_CENTERED_DISPLAY( display ),
		parms.width, parms.height, flags );

	if ( !window && parms.multiSamples > 1 ) {
		common->Printf( "couldn't create a window with %dx MSAA, retrying without\n", parms.multiSamples );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0 );
		parms.multiSamples = 0;
		window = SDL_CreateWindow( GAME_NAME,
			SDL_WINDOWPOS_CENTERED_DISPLAY( display ), SDL_WINDOWPOS_CENTERED_DISPLAY( display ),
			parms.width, parms.height, flags );
	}

	if ( !window ) {
		common->Printf( "SDL_CreateWindow failed: %s\n", SDL_GetError() );
		return false;
	}

	if ( !GLimp_SetFullscreenMode( parms ) ) {
		SDL_DestroyWindow( window );
		window = NULL;
		return false;
	}

	SDL_ShowWindow( window );
	SDL_RaiseWindow( window );

	context = SDL_GL_CreateContext( window );
	if ( !context ) {
		common->Printf( "SDL_GL_CreateContext failed: %s\n", SDL_GetError() );
		SDL_DestroyWindow( window );
		window = NULL;
		return false;
	}
	SDL_GL_MakeCurrent( window, context );

	currentSwapInterval = -999;
	GLimp_UpdateSwapInterval();

	int r, g, b, a, depth, stencil;
	SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &r );
	SDL_GL_GetAttribute( SDL_GL_GREEN_SIZE, &g );
	SDL_GL_GetAttribute( SDL_GL_BLUE_SIZE, &b );
	SDL_GL_GetAttribute( SDL_GL_ALPHA_SIZE, &a );
	SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE, &depth );
	SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &stencil );
	common->Printf( "...got %d bits color, %d bits alpha, %d bits depth, %d bits stencil\n", r + g + b, a, depth, stencil );
	if ( stencil < 8 ) {
		common->Printf( "WARNING: no 8 bit stencil buffer, shadows will not render correctly\n" );
	}

	glConfig.colorBits = r + g + b;
	glConfig.depthBits = depth;
	glConfig.stencilBits = stencil;
	glConfig.isFullscreen = parms.fullScreen;

	SDL_DisplayMode mode;
	if ( SDL_GetWindowDisplayMode( window, &mode ) == 0 && mode.refresh_rate ) {
		glConfig.displayFrequency = mode.refresh_rate;
	} else {
		glConfig.displayFrequency = parms.displayHz;
	}

	GLimp_UpdateDrawableSize();
	common->Printf( "...window %dx%d, drawable %dx%d%s\n", parms.width, parms.height,
		glConfig.vidWidth, glConfig.vidHeight, parms.fullScreen ? " fullscreen" : "" );

	// keep the original gamma so it can be restored at shutdown
	gammaSaved = ( SDL_GetWindowGammaRamp( window, savedGamma[ 0 ], savedGamma[ 1 ], savedGamma[ 2 ] ) == 0 );

	return true;
}

/*
================
GLimp_SetScreenParms
================
*/
bool GLimp_SetScreenParms( glimpParms_t parms ) {
	if ( !window ) {
		return false;
	}
	if ( !GLimp_SetFullscreenMode( parms ) ) {
		return false;
	}
	glConfig.isFullscreen = parms.fullScreen;
	GLimp_UpdateDrawableSize();
	return true;
}

/*
================
GLimp_Shutdown
================
*/
void GLimp_Shutdown( void ) {
	common->Printf( "Shutting down OpenGL subsystem\n" );

	if ( window && gammaSaved ) {
		SDL_SetWindowGammaRamp( window, savedGamma[ 0 ], savedGamma[ 1 ], savedGamma[ 2 ] );
	}
	gammaSaved = false;

	if ( context ) {
		SDL_GL_MakeCurrent( window, NULL );
		SDL_GL_DeleteContext( context );
		context = NULL;
	}
	if ( window ) {
		SDL_DestroyWindow( window );
		window = NULL;
	}
}

/*
================
GLimp_SwapBuffers
================
*/
void GLimp_SwapBuffers( void ) {
	GLimp_UpdateSwapInterval();
	SDL_GL_SwapWindow( window );
}

/*
================
GLimp_SetGamma

hardware gamma ramps don't work everywhere ( e.g. Wayland ), use r_gamma / r_brightness then
================
*/
void GLimp_SetGamma( unsigned short red[256], unsigned short green[256], unsigned short blue[256] ) {
	if ( !window ) {
		return;
	}
	if ( SDL_SetWindowGammaRamp( window, red, green, blue ) != 0 ) {
		static bool warned = false;
		if ( !warned ) {
			common->Printf( "WARNING: SDL_SetWindowGammaRamp failed: %s\n", SDL_GetError() );
			warned = true;
		}
	}
}

/*
================
GLimp_ExtensionPointer
================
*/
GLExtension_t GLimp_ExtensionPointer( const char *name ) {
	GLExtension_t ptr = (GLExtension_t)SDL_GL_GetProcAddress( name );
	if ( !ptr ) {
		common->Printf( "Couldn't find proc address for: %s\n", name );
	}
	return ptr;
}

/*
================
GLimp_EnableLogging
================
*/
void GLimp_EnableLogging( bool enable ) {
}

/*
================
SMP is not supported, the back end runs in the main thread
================
*/
bool GLimp_SpawnRenderThread( void (*function)( void ) ) {
	return false;
}

void *GLimp_BackEndSleep( void ) {
	return NULL;
}

void GLimp_FrontEndSleep( void ) {
}

void GLimp_WakeBackEnd( void *data ) {
}

void GLimp_ActivateContext( void ) {
	if ( window && context ) {
		SDL_GL_MakeCurrent( window, context );
	}
}

void GLimp_DeactivateContext( void ) {
	if ( window ) {
		SDL_GL_MakeCurrent( window, NULL );
	}
}

#ifndef _WIN32
static idCVar sys_videoRam( "sys_videoRam", "0", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER, "Texture memory on the video card (in megabytes) - 0: autodetect", 0, 65536 );

/*
================
Sys_GetVideoRam

returns in megabytes. Only used to pick default settings for very old cards,
any card that runs a modern desktop has more memory than DOOM 3 can use.
================
*/
int Sys_GetVideoRam( void ) {
	if ( sys_videoRam.GetInteger() > 0 ) {
		return sys_videoRam.GetInteger();
	}
	return 1024;
}
#endif

/*
================
Sys_ShowWindow
================
*/
void Sys_ShowWindow( bool show ) {
	if ( window ) {
		if ( show ) {
			SDL_ShowWindow( window );
		} else {
			SDL_HideWindow( window );
		}
	}
}

/*
================
Sys_IsWindowVisible
================
*/
bool Sys_IsWindowVisible( void ) {
	return window && ( SDL_GetWindowFlags( window ) & SDL_WINDOW_SHOWN ) != 0;
}

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

	SDL2 audio output for the DOOM 3 software mixer.

	The engine mixes MIXBUFFER_SAMPLES at a time from the async thread
	( com_asyncSound 3 ) and hands the 16 bit samples to Write(), which
	queues them on the SDL audio device. Works with every backend SDL
	supports: WASAPI, PulseAudio, PipeWire, ALSA, CoreAudio, ...

===============================================================================
*/

#include "../../idlib/precompiled.h"
#include "../../sound/snd_local.h"
#pragma hdrstop

#include <SDL.h>

static idCVar s_sdlDevice( "s_sdlDevice", "", CVAR_SYSTEM | CVAR_ARCHIVE, "name of the SDL audio output device, empty for the system default" );
// the sound settings of the menus have a driver choice, SDL picks the backend ( SDL_AUDIODRIVER )
static const char *s_driverArgs[] = { "best", "oss", "alsa", NULL };
static idCVar s_driver( "s_driver", s_driverArgs[0], CVAR_SYSTEM | CVAR_ARCHIVE, "unused, SDL picks the audio driver, set SDL_AUDIODRIVER to choose one", s_driverArgs, idCmdSystem::ArgCompletion_String<s_driverArgs> );
static idCVar s_sdlMaxQueuedBlocks( "s_sdlMaxQueuedBlocks", "3", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER, "maximum number of mixed blocks queued to the audio device ( latency )", 2, 16 );

class idAudioHardwareSDL : public idAudioHardware {
public:
							idAudioHardwareSDL();
	virtual					~idAudioHardwareSDL();

	virtual bool			Initialize( void );

	// the memory mapped API ( com_asyncSound 1 ) is not used with SDL
	virtual bool			Lock( void **pDSLockedBuffer, ulong *dwDSLockedBufferSize ) { return false; }
	virtual bool			Unlock( void *pDSLockedBuffer, dword dwDSLockedBufferSize ) { return false; }
	virtual bool			GetCurrentPosition( ulong *pdwCurrentWriteCursor ) { return false; }

	virtual bool			Flush( void );
	virtual void			Write( bool flushing );

	virtual int				GetNumberOfSpeakers( void ) { return channels; }
	virtual int				GetMixBufferSize( void ) { return bufferSize; }
	virtual short *			GetMixBuffer( void ) { return buffer; }

private:
	void					Release( void );

	SDL_AudioDeviceID		device;
	int						channels;
	int						bufferSize;		// in bytes
	short *					buffer;
};

/*
=================
idAudioHardware::Alloc
=================
*/
idAudioHardware *idAudioHardware::Alloc() {
	return new idAudioHardwareSDL;
}

/*
=================
idAudioHardware::~idAudioHardware
=================
*/
idAudioHardware::~idAudioHardware() {
}

/*
=================
idAudioHardwareSDL::idAudioHardwareSDL
=================
*/
idAudioHardwareSDL::idAudioHardwareSDL() {
	device = 0;
	channels = 2;
	bufferSize = 0;
	buffer = NULL;
}

/*
=================
idAudioHardwareSDL::~idAudioHardwareSDL
=================
*/
idAudioHardwareSDL::~idAudioHardwareSDL() {
	Release();
}

/*
=================
idAudioHardwareSDL::Release
=================
*/
void idAudioHardwareSDL::Release( void ) {
	if ( device ) {
		SDL_PauseAudioDevice( device, 1 );
		SDL_ClearQueuedAudio( device );
		SDL_CloseAudioDevice( device );
		device = 0;
	}
	if ( buffer ) {
		Mem_Free16( buffer );
		buffer = NULL;
	}
	bufferSize = 0;
}

/*
=================
idAudioHardwareSDL::Initialize
=================
*/
bool idAudioHardwareSDL::Initialize( void ) {
	common->Printf( "------ SDL Sound Initialization ------\n" );

	Release();

	if ( !SDL_WasInit( SDL_INIT_AUDIO ) ) {
		if ( SDL_InitSubSystem( SDL_INIT_AUDIO ) != 0 ) {
			common->Warning( "SDL_Init( SDL_INIT_AUDIO ) failed: %s", SDL_GetError() );
			return false;
		}
	}
	common->Printf( "SDL audio driver: %s\n", SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "none" );

	int speakers = idSoundSystemLocal::s_numberOfSpeakers.GetInteger();
	if ( speakers != 2 && speakers != 6 ) {
		common->Warning( "invalid value for s_numberOfSpeakers. Use either 2 or 6" );
		speakers = 2;
		idSoundSystemLocal::s_numberOfSpeakers.SetInteger( 2 );
	}

	SDL_AudioSpec want, have;
	memset( &want, 0, sizeof( want ) );
	want.freq = PRIMARYFREQ;
	want.format = AUDIO_S16SYS;
	want.channels = speakers;
	want.samples = 1024;
	want.callback = NULL;	// we push samples with SDL_QueueAudio

	const char *deviceName = s_sdlDevice.GetString()[ 0 ] ? s_sdlDevice.GetString() : NULL;
	device = SDL_OpenAudioDevice( deviceName, 0, &want, &have, 0 );
	if ( !device && speakers != 2 ) {
		common->Warning( "couldn't open a %d channel audio device ( %s ), falling back to stereo", speakers, SDL_GetError() );
		speakers = want.channels = 2;
		idSoundSystemLocal::s_numberOfSpeakers.SetInteger( 2 );
		device = SDL_OpenAudioDevice( deviceName, 0, &want, &have, 0 );
	}
	if ( !device ) {
		common->Warning( "SDL_OpenAudioDevice failed: %s", SDL_GetError() );
		return false;
	}

	channels = speakers;
	bufferSize = MIXBUFFER_SAMPLES * channels * sizeof( short );
	buffer = (short *)Mem_Alloc16( bufferSize );
	memset( buffer, 0, bufferSize );

	common->Printf( "%s - %d Hz, %d channels, 16 bit\n", deviceName ? deviceName : "default device", have.freq, have.channels );
	common->Printf( "allocated a mix buffer of %d bytes\n", bufferSize );

	SDL_PauseAudioDevice( device, 0 );

	common->Printf( "--------------------------------------\n" );
	return true;
}

/*
=================
idAudioHardwareSDL::Flush

returns true when there is room for another mixed block
=================
*/
bool idAudioHardwareSDL::Flush( void ) {
	if ( !device ) {
		return false;
	}
	return SDL_GetQueuedAudioSize( device ) < (Uint32)( bufferSize * s_sdlMaxQueuedBlocks.GetInteger() );
}

/*
=================
idAudioHardwareSDL::Write
=================
*/
void idAudioHardwareSDL::Write( bool flushing ) {
	if ( !device || flushing ) {
		return;
	}
	// the mixer runs on the system clock, the audio device on its own clock.
	// if they drift apart too much, drop a block instead of building up latency
	if ( SDL_GetQueuedAudioSize( device ) >= (Uint32)( bufferSize * s_sdlMaxQueuedBlocks.GetInteger() ) ) {
		return;
	}
	if ( SDL_QueueAudio( device, buffer, bufferSize ) != 0 ) {
		Sys_Printf( "SDL_QueueAudio failed: %s\n", SDL_GetError() );
	}
}

/*
===============
Sys_LoadOpenAL
===============
*/
bool Sys_LoadOpenAL( void ) {
	return false;
}

/*
===============
Sys_FreeOpenAL
===============
*/
void Sys_FreeOpenAL( void ) {
}

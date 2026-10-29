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

	Portable CPU, timer and FPU control functions for the SDL builds.
	These replace the 32-bit x86 inline assembly of the original
	win32 and linux back ends, so they work on x86_64 and ARM64 as well.

===============================================================================
*/

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include <SDL.h>
#include <SDL_cpuinfo.h>

#if defined( __i386__ ) || defined( __x86_64__ ) || defined( _M_IX86 ) || defined( _M_X64 )
#define ID_SDL_X86 1
#include <xmmintrin.h>
#include <float.h>
#else
#define ID_SDL_X86 0
#endif

#ifndef _WIN32
#include <fenv.h>
#endif

/*
================
Sys_GetClockTicks
================
*/
double Sys_GetClockTicks( void ) {
	return (double)SDL_GetPerformanceCounter();
}

/*
================
Sys_ClockTicksPerSecond
================
*/
double Sys_ClockTicksPerSecond( void ) {
	static double ticks = 0.0;
	if ( ticks == 0.0 ) {
		ticks = (double)SDL_GetPerformanceFrequency();
	}
	return ticks;
}

/*
================
Sys_GetProcessorId

The hand written SIMD code paths of the original engine are 32-bit x86
MSVC inline assembly. On every other target the generic C++ path is used
( which modern compilers vectorize well ). We still report FTZ / DAZ so
denormal handling is put into the fast mode on SSE capable CPUs.
================
*/
cpuid_t Sys_GetProcessorId( void ) {
	int id = CPUID_GENERIC;

#if defined( _MSC_VER ) && defined( _M_IX86 )
	if ( SDL_HasMMX() ) {
		id |= CPUID_MMX;
	}
	if ( SDL_Has3DNow() ) {
		id |= CPUID_3DNOW;
	}
	if ( SDL_HasSSE() ) {
		id |= CPUID_SSE;
	}
	if ( SDL_HasSSE2() ) {
		id |= CPUID_SSE2;
	}
	if ( SDL_HasSSE3() ) {
		id |= CPUID_SSE3;
	}
#endif

#if ID_SDL_X86
	if ( SDL_HasSSE() ) {
		id |= CPUID_FTZ;
	}
	if ( SDL_HasSSE2() ) {
		id |= CPUID_DAZ;
	}
#endif

	return (cpuid_t)id;
}

/*
================
Sys_GetProcessorString
================
*/
const char *Sys_GetProcessorString( void ) {
	static char str[ 256 ];
	idStr::snPrintf( str, sizeof( str ), "%s, %d logical cores%s%s%s%s%s",
		CPUSTRING,
		SDL_GetCPUCount(),
		SDL_HasSSE() ? " SSE" : "",
		SDL_HasSSE2() ? " SSE2" : "",
		SDL_HasSSE3() ? " SSE3" : "",
		SDL_HasAVX() ? " AVX" : "",
		SDL_HasNEON() ? " NEON" : "" );
	return str;
}

/*
================
Sys_FPU_StackIsEmpty
================
*/
bool Sys_FPU_StackIsEmpty( void ) {
	return true;
}

/*
================
Sys_FPU_ClearStack
================
*/
void Sys_FPU_ClearStack( void ) {
}

/*
================
Sys_FPU_GetState
================
*/
const char *Sys_FPU_GetState( void ) {
#if ID_SDL_X86
	static char buf[ 64 ];
	idStr::snPrintf( buf, sizeof( buf ), "MXCSR = 0x%08x\n", _mm_getcsr() );
	return buf;
#else
	return "";
#endif
}

/*
================
Sys_FPU_EnableExceptions
================
*/
void Sys_FPU_EnableExceptions( int exceptions ) {
}

/*
================
Sys_FPU_SetPrecision

SSE math always uses the precision of the data type
================
*/
void Sys_FPU_SetPrecision( int precision ) {
}

/*
================
Sys_FPU_SetRounding
================
*/
void Sys_FPU_SetRounding( int rounding ) {
#if ID_SDL_X86
	static const unsigned int modes[ 4 ] = { _MM_ROUND_NEAREST, _MM_ROUND_DOWN, _MM_ROUND_UP, _MM_ROUND_TOWARD_ZERO };
	_MM_SET_ROUNDING_MODE( modes[ rounding & 3 ] );
#elif !defined( _WIN32 )
	static const int modes[ 4 ] = { FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO };
	fesetround( modes[ rounding & 3 ] );
#endif
}

/*
================
Sys_FPU_SetDAZ
================
*/
void Sys_FPU_SetDAZ( bool enable ) {
#if ID_SDL_X86
	unsigned int csr = _mm_getcsr();
	csr &= ~( 1u << 6 );
	if ( enable ) {
		csr |= ( 1u << 6 );
	}
	_mm_setcsr( csr );
#endif
}

/*
================
Sys_FPU_SetFTZ
================
*/
void Sys_FPU_SetFTZ( bool enable ) {
#if ID_SDL_X86
	_MM_SET_FLUSH_ZERO_MODE( enable ? _MM_FLUSH_ZERO_ON : _MM_FLUSH_ZERO_OFF );
#endif
}

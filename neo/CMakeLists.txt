cmake_minimum_required(VERSION 3.16)

project(doom3 C CXX)

# Modern 64-bit (and 32-bit) build of the DOOM 3 GPL source.
# Produces:
#   doom3       - the engine executable
#   base        - the DOOM 3 game library      (base.so / base.dll / base.dylib)
#   d3xp        - the Resurrection of Evil game library (d3xp.so / ...)
# Copy the executable and the game libraries next to each other in the
# directory that contains your "base" (and optionally "d3xp") data folder.

option(DOOM3_GAME_LIBS   "Build the game libraries (base and d3xp)" ON)
option(DOOM3_DEDICATED   "Build a dedicated server executable" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(STATUS "Building DOOM 3 for a 64-bit target")
else()
	message(STATUS "Building DOOM 3 for a 32-bit target")
endif()

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# no SDL2main: Windows uses the engine's own WinMain ( see win_main.cpp )
if(TARGET SDL2::SDL2)
	set(SDL2_TARGETS SDL2::SDL2)
else()
	include_directories(${SDL2_INCLUDE_DIRS})
	set(SDL2_TARGETS ${SDL2_LIBRARIES})
endif()

# ---------------------------------------------------------------------------
# compiler settings
# ---------------------------------------------------------------------------

# no CD key checks: the Steam and GOG releases do not have keys
add_definitions(-DID_ENABLE_CURL=0 -DUSE_LIBC_MALLOC=1 -DID_GL_HARDLINK -DID_SDL -DID_ENFORCE_KEY=0)

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -DNOMINMAX)
	add_compile_options(/W3 /MP /wd4100 /wd4244 /wd4267 /wd4996 /wd4018)
else()
	add_compile_options(
		-pipe
		-fno-strict-aliasing
		-fmessage-length=0
		-Wno-unknown-pragmas
		-Wno-write-strings
		-Wno-narrowing
		-Wno-invalid-offsetof
		-Wno-format-security
		-Wno-deprecated-declarations
		$<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
		$<$<COMPILE_LANGUAGE:CXX>:-Wno-class-memaccess>
		$<$<COMPILE_LANGUAGE:CXX>:-Wno-nonnull-compare>
	)
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-template-body>)
		add_compile_options(-Wno-unknown-warning-option)
	endif()
	# the engine relies on IEEE float behavior in a few places, keep it precise
	add_compile_options(-fno-fast-math)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64|i.86")
		add_compile_options(-msse2)
		if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
			add_compile_options(-mfpmath=sse)
		endif()
	endif()
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		add_definitions(-D_DEBUG)
	endif()
endif()

if(WIN32)
	add_definitions(-DWIN32 -D_WINDOWS -DID_NO_TOOLS -DWIN32_LEAN_AND_MEAN)
	if(MINGW)
		add_compile_options(-Wno-attributes)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
	endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SRC ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# idlib
# ---------------------------------------------------------------------------

set(IDLIB_SOURCES
	idlib/bv/Bounds.cpp
	idlib/bv/Frustum.cpp
	idlib/bv/Sphere.cpp
	idlib/bv/Box.cpp
	idlib/geometry/DrawVert.cpp
	idlib/geometry/Winding2D.cpp
	idlib/geometry/Surface_SweptSpline.cpp
	idlib/geometry/Winding.cpp
	idlib/geometry/Surface.cpp
	idlib/geometry/Surface_Patch.cpp
	idlib/geometry/TraceModel.cpp
	idlib/geometry/JointTransform.cpp
	idlib/hashing/CRC32.cpp
	idlib/hashing/MD4.cpp
	idlib/hashing/MD5.cpp
	idlib/math/Angles.cpp
	idlib/math/Lcp.cpp
	idlib/math/Math.cpp
	idlib/math/Matrix.cpp
	idlib/math/Ode.cpp
	idlib/math/Plane.cpp
	idlib/math/Pluecker.cpp
	idlib/math/Polynomial.cpp
	idlib/math/Quat.cpp
	idlib/math/Rotation.cpp
	idlib/math/Simd.cpp
	idlib/math/Simd_Generic.cpp
	idlib/math/Simd_AltiVec.cpp
	idlib/math/Simd_MMX.cpp
	idlib/math/Simd_3DNow.cpp
	idlib/math/Simd_SSE.cpp
	idlib/math/Simd_SSE2.cpp
	idlib/math/Simd_SSE3.cpp
	idlib/math/Vector.cpp
	idlib/BitMsg.cpp
	idlib/LangDict.cpp
	idlib/Lexer.cpp
	idlib/Lib.cpp
	idlib/containers/HashIndex.cpp
	idlib/Dict.cpp
	idlib/Str.cpp
	idlib/Parser.cpp
	idlib/MapFile.cpp
	idlib/CmdArgs.cpp
	idlib/Token.cpp
	idlib/Base64.cpp
	idlib/Timer.cpp
	idlib/Heap.cpp
)

# idlib is compiled into the engine and each game library separately
# (the game libraries use a GAME_DLL flavored idlib)
add_library(idlib STATIC ${IDLIB_SOURCES})
add_library(idlib_game STATIC ${IDLIB_SOURCES})
target_compile_definitions(idlib_game PRIVATE GAME_DLL)
# the game libraries must use their own idlib, never the engine's copy
set_target_properties(idlib_game PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

# ---------------------------------------------------------------------------
# third party code bundled with the engine
# ---------------------------------------------------------------------------

file(GLOB JPEG_SOURCES ${SRC}/renderer/jpeg-6/j*.c)
list(REMOVE_ITEM JPEG_SOURCES
	${SRC}/renderer/jpeg-6/jmemansi.c
	${SRC}/renderer/jpeg-6/jmemdos.c
	${SRC}/renderer/jpeg-6/jmemname.c
	${SRC}/renderer/jpeg-6/jmemmac.c
	${SRC}/renderer/jpeg-6/jpegtran.c
)
foreach(f jmemansi.c jmemdos.c jmemname.c jmemmac.c jpegtran.c jload.c jpegtran.c)
	list(REMOVE_ITEM JPEG_SOURCES ${SRC}/renderer/jpeg-6/${f})
endforeach()

set(OGG_SOURCES
	sound/OggVorbis/oggsrc/bitwise.c
	sound/OggVorbis/oggsrc/framing.c
	sound/OggVorbis/vorbissrc/analysis.c
	sound/OggVorbis/vorbissrc/bitrate.c
	sound/OggVorbis/vorbissrc/block.c
	sound/OggVorbis/vorbissrc/codebook.c
	sound/OggVorbis/vorbissrc/envelope.c
	sound/OggVorbis/vorbissrc/floor0.c
	sound/OggVorbis/vorbissrc/floor1.c
	sound/OggVorbis/vorbissrc/info.c
	sound/OggVorbis/vorbissrc/lookup.c
	sound/OggVorbis/vorbissrc/lpc.c
	sound/OggVorbis/vorbissrc/lsp.c
	sound/OggVorbis/vorbissrc/mapping0.c
	sound/OggVorbis/vorbissrc/mdct.c
	sound/OggVorbis/vorbissrc/psy.c
	sound/OggVorbis/vorbissrc/registry.c
	sound/OggVorbis/vorbissrc/res0.c
	sound/OggVorbis/vorbissrc/sharedbook.c
	sound/OggVorbis/vorbissrc/smallft.c
	sound/OggVorbis/vorbissrc/synthesis.c
	sound/OggVorbis/vorbissrc/vorbisenc.c
	sound/OggVorbis/vorbissrc/vorbisfile.c
	sound/OggVorbis/vorbissrc/windowvb.c
)

add_library(thirdparty STATIC ${JPEG_SOURCES} ${OGG_SOURCES})
target_include_directories(thirdparty PRIVATE ${SRC}/sound/OggVorbis/vorbissrc ${SRC}/sound/OggVorbis)
if(NOT MSVC)
	target_compile_options(thirdparty PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
# engine
# ---------------------------------------------------------------------------

set(RENDERER_SOURCES
	renderer/Cinematic.cpp
	renderer/GuiModel.cpp
	renderer/Image_files.cpp
	renderer/Image_init.cpp
	renderer/Image_load.cpp
	renderer/Image_process.cpp
	renderer/Image_program.cpp
	renderer/Interaction.cpp
	renderer/Material.cpp
	renderer/MegaTexture.cpp
	renderer/Model.cpp
	renderer/ModelDecal.cpp
	renderer/ModelManager.cpp
	renderer/ModelOverlay.cpp
	renderer/Model_beam.cpp
	renderer/Model_ase.cpp
	renderer/Model_liquid.cpp
	renderer/Model_lwo.cpp
	renderer/Model_ma.cpp
	renderer/Model_md3.cpp
	renderer/Model_md5.cpp
	renderer/Model_prt.cpp
	renderer/Model_sprite.cpp
	renderer/RenderEntity.cpp
	renderer/RenderSystem.cpp
	renderer/RenderSystem_init.cpp
	renderer/RenderWorld.cpp
	renderer/RenderWorld_demo.cpp
	renderer/RenderWorld_load.cpp
	renderer/RenderWorld_portals.cpp
	renderer/VertexCache.cpp
	renderer/cg_explicit.cpp
	renderer/draw_arb.cpp
	renderer/draw_arb2.cpp
	renderer/draw_common.cpp
	renderer/draw_exp_stub.cpp
	renderer/draw_nv10.cpp
	renderer/draw_nv20.cpp
	renderer/draw_r200.cpp
	renderer/tr_backend.cpp
	renderer/tr_deform.cpp
	renderer/tr_font.cpp
	renderer/tr_guisurf.cpp
	renderer/tr_light.cpp
	renderer/tr_lightrun.cpp
	renderer/tr_main.cpp
	renderer/tr_orderIndexes.cpp
	renderer/tr_polytope.cpp
	renderer/tr_render.cpp
	renderer/tr_rendertools.cpp
	renderer/tr_shadowbounds.cpp
	renderer/tr_stencilshadow.cpp
	renderer/tr_subview.cpp
	renderer/tr_trace.cpp
	renderer/tr_trisurf.cpp
	renderer/tr_turboshadow.cpp
)

set(FRAMEWORK_SOURCES
	framework/CVarSystem.cpp
	framework/CmdSystem.cpp
	framework/Common.cpp
	framework/Compressor.cpp
	framework/Console.cpp
	framework/DemoFile.cpp
	framework/DeclAF.cpp
	framework/DeclEntityDef.cpp
	framework/DeclFX.cpp
	framework/DeclManager.cpp
	framework/DeclParticle.cpp
	framework/DeclPDA.cpp
	framework/DeclSkin.cpp
	framework/DeclTable.cpp
	framework/EditField.cpp
	framework/EventLoop.cpp
	framework/File.cpp
	framework/FileSystem.cpp
	framework/KeyInput.cpp
	framework/Unzip.cpp
	framework/UsercmdGen.cpp
	framework/Session_menu.cpp
	framework/Session.cpp
	framework/async/AsyncClient.cpp
	framework/async/AsyncNetwork.cpp
	framework/async/AsyncServer.cpp
	framework/async/MsgChannel.cpp
	framework/async/NetworkSystem.cpp
	framework/async/ServerScan.cpp
)

set(CM_SOURCES
	cm/CollisionModel_contacts.cpp
	cm/CollisionModel_contents.cpp
	cm/CollisionModel_debug.cpp
	cm/CollisionModel_files.cpp
	cm/CollisionModel_load.cpp
	cm/CollisionModel_rotate.cpp
	cm/CollisionModel_trace.cpp
	cm/CollisionModel_translate.cpp
)

set(TOOLS_SOURCES
	tools/compilers/dmap/dmap.cpp
	tools/compilers/dmap/facebsp.cpp
	tools/compilers/dmap/gldraw.cpp
	tools/compilers/dmap/glfile.cpp
	tools/compilers/dmap/leakfile.cpp
	tools/compilers/dmap/map.cpp
	tools/compilers/dmap/optimize.cpp
	tools/compilers/dmap/output.cpp
	tools/compilers/dmap/portals.cpp
	tools/compilers/dmap/shadowopt3.cpp
	tools/compilers/dmap/tritjunction.cpp
	tools/compilers/dmap/tritools.cpp
	tools/compilers/dmap/ubrush.cpp
	tools/compilers/dmap/usurface.cpp
	tools/compilers/aas/AASBuild.cpp
	tools/compilers/aas/AASBuild_file.cpp
	tools/compilers/aas/AASBuild_gravity.cpp
	tools/compilers/aas/AASBuild_ledge.cpp
	tools/compilers/aas/AASBuild_merge.cpp
	tools/compilers/aas/AASCluster.cpp
	tools/compilers/aas/AASFile.cpp
	tools/compilers/aas/AASFile_optimize.cpp
	tools/compilers/aas/AASFile_sample.cpp
	tools/compilers/aas/AASReach.cpp
	tools/compilers/aas/AASFileManager.cpp
	tools/compilers/aas/Brush.cpp
	tools/compilers/aas/BrushBSP.cpp
	tools/compilers/roqvq/NSBitmapImageRep.cpp
	tools/compilers/roqvq/codec.cpp
	tools/compilers/roqvq/roq.cpp
	tools/compilers/roqvq/roqParam.cpp
	tools/compilers/renderbump/renderbump.cpp
	tools/guied/GEWindowWrapper_stub.cpp
	TypeInfo/TypeInfoGen.cpp
)

set(SOUND_SOURCES
	sound/snd_cache.cpp
	sound/snd_decoder.cpp
	sound/snd_efxfile.cpp
	sound/snd_emitter.cpp
	sound/snd_shader.cpp
	sound/snd_system.cpp
	sound/snd_wavefile.cpp
	sound/snd_world.cpp
)

set(UI_SOURCES
	ui/BindWindow.cpp
	ui/ChoiceWindow.cpp
	ui/DeviceContext.cpp
	ui/EditWindow.cpp
	ui/FieldWindow.cpp
	ui/GameBearShootWindow.cpp
	ui/GameBustOutWindow.cpp
	ui/GameSSDWindow.cpp
	ui/GuiScript.cpp
	ui/ListGUI.cpp
	ui/ListWindow.cpp
	ui/MarkerWindow.cpp
	ui/RegExp.cpp
	ui/RenderWindow.cpp
	ui/SimpleWindow.cpp
	ui/SliderWindow.cpp
	ui/UserInterface.cpp
	ui/Window.cpp
	ui/Winvar.cpp
)

set(SYS_SOURCES
	sys/sys_local.cpp
	sys/sdl/sdl_glimp.cpp
	sys/sdl/sdl_events.cpp
	sys/sdl/sdl_sound.cpp
	sys/sdl/sdl_cpu.cpp
)

if(WIN32)
	list(APPEND SYS_SOURCES
		sys/win32/win_main.cpp
		sys/win32/win_net.cpp
		sys/win32/win_shared.cpp
		sys/win32/win_syscon.cpp
		sys/win32/win_stubs.cpp
	)
	set(SYS_LIBS winmm ws2_32 iphlpapi dbghelp)
else()
	list(APPEND SYS_SOURCES
		sys/posix/posix_net.cpp
		sys/posix/posix_main.cpp
		sys/posix/posix_signal.cpp
		sys/posix/posix_threads.cpp
		sys/stub/util_stub.cpp
		openal/stubs.cpp
	)
	if(APPLE)
		message(WARNING "macOS has not been tested with this build, expect to fix a few things")
	endif()
	list(APPEND SYS_SOURCES sys/linux/main.cpp sys/linux/stack.cpp)
	set(SYS_LIBS ${CMAKE_DL_LIBS})
endif()

set(ENGINE_SOURCES
	${RENDERER_SOURCES}
	${FRAMEWORK_SOURCES}
	${CM_SOURCES}
	${TOOLS_SOURCES}
	${SOUND_SOURCES}
	${UI_SOURCES}
	${SYS_SOURCES}
)

add_executable(doom3 WIN32 ${ENGINE_SOURCES})
target_compile_definitions(doom3 PRIVATE __DOOM_DLL__)
target_link_libraries(doom3 idlib thirdparty ${SDL2_TARGETS} OpenGL::GL Threads::Threads ${SYS_LIBS})

# ---------------------------------------------------------------------------
# game libraries
# ---------------------------------------------------------------------------

if(DOOM3_GAME_LIBS)
	set(GAME_COMMON
		AF.cpp
		AFEntity.cpp
		Actor.cpp
		Camera.cpp
		Entity.cpp
		BrittleFracture.cpp
		Fx.cpp
		GameEdit.cpp
		Game_local.cpp
		Game_network.cpp
		Item.cpp
		IK.cpp
		Light.cpp
		Misc.cpp
		Mover.cpp
		Moveable.cpp
		MultiplayerGame.cpp
		Player.cpp
		PlayerIcon.cpp
		PlayerView.cpp
		Projectile.cpp
		Pvs.cpp
		SecurityCamera.cpp
		SmokeParticles.cpp
		Sound.cpp
		Target.cpp
		Trigger.cpp
		Weapon.cpp
		WorldSpawn.cpp
		ai/AAS.cpp
		ai/AAS_debug.cpp
		ai/AAS_pathing.cpp
		ai/AAS_routing.cpp
		ai/AI.cpp
		ai/AI_events.cpp
		ai/AI_pathing.cpp
		ai/AI_Vagary.cpp
		gamesys/DebugGraph.cpp
		gamesys/Class.cpp
		gamesys/Event.cpp
		gamesys/SaveGame.cpp
		gamesys/SysCmds.cpp
		gamesys/SysCvar.cpp
		gamesys/TypeInfo.cpp
		anim/Anim.cpp
		anim/Anim_Blend.cpp
		anim/Anim_Import.cpp
		anim/Anim_Testmodel.cpp
		script/Script_Compiler.cpp
		script/Script_Interpreter.cpp
		script/Script_Program.cpp
		script/Script_Thread.cpp
		physics/Clip.cpp
		physics/Force.cpp
		physics/Force_Constant.cpp
		physics/Force_Drag.cpp
		physics/Force_Field.cpp
		physics/Force_Spring.cpp
		physics/Physics.cpp
		physics/Physics_AF.cpp
		physics/Physics_Actor.cpp
		physics/Physics_Base.cpp
		physics/Physics_Monster.cpp
		physics/Physics_Parametric.cpp
		physics/Physics_Player.cpp
		physics/Physics_RigidBody.cpp
		physics/Physics_Static.cpp
		physics/Physics_StaticMulti.cpp
		physics/Push.cpp
	)

	set(BASE_SOURCES)
	foreach(f ${GAME_COMMON})
		list(APPEND BASE_SOURCES game/${f})
	endforeach()

	set(D3XP_SOURCES)
	foreach(f ${GAME_COMMON} Grabber.cpp physics/Force_Grab.cpp)
		list(APPEND D3XP_SOURCES d3xp/${f})
	endforeach()

	add_library(base SHARED ${BASE_SOURCES})
	target_compile_definitions(base PRIVATE GAME_DLL)
	target_link_libraries(base idlib_game)

	add_library(d3xp SHARED ${D3XP_SOURCES})
	target_compile_definitions(d3xp PRIVATE GAME_DLL _D3XP CTF)
	target_link_libraries(d3xp idlib_game)

	foreach(t base d3xp)
		set_target_properties(${t} PROPERTIES PREFIX "")
		if(NOT WIN32)
			set_target_properties(${t} PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
			if(NOT APPLE)
				target_link_options(${t} PRIVATE -Wl,-Bsymbolic -Wl,--no-undefined)
			endif()
		endif()
		if(MINGW)
			set_target_properties(${t} PROPERTIES LINK_FLAGS "-Wl,--enable-stdcall-fixup")
		endif()
	endforeach()
endif()

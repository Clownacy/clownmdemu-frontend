cmake_minimum_required(VERSION 3.16.3)

option(CLOWNMDEMU_FRONTEND_FREETYPE "Use FreeType font rasteriser instead of stb_truetype for Dear ImGui" OFF)

project(clownmdemu-frontend LANGUAGES C CXX)

###########
# Sources #
###########

add_executable(clownmdemu WIN32
	"source/main.cpp"
	"source/audio-device.cpp"
	"source/audio-device.h"
	"source/audio-output.cpp"
	"source/audio-output.h"
	"source/cd-reader.cpp"
	"source/cd-reader.h"
	"source/colour.h"
	"source/debug-log.cpp"
	"source/debug-log.h"
	"source/emulator-extended.h"
	"source/emulator-instance.cpp"
	"source/emulator-instance.h"
	"source/file-utilities.cpp"
	"source/file-utilities.h"
	"source/frontend.cpp"
	"source/frontend.h"
	"source/raii-wrapper.h"
	"source/sdl-wrapper.h"
	"source/sdl-wrapper-extra.h"
	"source/tar.cpp"
	"source/tar.h"
	"source/text-encoding.cpp"
	"source/text-encoding.h"
	"source/windows/cheats.cpp"
	"source/windows/cheats.h"
	"source/windows/debug-m68k.cpp"
	"source/windows/debug-m68k.h"
	"source/windows/debug-memory.cpp"
	"source/windows/debug-memory.h"
	"source/windows/debug-fm.cpp"
	"source/windows/debug-fm.h"
	"source/windows/debug-frontend.cpp"
	"source/windows/debug-frontend.h"
	"source/windows/debug-log-viewer.cpp"
	"source/windows/debug-log-viewer.h"
	"source/windows/debug-pcm.cpp"
	"source/windows/debug-pcm.h"
	"source/windows/debug-psg.cpp"
	"source/windows/debug-psg.h"
	"source/windows/debug-vdp.cpp"
	"source/windows/debug-vdp.h"
	"source/windows/debug-z80.cpp"
	"source/windows/debug-z80.h"
	"source/windows/disassembler.cpp"
	"source/windows/disassembler.h"
	"source/windows/common/winapi.cpp"
	"source/windows/common/winapi.h"
	"source/windows/common/window.cpp"
	"source/windows/common/window.h"
	"source/windows/common/window-popup.cpp"
	"source/windows/common/window-popup.h"
	"source/windows/common/window-with-dear-imgui.cpp"
	"source/windows/common/window-with-dear-imgui.h"
	"source/windows/common/window-with-framebuffer.cpp"
	"source/windows/common/window-with-framebuffer.h"
	"libraries/imgui/imconfig.h"
	"libraries/imgui/imgui.cpp"
	"libraries/imgui/imgui.h"
	"libraries/imgui/imgui_demo.cpp"
	"libraries/imgui/imgui_draw.cpp"
	"libraries/imgui/imgui_internal.h"
	"libraries/imgui/imgui_tables.cpp"
	"libraries/imgui/imgui_widgets.cpp"
	"libraries/imgui/imstb_rectpack.h"
	"libraries/imgui/imstb_textedit.h"
	"libraries/imgui/backends/imgui_impl_sdl3.cpp"
	"libraries/imgui/backends/imgui_impl_sdl3.h"
	"libraries/imgui/backends/imgui_impl_sdlrenderer3.cpp"
	"libraries/imgui/backends/imgui_impl_sdlrenderer3.h"
	"libraries/imgui/misc/cpp/imgui_stdlib.cpp"
	"libraries/imgui/misc/cpp/imgui_stdlib.h"
)

set_property(DIRECTORY . PROPERTY VS_STARTUP_PROJECT clownmdemu)

if(WIN32)
	target_sources(clownmdemu PRIVATE "assets/clownmdemu.rc")
endif()

##################
# Version string #
##################

# Obtain the Git revision so that it can be displayed in the version string.
find_package(Git)
if(GIT_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
		OUTPUT_VARIABLE GIT_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(GIT_VERSION)
		target_compile_definitions(clownmdemu PRIVATE GIT_VERSION=" \(${GIT_VERSION}\)")
	endif()
endif()

##############################
# Required language versions #
##############################

# Only require C90 and C++20
set_target_properties(clownmdemu PROPERTIES
	C_STANDARD 90
	C_STANDARD_REQUIRED NO
	C_EXTENSIONS OFF
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS OFF
)

####################
# Emscripten hacks #
####################

if(EMSCRIPTEN)
	# Add the file dialog library
	target_sources(clownmdemu PRIVATE "libraries/emscripten-browser-file/emscripten_browser_file.h")

	# Dependencies of the 'emscripten-browser-file' library
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -sEXPORTED_FUNCTIONS=[_main,_malloc,_free] -sEXPORTED_RUNTIME_METHODS=[ccall,HEAPU8] -sALLOW_MEMORY_GROWTH=1")

	# Link the persistent file storage library
	target_link_libraries(clownmdemu "idbfs.js")

	# Hardcoded to specifically this for backwards-compatibility.
	set(SDL_EMSCRIPTEN_PERSISTENT_PATH "/libsdl/clownacy/clownmdemu-frontend/")
	target_compile_definitions(clownmdemu PRIVATE SDL_EMSCRIPTEN_PERSISTENT_PATH="${SDL_EMSCRIPTEN_PERSISTENT_PATH}")
endif()

##################
# Handle options #
##################

# Handle selecting either FreeType or stb_truetype font rendering
if(CLOWNMDEMU_FRONTEND_FREETYPE)
	target_sources(clownmdemu PRIVATE
		"libraries/imgui/misc/freetype/imgui_freetype.cpp"
		"libraries/imgui/misc/freetype/imgui_freetype.h"
	)

	target_compile_definitions(clownmdemu PRIVATE IMGUI_ENABLE_FREETYPE)
else()
	target_sources(clownmdemu PRIVATE
		"libraries/imgui/imstb_truetype.h"
	)
endif()

####################
# Dear ImGui stuff #
####################

# Disable development stuff in non-development builds
target_compile_definitions(clownmdemu PRIVATE $<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<CONFIG:RelWithDebInfo>>>:IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS>)

# Disable some deprecated junk in Dear ImGui
target_compile_definitions(clownmdemu PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_OBSOLETE_KEYIO)

# Allow performing mathematical operations on vectors
target_compile_definitions(clownmdemu PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

# Remove the useless (and ugly) default font
target_compile_definitions(clownmdemu PRIVATE IMGUI_DISABLE_DEFAULT_FONT)

# 'ImTextureID' needs to be a pointer, not an integer
target_compile_definitions(clownmdemu PRIVATE ImTextureID=void*)

# Dear ImGui needs these directories in the include path
target_include_directories(clownmdemu PRIVATE "libraries/imgui" "libraries/imgui/backends")

##################
# Link libraries #
##################

# Link clownmdemu-frontend-common, miniz, and chdr-lzma
set(MINIZ_ARCHIVE_APIS ON CACHE BOOL "" FORCE) # We need miniz's ZIP file support
add_subdirectory("common" EXCLUDE_FROM_ALL)
target_link_libraries(clownmdemu clownmdemu-frontend-common miniz chdr-lzma)

# Link SDL
find_package(SDL3 3.4.2 CONFIG COMPONENTS SDL3)
if(SDL3_FOUND)
	target_link_libraries(clownmdemu SDL3::SDL3)
else()
	set(SDL_STATIC ON)
	set(SDL_SHARED OFF)
	add_subdirectory("libraries/SDL" EXCLUDE_FROM_ALL)
	target_link_libraries(clownmdemu SDL3::SDL3-static)
endif()

if(CLOWNMDEMU_FRONTEND_FREETYPE)
	# Link FreeType
	find_package(Freetype)
	if(FREETYPE_FOUND)
		target_link_libraries(clownmdemu Freetype::Freetype)
	else()
		add_subdirectory("libraries/freetype" EXCLUDE_FROM_ALL)
		target_link_libraries(clownmdemu freetype)
	endif()
endif()

# Link disassemblers
target_link_libraries(clownmdemu clown68000-disassembler)
target_link_libraries(clownmdemu clownz80-disassembler)

# Link fmt
find_package(fmt 8.0.0)
if(NOT fmt_FOUND)
	add_subdirectory("libraries/fmt" EXCLUDE_FROM_ALL)
endif()
target_link_libraries(clownmdemu fmt::fmt)

# Link span
add_subdirectory("libraries/span" EXCLUDE_FROM_ALL)
target_link_libraries(clownmdemu span)

# Link Lyra
add_subdirectory("libraries/Lyra" EXCLUDE_FROM_ALL)
target_link_libraries(clownmdemu lyra)

################
# Installation #
################

install(TARGETS clownmdemu DESTINATION "bin")
install(FILES "assets/com.clownacy.clownmdemu.desktop" DESTINATION "share/applications")
install(FILES "assets/com.clownacy.clownmdemu.appdata.xml" DESTINATION "share/metainfo")
install(FILES "assets/logo.svg" DESTINATION "share/icons/hicolor/scalable/apps" RENAME "clownmdemu.svg")
install(FILES "assets/icon/icon-32.png" DESTINATION "share/icons/hicolor/32x32/apps" RENAME "clownmdemu.png")
install(FILES "assets/icon/icon-24.png" DESTINATION "share/icons/hicolor/24x24/apps" RENAME "clownmdemu.png")
install(FILES "assets/icon/icon-20.png" DESTINATION "share/icons/hicolor/20x20/apps" RENAME "clownmdemu.png")
install(FILES "assets/icon/icon-16.png" DESTINATION "share/icons/hicolor/16x16/apps" RENAME "clownmdemu.png")

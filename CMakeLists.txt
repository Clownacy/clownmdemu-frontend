cmake_minimum_required(VERSION 3.16.3)

option(CLOWNMDEMU_FRONTEND_FREETYPE "Use FreeType font rasteriser instead of stb_truetype for Dear ImGui" OFF)

project(clownmdemu-frontend LANGUAGES C CXX)

###########
# Sources #
###########

add_executable(clownmdemu WIN32
	"main.cpp"
	"audio-device.cpp"
	"audio-device.h"
	"audio-output.cpp"
	"audio-output.h"
	"cd-reader.cpp"
	"cd-reader.h"
	"colour.h"
	"debug-log.cpp"
	"debug-log.h"
	"emulator.cpp"
	"emulator.h"
	"emulator-extended.cpp"
	"emulator-extended.h"
	"emulator-instance.cpp"
	"emulator-instance.h"
	"file-utilities.cpp"
	"file-utilities.h"
	"frontend.cpp"
	"frontend.h"
	"raii-wrapper.h"
	"sdl-wrapper.h"
	"sdl-wrapper-inner.h"
	"state-ring-buffer.h"
	"text-encoding.cpp"
	"text-encoding.h"
	"common/cd-reader.c"
	"common/cd-reader.h"
	"common/mixer.h"
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
	"windows/debug-m68k.cpp"
	"windows/debug-m68k.h"
	"windows/debug-memory.cpp"
	"windows/debug-memory.h"
	"windows/debug-fm.cpp"
	"windows/debug-fm.h"
	"windows/debug-frontend.cpp"
	"windows/debug-frontend.h"
	"windows/debug-log-viewer.cpp"
	"windows/debug-log-viewer.h"
	"windows/debug-pcm.cpp"
	"windows/debug-pcm.h"
	"windows/debug-psg.cpp"
	"windows/debug-psg.h"
	"windows/debug-vdp.cpp"
	"windows/debug-vdp.h"
	"windows/debug-z80.cpp"
	"windows/debug-z80.h"
	"windows/disassembler.cpp"
	"windows/disassembler.h"
	"windows/common/inconsolata-regular.h"
	"windows/common/noto-sans-regular.h"
	"windows/common/winapi.cpp"
	"windows/common/winapi.h"
	"windows/common/window.cpp"
	"windows/common/window.h"
	"windows/common/window-popup.cpp"
	"windows/common/window-popup.h"
	"windows/common/window-with-dear-imgui.cpp"
	"windows/common/window-with-dear-imgui.h"
	"windows/common/window-with-framebuffer.cpp"
	"windows/common/window-with-framebuffer.h"
)

set_property(DIRECTORY . PROPERTY VS_STARTUP_PROJECT clownmdemu)

if(WIN32)
	target_sources(clownmdemu PRIVATE "assets/clownmdemu.rc")
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

# 'ImTextureID' needs to be a pointer, not an integer
target_compile_definitions(clownmdemu PRIVATE ImTextureID=void*)

# Dear ImGui needs these directories in the include path
target_include_directories(clownmdemu PRIVATE "libraries/imgui" "libraries/imgui/backends")

##################
# Link libraries #
##################

# Link clownmdemu-frontend-common
add_subdirectory("common" EXCLUDE_FROM_ALL)
target_link_libraries(clownmdemu clownmdemu-frontend-common)

# Link SDL
find_package(SDL3 3.1.3 CONFIG COMPONENTS SDL3)
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

################
# Installation #
################

install(TARGETS clownmdemu DESTINATION "bin")
install(FILES "assets/com.clownacy.clownmdemu.desktop" DESTINATION "share/applications")
install(FILES "assets/com.clownacy.clownmdemu.appdata.xml" DESTINATION "share/metainfo")
install(FILES "assets/icon/icon-512.png" DESTINATION "share/icons/hicolor/512x512/apps" RENAME "clownmdemu.png")
install(FILES "assets/icon/icon-256.png" DESTINATION "share/icons/hicolor/256x256/apps" RENAME "clownmdemu.png")
install(FILES "assets/icon/icon-128.png" DESTINATION "share/icons/hicolor/128x128/apps" RENAME "clownmdemu.png")
install(FILES "assets/icon/icon-64.png" DESTINATION "share/icons/hicolor/64x64/apps" RENAME "clownmdemu.png")
install(FILES "assets/icon/icon-48.png" DESTINATION "share/icons/hicolor/48x48/apps" RENAME "clownmdemu.png")
install(FILES "assets/icon/icon-40.png" DESTINATION "share/icons/hicolor/40x40/apps" RENAME "clownmdemu.png")
install(FILES "assets/icon/icon-32.png" DESTINATION "share/icons/hicolor/32x32/apps" RENAME "clownmdemu.png")
install(FILES "assets/icon/icon-24.png" DESTINATION "share/icons/hicolor/24x24/apps" RENAME "clownmdemu.png")
install(FILES "assets/icon/icon-20.png" DESTINATION "share/icons/hicolor/20x20/apps" RENAME "clownmdemu.png")
install(FILES "assets/icon/icon-16.png" DESTINATION "share/icons/hicolor/16x16/apps" RENAME "clownmdemu.png")

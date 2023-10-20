cmake_minimum_required(VERSION 3.18)

option(CLOWNMDEMU_FRONTEND_FREETYPE "Use FreeType font rasteriser instead of stb_truetype for Dear ImGui" ON)
option(CLOWNMDEMU_FRONTEND_REWINDING "Enable the ability to rewind the emulator (which uses a LOT of RAM)" ON)

project(clownmdemu-frontend LANGUAGES C CXX)

###########
# Sources #
###########

add_executable(clownmdemu-frontend WIN32
	"main.cpp"
	"error.cpp"
	"error.h"
	"debug_m68k.cpp"
	"debug_m68k.h"
	"debug_memory.cpp"
	"debug_memory.h"
	"debug_fm.cpp"
	"debug_fm.h"
	"debug_log.cpp"
	"debug_log.h"
	"debug_psg.cpp"
	"debug_psg.h"
	"debug_vdp.cpp"
	"debug_vdp.h"
	"debug_z80.cpp"
	"debug_z80.h"
	"doubly-linked-list.c"
	"doubly-linked-list.h"
	"clownmdemu-frontend-common/mixer.h"
	"inconsolata_regular.h"
	"karla_regular.h"
	"libraries/inih/ini.c"
	"libraries/inih/ini.h"
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
	"libraries/imgui/backends/imgui_impl_sdl2.cpp"
	"libraries/imgui/backends/imgui_impl_sdl2.h"
	"libraries/imgui/backends/imgui_impl_sdlrenderer2.cpp"
	"libraries/imgui/backends/imgui_impl_sdlrenderer2.h"
)

set_property(DIRECTORY . PROPERTY VS_STARTUP_PROJECT clownmdemu-frontend)

# Link a manifest file that enables Hi-DPI support on Windows
if(WIN32)
	target_sources(clownmdemu-frontend PRIVATE "clownmdemu-frontend.rc")

	if (MSVC)
		target_sources(clownmdemu-frontend PRIVATE "clownmdemu-frontend.manifest")
	endif()
endif()

##############################
# Required language versions #
##############################

# Only require C90 and C++11
set_target_properties(clownmdemu-frontend PROPERTIES
	C_STANDARD 90
	C_STANDARD_REQUIRED NO
	C_EXTENSIONS OFF
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED NO
	CXX_EXTENSIONS OFF
)

##################
# Handle options #
##################

# Handle selecting either FreeType or stb_truetype font rendering
if(CLOWNMDEMU_FRONTEND_FREETYPE)
	target_sources(clownmdemu-frontend PRIVATE
		"libraries/imgui/misc/freetype/imgui_freetype.cpp"
		"libraries/imgui/misc/freetype/imgui_freetype.h"
	)

	target_compile_definitions(clownmdemu-frontend PRIVATE IMGUI_ENABLE_FREETYPE)
else()
	target_sources(clownmdemu-frontend PRIVATE
		"libraries/imgui/imstb_truetype.h"
	)
endif()

# Handle enabling rewinding
if(CLOWNMDEMU_FRONTEND_REWINDING)
	target_compile_definitions(clownmdemu-frontend PRIVATE CLOWNMDEMU_FRONTEND_REWINDING)
endif()

####################
# Dear ImGui stuff #
####################

# Disable some deprecated junk in Dear ImGui
target_compile_definitions(clownmdemu-frontend PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_OBSOLETE_KEYIO)

# Dear ImGui needs these directories in the include path
target_include_directories(clownmdemu-frontend PRIVATE "libraries/imgui" "libraries/imgui/backends")

##################
# Link libraries #
##################

# Link clownmdemu core
add_subdirectory("clownmdemu-frontend-common/clownmdemu" EXCLUDE_FROM_ALL)
target_link_libraries(clownmdemu-frontend clownmdemu)

# Link SDL2
if(MINGW)
	# SDL2main and libmingw32 have a circular dependency issue
	# that breaks MinGW right now, so fix it by doing this.
	target_link_libraries(clownmdemu-frontend mingw32)
endif()
find_package(SDL2 2.0.18)
if(SDL2_FOUND)
	target_link_libraries(clownmdemu-frontend SDL2::SDL2main SDL2::SDL2)
else()
	add_subdirectory("libraries/SDL" EXCLUDE_FROM_ALL)
	target_link_libraries(clownmdemu-frontend SDL2::SDL2main SDL2::SDL2-static)
endif()

if(CLOWNMDEMU_FRONTEND_FREETYPE)
	# Link FreeType
	find_package(Freetype)
	if(FREETYPE_FOUND)
		target_link_libraries(clownmdemu-frontend Freetype::Freetype)
	else()
		add_subdirectory("libraries/freetype" EXCLUDE_FROM_ALL)
		target_link_libraries(clownmdemu-frontend freetype)
	endif()
endif()

# Link some Win32 stuff for tinyfiledialogs
if(WINDOWS)
	target_link_libraries(clownmdemu-frontend comdlg32 ole32)
endif()

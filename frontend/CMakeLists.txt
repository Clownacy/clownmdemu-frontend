cmake_minimum_required(VERSION 3.18)

option(CLOWNMDEMUFRONTEND_FREETYPE "Use FreeType font rasteriser instead of stb_truetype for Dear ImGui" ON)
option(CLOWNMDEMUFRONTEND_REWINDING "Enable the ability to rewind the emulator (which uses a LOT of RAM)" ON)

project(clownmdemufrontend LANGUAGES C CXX)


#
# Sources
#

add_executable(clownmdemufrontend WIN32
	"main.cpp"
	"error.cpp"
	"error.h"
	"mixer.c"
	"mixer.h"
	"debug_memory.cpp"
	"debug_memory.h"
	"debug_fm.cpp"
	"debug_fm.h"
	"debug_psg.cpp"
	"debug_psg.h"
	"debug_vdp.cpp"
	"debug_vdp.h"
	"inconsolata_regular.h"
	"karla_regular.h"
	"libraries/clownresampler/clownresampler.h"
	"libraries/tinyfiledialogs/tinyfiledialogs.c"
	"libraries/tinyfiledialogs/tinyfiledialogs.h"
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
	"libraries/imgui/backends/imgui_impl_sdl.cpp"
	"libraries/imgui/backends/imgui_impl_sdl.h"
	"libraries/imgui/backends/imgui_impl_sdlrenderer.cpp"
	"libraries/imgui/backends/imgui_impl_sdlrenderer.h"
)

# Link a manifest file that enables Hi-DPI support on Windows
if(WIN32)
	if (MSVC)
		target_sources(clownmdemufrontend PRIVATE "clownmdemufrontend.manifest")
	elseif (MINGW)
		target_sources(clownmdemufrontend PRIVATE "clownmdemufrontend.rc")
	endif()
endif()


#
# Required language versions
#

# Only require C90 and C++98
set_target_properties(clownmdemufrontend PROPERTIES
	C_STANDARD 90
	C_STANDARD_REQUIRED NO
	C_EXTENSIONS OFF
	CXX_STANDARD 98
	CXX_STANDARD_REQUIRED NO
	CXX_EXTENSIONS OFF
)


#
# Handle options
#

# Handle selecting either FreeType or stb_truetype font rendering
if(CLOWNMDEMUFRONTEND_FREETYPE)
	target_sources(clownmdemufrontend PRIVATE
		"libraries/imgui/misc/freetype/imgui_freetype.cpp"
		"libraries/imgui/misc/freetype/imgui_freetype.h"
	)

	target_compile_definitions(clownmdemufrontend PRIVATE IMGUI_ENABLE_FREETYPE)
else()
	target_sources(clownmdemufrontend PRIVATE
		"libraries/imgui/imstb_truetype.h"
	)
endif()

# Handle enabling rewinding
if(CLOWNMDEMUFRONTEND_REWINDING)
	target_compile_definitions(clownmdemufrontend PRIVATE CLOWNMDEMUFRONTEND_REWINDING)
endif()


#
# Dear ImGui stuff
#

# Dear ImGui requires some C++11 features
target_compile_features(clownmdemufrontend PRIVATE cxx_long_long_type cxx_static_assert)

# Disable some deprecated junk in Dear ImGui
target_compile_definitions(clownmdemufrontend PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_OBSOLETE_KEYIO)

# Dear ImGui needs these directories in the include path
target_include_directories(clownmdemufrontend PRIVATE "libraries/imgui" "libraries/imgui/backends")


#
# Link libraries
#

# Link clownmdemu core
add_subdirectory("../" EXCLUDE_FROM_ALL "clownmdemu")
target_link_libraries(clownmdemufrontend clownmdemu)

# Link SDL2
if(MINGW)
	# SDL2main and libmingw32 have a circular dependency issue
	# that breaks MinGW right now, so fix it by doing this.
	target_link_libraries(clownmdemufrontend mingw32)
endif()
find_package(SDL2 2.0.18)
if(SDL2_FOUND)
	target_link_libraries(clownmdemufrontend SDL2::SDL2main SDL2::SDL2)
else()
	add_subdirectory("libraries/SDL" EXCLUDE_FROM_ALL)
	target_link_libraries(clownmdemufrontend SDL2::SDL2main SDL2::SDL2-static)
endif()

if(CLOWNMDEMUFRONTEND_FREETYPE)
	# Link FreeType
	find_package(Freetype)
	if(FREETYPE_FOUND)
		target_link_libraries(clownmdemufrontend Freetype::Freetype)
	else()
		add_subdirectory("libraries/freetype" EXCLUDE_FROM_ALL)
		target_link_libraries(clownmdemufrontend freetype)
	endif()
endif()

# Link some Win32 stuff for tinyfiledialogs
if(WINDOWS)
	target_link_libraries(clownmdemufrontend comdlg32 ole32)
endif()

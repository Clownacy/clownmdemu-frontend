cmake_minimum_required(VERSION 3.20)

option(CLOWNMDEMUFRONTEND_FREETYPE "Use FreeType font rasteriser instead of stb_truetype for Dear ImGui" ON)
option(CLOWNMDEMUFRONTEND_REWINDING "Enable the ability to rewind the emulator (which uses a LOT of RAM)" ON)

project(clownmdemufrontend LANGUAGES C CXX)

add_executable(clownmdemufrontend WIN32
	"main.cpp"
	"audio.cpp"
	"audio.h"
	"colour.cpp"
	"colour.h"
	"error.cpp"
	"error.h"
	"video.cpp"
	"video.h"
	"debug_psg.cpp"
	"debug_psg.h"
	"debug_vdp.cpp"
	"debug_vdp.h"
	"libraries/clownresampler/clownresampler.h"
	"libraries/tinyfiledialogs/tinyfiledialogs.c"
	"libraries/tinyfiledialogs/tinyfiledialogs.h"
	"libraries/imgui/imconfig.h"
	"libraries/imgui/imgui.cpp"
	"libraries/imgui/imgui.h"
	"libraries/imgui/imgui_demo.cpp"
	"libraries/imgui/imgui_draw.cpp"
	"libraries/imgui/imgui_impl_sdl.cpp"
	"libraries/imgui/imgui_impl_sdl.h"
	"libraries/imgui/imgui_impl_sdlrenderer.cpp"
	"libraries/imgui/imgui_impl_sdlrenderer.h"
	"libraries/imgui/imgui_internal.h"
	"libraries/imgui/imgui_tables.cpp"
	"libraries/imgui/imgui_widgets.cpp"
	"libraries/imgui/imstb_rectpack.h"
	"libraries/imgui/imstb_textedit.h"
	"frontend.manifest"
)

if(CLOWNMDEMUFRONTEND_FREETYPE)
	target_sources(clownmdemufrontend PRIVATE
		"libraries/imgui/imgui_freetype.cpp"
		"libraries/imgui/imgui_freetype.h"
	)

	target_compile_definitions(clownmdemufrontend PRIVATE IMGUI_ENABLE_FREETYPE)
else()
	target_sources(clownmdemufrontend PRIVATE
		"libraries/imgui/imstb_truetype.h"
	)
endif()

if(CLOWNMDEMUFRONTEND_REWINDING)
	target_compile_definitions(clownmdemufrontend PRIVATE CLOWNMDEMUFRONTEND_REWINDING)
endif()

set_target_properties(clownmdemufrontend PROPERTIES
	C_STANDARD 99
	C_STANDARD_REQUIRED NO
	C_EXTENSIONS OFF
	CXX_STANDARD 98
	CXX_STANDARD_REQUIRED NO
	CXX_EXTENSIONS OFF
)

# Link clownmdemu core
add_subdirectory("../" EXCLUDE_FROM_ALL "clownmdemu")
target_link_libraries(clownmdemufrontend clownmdemu)

# Link SDL2
find_package(SDL2)
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

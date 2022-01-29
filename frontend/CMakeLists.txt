cmake_minimum_required(VERSION 3.20)

option(CLOWNMDEMUFRONTEND_FREETYPE "Use FreeType font rasteriser instead of stb_truetype for Dear ImGui" ON)
option(CLOWNMDEMUFRONTEND_REWINDING "Enable the ability to rewind the emulator (which uses a LOT of RAM)" ON)

project(clownmdemufrontend LANGUAGES C CXX)

add_executable(clownmdemufrontend WIN32
	"main.cpp"
	"error.cpp"
	"error.h"
	"clownresampler/clownresampler.h"
	"tinyfiledialogs.c"
	"tinyfiledialogs.h"
	"imgui/imconfig.h"
	"imgui/imgui.cpp"
	"imgui/imgui.h"
	"imgui/imgui_demo.cpp"
	"imgui/imgui_draw.cpp"
	"imgui/imgui_impl_sdl.cpp"
	"imgui/imgui_impl_sdl.h"
	"imgui/imgui_impl_sdlrenderer.cpp"
	"imgui/imgui_impl_sdlrenderer.h"
	"imgui/imgui_internal.h"
	"imgui/imgui_tables.cpp"
	"imgui/imgui_widgets.cpp"
	"imgui/imstb_rectpack.h"
	"imgui/imstb_textedit.h"
	"frontend.manifest"
)

if(CLOWNMDEMUFRONTEND_FREETYPE)
	target_sources(clownmdemufrontend PRIVATE
		"imgui/imgui_freetype.cpp"
		"imgui/imgui_freetype.h"
	)

	target_compile_definitions(clownmdemufrontend PRIVATE IMGUI_ENABLE_FREETYPE)
else()
	target_sources(clownmdemufrontend PRIVATE
		"imgui/imstb_truetype.h"
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
	add_subdirectory("SDL" EXCLUDE_FROM_ALL)
	target_link_libraries(clownmdemufrontend SDL2::SDL2main SDL2::SDL2-static)
endif()

if(CLOWNMDEMUFRONTEND_FREETYPE)
	# Link FreeType
	find_package(Freetype)
	if(FREETYPE_FOUND)
		target_link_libraries(clownmdemufrontend Freetype::Freetype)
	else()
		add_subdirectory("freetype" EXCLUDE_FROM_ALL)
		target_link_libraries(clownmdemufrontend freetype)
	endif()
endif()

# Link some Win32 stuff for tinyfiledialogs
if(WINDOWS)
	target_link_libraries(clownmdemufrontend comdlg32 ole32)
endif()

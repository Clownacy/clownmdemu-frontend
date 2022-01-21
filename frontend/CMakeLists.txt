cmake_minimum_required(VERSION 3.20)

project(clownmdemufrontend LANGUAGES C CXX)

add_executable(clownmdemufrontend WIN32
	"main.cpp"
	"clownresampler.h"
	"tinyfiledialogs.c"
	"tinyfiledialogs.h"
)

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

# Link some Win32 stuff for tinyfiledialogs
if(WINDOWS)
	target_link_libraries(clownmdemufrontend comdlg32 ole32)
endif()

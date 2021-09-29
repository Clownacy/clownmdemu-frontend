cmake_minimum_required(VERSION 3.21.2)

project(clownmdemufrontend LANGUAGES C)

add_executable(clownmdemufrontend
	"main.c"
)

set_target_properties(clownmdemufrontend PROPERTIES
	C_STANDARD 99
	C_STANDARD_REQUIRED NO
	C_EXTENSIONS OFF
	CXX_STANDARD 98
	CXX_STANDARD_REQUIRED NO
	CXX_EXTENSIONS OFF
)

add_subdirectory("../" EXCLUDE_FROM_ALL "clownmdemu")
target_link_libraries(clownmdemufrontend clownmdemu)

find_package(SDL2 REQUIRED)
target_link_libraries(clownmdemufrontend SDL2::SDL2main SDL2::SDL2)

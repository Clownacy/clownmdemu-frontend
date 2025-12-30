cmake_minimum_required(VERSION 3.19)
project(clownmdemu-frontend-qt LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core OpenGL OpenGLWidgets Widgets)

qt_standard_project_setup()

qt_add_executable(clownmdemu-frontend-qt
	WIN32 MACOSX_BUNDLE
	dialogs/about.cpp dialogs/about.h dialogs/about.ui
	dialogs/common.h
	dialogs/debug-cpu.cpp dialogs/debug-cpu.h dialogs/debug-cpu.ui
	dialogs/debug-toggles.cpp dialogs/debug-toggles.h dialogs/debug-toggles.ui
	dialogs/options.cpp dialogs/options.h dialogs/options.ui
	widgets/debug-cpu-common.h
	widgets/debug-cpu-m68k.cpp widgets/debug-cpu-m68k.h
	widgets/debug-cpu-z80.cpp widgets/debug-cpu-z80.h
	widgets/emulator.cpp widgets/emulator.h
	allocated-dialog.h
	colour.h
	main.cpp
	main-window.cpp main-window.h main-window.ui
	options.cpp options.h
	paste.h
	qt-extensions.h
	../audio-device.cpp ../audio-device.h
	../audio-output.cpp ../audio-output.h
	../cd-reader.cpp ../cd-reader.h
	../debug-log.cpp ../debug-log.h
	../emulator.h
	../emulator-extended.h
	../raii-wrapper.h ../sdl-wrapper.h
)

set_target_properties(clownmdemu-frontend-qt PROPERTIES
	CXX_STANDARD 20
)

target_link_libraries(clownmdemu-frontend-qt
	PRIVATE
		Qt::Core
		Qt::OpenGL
		Qt::OpenGLWidgets
		Qt::Widgets
)

# Add the Windows icon.
if(WIN32)
	target_sources(clownmdemu-frontend-qt PRIVATE "../assets/clownmdemu.rc")
endif()

add_subdirectory("../common" EXCLUDE_FROM_ALL "common")
target_link_libraries(clownmdemu-frontend-qt PRIVATE clownmdemu-frontend-common)

# Link SDL
find_package(SDL3 3.1.3 CONFIG COMPONENTS SDL3)
if(SDL3_FOUND)
	target_link_libraries(clownmdemu-frontend-qt PRIVATE SDL3::SDL3)
else()
	set(SDL_STATIC ON)
	set(SDL_SHARED OFF)
	add_subdirectory("../libraries/SDL" EXCLUDE_FROM_ALL "SDL")
	target_link_libraries(clownmdemu-frontend-qt PRIVATE SDL3::SDL3-static)
endif()

qt_add_resources(clownmdemu-frontend-qt "shaders"
	PREFIX
		"/"
	FILES
		"vertex.glsl"
		"fragment.glsl"
)

qt_add_resources(clownmdemu-frontend-qt "icons"
	PREFIX
		"/"
	BASE
		"../assets/icon/"
	FILES
		"../assets/icon/icon-qt.png"
		"../assets/icon/icon.ico"
)

# Deployment

include(GNUInstallDirs)

install(TARGETS clownmdemu-frontend-qt
	BUNDLE  DESTINATION .
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
	TARGET clownmdemu-frontend-qt
	OUTPUT_SCRIPT deploy_script
	NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

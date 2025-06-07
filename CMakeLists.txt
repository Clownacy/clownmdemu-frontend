cmake_minimum_required(VERSION 3.19)
project(clownmdemu-frontend-qt LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core OpenGL OpenGLWidgets Widgets)

qt_standard_project_setup()

qt_add_executable(clownmdemu-frontend-qt
	WIN32 MACOSX_BUNDLE
	main.cpp
	mainwindow.cpp
	mainwindow.h
	mainwindow.ui
	emulator.h emulator.cpp
	debug-cpu.h debug-cpu.cpp debug-cpu.ui
	emulator-widget.cpp
	emulator-widget.h
)

set_target_properties(clownmdemu-frontend-qt PROPERTIES
	CXX_STANDARD 23
)

target_link_libraries(clownmdemu-frontend-qt
	PRIVATE
		Qt::Core
		Qt::OpenGL
		Qt::OpenGLWidgets
		Qt::Widgets
)

add_subdirectory("common" EXCLUDE_FROM_ALL)
target_link_libraries(clownmdemu-frontend-qt PRIVATE clownmdemu-frontend-common)

qt_add_resources(clownmdemu-frontend-qt "shaders"
	PREFIX
		"/"
	FILES
		"vertex.glsl"
		"fragment.glsl"
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

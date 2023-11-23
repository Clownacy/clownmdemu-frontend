#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include <cstddef>

#include "SDL.h"

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#include "debug-log.h"
#include "emulator-instance.h"
#include "file-picker.h"
#include "window.h"

class DebugVDP
{
private:
	DebugLog &debug_log;
	const float &dpi_scale;
	const EmulatorInstance &emulator;
	FilePicker &file_picker;
	const unsigned int &frame_counter;
	ImFont* const &monospace_font;
	Window &window;

	struct
	{
		SDL_Texture *texture;
		std::size_t texture_width;
		std::size_t texture_height;
		int brightness_index;
		int palette_line;
		unsigned int cache_frame_counter;
	} vram_viewer;

	struct
	{
		int brightness = 1;
	} cram_viewer;

	struct PlaneViewer
	{
		int scale;
		SDL_Texture *texture;
		unsigned int cache_frame_counter;
	} window_plane_data, plane_a_data, plane_b_data;

	void Plane(bool &open, const char* const name, PlaneViewer &plane_viewer, const cc_u16l plane_address);

public:
	DebugVDP(
		DebugLog &debug_log,
		const float &dpi_scale,
		const EmulatorInstance &emulator,
		FilePicker &file_picker,
		const unsigned int &frame_counter,
		ImFont* const &monospace_font,
		Window &window
	) :
		debug_log(debug_log),
		dpi_scale(dpi_scale),
		emulator(emulator),
		file_picker(file_picker),
		frame_counter(frame_counter),
		monospace_font(monospace_font),
		window(window)
	{}
	void WindowPlane(bool &open);
	void PlaneA(bool &open);
	void PlaneB(bool &open);
	void VRAM(bool &open);
	void CRAM(bool &open);
	void Registers(bool &open);
};

#endif /* DEBUG_VDP_H */

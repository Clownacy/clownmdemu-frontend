#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include <cstddef>

#include "sdl-wrapper.h"

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#include "debug-log.h"
#include "emulator-instance.h"
#include "file-utilities.h"
#include "window.h"

class DebugVDP
{
private:
	DebugLog &debug_log;
	const float &dpi_scale;
	const EmulatorInstance &emulator;
	FileUtilities &file_utilities;
	const unsigned int &frame_counter;
	ImFont* const &monospace_font;
	Window &window;

	static constexpr cc_u8f TOTAL_SPRITES = 80;

	struct
	{
		SDL::Texture texture = nullptr;
		std::size_t texture_width = 0;
		std::size_t texture_height = 0;
		int brightness_index = 0;
		int palette_line = 0;
		unsigned int cache_frame_counter = 0;
	} vram_viewer;

	struct
	{
		int brightness = 1;
	} cram_viewer;

	struct
	{
		int scale = 0;
		SDL::Texture texture = nullptr;
		SDL::Texture textures[TOTAL_SPRITES] = {nullptr};
		unsigned int cache_frame_counter = 0;
	} sprite_viewer;

	struct PlaneViewer
	{
		int scale = 0;
		SDL::Texture texture = nullptr;
		unsigned int cache_frame_counter = 0;
	} window_plane_data, plane_a_data, plane_b_data;

	void DrawTile(const EmulatorInstance::State &state, VDP_TileMetadata tile_metadata, Uint8 *pixels, int pitch, cc_u16f x, cc_u16f y, bool transparency) const;
	void DisplayPlane(bool &open, const char *name, PlaneViewer &plane_viewer, cc_u16l plane_address, cc_u16l plane_width, cc_u16l plane_height);
	void DisplaySpriteCommon();

public:
	DebugVDP(
		DebugLog &debug_log,
		const float &dpi_scale,
		const EmulatorInstance &emulator,
		FileUtilities &file_utilities,
		const unsigned int &frame_counter,
		ImFont* const &monospace_font,
		Window &window
	) :
		debug_log(debug_log),
		dpi_scale(dpi_scale),
		emulator(emulator),
		file_utilities(file_utilities),
		frame_counter(frame_counter),
		monospace_font(monospace_font),
		window(window)
	{}
	void DisplayWindowPlane(bool &open);
	void DisplayPlaneA(bool &open);
	void DisplayPlaneB(bool &open);
	void DisplaySpritePlane(bool &open);
	void DisplaySpriteList(bool &open);
	void DisplayVRAM(bool &open);
	void DisplayCRAM(bool &open);
	void DisplayRegisters(bool &open);
};

#endif /* DEBUG_VDP_H */

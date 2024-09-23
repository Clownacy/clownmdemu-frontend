#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include <cstddef>

#include "sdl-wrapper.h"

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#include "debug-log.h"
#include "emulator-instance.h"
#include "file-utilities.h"
#include "window-popup.h"

namespace DebugVDP
{
	constexpr cc_u8f TOTAL_SPRITES = 80;

	struct SpriteCommon : public WindowPopup
	{
	private:
		unsigned int cache_frame_counter = 0;

	protected:
		// TODO: Any way to share this between the two sprite viewers again when using Dear ImGui windows?
		SDL::Texture textures[TOTAL_SPRITES] = {nullptr};
		void DisplaySpriteCommon();

	public:
		using WindowPopup::WindowPopup;
	};

	class SpriteViewer: public SpriteCommon
	{
	private:
		int scale = 0;
		SDL::Texture texture = nullptr;

	public:
		using SpriteCommon::SpriteCommon;

		bool Display();
	};

	class SpriteList : public SpriteCommon
	{
	public:
		using SpriteCommon::SpriteCommon;

		bool Display();
	};

	class PlaneViewer : public WindowPopup
	{
	private:
		int scale = 0;
		SDL::Texture texture = nullptr;
		unsigned int cache_frame_counter = 0;

	public:
		using WindowPopup::WindowPopup;

		bool Display(cc_u16l plane_address, cc_u16l plane_width, cc_u16l plane_height);
	};

	class VRAMViewer : public WindowPopup
	{
	private:
		SDL::Texture texture = nullptr;
		std::size_t texture_width = 0;
		std::size_t texture_height = 0;
		int brightness_index = 0;
		int palette_line = 0;
		unsigned int cache_frame_counter = 0;

	public:
		using WindowPopup::WindowPopup;

		bool Display();
	};

	class CRAMViewer : public WindowPopup
	{
	private:
		int brightness = 1;

	public:
		using WindowPopup::WindowPopup;

		bool Display();
	};

	class Registers : public WindowPopup
	{
	public:
		using WindowPopup::WindowPopup;

		bool Display();
	};
}

#endif /* DEBUG_VDP_H */

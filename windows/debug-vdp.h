#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include <cstddef>

#include "../sdl-wrapper.h"

#include "../common/core/clowncommon/clowncommon.h"

#include "common/window-popup.h"

namespace DebugVDP
{
	constexpr cc_u8f TOTAL_SPRITES = 80;

	struct SpriteCommon
	{
	private:
		unsigned int cache_frame_counter = 0;

	protected:
		// TODO: Any way to share this between the two sprite viewers again when using Dear ImGui windows?
		SDL::Texture textures[TOTAL_SPRITES] = {nullptr};

		void DisplaySpriteCommon(Window &window);
	};

	class SpriteViewer: public SpriteCommon, public WindowPopup<SpriteViewer>
	{
	private:
		using Base = WindowPopup<SpriteViewer>;

		static constexpr Uint32 window_flags = 0;

		int scale = 0;
		SDL::Texture texture = nullptr;

		void DisplayInternal();

	public:
		using Base::WindowPopup;

		friend Base;
	};

	class SpriteList : public SpriteCommon, public WindowPopup<SpriteList>
	{
	private:
		using Base = WindowPopup<SpriteList>;

		static constexpr Uint32 window_flags = 0;

		void DisplayInternal();

	public:
		using Base::WindowPopup;

		friend Base;
	};

	class PlaneViewer : public WindowPopup<PlaneViewer>
	{
	private:
		using Base = WindowPopup<PlaneViewer>;

		static constexpr Uint32 window_flags = 0;

		int scale = 0;
		SDL::Texture texture = nullptr;
		unsigned int cache_frame_counter = 0;

		void DisplayInternal(cc_u16l plane_address, cc_u16l plane_width, cc_u16l plane_height, cc_u16l plane_pitch);

	public:
		using Base::WindowPopup;

		friend Base;
	};

	class VRAMViewer : public WindowPopup<VRAMViewer>
	{
	private:
		using Base = WindowPopup<VRAMViewer>;

		static constexpr Uint32 window_flags = 0;

		SDL::Texture texture = nullptr;
		std::size_t texture_width = 0;
		std::size_t texture_height = 0;
		int brightness_index = 0;
		int palette_line = 0;
		unsigned int cache_frame_counter = 0;

		void DisplayInternal();

	public:
		using Base::WindowPopup;

		friend Base;
	};

	class CRAMViewer : public WindowPopup<CRAMViewer>
	{
	private:
		using Base = WindowPopup<CRAMViewer>;

		static constexpr Uint32 window_flags = 0;

		int brightness = 1;

		void DisplayInternal();

	public:
		using Base::WindowPopup;

		friend Base;
	};

	class Registers : public WindowPopup<Registers>
	{
	private:
		using Base = WindowPopup<Registers>;

		static constexpr Uint32 window_flags = 0;

		void DisplayInternal();

	public:
		using Base::WindowPopup;

		friend Base;
	};
}

#endif /* DEBUG_VDP_H */

#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include <cstddef>
#include <functional>

#include "../sdl-wrapper.h"

#include "../common/core/clowncommon/clowncommon.h"

#include "common/window-popup.h"

namespace DebugVDP
{
	constexpr cc_u8f TOTAL_SPRITES = 80;

	using ReadTileWord = std::function<cc_u16f(cc_u16f word_index)>;
	using DrawMapPiece = std::function<void(Uint32 *pixels, int pitch, cc_u16f x, cc_u16f y)>;
	using MapPieceTooltip = std::function<void(cc_u16f x, cc_u16f y)>;

	struct RegeneratingTextures
	{
	private:
		unsigned int cache_frame_counter = 0;

	protected:
		// TODO: Any way to share this between the two sprite viewers again when using Dear ImGui windows?
		std::vector<SDL::Texture> textures;

		void RegenerateTexturesIfNeeded(const std::function<void(unsigned int texture_index, Uint32 *pixels, int pitch)> &callback);
	};

	struct SpriteCommon : protected RegeneratingTextures
	{
	protected:
		void DisplaySpriteCommon(Window &window);
	};

	class SpriteViewer: public WindowPopup<SpriteViewer>, public SpriteCommon
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

	class SpriteList : public WindowPopup<SpriteList>, public SpriteCommon
	{
	private:
		using Base = WindowPopup<SpriteList>;

		static constexpr Uint32 window_flags = 0;

		void DisplayInternal();

	public:
		using Base::WindowPopup;

		friend Base;
	};

	template<typename Derived>
	class MapViewer : public WindowPopup<Derived>, protected RegeneratingTextures
	{
	private:
		static constexpr Uint32 window_flags = 0;

		int scale = 0;

	protected:
		void DisplayMap(
			cc_u16f plane_width,
			cc_u16f plane_height,
			cc_u16f maximum_plane_width_in_pixels,
			cc_u16f maximum_plane_height_in_pixels,
			cc_u8f tile_width,
			cc_u8f tile_height,
			cc_u16f maximum_tile_index,
			const DrawMapPiece &draw_piece,
			const MapPieceTooltip &tooltip);

	public:
		using Base = WindowPopup<Derived>;
		using Base::WindowPopup;

		friend Base;
	};

	class PlaneViewer : public MapViewer<PlaneViewer>
	{
	private:
		using Base = MapViewer<PlaneViewer>;

		void DisplayInternal(cc_u16l plane_address, cc_u16l plane_width, cc_u16l plane_height);

	public:
		using Base::MapViewer;

		friend Base;
		friend Base::Base;
	};

	class StampMapViewer : public MapViewer<StampMapViewer>
	{
	private:
		using Base = MapViewer<StampMapViewer>;

		void DisplayInternal();

	public:
		using Base::MapViewer;

		friend Base;
		friend Base::Base;
	};

	template<typename Derived>
	class GridViewer : public WindowPopup<Derived>, protected RegeneratingTextures
	{
	private:
		static constexpr Uint32 window_flags = 0;

		std::size_t texture_width = 0;
		std::size_t texture_height = 0;
		int brightness_index = 0;
		int palette_line = 0;

	protected:
		void DisplayGrid(
			cc_u16f entry_width,
			cc_u16f entry_height,
			std::size_t total_entries,
			cc_u16f maximum_entry_width,
			cc_u16f maximum_entry_height,
			std::size_t entry_buffer_size_in_pixels,
			const std::function<void(cc_u16f entry_index, cc_u8f brightness, cc_u8f palette_line, Uint32 *pixels, int pitch)> &render_entry_callback,
			const char *label_singular,
			const char *label_plural);

	public:
		using Base = WindowPopup<Derived>;
		using Base::WindowPopup;

		friend Base;
	};

	class VRAMViewer : public GridViewer<VRAMViewer>
	{
	private:
		using Base = GridViewer<VRAMViewer>;

		void DisplayInternal();

	public:
		using Base::GridViewer;

		friend Base;
		friend Base::Base;
	};

	class StampViewer : public GridViewer<StampViewer>
	{
	private:
		using Base = GridViewer<StampViewer>;

		void DisplayInternal();

	public:
		using Base::GridViewer;

		friend Base;
		friend Base::Base;
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

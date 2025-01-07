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
	using RenderPiece = std::function<void(cc_u16f piece_index, cc_u8f brightness, cc_u8f palette_line, Uint32 *pixels, int pitch)>;

	struct BrightnessAndPaletteLineSettings
	{
		int brightness_index = 0, palette_line_index = 0;

		bool DisplayBrightnessAndPaletteLineSettings();
	};

	struct RegeneratingTexturesBase
	{
	private:
		unsigned int cache_frame_counter = 0;

	public:
		// TODO: Any way to share this between the two sprite viewers again when using Dear ImGui windows?
		std::vector<SDL::Texture> textures;

		void RegenerateTexturesIfNeeded(const std::function<void(unsigned int texture_index, SDL::Texture &texture)> &callback, bool force_regenerate = false);
	};

	struct RegeneratingTextures : private RegeneratingTexturesBase
	{
		using RegeneratingTexturesBase::textures;

		void RegenerateTexturesIfNeeded(const std::function<void(unsigned int texture_index, Uint32 *pixels, int pitch)> &callback, bool force_regenerate = false);
	};

	struct RegeneratingPieces : public RegeneratingTextures
	{
	protected:
		std::size_t texture_width = 0;
		std::size_t texture_height = 0;

	public:
		void RegenerateIfNeeded(
			SDL::Renderer &renderer,
			cc_u8f piece_width,
			cc_u8f piece_height,
			cc_u8f maximum_piece_width,
			cc_u8f maximum_piece_height,
			std::size_t piece_buffer_size_in_pixels,
			cc_u8f brightness_index,
			cc_u8f palette_line_index,
			const RenderPiece &render_piece_callback,
			bool force_regenerate = false);

		auto GetTextureWidth() const
		{
			return texture_width;
		}

		auto GetTextureHeight() const
		{
			return texture_height;
		}
	};

	struct RegeneratingTiles : public RegeneratingPieces
	{
		using RegeneratingPieces::RegeneratingPieces;

		void RegenerateIfNeeded(SDL::Renderer &renderer, cc_u8f brightness_index, cc_u8f palette_line_index, bool force_regenerate = false);
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
			cc_u16f map_width_in_pieces,
			cc_u16f map_height_in_pieces,
			cc_u16f maximum_map_width_in_pixels,
			cc_u16f maximum_map_height_in_pixels,
			cc_u8f piece_width,
			cc_u8f piece_height,
			const DrawMapPiece &draw_piece,
			const MapPieceTooltip &piece_tooltip,
			bool force_regenerate = false);

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

	class StampMapViewer : public MapViewer<StampMapViewer>, protected BrightnessAndPaletteLineSettings
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
	class GridViewer : public WindowPopup<Derived>, protected BrightnessAndPaletteLineSettings
	{
	private:
		static constexpr Uint32 window_flags = 0;

	protected:
		RegeneratingPieces regenerating_pieces;

		void DisplayGrid(
			cc_u16f piece_width,
			cc_u16f piece_height,
			std::size_t total_pieces,
			cc_u16f maximum_piece_width,
			cc_u16f maximum_piece_height,
			std::size_t piece_buffer_size_in_pixels,
			const RenderPiece &render_piece_callback,
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

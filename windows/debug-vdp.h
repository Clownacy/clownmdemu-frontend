#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include <cstddef>
#include <functional>

#include "../sdl-wrapper.h"

#include "../common/core/clowncommon/clowncommon.h"
#include "../common/core/vdp.h"

#include "common/window-popup.h"

namespace DebugVDP
{
	constexpr cc_u8f TOTAL_SPRITES = 80;

	using ReadTileWord = std::function<cc_u16f(cc_u16f word_index)>;
	using DrawMapPiece = std::function<void(SDL::Renderer &renderer, cc_u16f x, cc_u16f y)>;
	using MapPieceTooltip = std::function<void(cc_u16f x, cc_u16f y)>;
	using RenderPiece = std::function<void(cc_u16f piece_index, cc_u8f brightness, cc_u8f palette_line, SDL::Pixel *pixels, int pitch)>;

	struct BrightnessAndPaletteLineSettings
	{
		int brightness_option_index = 0, palette_line_option_index = 0;

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

		void RegenerateTexturesIfNeeded(const std::function<void(unsigned int texture_index, SDL::Pixel *pixels, int pitch)> &callback, bool force_regenerate = false);
	};

	struct RegeneratingTexturesHardwareAccelerated : private RegeneratingTexturesBase
	{
		using RegeneratingTexturesBase::textures;

		void RegenerateTexturesIfNeeded(SDL::Renderer &renderer, const std::function<void(SDL::Renderer &renderer, unsigned int texture_index)> &callback, bool force_regenerate = false);
	};


	struct RegeneratingPieces : public RegeneratingTextures
	{
	protected:
		std::size_t texture_width = 0;
		std::size_t texture_height = 0;

	public:
		void RegenerateIfNeeded(
			SDL::Renderer &renderer,
			std::size_t piece_width,
			std::size_t piece_height,
			std::size_t maximum_piece_width,
			std::size_t maximum_piece_height,
			std::size_t piece_buffer_size_in_pixels,
			bool multiple_palette_lines,
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

		SDL_FRect GetPieceRect(const std::size_t piece_index, std::size_t piece_width, std::size_t piece_height, std::size_t piece_buffer_size_in_pixels, cc_u8f palette_line_option_index) const;

		void Draw(SDL::Renderer &renderer, VDP_TileMetadata piece_metadata, std::size_t piece_width, std::size_t piece_height, std::size_t piece_buffer_size_in_pixels, cc_u16f x, cc_u16f y, cc_u8f brightness_option_index, bool transparency, bool swap_coordinates = false);
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
	class MapViewer : public WindowPopup<Derived>, protected RegeneratingTexturesHardwareAccelerated
	{
	private:
		static constexpr Uint32 window_flags = 0;

		int scale = 0;

	protected:
		void DisplayMap(
			std::size_t map_width_in_pieces,
			std::size_t map_height_in_pieces,
			std::size_t maximum_map_width_in_pixels,
			std::size_t maximum_map_height_in_pixels,
			std::size_t piece_width,
			std::size_t piece_height,
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

		RegeneratingPieces regenerating_pieces;

		void DisplayInternal(cc_u16l plane_address, std::size_t plane_width, std::size_t plane_height);

	public:
		using Base::MapViewer;

		friend Base;
		friend Base::Base;
	};

	class StampMapViewer : public MapViewer<StampMapViewer>, protected BrightnessAndPaletteLineSettings
	{
	private:
		using Base = MapViewer<StampMapViewer>;

		RegeneratingPieces regenerating_pieces;

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
			std::size_t piece_width,
			std::size_t piece_height,
			std::size_t total_pieces,
			std::size_t maximum_piece_width,
			std::size_t maximum_piece_height,
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

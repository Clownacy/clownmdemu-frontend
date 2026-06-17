#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include <array>
#include <cstddef>
#include <functional>

#include "../sdl-wrapper-extra.h"

#include "../../common/core/libraries/clowncommon/clowncommon.h"
#include "../../common/core/source/vdp.h"

#include "common/window-popup.h"

namespace DebugVDP
{
	static constexpr std::size_t tile_width = 8;
	static constexpr std::size_t tile_height_normal = 8;
	static constexpr std::size_t tile_height_double_resolution = 16;

	static constexpr std::size_t bits_per_word = 16;
	static constexpr std::size_t bits_per_byte = 8;
	static constexpr std::size_t bits_per_pixel = 4;
	static constexpr std::size_t pixels_per_word = bits_per_word / bits_per_pixel;
	static constexpr std::size_t pixels_per_byte = bits_per_byte / bits_per_pixel;

	static constexpr std::size_t total_palette_lines = 4;

	static constexpr std::size_t maximum_stamp_diameter_in_tiles = 4;
	static constexpr std::size_t maximum_stamp_width_in_pixels = maximum_stamp_diameter_in_tiles * tile_width;
	static constexpr std::size_t maximum_stamp_height_in_pixels = maximum_stamp_diameter_in_tiles * tile_height_normal;

	static constexpr std::size_t maximum_stamp_map_diameter_in_pixels = 4096;
	static constexpr std::size_t maximum_stamp_map_size_in_pixels = maximum_stamp_map_diameter_in_pixels * maximum_stamp_map_diameter_in_pixels;

	constexpr cc_u8f TOTAL_SPRITES = 80;

	using ReadTileWord = std::function<cc_u16f(cc_u16f word_index)>;
	using DrawMapPiece = std::function<void(SDL::Renderer &renderer, cc_u16f x, cc_u16f y)>;
	using MapPieceTooltip = std::function<void(cc_u16f x, cc_u16f y)>;
	using PaletteLine = std::array<SDL::Pixel, VDP_PALETTE_LINE_LENGTH>;
	using RenderPiece = std::function<void(cc_u16f piece_index, const PaletteLine &palette_line, SDL::Pixel *pixels, int pitch)>;
	using DrawOverlay = std::function<void(SDL::Renderer &renderer)>;

	enum class Plane
	{
		WINDOW,
		A,
		B
	};

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
		const std::size_t piece_buffer_size_in_pixels;
		const bool multiple_palette_lines;

		std::size_t texture_width = 0;
		std::size_t texture_height = 0;

	public:
		RegeneratingPieces(
			SDL::Renderer &renderer,
			std::size_t maximum_piece_width,
			std::size_t maximum_piece_height,
			std::size_t piece_buffer_size_in_pixels,
			bool multiple_palette_lines);

		void RegenerateIfNeeded(
			SDL::Renderer &renderer,
			std::size_t piece_width,
			std::size_t piece_height,
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

		SDL_FRect GetPieceRect(const std::size_t piece_index, std::size_t piece_width, std::size_t piece_height, cc_u8f palette_line_index) const;

		void Draw(SDL::Renderer &renderer, VDP_TileMetadata piece_metadata, std::size_t piece_width, std::size_t piece_height, cc_u16f x, cc_u16f y, cc_u8f brightness_index, bool transparency, bool swap_coordinates = false);
	};

	struct SpriteCommon : protected RegeneratingTextures
	{
	protected:
		// TODO: Make this some kind of internal layer over DisplayInternal?
		void DisplaySpriteCommon();

		SpriteCommon(SDL::Renderer &renderer);
	};

	class SpriteViewer: public WindowPopup<SpriteViewer>, public SpriteCommon
	{
	private:
		using Base = WindowPopup<SpriteViewer>;

		static constexpr Uint32 window_flags = 0;

		static constexpr cc_u16f plane_texture_width = 512;
		static constexpr cc_u16f plane_texture_height = 1024;

		float scale = 2.0f;
		SDL::Texture texture = SDL::CreateTexture(Base::GetWindow().GetRenderer(), SDL_TEXTUREACCESS_TARGET, plane_texture_width, plane_texture_height, SDL_SCALEMODE_PIXELART);

		void DisplayInternal();

	public:
		template<typename... Args>
		SpriteViewer(Args &&...args)
			: Base(std::forward<Args>(args)...)
			, SpriteCommon(Base::GetWindow().GetRenderer())
		{}

		friend Base;
	};

	class SpriteList : public WindowPopup<SpriteList>, public SpriteCommon
	{
	private:
		using Base = WindowPopup<SpriteList>;

		static constexpr Uint32 window_flags = 0;

		void DisplayInternal();

	public:
		template<typename... Args>
		SpriteList(Args &&...args)
			: Base(std::forward<Args>(args)...)
			, SpriteCommon(Base::GetWindow().GetRenderer())
		{}

		friend Base;
	};

	template<typename Derived>
	class MapViewer : public WindowPopup<Derived>, protected RegeneratingTexturesHardwareAccelerated
	{
	private:
		static constexpr Uint32 window_flags = 0;

		const std::size_t maximum_map_width_in_pixels;
		const std::size_t maximum_map_height_in_pixels;

	protected:
		void DisplayMap(
			std::size_t map_width_in_pieces,
			std::size_t map_height_in_pieces,
			std::size_t piece_width,
			std::size_t piece_height,
			const DrawMapPiece &draw_piece,
			const MapPieceTooltip &piece_tooltip,
			const DrawOverlay &draw_overlay = {},
			bool force_regenerate = false);

	public:
		using Base = WindowPopup<Derived>;

		template<typename... Args>
		MapViewer(const std::size_t maximum_map_width_in_pixels, const std::size_t maximum_map_height_in_pixels, Args &&...args)
			: Base(std::forward<Args>(args)...)
			, maximum_map_width_in_pixels(maximum_map_width_in_pixels)
			, maximum_map_height_in_pixels(maximum_map_height_in_pixels)
		{
			textures.emplace_back(SDL::CreateTexture(Base::GetWindow().GetRenderer(), SDL_TEXTUREACCESS_TARGET, maximum_map_width_in_pixels, maximum_map_height_in_pixels, SDL_SCALEMODE_PIXELART));
		}

		friend Base;
	};

	class PlaneViewer : public MapViewer<PlaneViewer>
	{
	private:
		using Base = MapViewer<PlaneViewer>;

		float scale = 2.0f;
		bool scroll_overlay_enabled = false;
		std::array<float, 4> scroll_overlay_colour = {1.0f, 0.0f, 0.0f, 0.5f};
		RegeneratingPieces regenerating_pieces = {Base::GetWindow().GetRenderer(), tile_width, tile_height_double_resolution, sizeof(VDP_State::vram) * pixels_per_byte, true};

		void DisplayInternal(Plane plane);

	public:
		template<typename... Args>
		PlaneViewer(Args &&...args)
			: Base(128 * tile_width, 64 * tile_height_double_resolution, std::forward<Args>(args)...)
		{}

		friend Base;
		friend Base::Base;
	};

	class StampMapViewer : public MapViewer<StampMapViewer>, protected BrightnessAndPaletteLineSettings
	{
	private:
		using Base = MapViewer<StampMapViewer>;

		float scale = 1.0f;
		RegeneratingPieces regenerating_pieces = {Base::GetWindow().GetRenderer(), maximum_stamp_width_in_pixels, maximum_stamp_height_in_pixels, maximum_stamp_map_size_in_pixels, false};

		void DisplayInternal();

	public:
		template<typename... Args>
		StampMapViewer(Args &&...args)
			: Base(maximum_stamp_map_diameter_in_pixels, maximum_stamp_map_diameter_in_pixels, std::forward<Args>(args)...)
		{}

		friend Base;
		friend Base::Base;
	};

	template<typename Derived, unsigned int default_line_length>
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
			const RenderPiece &render_piece_callback,
			const char *label_singular,
			const char *label_plural);

		static auto GetTileScale(const float dpi_scale)
		{
			return 3.0f * dpi_scale;
		}

		static auto GetTileSpacing(const float dpi_scale)
		{
			return 2.0f * dpi_scale;
		}

		static std::pair<int, int> GetDefaultWindowSize(const std::size_t piece_size, const float dpi_scale)
		{
			const auto &style = ImGui::GetStyle();
			const int default_window_size = style.WindowPadding.x * 2 + ((piece_size * GetTileScale(dpi_scale) + GetTileSpacing(dpi_scale) * 2) * default_line_length) + style.ScrollbarSize;
			return {default_window_size, default_window_size};
		}
	public:
		using Base = WindowPopup<Derived>;

		GridViewer(
			const char* const window_title,
			Window &parent_window,
			WindowWithDearImGui* const host_window,
			const std::size_t piece_size,
			const std::size_t maximum_piece_width,
			const std::size_t maximum_piece_height,
			const std::size_t piece_buffer_size_in_pixels)
			: Base(window_title, parent_window, host_window, GetDefaultWindowSize(piece_size, host_window == nullptr ? parent_window.GetDPIScale() : host_window->GetDPIScale()), 1.0f)
			, regenerating_pieces(Base::GetWindow().GetRenderer(), maximum_piece_width, maximum_piece_height, piece_buffer_size_in_pixels, false)
		{}

		friend Base;
	};

	class VRAMViewer : public GridViewer<VRAMViewer, 0x20>
	{
	private:
		using Base = GridViewer<VRAMViewer, 0x20>;

		void DisplayInternal();

	public:
		template<typename... Args>
		VRAMViewer(Args &&...args)
			: Base(std::forward<Args>(args)..., 8, tile_width, tile_height_double_resolution, sizeof(VDP_State::vram) * pixels_per_byte)
		{}

		friend Base;
		friend Base::Base;
	};

	class StampViewer : public GridViewer<StampViewer, 0x10>
	{
	private:
		using Base = GridViewer<StampViewer, 0x10>;

		void DisplayInternal();

	public:
		template<typename... Args>
		StampViewer(Args &&...args)
			: Base(std::forward<Args>(args)..., 16, maximum_stamp_diameter_in_tiles * tile_width, maximum_stamp_diameter_in_tiles * tile_height_normal, maximum_stamp_map_size_in_pixels)
		{}

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

		std::size_t selected_tab = 0;

		void DisplayInternal();

	public:
		using Base::WindowPopup;

		friend Base;
	};
}

#endif /* DEBUG_VDP_H */

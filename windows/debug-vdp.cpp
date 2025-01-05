#define IMGUI_DEFINE_MATH_OPERATORS

#include "debug-vdp.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <vector>

#include "SDL.h"

#include "../libraries/imgui/imgui.h"
#include "../common/core/clowncommon/clowncommon.h"
#include "../common/core/clownmdemu.h"

#include "../frontend.h"

struct Sprite
{
	VDP_CachedSprite cached;
	VDP_TileMetadata tile_metadata;
	cc_u16f x;
};

static constexpr cc_u16f tile_width = 8;
static constexpr cc_u16f tile_height_normal = 8;
static constexpr cc_u16f tile_height_double_resolution = 16;

static constexpr cc_u16f TileWidth()
{
	return tile_width;
}

static cc_u16f TileHeight(const VDP_State &vdp)
{
	return vdp.double_resolution_enabled ? tile_height_double_resolution : tile_height_normal;
}

static constexpr cc_u16f PixelsToBytes(const cc_u16f pixels)
{
	constexpr cc_u8f bits_per_byte = 8;
	constexpr cc_u8f bits_per_pixel = 4;
	constexpr cc_u8f pixels_per_byte = bits_per_byte / bits_per_pixel;
	return pixels / pixels_per_byte;
}

static cc_u16f TileSizeInBytes(const VDP_State &vdp)
{
	return PixelsToBytes(TileWidth() * TileHeight(vdp));
}

static cc_u16f VRAMSizeInTiles(const VDP_State &vdp)
{
	return std::size(vdp.vram) / TileSizeInBytes(vdp);
}

static constexpr cc_u8f maximum_sprite_diameter_in_tiles = 4;
static constexpr cc_u8f sprite_texture_width = maximum_sprite_diameter_in_tiles * tile_width;
static constexpr cc_u8f sprite_texture_height = maximum_sprite_diameter_in_tiles * tile_height_double_resolution;

static Sprite GetSprite(const VDP_State &vdp, const cc_u16f sprite_index)
{
	const cc_u16f sprite_address = vdp.sprite_table_address + sprite_index * 8;

	Sprite sprite;
	sprite.cached = VDP_GetCachedSprite(&vdp, sprite_index);
	sprite.tile_metadata = VDP_DecomposeTileMetadata(VDP_ReadVRAMWord(&vdp, sprite_address + 4));
	sprite.x = VDP_ReadVRAMWord(&vdp, sprite_address + 6) & 0x1FF;
	return sprite;
}

static void DrawTile(const EmulatorInstance::State &state, const VDP_TileMetadata tile_metadata, const cc_u8f tile_width, const cc_u8f tile_height, const std::function<cc_u16f(cc_u16f word_index)> &read_tile_word, Uint32* const pixels, const int pitch, const cc_u16f x, const cc_u16f y, const bool transparency, const cc_u8f brightness)
{
	const cc_u16f tile_size_in_bytes = PixelsToBytes(tile_width * tile_height);

	const cc_u16f x_flip_xor = tile_metadata.x_flip ? tile_width - 1 : 0;
	const cc_u16f y_flip_xor = tile_metadata.y_flip ? tile_height - 1 : 0;

	const auto &palette_line = state.GetPaletteLine(brightness, tile_metadata.palette_line);

	cc_u16f vram_index = tile_metadata.tile_index * tile_size_in_bytes;

	for (cc_u16f pixel_y_in_tile = 0; pixel_y_in_tile < tile_height; ++pixel_y_in_tile)
	{
		Uint32* const plane_texture_pixels_row = &pixels[x * tile_width + (y * tile_height + (pixel_y_in_tile ^ y_flip_xor)) * pitch];

		for (cc_u16f i = 0; i < PixelsToBytes(tile_width) / 2; ++i)
		{
			const cc_u16f tile_pixels = read_tile_word(vram_index);
			vram_index += 2;

			for (cc_u16f j = 0; j < 16 / 4; ++j)
			{
				const cc_u16f colour_index = ((tile_pixels << (4 * j)) & 0xF000) >> 12;
				plane_texture_pixels_row[(i * 4 + j) ^ x_flip_xor] = transparency && colour_index == 0 ? 0 : palette_line[colour_index];
			}
		}
	}
}

static void DrawSprite(const EmulatorInstance::State &state, VDP_TileMetadata tile_metadata, const cc_u8f tile_width, const cc_u8f tile_height, const cc_u16f maximum_tile_index, const std::function<cc_u16f(cc_u16f word_index)> &read_tile_word, Uint32* const pixels, const int pitch, const cc_u8f width, const cc_u8f height, const bool transparency, const cc_u8f brightness)
{
	for (cc_u8f x = 0; x < width; ++x)
	{
		const cc_u8f x_corrected = tile_metadata.x_flip ? width - 1 - x : x;

		for (cc_u8f y = 0; y < height; ++y)
		{
			const cc_u8f y_corrected = tile_metadata.y_flip ? height - 1 - y : y;

			DrawTile(state, tile_metadata, tile_width, tile_height, read_tile_word, pixels, pitch, x_corrected, y_corrected, true, 0);
			tile_metadata.tile_index = (tile_metadata.tile_index + 1) % maximum_tile_index;
		}
	}
}

void DebugVDP::RegeneratingTextures::RegenerateTexturesIfNeeded(const std::function<void(unsigned int texture_index, Uint32 *pixels, int pitch)> &callback)
{
	// Only update the texture if we know that the frame has changed.
	// This prevents constant texture generation even when the emulator is paused.
	if (cache_frame_counter != Frontend::frame_counter)
	{
		cache_frame_counter = Frontend::frame_counter;

		for (std::size_t i = 0; i < std::size(textures); ++i)
		{
			// Lock texture so that we can write into it.
			Uint32 *sprite_texture_pixels;
			int sprite_texture_pitch;

			if (SDL_LockTexture(textures[i], nullptr, reinterpret_cast<void**>(&sprite_texture_pixels), &sprite_texture_pitch) == 0)
			{
				callback(i, sprite_texture_pixels, sprite_texture_pitch / sizeof(*sprite_texture_pixels));
				SDL_UnlockTexture(textures[i]);
			}
		}
	}
}

void DebugVDP::PlaneViewer::DisplayInternal(const cc_u16l plane_address, const cc_u16l plane_width, const cc_u16l plane_height)
{
	const auto &state = Frontend::emulator->CurrentState();
	const VDP_State &vdp = state.clownmdemu.vdp;

	const cc_u16f plane_texture_width = 128 * tile_width; // 128 is the maximum plane size
	const cc_u16f plane_texture_height = 64 * tile_height_double_resolution;

	if (textures.empty())
		textures.emplace_back(SDL::CreateTexture(GetWindow().GetRenderer(), SDL_TEXTUREACCESS_STREAMING, plane_texture_width, plane_texture_height, "nearest"));

	if (!textures.empty())
	{
		ImGui::InputInt("Zoom", &scale);
		if (scale < 1)
			scale = 1;

		if (ImGui::BeginChild("Plane View", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar))
		{
			RegenerateTexturesIfNeeded([&]([[maybe_unused]] const unsigned int texture_index, Uint32* const pixels, const int pitch)
			{
				for (cc_u16f tile_y_in_plane = 0; tile_y_in_plane < plane_height; ++tile_y_in_plane)
				{
					cc_u16f plane_index = plane_address + tile_y_in_plane * plane_width * 2;

					for (cc_u16f tile_x_in_plane = 0; tile_x_in_plane < plane_width; ++tile_x_in_plane)
					{
						const auto tile_metadata = VDP_DecomposeTileMetadata(VDP_ReadVRAMWord(&vdp, plane_index));
						const cc_u8f brightness = state.clownmdemu.vdp.shadow_highlight_enabled && !tile_metadata.priority;
						DrawTile(state, tile_metadata, TileWidth(), TileHeight(vdp), [&](const cc_u16f word_index){return VDP_ReadVRAMWord(&vdp, word_index);}, pixels, pitch, tile_x_in_plane, tile_y_in_plane, false, brightness);
						plane_index += 2;
					}
				}
			});

			constexpr cc_u16f tile_width = TileWidth();
			const cc_u16f tile_height = TileHeight(vdp);

			const float plane_width_in_pixels = static_cast<float>(plane_width * tile_width);
			const float plane_height_in_pixels = static_cast<float>(plane_height * tile_height);

			const ImVec2 image_position = ImGui::GetCursorScreenPos();

			ImGui::Image(textures[0], ImVec2(plane_width_in_pixels * scale, plane_height_in_pixels * scale), ImVec2(0.0f, 0.0f), ImVec2(plane_width_in_pixels / plane_texture_width, plane_height_in_pixels / plane_texture_height));

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();

				const ImVec2 mouse_position = ImGui::GetMousePos();

				const cc_u16f tile_x = static_cast<cc_u16f>((mouse_position.x - image_position.x) / scale / tile_width);
				const cc_u16f tile_y = static_cast<cc_u16f>((mouse_position.y - image_position.y) / scale / tile_height);

				const cc_u16f packed_tile_metadata = VDP_ReadVRAMWord(&vdp, plane_address + (tile_y * plane_width + tile_x) * 2);

				const VDP_TileMetadata tile_metadata = VDP_DecomposeTileMetadata(packed_tile_metadata);

				const auto dpi_scale = GetWindow().GetDPIScale();
				ImGui::Image(textures[0], ImVec2(tile_width * SDL_roundf(9.0f * dpi_scale), tile_height * SDL_roundf(9.0f * dpi_scale)), ImVec2(static_cast<float>(tile_x * tile_width) / plane_texture_width, static_cast<float>(tile_y * tile_height) / plane_texture_height), ImVec2(static_cast<float>((tile_x + 1) * tile_width) / plane_texture_width, static_cast<float>((tile_y + 1) * tile_height) / plane_texture_height));
				ImGui::SameLine();
				ImGui::TextFormatted("Tile Index: {}/0x{:X}" "\n" "Palette Line: {}" "\n" "X-Flip: {}" "\n" "Y-Flip: {}" "\n" "Priority: {}", tile_metadata.tile_index, tile_metadata.tile_index, tile_metadata.palette_line, tile_metadata.x_flip ? "True" : "False", tile_metadata.y_flip ? "True" : "False", tile_metadata.priority ? "True" : "False");

				ImGui::EndTooltip();
			}
		}

		ImGui::EndChild();
	}
}

void DebugVDP::SpriteCommon::DisplaySpriteCommon(Window &window)
{
	const auto &state = Frontend::emulator->CurrentState();
	const VDP_State &vdp = state.clownmdemu.vdp;

	if (textures.empty())
	{
		textures.reserve(TOTAL_SPRITES);

		for (cc_u8f i = 0; i < TOTAL_SPRITES; ++i)
			textures.emplace_back(SDL::CreateTextureWithBlending(window.GetRenderer(), SDL_TEXTUREACCESS_STREAMING, sprite_texture_width, sprite_texture_height, "nearest"));
	}

	RegenerateTexturesIfNeeded([&](const unsigned int texture_index, Uint32* const pixels, const int pitch)
	{
		std::fill(&pixels[0], &pixels[pitch * sprite_texture_height], 0);

		const Sprite sprite = GetSprite(vdp, texture_index);

		DrawSprite(state, sprite.tile_metadata, TileWidth(), TileHeight(vdp), VRAMSizeInTiles(vdp), [&](const cc_u16f word_index){return VDP_ReadVRAMWord(&vdp, word_index);}, pixels, pitch, sprite.cached.width, sprite.cached.height, true, 0);
	});
}

void DebugVDP::SpriteViewer::DisplayInternal()
{
	DisplaySpriteCommon(GetWindow());

	SDL::Renderer &renderer = GetWindow().GetRenderer();
	const VDP_State &vdp = Frontend::emulator->CurrentState().clownmdemu.vdp;

	constexpr cc_u16f plane_texture_width = 512;
	constexpr cc_u16f plane_texture_height = 1024;

	if (!texture)
		texture = SDL::CreateTexture(renderer, SDL_TEXTUREACCESS_TARGET, plane_texture_width, plane_texture_height, "nearest");

	constexpr cc_u16f tile_width = TileWidth();
	const cc_u16f tile_height = TileHeight(vdp);

	ImGui::InputInt("Zoom", &scale);
	if (scale < 1)
		scale = 1;

	if (ImGui::BeginChild("Plane View", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar))
	{
		SDL_SetRenderTarget(renderer, texture);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
		SDL_RenderClear(renderer);
		SDL_SetRenderDrawColor(renderer, 0x10, 0x10, 0x10, 0xFF);
		const int vertical_scale = vdp.double_resolution_enabled ? 2 : 1;
		const SDL_Rect visible_area_rectangle = {0x80, 0x80 * vertical_scale, vdp.h40_enabled ? 320 : 256, (vdp.v30_enabled ? 240 : 224) * vertical_scale};
		SDL_RenderFillRect(renderer, &visible_area_rectangle);

		std::vector<cc_u8l> sprite_vector;

		// Need to display sprites backwards for proper layering.
		for (cc_u8f i = 0, sprite_index = 0; i < TOTAL_SPRITES; ++i)
		{
			sprite_vector.push_back(static_cast<cc_u8l>(sprite_index));

			sprite_index = GetSprite(vdp, sprite_index).cached.link;

			if (sprite_index == 0 || sprite_index >= TOTAL_SPRITES)
				break;
		}

		// Draw sprites to the plane texture.
		for (auto it = sprite_vector.crbegin(); it != sprite_vector.crend(); ++it)
		{
			const cc_u8f sprite_index = *it;
			const Sprite sprite = GetSprite(vdp, sprite_index);

			const SDL_Rect src_rect = {0, 0, static_cast<int>(sprite.cached.width * tile_width), static_cast<int>(sprite.cached.height * tile_height)};
			const SDL_Rect dst_rect = {static_cast<int>(sprite.x), static_cast<int>(sprite.cached.y), static_cast<int>(sprite.cached.width * tile_width), static_cast<int>(sprite.cached.height * tile_height)};
			SDL_RenderCopy(renderer, textures[sprite_index], &src_rect, &dst_rect);
		}

		SDL_SetRenderTarget(renderer, nullptr);

		const float plane_width_in_pixels = static_cast<float>(plane_texture_width);
		const float plane_height_in_pixels = static_cast<float>(vdp.double_resolution_enabled ? plane_texture_height : plane_texture_height / 2);

		const ImVec2 image_position = ImGui::GetCursorScreenPos();

		ImGui::Image(texture, ImVec2(plane_width_in_pixels * scale, plane_height_in_pixels * scale), ImVec2(0.0f, 0.0f), ImVec2(plane_width_in_pixels / plane_texture_width, plane_height_in_pixels / plane_texture_height));

		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();

			const ImVec2 mouse_position = ImGui::GetMousePos();

			const cc_u16f pixel_x = static_cast<cc_u16f>((mouse_position.x - image_position.x) / scale);
			const cc_u16f pixel_y = static_cast<cc_u16f>((mouse_position.y - image_position.y) / scale);

			ImGui::TextFormatted("{},{}", pixel_x, pixel_y);

			ImGui::EndTooltip();
		}
	}

	ImGui::EndChild();
}

void DebugVDP::SpriteList::DisplayInternal()
{
	DisplaySpriteCommon(GetWindow());

	const VDP_State &vdp = Frontend::emulator->CurrentState().clownmdemu.vdp;
#if 0
			for (cc_u8f i = 0; i < TOTAL_SPRITES; ++i)
			{
				ImGui::TextUnformatted("my ass");
				ImGui::SameLine();
			}
#endif
	if (ImGui::BeginTable("Sprite Table", 1 + TOTAL_SPRITES, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX))
	{
		ImGui::TableSetupScrollFreeze(1, 0);
		ImGui::TableSetupColumn("Sprite #", ImGuiTableColumnFlags_WidthFixed);
		for (cc_u8f i = 0; i < TOTAL_SPRITES; ++i)
			ImGui::TableSetupColumn(fmt::format("{}", i).c_str(), ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		const auto EverySprite = [&vdp](const std::string &label, const std::function<void(const Sprite &sprite, cc_u8f index)> &callback)
		{
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(&label.front(), &label.back() + 1);
			for (cc_u8f i = 0; i < TOTAL_SPRITES; ++i)
			{
				const Sprite sprite = GetSprite(vdp, i);
				ImGui::TableNextColumn();
				callback(sprite, i);
			}
		};

		EverySprite("Image", [&](const Sprite &sprite, const cc_u8f index)
		{
			constexpr cc_u16f tile_width = TileWidth();
			const cc_u16f tile_height = TileHeight(vdp);

			const auto dpi_scale = GetWindow().GetDPIScale();

			const auto image_scale  = SDL_roundf(2.0f * dpi_scale);
			auto image_space = ImVec2(sprite_texture_width, sprite_texture_height) * image_scale;

			const auto image_internal_size = ImVec2(sprite_texture_width, sprite_texture_height) * image_scale;
			const auto image_size = ImVec2(sprite.cached.width * tile_width, sprite.cached.height * tile_height) * image_scale;
			const auto image_offset = (image_space - image_size) / 2;
			const auto cursor_position = ImGui::GetCursorPos();
			ImGui::Dummy(image_space);
			ImGui::SetCursorPos(cursor_position + image_offset);
			ImGui::Image(textures[index], image_size, ImVec2(0, 0), image_size / image_internal_size);
		});

		EverySprite("Position", [](const Sprite &sprite, [[maybe_unused]] const cc_u8f index)
		{
			ImGui::TextFormatted("{},{}", sprite.x, sprite.cached.y);
		});

		EverySprite("Size", [](const Sprite &sprite, [[maybe_unused]] const cc_u8f index)
		{
			ImGui::TextFormatted("{}x{}", sprite.cached.width, sprite.cached.height);
		});

		EverySprite("Link", [](const Sprite &sprite, [[maybe_unused]] const cc_u8f index)
		{
			ImGui::TextFormatted("{}", sprite.cached.link);
		});

		EverySprite("Tile Index", [](const Sprite &sprite, [[maybe_unused]] const cc_u8f index)
		{
			ImGui::TextFormatted("0x{:X}", sprite.tile_metadata.tile_index);
		});

		EverySprite("Palette Line", [](const Sprite &sprite, [[maybe_unused]] const cc_u8f index)
		{
			ImGui::TextFormatted("{}", sprite.tile_metadata.palette_line);
		});

		EverySprite("X-Flip", [](const Sprite &sprite, [[maybe_unused]] const cc_u8f index)
		{
			ImGui::TextUnformatted(sprite.tile_metadata.x_flip ? "Yes" : "No");
		});

		EverySprite("Y-Flip", [](const Sprite &sprite, [[maybe_unused]] const cc_u8f index)
		{
			ImGui::TextUnformatted(sprite.tile_metadata.y_flip ? "Yes" : "No");
		});

		EverySprite("Priority", [](const Sprite &sprite, [[maybe_unused]] const cc_u8f index)
		{
			ImGui::TextUnformatted(sprite.tile_metadata.priority ? "Yes" : "No");
		});

		ImGui::EndTable();
	}
}

template<typename Derived>
void DebugVDP::GridViewer<Derived>::DisplayGrid(const cc_u16f entry_width, const cc_u16f entry_height, const std::size_t total_entries, const cc_u16f maximum_entry_width, const cc_u16f maximum_entry_height, const std::size_t entry_buffer_size_in_pixels, const std::function<void(cc_u16f entry_index, cc_u8f brightness, cc_u8f palette_line, Uint32 *pixels, int pitch)> &render_entry_callback)
{
	const auto derived = static_cast<Derived*>(this);

	// Variables relating to the sizing and spacing of the tiles in the viewer.
	const float dpi_scale = derived->GetWindow().GetDPIScale();
	const float tile_scale = SDL_roundf(3.0f * dpi_scale);
	const float tile_spacing = SDL_roundf(2.0f * dpi_scale);

	// TODO: This.
#if 0
	// Don't let the window become too small, or we can get division by zero errors later on.
	ImGui::SetNextWindowSizeConstraints(ImVec2(100.0f * dpi_scale, 100.0f * dpi_scale), ImVec2(FLT_MAX, FLT_MAX)); // Width > 100, Height > 100

	// Give the window a default size of 16 tiles wide.
	const float default_window_size = ((8 * tile_scale + tile_spacing * 2) * 0x10) + 40.0f * dpi_scale;
	ImGui::SetNextWindowSize(ImVec2(default_window_size, default_window_size), ImGuiCond_FirstUseEver);
#endif

	const auto &state = Frontend::emulator->CurrentState();

	// Create VRAM texture if it does not exist.
	if (textures.empty())
	{
		// Create a square-ish texture that's big enough to hold all tiles, in both 8x8 and 8x16 form.
		const std::size_t vram_texture_width_in_progress = static_cast<std::size_t>(SDL_ceilf(SDL_sqrtf(static_cast<float>(entry_buffer_size_in_pixels))));
		const std::size_t vram_texture_width_rounded_up_to_8 = (vram_texture_width_in_progress + (maximum_entry_width - 1)) / maximum_entry_width * maximum_entry_width;
		const std::size_t vram_texture_height_in_progress = (entry_buffer_size_in_pixels + (vram_texture_width_rounded_up_to_8 - 1)) / vram_texture_width_rounded_up_to_8;
		const std::size_t vram_texture_height_rounded_up_to_16 = (vram_texture_height_in_progress + (maximum_entry_height - 1)) / maximum_entry_height * maximum_entry_height;

		texture_width = vram_texture_width_rounded_up_to_8;
		texture_height = vram_texture_height_rounded_up_to_16;

		textures.emplace_back(SDL::CreateTexture(derived->GetWindow().GetRenderer(), SDL_TEXTUREACCESS_STREAMING, static_cast<int>(texture_width), static_cast<int>(texture_height), "nearest"));
	}

	if (!textures.empty())
	{
		bool options_changed = false;

		// Handle VRAM viewing options.
		ImGui::SeparatorText("Brightness");
		for (std::size_t i = 0; i < state.total_brightnesses; ++i)
		{
			if (i != 0)
				ImGui::SameLine();

			static const std::array brightness_names = {
				"Normal",
				"Shadow",
				"Highlight"
			};

			options_changed |= ImGui::RadioButton(brightness_names[i], &brightness_index, i);
		}

		ImGui::SeparatorText("Palette Line");
		for (std::size_t i = 0; i < state.total_palette_lines; ++i)
		{
			if (i != 0)
				ImGui::SameLine();

			options_changed |= ImGui::RadioButton(std::to_string(i).c_str(), &palette_line, i);
		}

		ImGui::SeparatorText("Tiles");

		// Set up some variables that we're going to need soon.
		const std::size_t vram_texture_width_in_tiles = texture_width / entry_width;
		const std::size_t vram_texture_height_in_tiles = texture_height / entry_height;

		RegenerateTexturesIfNeeded([&]([[maybe_unused]] const unsigned int texture_index, Uint32* const pixels, const int pitch)
		{
			// Generate VRAM bitmap.
			cc_u16f entry_index = 0;

			for (std::size_t y = 0; y < vram_texture_height_in_tiles; ++y)
			{
				for (std::size_t x = 0; x < vram_texture_width_in_tiles; ++x)
				{
					Uint32* const entry_pixels = pixels + y * entry_height * pitch + x * entry_width;
					render_entry_callback(entry_index++, brightness_index, palette_line, entry_pixels, pitch);
				}
			}
		});

		// Actually display the VRAM now.
		ImGui::BeginChild("VRAM contents");

		const ImVec2 dst_tile_size(
			entry_width  * tile_scale,
			entry_height * tile_scale);

		const ImVec2 dst_tile_size_and_padding(
			dst_tile_size.x + tile_spacing * 2,
			dst_tile_size.y + tile_spacing * 2);

		// Calculate the size of the VRAM display region.
		// Round down to the nearest multiple of the tile size + spacing, to simplify some calculations later on.
		const float vram_display_region_width = ImGui::GetContentRegionAvail().x - SDL_fmodf(ImGui::GetContentRegionAvail().x, dst_tile_size_and_padding.x);
		const std::size_t vram_display_region_width_in_tiles = SDL_floorf(vram_display_region_width / dst_tile_size_and_padding.x);

		const ImVec2 canvas_position = ImGui::GetCursorScreenPos();
		const bool window_is_hovered = ImGui::IsWindowHovered();

		// Draw the list of tiles.
		ImDrawList *draw_list = ImGui::GetWindowDrawList();

		// Here we use a clipper so that we only render the tiles that we can actually see.
		ImGuiListClipper clipper;
		clipper.Begin(CC_DIVIDE_CEILING(total_entries, vram_display_region_width_in_tiles), dst_tile_size_and_padding.y);
		while (clipper.Step())
		{
			for (std::size_t y = static_cast<std::size_t>(clipper.DisplayStart); y < static_cast<std::size_t>(clipper.DisplayEnd); ++y)
			{
				for (std::size_t x = 0; x < std::min(vram_display_region_width_in_tiles, total_entries - (y * vram_display_region_width_in_tiles)); ++x)
				{
					const std::size_t tile_index = (y * vram_display_region_width_in_tiles) + x;

					// Obtain texture coordinates for the current tile.
					const std::size_t current_tile_src_x = (tile_index % vram_texture_width_in_tiles) * entry_width;
					const std::size_t current_tile_src_y = (tile_index / vram_texture_width_in_tiles) * entry_height;

					const ImVec2 current_tile_uv0(
						static_cast<float>(current_tile_src_x) / static_cast<float>(texture_width),
						static_cast<float>(current_tile_src_y) / static_cast<float>(texture_height));

					const ImVec2 current_tile_uv1(
						static_cast<float>(current_tile_src_x + entry_width) / static_cast<float>(texture_width),
						static_cast<float>(current_tile_src_y + entry_height) / static_cast<float>(texture_height));

					// Figure out where the tile goes in the viewer.
					const float current_tile_dst_x = static_cast<float>(x) * dst_tile_size_and_padding.x;
					const float current_tile_dst_y = static_cast<float>(y) * dst_tile_size_and_padding.y;

					const ImVec2 tile_boundary_position_top_left(
						canvas_position.x + current_tile_dst_x,
						canvas_position.y + current_tile_dst_y);

					const ImVec2 tile_boundary_position_bottom_right(
						tile_boundary_position_top_left.x + dst_tile_size_and_padding.x,
						tile_boundary_position_top_left.y + dst_tile_size_and_padding.y);

					const ImVec2 tile_position_top_left(
						tile_boundary_position_top_left.x + tile_spacing,
						tile_boundary_position_top_left.y + tile_spacing);

					const ImVec2 tile_position_bottom_right(
						tile_boundary_position_bottom_right.x - tile_spacing,
						tile_boundary_position_bottom_right.y - tile_spacing);

					// Finally, display the tile.
					draw_list->AddImage(textures[0], tile_position_top_left, tile_position_bottom_right, current_tile_uv0, current_tile_uv1);

					if (window_is_hovered && ImGui::IsMouseHoveringRect(tile_boundary_position_top_left, tile_boundary_position_bottom_right))
					{
						ImGui::BeginTooltip();

						// Display the tile's index.
						const auto bytes_per_entry = PixelsToBytes(entry_width * entry_height);
						ImGui::TextFormatted("Tile: 0x{:X}\nAddress: 0x{:X}", tile_index, tile_index * bytes_per_entry);

						// Display a zoomed-in version of the tile, so that the user can get a good look at it.
						ImGui::Image(textures[0], ImVec2(dst_tile_size.x * 3.0f, dst_tile_size.y * 3.0f), current_tile_uv0, current_tile_uv1);

						ImGui::EndTooltip();
					}
				}
			}
		}

		ImGui::EndChild();
	}
}

void DebugVDP::VRAMViewer::DisplayInternal()
{
	const auto &state = Frontend::emulator->CurrentState();
	const VDP_State &vdp = state.clownmdemu.vdp;

	constexpr cc_u16f entry_width = TileWidth();
	const cc_u16f entry_height = TileHeight(vdp);
	const std::size_t total_entries = VRAMSizeInTiles(vdp);
	const std::size_t entry_buffer_size_in_pixels = std::size(vdp.vram) * 2;

	DisplayGrid(entry_width, entry_height, total_entries, tile_width, tile_height_double_resolution, entry_buffer_size_in_pixels, [&](const cc_u16f entry_index, const cc_u8f brightness, const cc_u8f palette_line, Uint32* const pixels, const int pitch)
	{
		if (entry_index >= VRAMSizeInTiles(vdp))
			return;

		const VDP_TileMetadata tile_metadata = {.tile_index = entry_index, .palette_line = palette_line, .x_flip = cc_false, .y_flip = cc_false, .priority = cc_false};
		DrawTile(state, tile_metadata, TileWidth(), TileHeight(vdp), [&](const cc_u16f word_index){return VDP_ReadVRAMWord(&vdp, word_index);}, pixels, pitch, 0, 0, false, brightness);
	});
}

void DebugVDP::StampViewer::DisplayInternal()
{
	const auto &state = Frontend::emulator->CurrentState();
	const auto &rotation = state.clownmdemu.mega_cd.rotation;

	const cc_u8f entry_diameter_in_tiles = rotation.large_stamp ? 4 : 2;
	const cc_u16f entry_width_in_pixels = entry_diameter_in_tiles * tile_width;
	const cc_u16f entry_height_in_pixels = entry_diameter_in_tiles * tile_height_normal;
	const cc_u16f tiles_per_stamp = entry_diameter_in_tiles * entry_diameter_in_tiles;
	const std::size_t total_entries = 0x200 / tiles_per_stamp;
	const std::size_t entry_buffer_size_in_pixels = total_entries * entry_width_in_pixels * entry_height_in_pixels;

	DisplayGrid(entry_width_in_pixels, entry_height_in_pixels, total_entries, tile_width, tile_height_double_resolution, entry_buffer_size_in_pixels, [&](const cc_u16f entry_index, const cc_u8f brightness, const cc_u8f palette_line, Uint32* const pixels, const int pitch)
	{
		if (entry_index >= total_entries)
			return;

		const VDP_TileMetadata tile_metadata = {.tile_index = entry_index * tiles_per_stamp, .palette_line = palette_line, .x_flip = cc_false, .y_flip = cc_false, .priority = cc_false};
		DrawSprite(state, tile_metadata, tile_width, tile_height_normal, total_entries * tiles_per_stamp, [&](const cc_u16f word_index){return state.clownmdemu.mega_cd.word_ram.buffer[word_index];}, pixels, pitch, entry_diameter_in_tiles, entry_diameter_in_tiles, false, brightness);
	});
}

void DebugVDP::CRAMViewer::DisplayInternal()
{
	const auto &state = Frontend::emulator->CurrentState();

	ImGui::SeparatorText("Brightness");
	ImGui::RadioButton("Shadow", &brightness, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Normal", &brightness, 1);
	ImGui::SameLine();
	ImGui::RadioButton("Highlight", &brightness, 2);

	ImGui::SeparatorText("Colours");

	for (auto it = std::cbegin(state.clownmdemu.vdp.cram); it != std::cend(state.clownmdemu.vdp.cram); ++it)
	{
		const auto cram_index = std::distance(std::cbegin(state.clownmdemu.vdp.cram), it);
		constexpr cc_u16f length_of_palette_line = 16;
		const cc_u16f palette_line_index = cram_index / length_of_palette_line;
		const cc_u16f colour_index = cram_index % length_of_palette_line;

		ImGui::PushID(cram_index);

		const cc_u16f value = *it;
		const cc_u16f blue = (value >> 9) & 7;
		const cc_u16f green = (value >> 5) & 7;
		const cc_u16f red = (value >> 1) & 7;

		cc_u16f value_shaded = value & 0xEEE;

		switch (brightness)
		{
			case 0:
				value_shaded >>= 1;
				break;

			case 1:
				break;

			case 2:
				value_shaded >>= 1;
				value_shaded |= 0x888;
				break;
		}

		const cc_u16f blue_shaded = (value_shaded >> 8) & 0xF;
		const cc_u16f green_shaded = (value_shaded >> 4) & 0xF;
		const cc_u16f red_shaded = (value_shaded >> 0) & 0xF;

		if (colour_index != 0)
		{
			// Split the colours into palette lines.
			ImGui::SameLine();
		}

		const float dpi_scale = GetWindow().GetDPIScale();
		ImGui::ColorButton("##colour-button", ImVec4(static_cast<float>(red_shaded) / 0xF, static_cast<float>(green_shaded) / 0xF, static_cast<float>(blue_shaded) / 0xF, 1.0f), ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(20.0f * dpi_scale, 20.0f * dpi_scale));

		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();

			ImGui::TextFormatted("Line {}, Colour {}", palette_line_index, colour_index);
			ImGui::Separator();
			ImGui::PushFont(GetMonospaceFont());
			ImGui::TextFormatted("Value: {:03X}", value);
			ImGui::TextFormatted("Blue:  {}/7", blue);
			ImGui::TextFormatted("Green: {}/7", green);
			ImGui::TextFormatted("Red:   {}/7", red);
			ImGui::PopFont();

			ImGui::EndTooltip();
		}

		ImGui::PopID();
	}
}

void DebugVDP::Registers::DisplayInternal()
{
	const auto monospace_font = GetMonospaceFont();
	const VDP_State &vdp = Frontend::emulator->CurrentState().clownmdemu.vdp;

	ImGui::SeparatorText("Miscellaneous");

	if (ImGui::BeginTable("VDP Registers", 2, ImGuiTableFlags_Borders))
	{
		ImGui::TableSetupColumn("Property");
		ImGui::TableSetupColumn("Value");
		ImGui::TableHeadersRow();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Sprite Table Address");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("0x{:04X}", vdp.sprite_table_address);
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Display Enabled");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.display_enabled ? "Yes" : "No");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("V-Int Enabled");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.v_int_enabled ? "Yes" : "No");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("H-Int Enabled");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.h_int_enabled ? "Yes" : "No");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("H40 Enabled");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.h40_enabled ? "Yes" : "No");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("V30 Enabled");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.v30_enabled ? "Yes" : "No");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Shadow/Highlight Enabled");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.shadow_highlight_enabled ? "Yes" : "No");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Double-Resolution Enabled");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.double_resolution_enabled ? "Yes" : "No");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Background Colour");
		ImGui::TableNextColumn();
		ImGui::TextFormatted("Palette Line {}, Entry {}", vdp.background_colour / 16, vdp.background_colour % 16);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("H-Int Interval");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{}", vdp.h_int_interval);
		ImGui::PopFont();

		ImGui::EndTable();
	}

	ImGui::SeparatorText("Scroll Planes");

	if (ImGui::BeginTable("Scroll Planes", 2, ImGuiTableFlags_Borders))
	{
		ImGui::TableSetupColumn("Property");
		ImGui::TableSetupColumn("Value");
		ImGui::TableHeadersRow();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Plane A Address");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("0x{:04X}", vdp.plane_a_address);
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Plane B Address");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("0x{:04X}", vdp.plane_b_address);
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Horizontal Scroll Table Address");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("0x{:04X}", vdp.hscroll_address);
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Plane Width");
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{} Tiles", vdp.plane_width_bitmask + 1);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Plane Height");
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{} Tiles", vdp.plane_height_bitmask + 1);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Horizontal Scrolling Mode");
		ImGui::TableNextColumn();
		const std::array<const char*, 3> horizontal_scrolling_modes = {
			"Whole Screen",
			"1-Tile Rows",
			"1-Pixel Rows"
		};
		ImGui::TextUnformatted(horizontal_scrolling_modes[vdp.hscroll_mode]);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Vertical Scrolling Mode");
		ImGui::TableNextColumn();
		const std::array<const char*, 2> vertical_scrolling_modes = {
			"Whole Screen",
			"2-Tile Columns"
		};
		ImGui::TextUnformatted(vertical_scrolling_modes[vdp.vscroll_mode]);

		ImGui::EndTable();
	}

	ImGui::SeparatorText("Window Plane");

	if (ImGui::BeginTable("Window Plane", 2, ImGuiTableFlags_Borders))
	{
		ImGui::TableSetupColumn("Property");
		ImGui::TableSetupColumn("Value");
		ImGui::TableHeadersRow();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Address");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("0x{:04X}", vdp.window_address);
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Horizontal Alignment");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.window.aligned_right ? "Right" : "Left");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Horizontal Boundary");
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{} Tiles", vdp.window.horizontal_boundary * 2);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Vertical Alignment");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.window.aligned_bottom ? "Bottom" : "Top");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Vertical Boundary");
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{} Tiles", vdp.window.vertical_boundary / TileHeight(vdp));

		ImGui::EndTable();
	}

	ImGui::SeparatorText("DMA");

	if (ImGui::BeginTable("DMA", 2, ImGuiTableFlags_Borders))
	{
		ImGui::TableSetupColumn("Property");
		ImGui::TableSetupColumn("Value");
		ImGui::TableHeadersRow();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Enabled");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.dma.enabled ? "Yes" : "No");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Pending");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted((vdp.access.code_register & 0x20) != 0 ? "Yes" : "No");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Mode");
		ImGui::TableNextColumn();
		static const std::array<const char*, 3> dma_modes = {
			"ROM/RAM to VRAM/CRAM/VSRAM",
			"VRAM Fill",
			"VRAM to VRAM"
		};
		ImGui::TextUnformatted(dma_modes[vdp.dma.mode]);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Source Address");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("0x{:06X}", (static_cast<cc_u32f>(vdp.dma.source_address_high) << 16) | vdp.dma.source_address_low);
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Length");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("0x{:04X}", vdp.dma.length);
		ImGui::PopFont();

		ImGui::EndTable();
	}

	ImGui::SeparatorText("Access");

	if (ImGui::BeginTable("Access", 2, ImGuiTableFlags_Borders))
	{
		ImGui::TableSetupColumn("Property");
		ImGui::TableSetupColumn("Value");
		ImGui::TableHeadersRow();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Write Pending");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.access.write_pending ? "Yes" : "No");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Address Register");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("0x{:04X}", vdp.access.address_register);
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Command Register");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("0x{:04X}", vdp.access.code_register); // TODO: Enums or something.
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Selected RAM");
		ImGui::TableNextColumn();
		static const std::array<const char*, 5> rams = {
			"VRAM",
			"CRAM",
			"VSRAM",
			"VRAM (8-bit)",
			"Invalid"
		};
		ImGui::TextUnformatted(rams[vdp.access.selected_buffer]);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Mode");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted((vdp.access.code_register & 1) != 0 ? "Write" : "Read");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Increment");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("0x{:04X}", vdp.access.increment);
		ImGui::PopFont();

		ImGui::EndTable();
	}
}

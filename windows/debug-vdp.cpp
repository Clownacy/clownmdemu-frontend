#include "debug-vdp.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <vector>

#include "SDL.h"

#include "../libraries/imgui/imgui.h"
#include "../common/clownmdemu/clowncommon/clowncommon.h"
#include "../common/clownmdemu/clownmdemu.h"

#include "../frontend.h"

struct Sprite
{
	VDP_CachedSprite cached;
	VDP_TileMetadata tile_metadata;
	cc_u16f x;
};

static constexpr cc_u16f TileWidth()
{
	return 8;
}

static cc_u16f TileHeight(const VDP_State &vdp)
{
	return vdp.double_resolution_enabled ? 16 : 8;
}

static cc_u16f TileSizeInBytes(const VDP_State &vdp)
{
	return TileWidth() * TileHeight(vdp) / 2;
}

static cc_u16f VRAMSizeInTiles(const VDP_State &vdp)
{
	return std::size(vdp.vram) / TileSizeInBytes(vdp);
}

static constexpr cc_u8f sprite_texture_width = 4 * 8;
static constexpr cc_u8f sprite_texture_height = 4 * 16;

static Sprite GetSprite(const VDP_State &vdp, const cc_u16f sprite_index)
{
	const cc_u16f sprite_address = vdp.sprite_table_address + sprite_index * 8;

	Sprite sprite;
	sprite.cached = VDP_GetCachedSprite(&vdp, sprite_index);
	sprite.tile_metadata = VDP_DecomposeTileMetadata(VDP_ReadVRAMWord(&vdp, sprite_address + 4));
	sprite.x = VDP_ReadVRAMWord(&vdp, sprite_address + 6) & 0x1FF;
	return sprite;
}

static void DrawTile(const EmulatorInstance::State &state, const VDP_TileMetadata tile_metadata, Uint8* const pixels, const int pitch, const cc_u16f x, const cc_u16f y, const bool transparency)
{
	const cc_u16f tile_width = TileWidth();
	const cc_u16f tile_height = TileHeight(state.clownmdemu.vdp);
	const cc_u16f tile_size_in_bytes = TileSizeInBytes(state.clownmdemu.vdp);

	const cc_u16f x_flip_xor = tile_metadata.x_flip ? tile_width - 1 : 0;
	const cc_u16f y_flip_xor = tile_metadata.y_flip ? tile_height - 1 : 0;

	const auto &palette_line = state.colours[state.clownmdemu.vdp.shadow_highlight_enabled && !tile_metadata.priority][tile_metadata.palette_line];

	cc_u16f vram_index = tile_metadata.tile_index * tile_size_in_bytes;

	for (cc_u16f pixel_y_in_tile = 0; pixel_y_in_tile < tile_height; ++pixel_y_in_tile)
	{
		Uint32 *plane_texture_pixels_row = reinterpret_cast<Uint32*>(&pixels[x * tile_width * sizeof(*plane_texture_pixels_row) + (y * tile_height + (pixel_y_in_tile ^ y_flip_xor)) * pitch]);

		for (cc_u16f i = 0; i < 2; ++i)
		{
			const cc_u16l tile_pixels = VDP_ReadVRAMWord(&state.clownmdemu.vdp, vram_index);
			vram_index += 2;

			for (cc_u16f j = 0; j < 4; ++j)
			{
				const cc_u16f colour_index = ((tile_pixels << (4 * j)) & 0xF000) >> 12;
				plane_texture_pixels_row[(i * 4 + j) ^ x_flip_xor] = transparency && colour_index == 0 ? 0 : palette_line[colour_index];
			}
		}
	}
}

void DebugVDP::PlaneViewer::DisplayInternal(const cc_u16l plane_address, const cc_u16l plane_width, const cc_u16l plane_height)
{
	const auto &state = Frontend::emulator->CurrentState();
	const VDP_State &vdp = state.clownmdemu.vdp;

	const cc_u16f plane_texture_width = 128 * 8; // 128 is the maximum plane size
	const cc_u16f plane_texture_height = 64 * 16;

	if (!texture)
		texture = SDL::CreateTexture(GetWindow().GetRenderer(), SDL_TEXTUREACCESS_STREAMING, plane_texture_width, plane_texture_height, "nearest");

	if (texture)
	{
		ImGui::InputInt("Zoom", &scale);
		if (scale < 1)
			scale = 1;

		if (ImGui::BeginChild("Plane View", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar))
		{
			// Only update the texture if we know that the frame has changed.
			// This prevents constant texture generation even when the emulator is paused.
			if (cache_frame_counter != Frontend::frame_counter)
			{
				cache_frame_counter = Frontend::frame_counter;

				// Lock texture so that we can write into it.
				Uint8 *plane_texture_pixels;
				int plane_texture_pitch;

				if (SDL_LockTexture(texture, nullptr, reinterpret_cast<void**>(&plane_texture_pixels), &plane_texture_pitch) == 0)
				{
					cc_u16f plane_index = plane_address;

					for (cc_u16f tile_y_in_plane = 0; tile_y_in_plane < plane_height; ++tile_y_in_plane)
					{
						for (cc_u16f tile_x_in_plane = 0; tile_x_in_plane < plane_width; ++tile_x_in_plane)
						{
							DrawTile(state, VDP_DecomposeTileMetadata(VDP_ReadVRAMWord(&vdp, plane_index)), plane_texture_pixels, plane_texture_pitch, tile_x_in_plane, tile_y_in_plane, false);
							plane_index += 2;
						}
					}

					SDL_UnlockTexture(texture);
				}
			}

			const cc_u16f tile_width = TileWidth();
			const cc_u16f tile_height = TileHeight(vdp);

			const float plane_width_in_pixels = static_cast<float>(plane_width * tile_width);
			const float plane_height_in_pixels = static_cast<float>(plane_height * tile_height);

			const ImVec2 image_position = ImGui::GetCursorScreenPos();

			ImGui::Image(texture, ImVec2(plane_width_in_pixels * scale, plane_height_in_pixels * scale), ImVec2(0.0f, 0.0f), ImVec2(plane_width_in_pixels / plane_texture_width, plane_height_in_pixels / plane_texture_height));

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();

				const ImVec2 mouse_position = ImGui::GetMousePos();

				const cc_u16f tile_x = static_cast<cc_u16f>((mouse_position.x - image_position.x) / scale / tile_width);
				const cc_u16f tile_y = static_cast<cc_u16f>((mouse_position.y - image_position.y) / scale / tile_height);

				const cc_u16f packed_tile_metadata = VDP_ReadVRAMWord(&vdp, plane_address + (tile_y * plane_width + tile_x) * 2);

				const VDP_TileMetadata tile_metadata = VDP_DecomposeTileMetadata(packed_tile_metadata);

				const auto dpi_scale = GetWindow().GetDPIScale();
				ImGui::Image(texture, ImVec2(tile_width * SDL_roundf(9.0f * dpi_scale), tile_height * SDL_roundf(9.0f * dpi_scale)), ImVec2(static_cast<float>(tile_x * tile_width) / plane_texture_width, static_cast<float>(tile_y * tile_height) / plane_texture_height), ImVec2(static_cast<float>((tile_x + 1) * tile_width) / plane_texture_width, static_cast<float>((tile_y + 1) * tile_height) / plane_texture_height));
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

	for (auto &texture : textures)
		if (!texture)
			texture = SDL::CreateTexture(window.GetRenderer(), SDL_TEXTUREACCESS_STREAMING, sprite_texture_width, sprite_texture_height, "nearest");

	const cc_u16f size_of_vram_in_tiles = VRAMSizeInTiles(vdp);

	for (cc_u8f i = 0; i < TOTAL_SPRITES; ++i)
	{
		const Sprite sprite = GetSprite(vdp, i);

		// Only update the texture if we know that the frame has changed.
		// This prevents constant texture generation even when the emulator is paused.
		if (cache_frame_counter != Frontend::frame_counter)
		{
			// Lock texture so that we can write into it.
			Uint8 *sprite_texture_pixels;
			int sprite_texture_pitch;

			if (SDL_LockTexture(textures[i], nullptr, reinterpret_cast<void**>(&sprite_texture_pixels), &sprite_texture_pitch) == 0)
			{
				auto tile_metadata = sprite.tile_metadata;

				for (cc_u8f x = 0; x < sprite.cached.width; ++x)
				{
					const cc_u8f x_corrected = tile_metadata.x_flip ? sprite.cached.width - 1 - x : x;

					for (cc_u8f y = 0; y < sprite.cached.height; ++y)
					{
						const cc_u8f y_corrected = tile_metadata.y_flip ? sprite.cached.height - 1 - y : y;

						DrawTile(state, tile_metadata, sprite_texture_pixels, sprite_texture_pitch, x_corrected, y_corrected, true);
						tile_metadata.tile_index = (tile_metadata.tile_index + 1) % size_of_vram_in_tiles;
					}
				}

				SDL_UnlockTexture(textures[i]);
			}
		}
	}

	cache_frame_counter = Frontend::frame_counter;
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

	const cc_u16f tile_width = TileWidth();
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

	if (ImGui::BeginChild("Sprites", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar))
	{
		if (ImGui::BeginTable("Sprite Table", 10, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Image");
			ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Link", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Tile Index", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Palette Line", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("X-Flip", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Y-Flip", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableHeadersRow();

			for (cc_u8f i = 0; i < TOTAL_SPRITES; ++i)
			{
				const cc_u16f tile_width = TileWidth();
				const cc_u16f tile_height = TileHeight(vdp);

				const Sprite sprite = GetSprite(vdp, i);
				const auto dpi_scale = GetWindow().GetDPIScale();

				ImGui::TableNextColumn();
				ImGui::TextFormatted("{}", i);
				ImGui::TableNextColumn();
				ImGui::Image(textures[i], ImVec2(sprite.cached.width * tile_width * SDL_roundf(2.0f * dpi_scale), sprite.cached.height * tile_height * SDL_roundf(2.0f * dpi_scale)), ImVec2(0, 0), ImVec2(static_cast<float>(sprite.cached.width * tile_width) / sprite_texture_width, static_cast<float>(sprite.cached.height * tile_height) / sprite_texture_height));
				ImGui::TableNextColumn();
				ImGui::TextFormatted("{},{}", sprite.x, sprite.cached.y);
				ImGui::TableNextColumn();
				ImGui::TextFormatted("{}x{}", sprite.cached.width, sprite.cached.height);
				ImGui::TableNextColumn();
				ImGui::TextFormatted("{}", sprite.cached.link);
				ImGui::TableNextColumn();
				ImGui::TextFormatted("{}/0x{:X}", sprite.tile_metadata.tile_index, sprite.tile_metadata.tile_index);
				ImGui::TableNextColumn();
				ImGui::TextFormatted("Line {}", sprite.tile_metadata.palette_line);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(sprite.tile_metadata.x_flip ? "Yes" : "No");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(sprite.tile_metadata.y_flip ? "Yes" : "No");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(sprite.tile_metadata.priority ? "Yes" : "No");
			}

			ImGui::EndTable();
		}
	}

	ImGui::EndChild();
}

void DebugVDP::VRAMViewer::DisplayInternal()
{
	// Variables relating to the sizing and spacing of the tiles in the viewer.
	const float dpi_scale = GetWindow().GetDPIScale();
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
	const VDP_State &vdp = state.clownmdemu.vdp;

	const cc_u16f tile_width = TileWidth();
	const cc_u16f tile_height = TileHeight(vdp);
	const std::size_t size_of_vram_in_tiles = VRAMSizeInTiles(vdp);

	// Create VRAM texture if it does not exist.
	if (!texture)
	{
		// Create a square-ish texture that's big enough to hold all tiles, in both 8x8 and 8x16 form.
		const std::size_t size_of_vram_in_pixels = std::size(vdp.vram) * 2;
		const std::size_t vram_texture_width_in_progress = static_cast<std::size_t>(SDL_ceilf(SDL_sqrtf(static_cast<float>(size_of_vram_in_pixels))));
		const std::size_t vram_texture_width_rounded_up_to_8 = (vram_texture_width_in_progress + (8 - 1)) / 8 * 8;
		const std::size_t vram_texture_height_in_progress = (size_of_vram_in_pixels + (vram_texture_width_rounded_up_to_8 - 1)) / vram_texture_width_rounded_up_to_8;
		const std::size_t vram_texture_height_rounded_up_to_16 = (vram_texture_height_in_progress + (16 - 1)) / 16 * 16;

		texture_width = vram_texture_width_rounded_up_to_8;
		texture_height = vram_texture_height_rounded_up_to_16;

		texture = SDL::CreateTexture(GetWindow().GetRenderer(), SDL_TEXTUREACCESS_STREAMING, static_cast<int>(texture_width), static_cast<int>(texture_height), "nearest");
	}

	if (ImGui::Button("Save to File"))
	{
		Frontend::file_utilities.SaveFile(GetWindow(), "Save VRAM Dump",
		[vdp](const FileUtilities::SaveFileInnerCallback &callback)
		{
			return callback(vdp.vram, sizeof(vdp.vram));
		});
	}

	if (texture)
	{
		bool options_changed = false;

		// Handle VRAM viewing options.
		ImGui::SeparatorText("Brightness");
		for (std::size_t i = 0; i < state.colours.size(); ++i)
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
		for (std::size_t i = 0; i < state.colours[0].size(); ++i)
		{
			if (i != 0)
				ImGui::SameLine();

			static const std::array brightness_names = {
				"Normal",
				"Shadow",
				"Highlight"
			};

			options_changed |= ImGui::RadioButton(std::to_string(i).c_str(), &palette_line, i);
		}

		ImGui::SeparatorText("Tiles");

		// Set up some variables that we're going to need soon.
		const std::size_t vram_texture_width_in_tiles = texture_width / tile_width;
		const std::size_t vram_texture_height_in_tiles = texture_height / tile_height;

		// Only update the texture if we know that the frame has changed.
		// This prevents constant texture generation even when the emulator is paused.
		if (cache_frame_counter != Frontend::frame_counter || options_changed)
		{
			cache_frame_counter = Frontend::frame_counter;

			// Select the correct palette line.
			const auto &selected_palette = state.colours[brightness_index][palette_line];

			// Lock texture so that we can write into it.
			Uint8 *vram_texture_pixels;
			int vram_texture_pitch;

			if (SDL_LockTexture(texture, nullptr, reinterpret_cast<void**>(&vram_texture_pixels), &vram_texture_pitch) == 0)
			{
				// Generate VRAM bitmap.
				cc_u16f vram_index = 0;

				// As an optimisation, the tiles are ordered top-to-bottom then left-to-right,
				// instead of left-to-right then top-to-bottom.
				for (std::size_t x = 0; x < vram_texture_width_in_tiles; ++x)
				{
					for (std::size_t y = 0; y < vram_texture_height_in_tiles * tile_height; ++y)
					{
						Uint32 *pixels_pointer = reinterpret_cast<Uint32*>(vram_texture_pixels + x * tile_width * sizeof(Uint32) + y * vram_texture_pitch);

						for (cc_u16f i = 0; i < 2; ++i)
						{
							// TODO: Stop reading past the end of VRAM so that this bitmask is not necessary!
							const cc_u16l tile_row = VDP_ReadVRAMWord(&vdp, vram_index & 0xFFFF);
							vram_index += 2;

							for (cc_u16f j = 0; j < 4; ++j)
							{
								const cc_u16f colour_index = ((tile_row << (4 * j)) & 0xF000) >> 12;
								*pixels_pointer++ = selected_palette[colour_index];
							}
						}
					}
				}

				SDL_UnlockTexture(texture);
			}
		}

		// Actually display the VRAM now.
		ImGui::BeginChild("VRAM contents");

		const ImVec2 dst_tile_size(
			tile_width  * tile_scale,
			tile_height * tile_scale);

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
		clipper.Begin(CC_DIVIDE_CEILING(size_of_vram_in_tiles, vram_display_region_width_in_tiles), dst_tile_size_and_padding.y);
		while (clipper.Step())
		{
			for (std::size_t y = static_cast<std::size_t>(clipper.DisplayStart); y < static_cast<std::size_t>(clipper.DisplayEnd); ++y)
			{
				for (std::size_t x = 0; x < std::min(vram_display_region_width_in_tiles, size_of_vram_in_tiles - (y * vram_display_region_width_in_tiles)); ++x)
				{
					const std::size_t tile_index = (y * vram_display_region_width_in_tiles) + x;

					// Obtain texture coordinates for the current tile.
					const std::size_t current_tile_src_x = (tile_index / vram_texture_height_in_tiles) * tile_width;
					const std::size_t current_tile_src_y = (tile_index % vram_texture_height_in_tiles) * tile_height;

					const ImVec2 current_tile_uv0(
						static_cast<float>(current_tile_src_x) / static_cast<float>(texture_width),
						static_cast<float>(current_tile_src_y) / static_cast<float>(texture_height));

					const ImVec2 current_tile_uv1(
						static_cast<float>(current_tile_src_x + tile_width) / static_cast<float>(texture_width),
						static_cast<float>(current_tile_src_y + tile_height) / static_cast<float>(texture_height));

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
					draw_list->AddImage(texture, tile_position_top_left, tile_position_bottom_right, current_tile_uv0, current_tile_uv1);

					if (window_is_hovered && ImGui::IsMouseHoveringRect(tile_boundary_position_top_left, tile_boundary_position_bottom_right))
					{
						ImGui::BeginTooltip();

						// Display the tile's index.
						ImGui::TextFormatted("{}/0x{:X}", tile_index, tile_index);

						// Display a zoomed-in version of the tile, so that the user can get a good look at it.
						ImGui::Image(texture, ImVec2(dst_tile_size.x * 3.0f, dst_tile_size.y * 3.0f), current_tile_uv0, current_tile_uv1);

						ImGui::EndTooltip();
					}
				}
			}
		}

		ImGui::EndChild();
	}
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
		ImGui::TextUnformatted("Window Plane Address");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("0x{:04X}", vdp.window_address);
		ImGui::PopFont();

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
		ImGui::TextUnformatted("Window Plane Horizontal Boundary");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{}", vdp.window.horizontal_boundary);
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Window Plane Horizontal Alignment");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.window.aligned_right ? "Right" : "Left");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Window Plane Vertical Boundary");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{}", vdp.window.vertical_boundary);
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Window Plane Vertical Alignment");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(vdp.window.aligned_bottom ? "Bottom" : "Top");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Plane Width");
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{} Tiles", vdp.plane_width);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Plane Height");
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{} Tiles", vdp.plane_height);

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
		static const std::array<const char*, 3> rams = {
			"VRAM",
			"CRAM",
			"VSRAM"
		};
		ImGui::TextUnformatted(rams[vdp.access.selected_buffer]);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Mode");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted((vdp.access.code_register & 1 ) != 0 ? "Write" : "Read");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Increment");
		ImGui::PushFont(monospace_font);
		ImGui::TableNextColumn();
		ImGui::TextFormatted("0x{:04X}", vdp.access.increment);
		ImGui::PopFont();

		ImGui::EndTable();
	}
}

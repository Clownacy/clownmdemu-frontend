#include "debug_vdp.h"

#include <stddef.h>

#include "SDL.h"
#include "libraries/imgui/imgui.h"
#include "../clowncommon.h"
#include "../clownmdemu.h"

#include "error.h"

typedef struct TileMetadata
{
	cc_u16f tile_index;
	cc_u16f palette_line;
	bool x_flip;
	bool y_flip;
	bool priority;
} TileMetadata;

static void DecomposeTileMetadata(cc_u16f packed_tile_metadata, TileMetadata *tile_metadata)
{
	tile_metadata->tile_index = packed_tile_metadata & 0x7FF;
	tile_metadata->palette_line = (packed_tile_metadata >> 13) & 3;
	tile_metadata->x_flip = (packed_tile_metadata & 0x800) != 0;
	tile_metadata->y_flip = (packed_tile_metadata & 0x1000) != 0;
	tile_metadata->priority = (packed_tile_metadata & 0x8000) != 0;
}

static void Debug_Plane(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data, bool plane_b)
{
	if (ImGui::Begin(plane_b ? "Plane B" : "Plane A", open))
	{
		static SDL_Texture *plane_textures[2];
		SDL_Texture *&plane_texture = plane_textures[plane_b];
		const cc_u16f plane_texture_width = 128 * 8; // 128 is the maximum plane size
		const cc_u16f plane_texture_height = 64 * 16;

		if (plane_texture == NULL)
		{
			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
			plane_texture = SDL_CreateTexture(data->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, plane_texture_width, plane_texture_height);

			if (plane_texture == NULL)
			{
				PrintError("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
			}
			else
			{
				// Disable blending, since we don't need it
				if (SDL_SetTextureBlendMode(plane_texture, SDL_BLENDMODE_NONE) < 0)
					PrintError("SDL_SetTextureBlendMode failed with the following message - '%s'", SDL_GetError());
			}
		}

		if (plane_texture != NULL)
		{
			static int plane_scales[2] = {1, 1};
			int &plane_scale = plane_scales[plane_b];
			ImGui::InputInt("Zoom", &plane_scale);
			if (plane_scale < 1)
				plane_scale = 1;

			if (ImGui::BeginChild("Plane View"))
			{
				const cc_u16l *plane = &clownmdemu->state->vdp.vram[plane_b ? clownmdemu->state->vdp.plane_b_address : clownmdemu->state->vdp.plane_a_address];

				const cc_u16f tile_width = 8;
				const cc_u16f tile_height = clownmdemu->state->vdp.double_resolution_enabled ? 16 : 8;

				// Lock texture so that we can write into it.
				Uint8 *plane_texture_pixels;
				int plane_texture_pitch;

				if (SDL_LockTexture(plane_texture, NULL, (void**)&plane_texture_pixels, &plane_texture_pitch) == 0)
				{
					const cc_u16l *plane_pointer = plane;

					for (cc_u16f tile_y_in_plane = 0; tile_y_in_plane < clownmdemu->state->vdp.plane_height; ++tile_y_in_plane)
					{
						for (cc_u16f tile_x_in_plane = 0; tile_x_in_plane < clownmdemu->state->vdp.plane_width; ++tile_x_in_plane)
						{
							TileMetadata tile_metadata;
							DecomposeTileMetadata(*plane_pointer++, &tile_metadata);

							const cc_u16f x_flip_xor = tile_metadata.x_flip ? tile_width - 1 : 0;
							const cc_u16f y_flip_xor = tile_metadata.y_flip ? tile_height - 1 : 0;

							const cc_u16f palette_index = tile_metadata.palette_line * 16 + (clownmdemu->state->vdp.shadow_highlight_enabled && !tile_metadata.priority) * 16 * 4;

							const Uint32 *palette_line = &data->colours[palette_index];

							const cc_u16l *tile_pointer = &clownmdemu->state->vdp.vram[tile_metadata.tile_index * (tile_width * tile_height / 4)];

							for (cc_u16f pixel_y_in_tile = 0; pixel_y_in_tile < tile_height; ++pixel_y_in_tile)
							{
								Uint32 *plane_texture_pixels_row = (Uint32*)&plane_texture_pixels[tile_x_in_plane * tile_width * sizeof(*plane_texture_pixels_row) + (tile_y_in_plane * tile_height + (pixel_y_in_tile ^ y_flip_xor)) * plane_texture_pitch];

								for (cc_u16f i = 0; i < 2; ++i)
								{
									const cc_u16l tile_pixels = *tile_pointer++;

									for (cc_u16f j = 0; j < 4; ++j)
									{
										const cc_u16f colour_index = ((tile_pixels << (4 * j)) & 0xF000) >> 12;
										plane_texture_pixels_row[(i * 4 + j) ^ x_flip_xor] = palette_line[colour_index];
									}
								}
							}
						}
					}

					SDL_UnlockTexture(plane_texture);
				}

				const float plane_width_in_pixels = (float)(clownmdemu->state->vdp.plane_width * tile_width);
				const float plane_height_in_pixels = (float)(clownmdemu->state->vdp.plane_height * tile_height);

				const ImVec2 image_position = ImGui::GetCursorScreenPos();

				ImGui::Image(plane_texture, ImVec2((float)plane_width_in_pixels * plane_scale, (float)plane_height_in_pixels * plane_scale), ImVec2(0.0f, 0.0f), ImVec2((float)plane_width_in_pixels / (float)plane_texture_width, (float)plane_height_in_pixels / (float)plane_texture_height));

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();

					const ImVec2 mouse_position = ImGui::GetMousePos();

					const cc_u16f tile_x = (cc_u16f)((mouse_position.x - image_position.x) / plane_scale / tile_width);
					const cc_u16f tile_y = (cc_u16f)((mouse_position.y - image_position.y) / plane_scale / tile_height);

					const cc_u16l *plane = &clownmdemu->state->vdp.vram[plane_b ? clownmdemu->state->vdp.plane_b_address : clownmdemu->state->vdp.plane_a_address];
					const cc_u16f packed_tile_metadata = plane[tile_y * clownmdemu->state->vdp.plane_width + tile_x];

					TileMetadata tile_metadata;
					DecomposeTileMetadata(packed_tile_metadata, &tile_metadata);

					ImGui::Image(plane_texture, ImVec2(tile_width * SDL_roundf(9.0f * data->dpi_scale), tile_height * SDL_roundf(9.0f * data->dpi_scale)), ImVec2((float)(tile_x * tile_width) / (float)plane_texture_width, (float)(tile_y * tile_height) / (float)plane_texture_height), ImVec2((float)((tile_x + 1) * tile_width) / (float)plane_texture_width, (float)((tile_y + 1) * tile_height) / (float)plane_texture_width));
					ImGui::SameLine();
					ImGui::Text("Tile Index: %" CC_PRIuFAST16 "/0x%" CC_PRIXFAST16 "\n" "Palette Line: %" CC_PRIdFAST16 "\n" "X-Flip: %s" "\n" "Y-Flip: %s" "\n" "Priority: %s", tile_metadata.tile_index, tile_metadata.tile_index, tile_metadata.palette_line, tile_metadata.x_flip ? "True" : "False", tile_metadata.y_flip ? "True" : "False", tile_metadata.priority ? "True" : "False");

					ImGui::EndTooltip();
				}
			}

			ImGui::EndChild();
		}
	}

	ImGui::End();
}

void Debug_PlaneA(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data)
{
	Debug_Plane(open, clownmdemu, data, false);
}

void Debug_PlaneB(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data)
{
	Debug_Plane(open, clownmdemu, data, true);
}

void Debug_VRAM(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data)
{
	// Don't let the window become too small, or we can get division by zero errors later on.
	ImGui::SetNextWindowSizeConstraints(ImVec2(100.0f * data->dpi_scale, 100.0f * data->dpi_scale), ImVec2(FLT_MAX, FLT_MAX)); // Width > 100, Height > 100

	if (ImGui::Begin("VRAM", open))
	{
		static SDL_Texture *vram_texture;
		static size_t vram_texture_width;
		static size_t vram_texture_height;

		const size_t tile_width = 8;
		const size_t tile_height = clownmdemu->state->vdp.double_resolution_enabled ? 16 : 8;

		const size_t size_of_vram_in_tiles = CC_COUNT_OF(clownmdemu->state->vdp.vram) * 4 / (tile_width * tile_height);

		// Create VRAM texture if it does not exist.
		if (vram_texture == NULL)
		{
			// Create a square-ish texture that's big enough to hold all tiles, in both 8x8 and 8x16 form.
			const size_t size_of_vram_in_pixels = CC_COUNT_OF(clownmdemu->state->vdp.vram) * 4;
			const size_t vram_texture_width_in_progress = (size_t)SDL_ceilf(SDL_sqrtf((float)size_of_vram_in_pixels));
			const size_t vram_texture_width_rounded_up_to_8 = (vram_texture_width_in_progress + (8 - 1)) / 8 * 8;
			const size_t vram_texture_height_in_progress = (size_of_vram_in_pixels + (vram_texture_width_rounded_up_to_8 - 1)) / vram_texture_width_rounded_up_to_8;
			const size_t vram_texture_height_rounded_up_to_16 = (vram_texture_height_in_progress + (16 - 1)) / 16 * 16;

			vram_texture_width = vram_texture_width_rounded_up_to_8;
			vram_texture_height = vram_texture_height_rounded_up_to_16;

			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
			vram_texture = SDL_CreateTexture(data->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (int)vram_texture_width, (int)vram_texture_height);

			if (vram_texture == NULL)
			{
				PrintError("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
			}
			else
			{
				// Disable blending, since we don't need it
				if (SDL_SetTextureBlendMode(vram_texture, SDL_BLENDMODE_NONE) < 0)
					PrintError("SDL_SetTextureBlendMode failed with the following message - '%s'", SDL_GetError());
			}
		}

		if (vram_texture != NULL)
		{
			// Handle VRAM viewing options.
			static int brightness_index;
			static int palette_line;

			if (ImGui::TreeNodeEx("Options", ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_DefaultOpen))
			{
				const int length_of_palette_line = 16;
				const int length_of_palette = length_of_palette_line * 4;

				ImGui::Text("Brightness");
				ImGui::RadioButton("Shadow", &brightness_index, length_of_palette * 1);
				ImGui::SameLine();
				ImGui::RadioButton("Normal", &brightness_index, length_of_palette * 0);
				ImGui::SameLine();
				ImGui::RadioButton("Highlight", &brightness_index, length_of_palette * 2);

				ImGui::Text("Palette Line");
				ImGui::RadioButton("0", &palette_line, length_of_palette_line * 0);
				ImGui::SameLine();
				ImGui::RadioButton("1", &palette_line, length_of_palette_line * 1);
				ImGui::SameLine();
				ImGui::RadioButton("2", &palette_line, length_of_palette_line * 2);
				ImGui::SameLine();
				ImGui::RadioButton("3", &palette_line, length_of_palette_line * 3);
			}

			// Select the correct palette line.
			const Uint32 *selected_palette = &data->colours[brightness_index + palette_line];

			// Set up some variables that we're going to need soon.
			const size_t vram_texture_width_in_tiles = vram_texture_width / tile_width;
			const size_t vram_texture_height_in_tiles = vram_texture_height / tile_height;

			// Lock texture so that we can write into it.
			Uint8 *vram_texture_pixels;
			int vram_texture_pitch;

			if (SDL_LockTexture(vram_texture, NULL, (void**)&vram_texture_pixels, &vram_texture_pitch) == 0)
			{
				// Generate VRAM bitmap.
				const cc_u16l *vram_pointer = clownmdemu->state->vdp.vram;

				// As an optimisation, the tiles are ordered top-to-bottom then left-to-right,
				// instead of left-to-right then top-to-bottom.
				for (size_t x = 0; x < vram_texture_width_in_tiles; ++x)
				{
					for (size_t y = 0; y < vram_texture_height_in_tiles * tile_height; ++y)
					{
						Uint32 *pixels_pointer = (Uint32*)(vram_texture_pixels + x * tile_width * sizeof(Uint32) + y * vram_texture_pitch);

						for (cc_u16f i = 0; i < 2; ++i)
						{
							const cc_u16l tile_row = *vram_pointer++;

							for (cc_u16f j = 0; j < 4; ++j)
							{
								const cc_u16f colour_index = ((tile_row << (4 * j)) & 0xF000) >> 12;
								*pixels_pointer++ = selected_palette[colour_index];
							}
						}
					}
				}

				SDL_UnlockTexture(vram_texture);
			}

			// Actually display the VRAM now.
			ImGui::BeginChild("VRAM contents");

			// Variables relating to the sizing and spacing of the tiles in the viewer.
			const float dst_tile_scale = SDL_roundf(3.0f * data->dpi_scale);
			const ImVec2 dst_tile_size(
				tile_width  * dst_tile_scale,
				tile_height * dst_tile_scale);
			const float spacing = SDL_roundf(5.0f * data->dpi_scale);

			// Calculate the size of the VRAM display region.
			// Round down to the nearest multiple of the tile size + spacing, to simplify some calculations later on.
			const float vram_display_region_width = ImGui::GetContentRegionAvail().x - SDL_fmodf(ImGui::GetContentRegionAvail().x, dst_tile_size.x + spacing);
			const float vram_display_region_height = SDL_ceilf((float)size_of_vram_in_tiles * (dst_tile_size.x + spacing) / vram_display_region_width) * (dst_tile_size.y + spacing);

			const ImVec2 canvas_position = ImGui::GetCursorScreenPos();
			const bool window_is_hovered = ImGui::IsWindowHovered();

			// Extend the region down so that it's given a scroll bar.
			ImGui::SetCursorScreenPos(ImVec2(canvas_position.x, canvas_position.y + vram_display_region_height));

			// Draw the list of tiles.
			ImDrawList *draw_list = ImGui::GetWindowDrawList();

			for (size_t tile_index = 0; tile_index < size_of_vram_in_tiles; ++tile_index)
			{
				// Obtain texture coordinates for the current tile.
				const size_t current_tile_src_x = (tile_index / vram_texture_height_in_tiles) * tile_width;
				const size_t current_tile_src_y = (tile_index % vram_texture_height_in_tiles) * tile_height;

				const ImVec2 current_tile_uv0(
					(float)current_tile_src_x / (float)vram_texture_width,
					(float)current_tile_src_y / (float)vram_texture_height);

				const ImVec2 current_tile_uv1(
					(float)(current_tile_src_x + tile_width) / (float)vram_texture_width,
					(float)(current_tile_src_y + tile_height) / (float)vram_texture_height);

				// Figure out where the tile goes in the viewer.
				const float current_tile_dst_x = SDL_fmodf((float)tile_index * (dst_tile_size.x + spacing), vram_display_region_width);
				const float current_tile_dst_y = SDL_floorf((float)tile_index * (dst_tile_size.x + spacing) / vram_display_region_width) * (dst_tile_size.y + spacing);

				const ImVec2 tile_boundary_position_top_left(
					canvas_position.x + current_tile_dst_x,
					canvas_position.y + current_tile_dst_y);

				const ImVec2 tile_boundary_position_bottom_right(
					tile_boundary_position_top_left.x + dst_tile_size.x + spacing,
					tile_boundary_position_top_left.y + dst_tile_size.y + spacing);

				const ImVec2 tile_position_top_left(
					tile_boundary_position_top_left.x + spacing * 0.5f,
					tile_boundary_position_top_left.y + spacing * 0.5f);

				const ImVec2 tile_position_bottom_right(
					tile_boundary_position_bottom_right.x - spacing * 0.5f,
					tile_boundary_position_bottom_right.y - spacing * 0.5f);

				// Finally, display the tile.
				draw_list->AddImage(vram_texture, tile_position_top_left, tile_position_bottom_right, current_tile_uv0, current_tile_uv1);

				if (window_is_hovered && ImGui::IsMouseHoveringRect(tile_boundary_position_top_left, tile_boundary_position_bottom_right))
				{
					ImGui::BeginTooltip();

					// Display the tile's index.
					ImGui::Text("%zd/0x%zX", tile_index, tile_index);

					// Display a zoomed-in version of the tile, so that the user can get a good look at it.
					ImGui::Image(vram_texture, ImVec2(dst_tile_size.x * 3.0f, dst_tile_size.y * 3.0f), current_tile_uv0, current_tile_uv1);

					ImGui::EndTooltip();
				}
			}

			ImGui::EndChild();
		}
	}

	ImGui::End();
}

void Debug_CRAM(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data, ImFont *monospace_font)
{
	if (ImGui::Begin("CRAM", open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static int brightness = 1;

		ImGui::Text("Brightness");
		ImGui::RadioButton("Shadow", &brightness, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Normal", &brightness, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Highlight", &brightness, 2);
		ImGui::Separator();

		const cc_u16f length_of_palette_line = 16;
		const cc_u16f length_of_palette = length_of_palette_line * 4;

		for (cc_u16f j = 0; j < length_of_palette; ++j)
		{
			ImGui::PushID(j);

			const cc_u16f value = clownmdemu->state->vdp.cram[j];
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

			if (j % length_of_palette_line != 0)
			{
				// Split the colours into palette lines.
				ImGui::SameLine();
			}

			ImGui::ColorButton("", ImVec4((float)red_shaded / (float)0xF, (float)green_shaded / (float)0xF, (float)blue_shaded / (float)0xF, 1.0f), ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoTooltip, ImVec2(20.0f * data->dpi_scale, 20.0f * data->dpi_scale));

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();

				ImGui::Text("Line %" CC_PRIdFAST16 ", Colour %" CC_PRIdFAST16, j / length_of_palette_line, j % length_of_palette_line);
				ImGui::Separator();
				ImGui::PushFont(monospace_font);
				ImGui::Text("Value: %03" CC_PRIXFAST16, value);
				ImGui::Text("Blue:  %" CC_PRIdFAST16 "/7", blue);
				ImGui::Text("Green: %" CC_PRIdFAST16 "/7", green);
				ImGui::Text("Red:   %" CC_PRIdFAST16 "/7", red);
				ImGui::PopFont();

				ImGui::EndTooltip();
			}

			ImGui::PopID();
		}
	}

	ImGui::End();
}

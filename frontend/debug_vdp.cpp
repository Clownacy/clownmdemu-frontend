#include "debug_vdp.h"

#include <stddef.h>

#include "SDL.h"
#include "libraries/imgui/imgui.h"
#include "../clowncommon.h"
#include "../clownmdemu.h"

#include "error.h"
#include "video.h"

typedef struct TileMetadata
{
	unsigned int tile_index;
	unsigned int palette_line;
	bool x_flip;
	bool y_flip;
	bool priority;
} TileMetadata;

static void DecomposeTileMetadata(unsigned int packed_tile_metadata, TileMetadata *tile_metadata)
{
	tile_metadata->tile_index = packed_tile_metadata & 0x7FF;
	tile_metadata->palette_line = (packed_tile_metadata >> 13) & 3;
	tile_metadata->x_flip = (packed_tile_metadata & 0x800) != 0;
	tile_metadata->y_flip = (packed_tile_metadata & 0x1000) != 0;
	tile_metadata->priority = (packed_tile_metadata & 0x8000) != 0;
}

static void Debug_Plane(bool *open, const ClownMDEmu_State *clownmdemu_state, Uint32 colours[16 * 4 * 3], bool plane_b)
{
	if (ImGui::Begin(plane_b ? "Plane B Viewer" : "Plane A Viewer", open))
	{
		static SDL_Texture *plane_textures[2];
		SDL_Texture *&plane_texture = plane_textures[plane_b];
		const unsigned int plane_texture_width = 128 * 8; // 128 is the maximum plane size
		const unsigned int plane_texture_height = 64 * 16;

		if (plane_texture == NULL)
		{
			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
			plane_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, plane_texture_width, plane_texture_height);

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
				const unsigned short *plane = &clownmdemu_state->vdp.vram[plane_b ? clownmdemu_state->vdp.plane_b_address : clownmdemu_state->vdp.plane_a_address];

				const unsigned int tile_width = 8;
				const unsigned int tile_height = clownmdemu_state->vdp.interlace_mode_2_enabled ? 16 : 8;

				// Lock texture so that we can write into it.
				unsigned char *plane_texture_pixels;
				int plane_texture_pitch;

				if (SDL_LockTexture(plane_texture, NULL, (void**)&plane_texture_pixels, &plane_texture_pitch) == 0)
				{
					const unsigned short *plane_pointer = plane;

					for (unsigned int tile_y_in_plane = 0; tile_y_in_plane < clownmdemu_state->vdp.plane_height; ++tile_y_in_plane)
					{
						for (unsigned int tile_x_in_plane = 0; tile_x_in_plane < clownmdemu_state->vdp.plane_width; ++tile_x_in_plane)
						{
							TileMetadata tile_metadata;
							DecomposeTileMetadata(*plane_pointer++, &tile_metadata);

							const unsigned int palette_line_index = tile_metadata.palette_line * 16;
							const unsigned x_flip_xor = tile_metadata.x_flip ? tile_width - 1 : 0;
							const unsigned y_flip_xor = tile_metadata.y_flip ? tile_height - 1 : 0;

							const unsigned short *tile_pointer = &clownmdemu_state->vdp.vram[tile_metadata.tile_index * (tile_width * tile_height / 4)];

							for (unsigned int pixel_y_in_tile = 0; pixel_y_in_tile < tile_height; ++pixel_y_in_tile)
							{
								Uint32 *plane_texture_pixels_row = (Uint32*)&plane_texture_pixels[tile_x_in_plane * tile_width * sizeof(*plane_texture_pixels_row) + (tile_y_in_plane * tile_height + pixel_y_in_tile ^ y_flip_xor) * plane_texture_pitch];

								for (unsigned int i = 0; i < 2; ++i)
								{
									const unsigned short tile_pixels = *tile_pointer++;

									for (unsigned int j = 0; j < 4; ++j)
									{
										const unsigned int colour_index = ((tile_pixels << (4 * j)) & 0xF000) >> 12;
										plane_texture_pixels_row[(i * 4 + j) ^ x_flip_xor] = colours[palette_line_index + colour_index];
									}
								}
							}
						}
					}

					SDL_UnlockTexture(plane_texture);
				}

				const float plane_width_in_pixels = (float)(clownmdemu_state->vdp.plane_width * tile_width);
				const float plane_height_in_pixels = (float)(clownmdemu_state->vdp.plane_height * tile_height);

				const ImVec2 image_position = ImGui::GetCursorScreenPos();

				ImGui::Image(plane_texture, ImVec2((float)plane_width_in_pixels * plane_scale, (float)plane_height_in_pixels * plane_scale), ImVec2(0.0f, 0.0f), ImVec2((float)plane_width_in_pixels / (float)plane_texture_width, (float)plane_height_in_pixels / (float)plane_texture_height));

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();

					const ImVec2 mouse_position = ImGui::GetMousePos();

					const unsigned int tile_x = (unsigned int)((mouse_position.x - image_position.x) / plane_scale / tile_width);
					const unsigned int tile_y = (unsigned int)((mouse_position.y - image_position.y) / plane_scale / tile_height);

					const unsigned short *plane = &clownmdemu_state->vdp.vram[plane_b ? clownmdemu_state->vdp.plane_b_address : clownmdemu_state->vdp.plane_a_address];
					const unsigned int packed_tile_metadata = plane[tile_y * clownmdemu_state->vdp.plane_width + tile_x];

					TileMetadata tile_metadata;
					DecomposeTileMetadata(packed_tile_metadata, &tile_metadata);

					ImGui::Image(plane_texture, ImVec2(tile_width * SDL_roundf(9.0f * dpi_scale), tile_height * SDL_roundf(9.0f * dpi_scale)), ImVec2((float)(tile_x * tile_width) / (float)plane_texture_width, (float)(tile_y * tile_height) / (float)plane_texture_height), ImVec2((float)((tile_x + 1) * tile_width) / (float)plane_texture_width, (float)((tile_y + 1) * tile_height) / (float)plane_texture_width));
					ImGui::SameLine();
					ImGui::Text("Tile Index: %u/0x%X" "\n" "Palette Line: %d" "\n" "X-Flip: %s" "\n" "Y-Flip: %s" "\n" "Priority: %s", tile_metadata.tile_index, tile_metadata.tile_index, tile_metadata.palette_line, tile_metadata.x_flip ? "True" : "False", tile_metadata.y_flip ? "True" : "False", tile_metadata.priority ? "True" : "False");

					ImGui::EndTooltip();
				}
			}

			ImGui::EndChild();
		}
	}

	ImGui::End();
}

void Debug_PlaneA(bool *open, const ClownMDEmu_State *clownmdemu_state, Uint32 colours[16 * 4 * 3])
{
	Debug_Plane(open, clownmdemu_state, colours, false);
}

void Debug_PlaneB(bool *open, const ClownMDEmu_State *clownmdemu_state, Uint32 colours[16 * 4 * 3])
{
	Debug_Plane(open, clownmdemu_state, colours, true);
}

void Debug_VRAM(bool *open, const ClownMDEmu_State *clownmdemu_state, Uint32 colours[16 * 4 * 3])
{
	// Don't let the window become too small, or we can get division by zero errors later on.
	ImGui::SetNextWindowSizeConstraints(ImVec2(100.0f * dpi_scale, 100.0f * dpi_scale), ImVec2(FLT_MAX, FLT_MAX)); // Width > 100, Height > 100

	if (ImGui::Begin("VRAM Viewer", open))
	{
		static SDL_Texture *vram_texture;
		static size_t vram_texture_width;
		static size_t vram_texture_height;

		const size_t tile_width = 8;
		const size_t tile_height = clownmdemu_state->vdp.interlace_mode_2_enabled ? 16 : 8;

		const size_t size_of_vram_in_tiles = CC_COUNT_OF(clownmdemu_state->vdp.vram) * 4 / (tile_width * tile_height);

		// Create VRAM texture if it does not exist.
		if (vram_texture == NULL)
		{
			// Create a square-ish texture that's big enough to hold all tiles, in both 8x8 and 8x16 form.
			const size_t size_of_vram_in_pixels = CC_COUNT_OF(clownmdemu_state->vdp.vram) * 4;
			const size_t vram_texture_width_in_progress = (size_t)SDL_ceilf(SDL_sqrtf((float)size_of_vram_in_pixels));
			const size_t vram_texture_width_rounded_up_to_8 = (vram_texture_width_in_progress + (8 - 1)) / 8 * 8;
			const size_t vram_texture_height_in_progress = (size_of_vram_in_pixels + (vram_texture_width_rounded_up_to_8 - 1)) / vram_texture_width_rounded_up_to_8;
			const size_t vram_texture_height_rounded_up_to_16 = (vram_texture_height_in_progress + (16 - 1)) / 16 * 16;

			vram_texture_width = vram_texture_width_rounded_up_to_8;
			vram_texture_height = vram_texture_height_rounded_up_to_16;

			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
			vram_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (int)vram_texture_width, (int)vram_texture_height);

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
				const unsigned int length_of_palette_line = 16;
				const unsigned int length_of_palette = length_of_palette_line * 4;

				ImGui::Text("Brightness");
				ImGui::RadioButton("Normal", &brightness_index, length_of_palette * 0);
				ImGui::SameLine();
				ImGui::RadioButton("Shadow", &brightness_index, length_of_palette * 1);
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
			const Uint32 *selected_palette = &colours[brightness_index + palette_line];

			// Set up some variables that we're going to need soon.
			const size_t vram_texture_width_in_tiles = vram_texture_width / tile_width;
			const size_t vram_texture_height_in_tiles = vram_texture_height / tile_height;

			// Lock texture so that we can write into it.
			unsigned char *vram_texture_pixels;
			int vram_texture_pitch;

			if (SDL_LockTexture(vram_texture, NULL, (void**)&vram_texture_pixels, &vram_texture_pitch) == 0)
			{
				// Generate VRAM bitmap.
				const unsigned short *vram_pointer = clownmdemu_state->vdp.vram;

				// As an optimisation, the tiles are ordered top-to-bottom then left-to-right,
				// instead of left-to-right then top-to-bottom.
				for (size_t x = 0; x < vram_texture_width_in_tiles; ++x)
				{
					for (size_t y = 0; y < vram_texture_height_in_tiles * tile_height; ++y)
					{
						Uint32 *pixels_pointer = (Uint32*)(vram_texture_pixels + x * tile_width * sizeof(Uint32) + y * vram_texture_pitch);

						for (unsigned int i = 0; i < 2; ++i)
						{
							const unsigned short tile_row = *vram_pointer++;

							for (unsigned int j = 0; j < 4; ++j)
							{
								const unsigned int colour_index = ((tile_row << (4 * j)) & 0xF000) >> 12;
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
			const float dst_tile_scale = SDL_roundf(3.0f * dpi_scale);
			const ImVec2 dst_tile_size(tile_width * dst_tile_scale, tile_height * dst_tile_scale);
			const float spacing = SDL_roundf(5.0f * dpi_scale);

			// Calculate the size of the VRAM display region.
			ImVec2 vram_display_region = ImGui::GetContentRegionAvail();

			// Round down to the nearest multiple of the tile size + spacing, to simplify some calculations later on.
			vram_display_region.x -= SDL_fmodf(vram_display_region.x, dst_tile_size.x + spacing);

			// Extend the region down so that it's given a scroll bar.
			vram_display_region.y = SDL_ceilf((float)size_of_vram_in_tiles * (dst_tile_size.x + spacing) / vram_display_region.x) * (dst_tile_size.y + spacing);

			const ImVec2 canvas_position = ImGui::GetCursorScreenPos();

			// Goofy little trick to eliminate some duplicate code.
			struct FunctionHolder
			{
				static void TileIndexToTextureCoordinates(size_t tile_index, ImVec2 *uv0, ImVec2 *uv1, size_t vram_texture_height_in_tiles, size_t tile_width, size_t tile_height)
				{
					const size_t current_tile_src_x = (tile_index / vram_texture_height_in_tiles) * tile_width;
					const size_t current_tile_src_y = (tile_index % vram_texture_height_in_tiles) * tile_height;

					uv0->x = (float)current_tile_src_x / (float)(vram_texture_width);
					uv0->y = (float)current_tile_src_y / (float)(vram_texture_height);

					uv1->x = (float)(current_tile_src_x + tile_width) / (float)(vram_texture_width);
					uv1->y = (float)(current_tile_src_y + tile_height) / (float)(vram_texture_height);
				}
			};

			// This is used to detect the mouse.
			ImGui::InvisibleButton("VRAM canvas", vram_display_region);

			// When hovered over a tile, display info about it.
			if (ImGui::IsItemHovered())
			{
				const ImVec2 mouse_position = ImGui::GetMousePos();

				// Figure out which tile we're hovering over.
				size_t tile_index = 0;
				tile_index += (size_t)SDL_floorf((mouse_position.x - canvas_position.x) / (dst_tile_size.x + spacing));
				tile_index += (size_t)SDL_floorf((mouse_position.y - canvas_position.y) / (dst_tile_size.y + spacing)) * (size_t)(vram_display_region.x / (dst_tile_size.x + spacing));

				// Make sure it's a valid tile (the user could have their mouse *after* the last tile).
				if (tile_index < size_of_vram_in_tiles)
				{
					ImGui::BeginTooltip();

					// Obtain texture coordinates for the hovered tile.
					ImVec2 current_tile_uv0, current_tile_uv1;
					FunctionHolder::TileIndexToTextureCoordinates(tile_index, &current_tile_uv0, &current_tile_uv1, vram_texture_height_in_tiles, tile_width, tile_height);

					// Display the tile's index.
					ImGui::Text("%zd/0x%zX", tile_index, tile_index);

					// Display a zoomed-in version of the tile, so that the user can get a good look at it.
					ImGui::Image(vram_texture, ImVec2(dst_tile_size.x * 3.0f, dst_tile_size.y * 3.0f), current_tile_uv0, current_tile_uv1);

					ImGui::EndTooltip();
				}
			}

			// Draw the list of tiles.
			ImDrawList *draw_list = ImGui::GetWindowDrawList();

			for (size_t i = 0; i < size_of_vram_in_tiles; ++i)
			{
				// Obtain texture coordinates for the current tile.
				ImVec2 current_tile_uv0, current_tile_uv1;
				FunctionHolder::TileIndexToTextureCoordinates(i, &current_tile_uv0, &current_tile_uv1, vram_texture_height_in_tiles, tile_width, tile_height);

				// Figure out where the tile goes in the viewer.
				const float current_tile_dst_x = SDL_fmodf((float)i * (dst_tile_size.x + spacing), vram_display_region.x);
				const float current_tile_dst_y = SDL_floorf((float)i * (dst_tile_size.x + spacing) / vram_display_region.x) * (dst_tile_size.y + spacing);

				const ImVec2 tile_position(canvas_position.x + current_tile_dst_x + spacing * 0.5f, canvas_position.y + current_tile_dst_y + spacing * 0.5f);

				// Finally, display the tile.
				draw_list->AddImage(vram_texture, tile_position, ImVec2(tile_position.x + dst_tile_size.x, tile_position.y + dst_tile_size.y), current_tile_uv0, current_tile_uv1);
			}

			ImGui::EndChild();
		}
	}

	ImGui::End();
}

void Debug_CRAM(bool *open, Uint32 colours[16 * 4 * 3])
{
	if (ImGui::Begin("CRAM Viewer", open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const unsigned int length_of_palette_line = 16;
		const unsigned int length_of_palette = length_of_palette_line * 4;
		const unsigned int total_colours = length_of_palette * 3;

		for (unsigned int i = 0; i < total_colours; ++i)
		{
			ImGui::PushID(i);

			// Decompose the ARGB8888 colour into something that Dear Imgui can use.
			const float alpha = (float)((colours[i] >> (8 * 3)) & 0xFF) / (float)0xFF;
			const float red = (float)((colours[i] >> (8 * 2)) & 0xFF) / (float)0xFF;
			const float green = (float)((colours[i] >> (8 * 1)) & 0xFF) / (float)0xFF;
			const float blue = (float)((colours[i] >> (8 * 0)) & 0xFF) / (float)0xFF;

			if (i % length_of_palette_line != 0)
			{
				// Split the colours into palette lines.
				ImGui::SameLine();
			}
			else if (i % length_of_palette == 0)
			{
				// Display headers for each palette.
				switch (i / length_of_palette)
				{
					case 0:
						ImGui::Text("Normal");
						break;

					case 1:
						ImGui::Text("Shadow");
						break;

					case 2:
						ImGui::Text("Highlight");
						break;
				}
			}

			ImGui::ColorButton("", ImVec4(red, green, blue, alpha), ImGuiColorEditFlags_NoBorder, ImVec2(20.0f * dpi_scale, 20.0f * dpi_scale));

			ImGui::PopID();
		};
	}

	ImGui::End();
}


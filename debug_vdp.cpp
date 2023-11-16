#include "debug_vdp.h"

#include <stddef.h>

#include <functional>

#include "SDL.h"
#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#include "error.h"

typedef struct TileMetadata
{
	cc_u16f tile_index;
	cc_u16f palette_line;
	bool x_flip;
	bool y_flip;
	bool priority;
} TileMetadata;

void OpenFileDialog(char const* const title, const std::function<bool (const char *path)> callback);
void SaveFileDialog(char const* const title, const std::function<bool (const char *path)> callback);

static void DecomposeTileMetadata(cc_u16f packed_tile_metadata, TileMetadata *tile_metadata)
{
	tile_metadata->tile_index = packed_tile_metadata & 0x7FF;
	tile_metadata->palette_line = (packed_tile_metadata >> 13) & 3;
	tile_metadata->x_flip = (packed_tile_metadata & 0x800) != 0;
	tile_metadata->y_flip = (packed_tile_metadata & 0x1000) != 0;
	tile_metadata->priority = (packed_tile_metadata & 0x8000) != 0;
}

static void Debug_Plane(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data, const char *name, int &plane_scale, cc_u16l plane_address, SDL_Texture *&plane_texture)
{
	ImGui::SetNextWindowSize(ImVec2(1050, 610), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(name, open))
	{
		const cc_u16f plane_texture_width = 128 * 8; // 128 is the maximum plane size
		const cc_u16f plane_texture_height = 64 * 16;

		if (plane_texture == nullptr)
		{
			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
			plane_texture = SDL_CreateTexture(data->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, plane_texture_width, plane_texture_height);

			if (plane_texture == nullptr)
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

		if (plane_texture != nullptr)
		{
			ImGui::InputInt("Zoom", &plane_scale);
			if (plane_scale < 1)
				plane_scale = 1;

			if (ImGui::BeginChild("Plane View", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar))
			{
				static unsigned int cache_frame_counter;

				const cc_u16f tile_width = 8;
				const cc_u16f tile_height = clownmdemu->state->vdp.double_resolution_enabled ? 16 : 8;

				// Only update the texture if we know that the frame has changed.
				// This prevents constant texture generation even when the emulator is paused.
				if (cache_frame_counter != data->frame_counter)
				{
					cache_frame_counter = data->frame_counter;

					const cc_u8l *plane = &clownmdemu->state->vdp.vram[plane_address];

					// Lock texture so that we can write into it.
					Uint8 *plane_texture_pixels;
					int plane_texture_pitch;

					if (SDL_LockTexture(plane_texture, nullptr, (void**)&plane_texture_pixels, &plane_texture_pitch) == 0)
					{
						const cc_u8l *plane_pointer = plane;

						for (cc_u16f tile_y_in_plane = 0; tile_y_in_plane < clownmdemu->state->vdp.plane_height; ++tile_y_in_plane)
						{
							for (cc_u16f tile_x_in_plane = 0; tile_x_in_plane < clownmdemu->state->vdp.plane_width; ++tile_x_in_plane)
							{
								TileMetadata tile_metadata;
								DecomposeTileMetadata((plane_pointer[0] << 8) | plane_pointer[1], &tile_metadata);
								plane_pointer += 2;

								const cc_u16f x_flip_xor = tile_metadata.x_flip ? tile_width - 1 : 0;
								const cc_u16f y_flip_xor = tile_metadata.y_flip ? tile_height - 1 : 0;

								const cc_u16f palette_index = tile_metadata.palette_line * 16 + (clownmdemu->state->vdp.shadow_highlight_enabled && !tile_metadata.priority) * 16 * 4;

								const Uint32 *palette_line = &data->colours[palette_index];

								const cc_u8l *tile_pointer = &clownmdemu->state->vdp.vram[tile_metadata.tile_index * (tile_width * tile_height / 2)];

								for (cc_u16f pixel_y_in_tile = 0; pixel_y_in_tile < tile_height; ++pixel_y_in_tile)
								{
									Uint32 *plane_texture_pixels_row = (Uint32*)&plane_texture_pixels[tile_x_in_plane * tile_width * sizeof(*plane_texture_pixels_row) + (tile_y_in_plane * tile_height + (pixel_y_in_tile ^ y_flip_xor)) * plane_texture_pitch];

									for (cc_u16f i = 0; i < 2; ++i)
									{
										const cc_u16l tile_pixels = (tile_pointer[0] << 8) | tile_pointer[1];
										tile_pointer += 2;

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

					const cc_u8l *plane_pointer = &clownmdemu->state->vdp.vram[plane_address + (tile_y * clownmdemu->state->vdp.plane_width + tile_x) * 2];
					const cc_u8f packed_tile_metadata = (plane_pointer[0] << 8) | plane_pointer[1];

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

void Debug_WindowPlane(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data)
{
	static int scale;
	static SDL_Texture *texture;
	Debug_Plane(open, clownmdemu, data, "Window Plane", scale, clownmdemu->vdp.state->window_address, texture);
}

void Debug_PlaneA(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data)
{
	static int scale;
	static SDL_Texture *texture;
	Debug_Plane(open, clownmdemu, data, "Plane A", scale, clownmdemu->vdp.state->plane_a_address, texture);
}

void Debug_PlaneB(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data)
{
	static int scale;
	static SDL_Texture *texture;
	Debug_Plane(open, clownmdemu, data, "Plane B", scale, clownmdemu->vdp.state->plane_b_address, texture);
}

void Debug_VRAM(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data)
{
	// Variables relating to the sizing and spacing of the tiles in the viewer.
	const float tile_scale = SDL_roundf(3.0f * data->dpi_scale);
	const float tile_spacing = SDL_roundf(2.0f * data->dpi_scale);

	// Don't let the window become too small, or we can get division by zero errors later on.
	ImGui::SetNextWindowSizeConstraints(ImVec2(100.0f * data->dpi_scale, 100.0f * data->dpi_scale), ImVec2(FLT_MAX, FLT_MAX)); // Width > 100, Height > 100

	// Give the window a default size of 16 tiles wide.
	const float default_window_size = ((8 * tile_scale + tile_spacing * 2) * 0x10) + 40.0f * data->dpi_scale;
	ImGui::SetNextWindowSize(ImVec2(default_window_size, default_window_size), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("VRAM", open))
	{
		static SDL_Texture *vram_texture;
		static size_t vram_texture_width;
		static size_t vram_texture_height;

		const size_t tile_width = 8;
		const size_t tile_height = clownmdemu->state->vdp.double_resolution_enabled ? 16 : 8;

		const size_t size_of_vram_in_tiles = CC_COUNT_OF(clownmdemu->state->vdp.vram) * 2 / (tile_width * tile_height);

		// Create VRAM texture if it does not exist.
		if (vram_texture == nullptr)
		{
			// Create a square-ish texture that's big enough to hold all tiles, in both 8x8 and 8x16 form.
			const size_t size_of_vram_in_pixels = CC_COUNT_OF(clownmdemu->state->vdp.vram) * 2;
			const size_t vram_texture_width_in_progress = (size_t)SDL_ceilf(SDL_sqrtf((float)size_of_vram_in_pixels));
			const size_t vram_texture_width_rounded_up_to_8 = (vram_texture_width_in_progress + (8 - 1)) / 8 * 8;
			const size_t vram_texture_height_in_progress = (size_of_vram_in_pixels + (vram_texture_width_rounded_up_to_8 - 1)) / vram_texture_width_rounded_up_to_8;
			const size_t vram_texture_height_rounded_up_to_16 = (vram_texture_height_in_progress + (16 - 1)) / 16 * 16;

			vram_texture_width = vram_texture_width_rounded_up_to_8;
			vram_texture_height = vram_texture_height_rounded_up_to_16;

			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
			vram_texture = SDL_CreateTexture(data->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (int)vram_texture_width, (int)vram_texture_height);

			if (vram_texture == nullptr)
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


		if (ImGui::Button("Save to File"))
		{
			SaveFileDialog("Create Save State", [data, clownmdemu](const char* const save_state_path)
			{
				bool success = false;

				// Save the current state to the specified file.
				SDL_RWops *file = SDL_RWFromFile(save_state_path, "wb");

				if (file == nullptr)
				{
					PrintError("Could not open save state file for writing");
					SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not create VRAM file.", data->window);
				}
				else
				{
					if (SDL_RWwrite(file, clownmdemu->vdp.state->vram, sizeof(clownmdemu->vdp.state->vram), 1) != 1)
					{
						PrintError("Could not write save state file");
						SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not create VRAM file.", data->window);
					}
					else
					{
						success = true;
					}

					SDL_RWclose(file);
				}

				return success;
			});
		}

		if (vram_texture != nullptr)
		{
			bool options_changed = false;

			// Handle VRAM viewing options.
			static int brightness_index;
			static int palette_line;

			const int length_of_palette_line = 16;
			const int length_of_palette = length_of_palette_line * 4;

			ImGui::SeparatorText("Brightness");
			options_changed |= ImGui::RadioButton("Shadow", &brightness_index, length_of_palette * 1);
			ImGui::SameLine();
			options_changed |= ImGui::RadioButton("Normal", &brightness_index, length_of_palette * 0);
			ImGui::SameLine();
			options_changed |= ImGui::RadioButton("Highlight", &brightness_index, length_of_palette * 2);

			ImGui::SeparatorText("Palette Line");
			options_changed |= ImGui::RadioButton("0", &palette_line, length_of_palette_line * 0);
			ImGui::SameLine();
			options_changed |= ImGui::RadioButton("1", &palette_line, length_of_palette_line * 1);
			ImGui::SameLine();
			options_changed |= ImGui::RadioButton("2", &palette_line, length_of_palette_line * 2);
			ImGui::SameLine();
			options_changed |= ImGui::RadioButton("3", &palette_line, length_of_palette_line * 3);

			ImGui::SeparatorText("Tiles");

			// Set up some variables that we're going to need soon.
			const size_t vram_texture_width_in_tiles = vram_texture_width / tile_width;
			const size_t vram_texture_height_in_tiles = vram_texture_height / tile_height;

			static unsigned int cache_frame_counter;

			// Only update the texture if we know that the frame has changed.
			// This prevents constant texture generation even when the emulator is paused.
			if (cache_frame_counter != data->frame_counter || options_changed)
			{
				cache_frame_counter = data->frame_counter;

				// Select the correct palette line.
				const Uint32 *selected_palette = &data->colours[brightness_index + palette_line];

				// Lock texture so that we can write into it.
				Uint8 *vram_texture_pixels;
				int vram_texture_pitch;

				if (SDL_LockTexture(vram_texture, nullptr, (void**)&vram_texture_pixels, &vram_texture_pitch) == 0)
				{
					// Generate VRAM bitmap.
					const cc_u8l *vram_pointer = clownmdemu->state->vdp.vram;

					// As an optimisation, the tiles are ordered top-to-bottom then left-to-right,
					// instead of left-to-right then top-to-bottom.
					for (size_t x = 0; x < vram_texture_width_in_tiles; ++x)
					{
						for (size_t y = 0; y < vram_texture_height_in_tiles * tile_height; ++y)
						{
							Uint32 *pixels_pointer = (Uint32*)(vram_texture_pixels + x * tile_width * sizeof(Uint32) + y * vram_texture_pitch);

							for (cc_u16f i = 0; i < 2; ++i)
							{
								const cc_u16l tile_row = (vram_pointer[0] << 8) |  vram_pointer[1];
								vram_pointer += 2;

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
			const size_t vram_display_region_width_in_tiles = SDL_floorf(vram_display_region_width / dst_tile_size_and_padding.x);

			const ImVec2 canvas_position = ImGui::GetCursorScreenPos();
			const bool window_is_hovered = ImGui::IsWindowHovered();

			// Draw the list of tiles.
			ImDrawList *draw_list = ImGui::GetWindowDrawList();

			// Here we use a clipper so that we only render the tiles that we can actually see.
			ImGuiListClipper clipper;
			clipper.Begin(CC_DIVIDE_CEILING(size_of_vram_in_tiles, vram_display_region_width_in_tiles), dst_tile_size_and_padding.y);
			while (clipper.Step())
			{
				for (size_t y = (size_t)clipper.DisplayStart; y < (size_t)clipper.DisplayEnd; ++y)
				{
					for (size_t x = 0; x < CC_MIN(vram_display_region_width_in_tiles, size_of_vram_in_tiles - (y * vram_display_region_width_in_tiles)); ++x)
					{
						const size_t tile_index = (y * vram_display_region_width_in_tiles) + x;

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
						const float current_tile_dst_x = (float)x * dst_tile_size_and_padding.x;
						const float current_tile_dst_y = (float)y * dst_tile_size_and_padding.y;

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

		ImGui::SeparatorText("Brightness");
		ImGui::RadioButton("Shadow", &brightness, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Normal", &brightness, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Highlight", &brightness, 2);

		ImGui::SeparatorText("Colours");

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

void Debug_VDP(bool *open, const ClownMDEmu *clownmdemu, ImFont *monospace_font)
{
	if (ImGui::Begin("VDP Registers", open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const VDP_State* const vdp = &clownmdemu->state->vdp;

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
			ImGui::Text("0x%04" CC_PRIXLEAST16, vdp->sprite_table_address);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Window Plane Address");
			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%04" CC_PRIXLEAST16, vdp->window_address);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Plane A Address");
			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%04" CC_PRIXLEAST16, vdp->plane_a_address);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Plane B Address");
			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%04" CC_PRIXLEAST16, vdp->plane_b_address);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Horizontal Scroll Table Address");
			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%04" CC_PRIXLEAST16, vdp->hscroll_address);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Window Plane Horizontal Boundary");
			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIdLEAST8, vdp->window_horizontal_boundary);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Window Plane Horizontal Alignment");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(vdp->window_aligned_right ? "Right" : "Left");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Window Plane Vertical Boundary");
			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIdLEAST8, vdp->window_vertical_boundary);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Window Plane Vertical Alignment");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(vdp->window_aligned_bottom ? "Bottom" : "Top");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Plane Width");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIdLEAST16 " Tiles", vdp->plane_width);

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Plane Height");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIdLEAST16 " Tiles", vdp->plane_height);

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Display Enabled");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(vdp->display_enabled ? "Yes" : "No");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("V-Int Enabled");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(vdp->v_int_enabled ? "Yes" : "No");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("H-Int Enabled");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(vdp->h_int_enabled ? "Yes" : "No");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("H40 Enabled");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(vdp->h40_enabled ? "Yes" : "No");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("V30 Enabled");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(vdp->v30_enabled ? "Yes" : "No");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Shadow/Highlight Enabled");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(vdp->shadow_highlight_enabled ? "Yes" : "No");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Double-Resolution Enabled");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(vdp->double_resolution_enabled ? "Yes" : "No");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Background Colour");
			ImGui::TableNextColumn();
			ImGui::Text("Palette Line %" CC_PRIdLEAST8 ", Entry %" CC_PRIdLEAST8, (cc_u8f)vdp->background_colour / 16, (cc_u8f)vdp->background_colour % 16);

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("H-Int Interval");
			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIdLEAST8, vdp->h_int_interval);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Horizontal Scrolling Mode");
			ImGui::TableNextColumn();
			const char* const horizontal_scrolling_modes[3] = {
				"Whole Screen",
				"1-Tile Rows",
				"1-Pixel Rows"
			};
			ImGui::TextUnformatted(horizontal_scrolling_modes[vdp->hscroll_mode]);

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Vertical Scrolling Mode");
			ImGui::TableNextColumn();
			const char* const vertical_scrolling_modes[2] = {
				"Whole Screen",
				"2-Tile Columns"
			};
			ImGui::TextUnformatted(vertical_scrolling_modes[vdp->vscroll_mode]);

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
			ImGui::TextUnformatted(vdp->dma.enabled ? "Yes" : "No");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Pending");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(vdp->dma.pending ? "Yes" : "No");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Mode");
			ImGui::TableNextColumn();
			static const char* const dma_modes[] = {
				"ROM/RAM to VRAM/CRAM/VSRAM",
				"VRAM Fill",
				"VRAM to VRAM"
			};
			ImGui::TextUnformatted(dma_modes[vdp->dma.mode]);

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Source Address");
			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%06" CC_PRIXFAST32, ((cc_u32f)vdp->dma.source_address_high << 16) | vdp->dma.source_address_low);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Length");
			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%04" CC_PRIXLEAST16, vdp->dma.length);
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
			ImGui::TextUnformatted(vdp->access.write_pending ? "Yes" : "No");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Cached Write");
			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%04" CC_PRIXLEAST16, vdp->access.cached_write);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Selected RAM");
			ImGui::TableNextColumn();
			static const char* const rams[] = {
				"VRAM",
				"CRAM",
				"VSRAM"
			};
			ImGui::TextUnformatted(rams[vdp->access.selected_buffer]);

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Mode");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(vdp->access.read_mode ? "Read" : "Write");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Index");
			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%04" CC_PRIXLEAST16, vdp->access.index);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Increment");
			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%04" CC_PRIXLEAST16, vdp->access.increment);
			ImGui::PopFont();

			ImGui::EndTable();
		}
	}

	ImGui::End();
}

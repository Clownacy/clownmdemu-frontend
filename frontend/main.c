#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "SDL.h"

#include "clownmdemu.h"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *framebuffer_texture;
static SDL_PixelFormatEnum framebuffer_texture_format;
static unsigned int framebuffer_texture_bytes_per_pixel;
static void *framebuffer_texture_pixels;
static int framebuffer_texture_pitch;

static unsigned char *colours;

static unsigned int current_screen_width;
static unsigned int current_screen_height;

static struct
{
	bool up,down,left,right;
	bool a,b,c;
	bool start;
} inputs[2];

static void PrintError(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fputs("Error: ", stderr);
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
//	SDL_LogMessageV(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR, fmt, args);
	va_end(args);
}

// https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetKernighan
static unsigned int CountSetBits(unsigned int bits)
{
	unsigned int total_bits_set = 0;

	while (bits != 0)
	{
		bits &= bits - 1; // clear the least significant bit set

		++total_bits_set;
	}

	return total_bits_set;
}

static void LoadFileToBuffer(const char *filename, unsigned char **file_buffer, size_t *file_size)
{
	*file_buffer = NULL;

	FILE *file = fopen(filename, "rb");

	if (file == NULL)
	{
		PrintError("Could not open file");
	}
	else
	{
		fseek(file, 0, SEEK_END);
		*file_size = ftell(file);
		rewind(file);
		*file_buffer = malloc(*file_size);

		if (*file_buffer == NULL)
		{
			PrintError("Could not allocate memory for file");
		}
		else
		{
			fread(*file_buffer, 1, *file_size, file);
		}

		fclose(file);
	}
}

static void ColourUpdatedCallback(unsigned int index, unsigned int colour)
{
	// Convert colour to native pixel format
	const Uint16 colour_uint16 = (Uint16)colour;
	SDL_ConvertPixels(1, 1, SDL_PIXELFORMAT_XBGR4444, &colour_uint16, SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_XBGR4444), framebuffer_texture_format, &colours[index * framebuffer_texture_bytes_per_pixel], framebuffer_texture_bytes_per_pixel);
}

static void ScanlineRenderedCallback(unsigned int scanline, const unsigned char *pixels, unsigned int screen_width, unsigned int screen_height)
{
	// Use the indexed pixels in the scanline buffer to copy native-format colours to our locked framebuffer texture's pixel buffer
	for (unsigned int i = 0; i < screen_width; ++i)
		memcpy(&((unsigned char*)framebuffer_texture_pixels)[scanline * framebuffer_texture_pitch + i * framebuffer_texture_bytes_per_pixel], &colours[pixels[i] * framebuffer_texture_bytes_per_pixel], framebuffer_texture_bytes_per_pixel);

	current_screen_width = screen_width;
	current_screen_height = screen_height;
}

static unsigned char ReadInputCallback(unsigned int player_id, unsigned int button_id)
{
	assert(player_id < 2);

	switch (button_id)
	{
		case CLOWNMDEMU_BUTTON_UP:
			return inputs[player_id].up;

		case CLOWNMDEMU_BUTTON_DOWN:
			return inputs[player_id].down;

		case CLOWNMDEMU_BUTTON_LEFT:
			return inputs[player_id].left;

		case CLOWNMDEMU_BUTTON_RIGHT:
			return inputs[player_id].right;

		case CLOWNMDEMU_BUTTON_A:
			return inputs[player_id].a;

		case CLOWNMDEMU_BUTTON_B:
			return inputs[player_id].b;

		case CLOWNMDEMU_BUTTON_C:
			return inputs[player_id].c;

		case CLOWNMDEMU_BUTTON_START:
			return inputs[player_id].start;

		default:
			return false;
	}
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		PrintError("Must provide path to input ROM as a parameter");
	}
	else
	{
		if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
		{
			PrintError("SDL_Init failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			window = SDL_CreateWindow("clownmdemufrontend", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320 * 2, 224 * 2, SDL_WINDOW_RESIZABLE);

			if (window == NULL)
			{
				PrintError("SDL_CreateWindow failed with the following message - '%s'", SDL_GetError());
			}
			else
			{
				bool use_vsync = false;

				SDL_DisplayMode display_mode;
				if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &display_mode) == 0)
					use_vsync = display_mode.refresh_rate >= 60;

				renderer = SDL_CreateRenderer(window, -1, use_vsync ? SDL_RENDERER_PRESENTVSYNC : 0);

				if (renderer == NULL)
				{
					PrintError("SDL_CreateRenderer failed with the following message - '%s'", SDL_GetError());
				}
				else
				{
					framebuffer_texture_format = SDL_PIXELFORMAT_XBGR4444; // Fallback value

					// Get list of 'native' texture formats
					SDL_RendererInfo renderer_info;
					if (SDL_GetRendererInfo(renderer, &renderer_info) < 0)
					{
						PrintError("SDL_GetRendererInfo failed with the following message - '%s'", SDL_GetError());
					}
					else
					{
						// Find the smallest texture format that supports at least four bits per pixel
						unsigned int lowest_bits_per_pixel_so_far = -1;

						for (Uint32 i = 0; i < renderer_info.num_texture_formats; ++i)
						{
							const SDL_PixelFormatEnum renderer_pixel_format = renderer_info.texture_formats[i];

							int bits_per_pixel;
							Uint32 red_mask, green_mask, blue_mask, alpha_mask;
							if (SDL_PixelFormatEnumToMasks(renderer_pixel_format, &bits_per_pixel, &red_mask, &green_mask, &blue_mask, &alpha_mask))
							{
								if (CountSetBits(red_mask) >= 4 && CountSetBits(green_mask) >= 4 && CountSetBits(blue_mask) >= 4 && SDL_BITSPERPIXEL(renderer_pixel_format) < lowest_bits_per_pixel_so_far)
								{
									lowest_bits_per_pixel_so_far = SDL_BITSPERPIXEL(renderer_pixel_format);
									framebuffer_texture_format = renderer_pixel_format;
								}
							}
						}
					}

					// We'll need this for later
					framebuffer_texture_bytes_per_pixel = SDL_BYTESPERPIXEL(framebuffer_texture_format);

					// Allocate our native-format colour buffer
					colours = malloc(3 * 4 * 16 * framebuffer_texture_bytes_per_pixel); // Three brightnesses, four palette lines, sixteen colours

					if (colours == NULL)
					{
						PrintError("Could not allocate memory for the colour buffer");
					}
					else
					{
						framebuffer_texture = SDL_CreateTexture(renderer, framebuffer_texture_format, SDL_TEXTUREACCESS_STREAMING, 320, 480);

						if (framebuffer_texture == NULL)
						{
							PrintError("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
						}
						else
						{
							// Lock the texture so its pixels are available for writing to later on
							SDL_LockTexture(framebuffer_texture, NULL, &framebuffer_texture_pixels, &framebuffer_texture_pitch);

							const size_t clownmdemu_state_size = ClownMDEmu_GetStateSize();
							unsigned char *clownmdemu_state = malloc(clownmdemu_state_size * 2); // *2 because we're allocating room for a save state

							if (clownmdemu_state == NULL)
							{
								PrintError("Could not allocate memory for the ClownMDEmu state");
							}
							else
							{
								ClownMDEmu_Init(clownmdemu_state);

								// For now, let's emulate a North American console
								ClownMDEmu_SetJapanese(clownmdemu_state, false);
								ClownMDEmu_SetPAL(clownmdemu_state, false);

								unsigned char *file_buffer;
								size_t file_size;
								LoadFileToBuffer(argv[1], &file_buffer, &file_size);

								if (file_buffer == NULL)
								{
									PrintError("Could not load the ROM");
								}
								else
								{
									ClownMDEmu_UpdateROM(clownmdemu_state, file_buffer, file_size);
									free(file_buffer);

									ClownMDEmu_Reset(clownmdemu_state);

									// Initialise save state
									memcpy(clownmdemu_state + clownmdemu_state_size, clownmdemu_state, clownmdemu_state_size);

									bool quit = false;

									bool fast_forward = false;
									unsigned int frameskip_counter = 0;

									while (!quit)
									{
										// Process events
										SDL_Event event;
										while (SDL_PollEvent(&event))
										{
											switch (event.type)
											{
												case SDL_QUIT:
													quit = true;
													break;

												case SDL_KEYDOWN:
												case SDL_KEYUP:
												{
													bool pressed = event.type == SDL_KEYDOWN;

													switch (event.key.keysym.scancode)
													{
														#define DO_KEY(state, code) case code: state = pressed; break;

														DO_KEY(inputs[0].up,    SDL_SCANCODE_W)
														DO_KEY(inputs[0].down,  SDL_SCANCODE_S)
														DO_KEY(inputs[0].left,  SDL_SCANCODE_A)
														DO_KEY(inputs[0].right, SDL_SCANCODE_D)
														DO_KEY(inputs[0].a,     SDL_SCANCODE_O)
														DO_KEY(inputs[0].b,     SDL_SCANCODE_P)
														DO_KEY(inputs[0].c,     SDL_SCANCODE_LEFTBRACKET)
														DO_KEY(inputs[0].start, SDL_SCANCODE_RETURN)

														DO_KEY(inputs[1].up,    SDL_SCANCODE_UP)
														DO_KEY(inputs[1].down,  SDL_SCANCODE_DOWN)
														DO_KEY(inputs[1].left,  SDL_SCANCODE_LEFT)
														DO_KEY(inputs[1].right, SDL_SCANCODE_RIGHT)
														DO_KEY(inputs[1].a,     SDL_SCANCODE_Z)
														DO_KEY(inputs[1].b,     SDL_SCANCODE_X)
														DO_KEY(inputs[1].c,     SDL_SCANCODE_C)
														DO_KEY(inputs[1].start, SDL_SCANCODE_V)

														DO_KEY(fast_forward,    SDL_SCANCODE_SPACE)

														#undef DO_KEY

														case SDL_SCANCODE_TAB:
															// Soft-reset console
															if (pressed)
																ClownMDEmu_Reset(clownmdemu_state);

															break;

														case SDL_SCANCODE_F5:
															// Save save state
															if (pressed)
																memcpy(clownmdemu_state + clownmdemu_state_size, clownmdemu_state, clownmdemu_state_size);

															break;

														case SDL_SCANCODE_F9:
															// Load save state
															if (pressed)
																memcpy(clownmdemu_state, clownmdemu_state + clownmdemu_state_size, clownmdemu_state_size);

															break;

														default:
															break;
													}

													break;
												}
											}
										}

										// Run the emulator for a frame
										ClownMDEmu_Iterate(clownmdemu_state, ColourUpdatedCallback, ScanlineRenderedCallback, ReadInputCallback);

										// Don't bother rendering if we're frame-skipping
										if (!fast_forward || (++frameskip_counter & 3) == 0)
										{
											// Correct the aspect ratio of the rendered frame
											// (256x224 and 320x240 should be the same width, but 320x224 and 320x240 should be different heights - this matches the behaviour of a real Mega Drive)
											int renderer_width, renderer_height;
											SDL_GetRendererOutputSize(renderer, &renderer_width, &renderer_height);

											SDL_Rect destination_rect;

											if ((unsigned int)renderer_width > renderer_height * 320 / current_screen_height)
											{
												destination_rect.w = renderer_height * 320 / current_screen_height;
												destination_rect.h = renderer_height;
											}
											else
											{
												destination_rect.w = renderer_width;
												destination_rect.h = renderer_width * current_screen_height / 320;
											}

											destination_rect.x = (renderer_width - destination_rect.w) / 2;
											destination_rect.y = (renderer_height - destination_rect.h) / 2;

											// Unlock the framebuffer texture so we can draw it
											SDL_UnlockTexture(framebuffer_texture);

											// Draw the rendered frame to the screen
											SDL_RenderClear(renderer);
											SDL_RenderCopy(renderer, framebuffer_texture, &(SDL_Rect){.x = 0, .y = 0, .w = current_screen_width, .h = current_screen_height}, &destination_rect);
											SDL_RenderPresent(renderer);

											// Lock the framebuffer texture again so we can write to its pixels later
											SDL_LockTexture(framebuffer_texture, NULL, &framebuffer_texture_pixels, &framebuffer_texture_pitch);

											// Framerate manager - run at roughly 60FPS
											static Uint32 next_time;
											const Uint32 current_time = SDL_GetTicks();

											if (!SDL_TICKS_PASSED(current_time, next_time))
												SDL_Delay(next_time - current_time);
											else if (SDL_TICKS_PASSED(current_time, next_time + 100)) // If we're way past our deadline, then resync to the current tick instead of a fast-forwarding
												next_time = current_time;

											next_time += 1000 / 60;
										}
									}
								}

								FILE *state_file = fopen("state.bin", "wb");

								if (state_file != NULL)
								{
									fwrite(clownmdemu_state, 1, clownmdemu_state_size, state_file);

									fclose(state_file);
								}

								ClownMDEmu_Deinit(clownmdemu_state);

								free(clownmdemu_state);
							}

							SDL_DestroyTexture(framebuffer_texture);
						}

						free(colours);
					}

					SDL_DestroyRenderer(renderer);
				}

				SDL_DestroyWindow(window);
			}

			SDL_Quit();
		}
	}

	return EXIT_SUCCESS;
}

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "SDL.h"

#include "../clownmdemu.h"

// The NTSC framerate is 59.94FPS (60 divided by 1.001)
#define DIVIDE_BY_NTSC_FRAMERATE(x) ((x) * 1001ul / ((60ul * 1001ul * 1000ul) / 1001ul)) // 1001 being 1.001 multiplied by 1000

// The PAL framerate is 50FPS
#define DIVIDE_BY_PAL_FRAMERATE(x) ((x) / 50)

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *framebuffer_texture;
static Uint32 *framebuffer_texture_pixels;
static int framebuffer_texture_pitch;

static Uint32 colours[3 * 4 * 16];

static unsigned int current_screen_width;
static unsigned int current_screen_height;

static bool inputs[2][CLOWNMDEMU_BUTTON_MAX];

static ClownMDEmu_State clownmdemu_state;
static ClownMDEmu_State clownmdemu_save_state;

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
	// Decompose XBGR4444 into individual colour channels
	const Uint32 red = (colour >> 4 * 0) & 0xF;
	const Uint32 green = (colour >> 4 * 1) & 0xF;
	const Uint32 blue = (colour >> 4 * 2) & 0xF;

	// Reassemble into XBGR8888
	colours[index] = (red << 4 * 0) | (red << 4 * 1) | (green << 4 * 2) | (green << 4 * 3) | (blue << 4 * 4) | (blue << 4 * 5);
}

static void ScanlineRenderedCallback(unsigned int scanline, const unsigned char *pixels, unsigned int screen_width, unsigned int screen_height)
{
	if (framebuffer_texture_pixels != NULL)
		for (unsigned int i = 0; i < screen_width; ++i)
			framebuffer_texture_pixels[scanline * framebuffer_texture_pitch + i] = colours[pixels[i]];

	current_screen_width = screen_width;
	current_screen_height = screen_height;
}

static unsigned char ReadInputCallback(unsigned int player_id, unsigned int button_id)
{
	assert(player_id < 2);

	return inputs[player_id][button_id];
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		PrintError("Must provide path to input ROM as a parameter");
	}
	else
	{
		// Initialise SDL2
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
		{
			PrintError("SDL_Init failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			// Create window
			window = SDL_CreateWindow("clownmdemufrontend", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320 * 2, 224 * 2, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

			if (window == NULL)
			{
				PrintError("SDL_CreateWindow failed with the following message - '%s'", SDL_GetError());
			}
			else
			{
				// Figure out if we should use V-sync or not
				bool use_vsync = false;

				SDL_DisplayMode display_mode;
				if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &display_mode) == 0)
					use_vsync = display_mode.refresh_rate >= 60;

				// Create renderer
				renderer = SDL_CreateRenderer(window, -1, use_vsync ? SDL_RENDERER_PRESENTVSYNC : 0);

				if (renderer == NULL)
				{
					PrintError("SDL_CreateRenderer failed with the following message - '%s'", SDL_GetError());
				}
				else
				{
					// Create framebuffer texture
					// We're using XBGR8888 because it's more likely to be supported natively by the GPU, avoiding the need for constant conversions
					framebuffer_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_STREAMING, 320, 480);

					if (framebuffer_texture == NULL)
					{
						PrintError("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
					}
					else
					{
						// Lock the texture so that we can write to its pixels later
						if (SDL_LockTexture(framebuffer_texture, NULL, (void*)&framebuffer_texture_pixels, &framebuffer_texture_pitch) < 0)
							framebuffer_texture_pixels = NULL;

						framebuffer_texture_pitch /= sizeof(Uint32);

						ClownMDEmu_Init(&clownmdemu_state);

						// For now, let's emulate a North American console
						ClownMDEmu_SetJapanese(&clownmdemu_state, false);
						ClownMDEmu_SetPAL(&clownmdemu_state, false);

						// Load ROM to memory
						unsigned char *file_buffer;
						size_t file_size;
						LoadFileToBuffer(argv[1], &file_buffer, &file_size);

						if (file_buffer == NULL)
						{
							PrintError("Could not load the ROM");
						}
						else
						{
							ClownMDEmu_UpdateROM(&clownmdemu_state, file_buffer, file_size);
							free(file_buffer);

							ClownMDEmu_Reset(&clownmdemu_state);

							// Initialise save state
							clownmdemu_save_state = clownmdemu_state;

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
											// Ignore repeated key inputs caused by holding the key down
											if (event.key.repeat)
												break;

											bool pressed = event.type == SDL_KEYDOWN;

											switch (event.key.keysym.scancode)
											{
												#define DO_KEY(state, code) case code: state = pressed; break;

												DO_KEY(inputs[0][CLOWNMDEMU_BUTTON_UP],    SDL_SCANCODE_W)
												DO_KEY(inputs[0][CLOWNMDEMU_BUTTON_DOWN],  SDL_SCANCODE_S)
												DO_KEY(inputs[0][CLOWNMDEMU_BUTTON_LEFT],  SDL_SCANCODE_A)
												DO_KEY(inputs[0][CLOWNMDEMU_BUTTON_RIGHT], SDL_SCANCODE_D)
												DO_KEY(inputs[0][CLOWNMDEMU_BUTTON_A],     SDL_SCANCODE_O)
												DO_KEY(inputs[0][CLOWNMDEMU_BUTTON_B],     SDL_SCANCODE_P)
												DO_KEY(inputs[0][CLOWNMDEMU_BUTTON_C],     SDL_SCANCODE_LEFTBRACKET)
												DO_KEY(inputs[0][CLOWNMDEMU_BUTTON_START], SDL_SCANCODE_RETURN)

												DO_KEY(inputs[1][CLOWNMDEMU_BUTTON_UP],    SDL_SCANCODE_UP)
												DO_KEY(inputs[1][CLOWNMDEMU_BUTTON_DOWN],  SDL_SCANCODE_DOWN)
												DO_KEY(inputs[1][CLOWNMDEMU_BUTTON_LEFT],  SDL_SCANCODE_LEFT)
												DO_KEY(inputs[1][CLOWNMDEMU_BUTTON_RIGHT], SDL_SCANCODE_RIGHT)
												DO_KEY(inputs[1][CLOWNMDEMU_BUTTON_A],     SDL_SCANCODE_Z)
												DO_KEY(inputs[1][CLOWNMDEMU_BUTTON_B],     SDL_SCANCODE_X)
												DO_KEY(inputs[1][CLOWNMDEMU_BUTTON_C],     SDL_SCANCODE_C)
												DO_KEY(inputs[1][CLOWNMDEMU_BUTTON_START], SDL_SCANCODE_V)

												DO_KEY(fast_forward,    SDL_SCANCODE_SPACE)

												#undef DO_KEY

												case SDL_SCANCODE_TAB:
													// Soft-reset console
													if (pressed)
														ClownMDEmu_Reset(&clownmdemu_state);

													break;

												case SDL_SCANCODE_F5:
													// Save save state
													if (pressed)
														clownmdemu_save_state = clownmdemu_state;

													break;

												case SDL_SCANCODE_F9:
													// Load save state
													if (pressed)
														clownmdemu_state = clownmdemu_save_state;

													break;

												default:
													break;
											}

											break;
										}
									}
								}

								// Run the emulator for a frame
								ClownMDEmu_Iterate(&clownmdemu_state, ColourUpdatedCallback, ScanlineRenderedCallback, ReadInputCallback);

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

									// Unlock the texture so that we can draw it
									SDL_UnlockTexture(framebuffer_texture);

									// Draw the rendered frame to the screen
									SDL_RenderClear(renderer);
									SDL_RenderCopy(renderer, framebuffer_texture, &(SDL_Rect){.x = 0, .y = 0, .w = current_screen_width, .h = current_screen_height}, &destination_rect);
									SDL_RenderPresent(renderer);

									// Lock the texture so that we can write to its pixels later
									if (SDL_LockTexture(framebuffer_texture, NULL, (void*)&framebuffer_texture_pixels, &framebuffer_texture_pitch) < 0)
										framebuffer_texture_pixels = NULL;

									framebuffer_texture_pitch /= sizeof(Uint32);

									// Framerate manager - run at roughly 59.94FPS (60 divided by 1.001)
									static Uint32 next_time;
									const Uint32 current_time = SDL_GetTicks() * 300;

									if (!SDL_TICKS_PASSED(current_time, next_time))
										SDL_Delay((next_time - current_time) / 300);
									else if (SDL_TICKS_PASSED(current_time, next_time + 100 * 300)) // If we're way past our deadline, then resync to the current tick instead of fast-forwarding
										next_time = current_time;

									// 300 is the magic number that prevents these calculations from ever dipping into numbers smaller than 1
									next_time += DIVIDE_BY_NTSC_FRAMERATE(1000ul * 300ul);
								}
							}
						}

						FILE *state_file = fopen("state.bin", "wb");

						if (state_file != NULL)
						{
							fwrite(&clownmdemu_state, 1, sizeof(clownmdemu_state), state_file);

							fclose(state_file);
						}

						ClownMDEmu_Deinit(&clownmdemu_state);

						SDL_DestroyTexture(framebuffer_texture);
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

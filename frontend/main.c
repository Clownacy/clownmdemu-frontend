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

static unsigned char framebuffer[480][320][3];

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

static void ScanlineRenderedCallback(unsigned int scanline, void *pixels, unsigned int screen_width, unsigned int screen_height)
{
	memcpy(framebuffer[scanline], pixels, screen_width * 3);

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
					framebuffer_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 320, 480);

					if (framebuffer_texture == NULL)
					{
						PrintError("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
					}
					else
					{
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
									ClownMDEmu_Iterate(clownmdemu_state, ScanlineRenderedCallback, ReadInputCallback);

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

										// Upload framebuffer to texture
										void *texture_pixels;
										int texture_pitch;

										if (SDL_LockTexture(framebuffer_texture, &(SDL_Rect){.x = 0, .y = 0, .w = current_screen_width, .h = current_screen_height}, &texture_pixels, &texture_pitch) < 0)
										{
											PrintError("SDL_LockTexture failed with the following message - '%s'", SDL_GetError());
										}
										else
										{
											for (unsigned int i = 0; i < current_screen_height; ++i)
												memcpy((unsigned char*)texture_pixels + i * texture_pitch, framebuffer[i], current_screen_width * 3);

											SDL_UnlockTexture(framebuffer_texture);
										}

										// Draw the rendered frame to the screen
										SDL_RenderClear(renderer);
										SDL_RenderCopy(renderer, framebuffer_texture, &(SDL_Rect){.x = 0, .y = 0, .w = current_screen_width, .h = current_screen_height}, &destination_rect);
										SDL_RenderPresent(renderer);

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

					SDL_DestroyRenderer(renderer);
				}

				SDL_DestroyWindow(window);
			}

			SDL_Quit();
		}
	}

	return EXIT_SUCCESS;
}

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "SDL.h"

#include "clownmdemu.h"

static SDL_Window *window;

static struct
{
	bool up,down,left,right;
	bool a,b,c;
	bool start;
} input;

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

static void ScanlineRenderedCallback(unsigned short scanline, void *pixels, unsigned short screen_width, unsigned short screen_height)
{
	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, screen_width, 1, 0, screen_width * 3, SDL_PIXELFORMAT_RGB24);

	if (surface != NULL)
	{
		SDL_Surface *window_surface = SDL_GetWindowSurface(window);

		if (window_surface != NULL)
		{
			SDL_Rect destination_rect = {.x = 0, .y = scanline * window_surface->h / screen_height, .w = window_surface->w, .h = window_surface->h / screen_height};
			SDL_BlitScaled(surface, NULL, window_surface, &destination_rect);
		}

		SDL_FreeSurface(surface);
	}
}

static unsigned char ReadInputCallback(unsigned char button_id)
{
	switch (button_id)
	{
		case CLOWNMDEMU_BUTTON_UP:
			return input.up;

		case CLOWNMDEMU_BUTTON_DOWN:
			return input.down;

		case CLOWNMDEMU_BUTTON_LEFT:
			return input.left;

		case CLOWNMDEMU_BUTTON_RIGHT:
			return input.right;

		case CLOWNMDEMU_BUTTON_A:
			return input.a;

		case CLOWNMDEMU_BUTTON_B:
			return input.b;

		case CLOWNMDEMU_BUTTON_C:
			return input.c;

		case CLOWNMDEMU_BUTTON_START:
			return input.start;

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
				void *clownmdemu_state = malloc(ClownMDEmu_GetStateSize());

				if (clownmdemu_state == NULL)
				{
					PrintError("Could not allocate memory for the ClownMDEmu state");
				}
				else
				{
					ClownMDEmu_Init(clownmdemu_state);

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

						bool quit = false;

						while (!quit)
						{
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
									bool pressed = event.type == SDL_KEYDOWN;

									switch (event.key.keysym.scancode)
									{
										#define DO_KEY(state, code) case code: state = pressed; break;
										DO_KEY(input.up,    SDL_SCANCODE_W)
										DO_KEY(input.down,  SDL_SCANCODE_S)
										DO_KEY(input.left,  SDL_SCANCODE_A)
										DO_KEY(input.right, SDL_SCANCODE_D)
										DO_KEY(input.a,     SDL_SCANCODE_O)
										DO_KEY(input.b,     SDL_SCANCODE_P)
										DO_KEY(input.c,     SDL_SCANCODE_LEFTBRACKET)
										DO_KEY(input.start, SDL_SCANCODE_RETURN)
										#undef DO_KEY

									default:
										break;
									}

									break;
								}
							}

							ClownMDEmu_Iterate(clownmdemu_state, ScanlineRenderedCallback, ReadInputCallback);

							SDL_UpdateWindowSurface(window);

							// Framerate manager - run at roughly 60FPS
							static Uint32 next_time;
							const Uint32 current_time = SDL_GetTicks();

							if (current_time < next_time)
								SDL_Delay(next_time - current_time);

							next_time = SDL_GetTicks() + 1000 / 60;
						}
					}

					FILE *state_file = fopen("state.bin", "wb");

					if (state_file != NULL)
					{
						fwrite(clownmdemu_state, 1, ClownMDEmu_GetStateSize(), state_file);

						fclose(state_file);
					}

					ClownMDEmu_Deinit(clownmdemu_state);

					free(clownmdemu_state);
				}

				SDL_DestroyWindow(window);
			}

			SDL_Quit();
		}
	}

	return EXIT_SUCCESS;
}

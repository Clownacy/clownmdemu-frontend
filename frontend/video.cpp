#include "video.h"

#include <stddef.h>

#include "SDL.h"

#include "error.h"

SDL_Window *window;
SDL_Renderer *renderer;
float dpi_scale;

bool InitVideo(void)
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		PrintError("SDL_InitSubSystem(SDL_INIT_VIDEO) failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		// Create window
		window = SDL_CreateWindow("clownmdemufrontend", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320 * 2, 224 * 2, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN);

		if (window == NULL)
		{
			PrintError("SDL_CreateWindow failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			// Create renderer
			renderer = SDL_CreateRenderer(window, -1, 0);

			if (renderer == NULL)
			{
				PrintError("SDL_CreateRenderer failed with the following message - '%s'", SDL_GetError());
			}
			else
			{
				// Get DPI scale
				dpi_scale = 1.0f;

				float ddpi;
				if (SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(window), &ddpi, NULL, NULL) == 0)
					dpi_scale = ddpi / 96.0f; // 96 DPI appears to be the "normal" DPI

				return true;
			}

			SDL_DestroyWindow(window);
		}

		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

	return false;
}

void DeinitVideo(void)
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

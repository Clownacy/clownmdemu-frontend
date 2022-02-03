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
			// Use batching even if the user forces a specific rendering backend (wtf SDL).
			//
			// Personally, I like to force SDL2 to use D3D11 instead of D3D9 by setting an environment
			// variable, but it turns out that, if I do that, then SDL2 will disable its render batching
			// for backwards compatibility reasons. Setting this hint prevents that.
			//
			// Normally if a user is setting an environment variable to force a specific rendering
			// backend, then they're expected to set another environment variable to set this hint too,
			// but I might as well just do it myself and save everyone else the hassle.
			SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

			// Create renderer
			renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE);

			if (renderer == NULL)
			{
				PrintError("SDL_CreateRenderer failed with the following message - '%s'", SDL_GetError());
			}
			else
			{
				dpi_scale = GetNewDPIScale();

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

float GetNewDPIScale(void)
{
	float dpi_scale = 1.0f;

	float ddpi;
	if (SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(window), &ddpi, NULL, NULL) == 0)
		dpi_scale = ddpi / 96.0f;

	return dpi_scale;
}
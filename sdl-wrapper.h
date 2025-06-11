#ifndef SDL_WRAPPER_H
#define SDL_WRAPPER_H

#include "debug-log.h"
#include "sdl-wrapper-inner.h"

namespace SDL
{
	using Pixel = Uint32;

	inline Texture CreateTexture(Renderer &renderer, const SDL_TextureAccess access, const int width, const int height, const SDL_ScaleMode scale_mode, const bool blending = false)
	{
		// We're using ARGB8888 because it's more likely to be supported natively by the GPU, avoiding the need for constant conversions
		Texture texture(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, access, width, height));

		if (!texture)
		{
			Frontend::debug_log.Log("SDL_CreateTexture failed with the following message - '{}'", SDL_GetError());
		}
		else
		{
			// Disable blending, since we don't need it
			if (!SDL_SetTextureBlendMode(texture, blending ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE))
				Frontend::debug_log.Log("SDL_SetTextureBlendMode failed with the following message - '{}'", SDL_GetError());

			if (!SDL_SetTextureScaleMode(texture, scale_mode))
				Frontend::debug_log.Log("SDL_SetTextureScaleMode failed with the following message - '{}'", SDL_GetError());
		}

		return texture;
	}

	inline Texture CreateTextureWithBlending(Renderer &renderer, const SDL_TextureAccess access, const int width, const int height, const SDL_ScaleMode scale_mode)
	{
		return CreateTexture(renderer, access, width, height, scale_mode, true);
	}
}

#endif /* SDL_WRAPPER_H */

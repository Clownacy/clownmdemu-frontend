#ifndef SDL_WRAPPER_EXTRA_H
#define SDL_WRAPPER_EXTRA_H

#include "debug-log.h"
#include "sdl-wrapper.h"

namespace SDL
{
	using Pixel = Uint32;
	constexpr SDL_PixelFormat pixel_format = SDL_PIXELFORMAT_ARGB8888;

	inline Texture CreateTexture(Renderer &renderer, const SDL_TextureAccess access, const int width, const int height, const SDL_ScaleMode scale_mode, const bool blending = false)
	{
		// We're using ARGB8888 because it's more likely to be supported natively by the GPU, avoiding the need for constant conversions
		Texture texture(SDL_CreateTexture(renderer, pixel_format, access, width, height));

		if (!texture)
		{
			debug_log.SDLError("SDL_CreateTexture");
		}
		else
		{
			// Disable blending, since we don't need it
			if (!SDL_SetTextureBlendMode(texture, blending ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE))
				debug_log.SDLError("SDL_SetTextureBlendMode");

			if (!SDL_SetTextureScaleMode(texture, scale_mode))
				debug_log.SDLError("SDL_SetTextureScaleMode");
		}

		return texture;
	}

	inline Texture CreateTextureWithBlending(Renderer &renderer, const SDL_TextureAccess access, const int width, const int height, const SDL_ScaleMode scale_mode)
	{
		return CreateTexture(renderer, access, width, height, scale_mode, true);
	}
}

#endif /* SDL_WRAPPER_EXTRA_H */

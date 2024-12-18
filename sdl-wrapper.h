#ifndef SDL_WRAPPER_H
#define SDL_WRAPPER_H

#include <filesystem>
#include <string>

#include "SDL.h"

#include "debug-log.h"
#include "raii-wrapper.h"

namespace SDL
{
	struct FreeFunctor
	{
		void operator()(void* const pointer) { return SDL_free(pointer); }
	};

	template<typename T>
	using Pointer = std::unique_ptr<T, FreeFunctor>;

	MAKE_RAII_POINTER(Window,   SDL_Window,   SDL_DestroyWindow  );
	MAKE_RAII_POINTER(Renderer, SDL_Renderer, SDL_DestroyRenderer);
	MAKE_RAII_POINTER(Texture,  SDL_Texture,  SDL_DestroyTexture );
	MAKE_RAII_POINTER(RWops,    SDL_RWops,    SDL_RWclose        );

	inline RWops RWFromFile(const char* const path, const char* const mode) { return RWops(SDL_RWFromFile(path, mode)); }
	inline RWops RWFromFile(const std::string &path, const char* const mode) { return RWFromFile(path.c_str(), mode); }
	inline RWops RWFromFile(const std::filesystem::path &path, const char* const mode) { return RWFromFile(reinterpret_cast<const char*>(path.u8string().c_str()), mode); }

	inline Texture CreateTexture(Renderer &renderer, const SDL_TextureAccess access, const int width, const int height, const char* const scale_mode, const bool blending = false)
	{
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, scale_mode);
		Texture texture(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, access, width, height));

		if (!texture)
		{
			Frontend::debug_log.Log("SDL_CreateTexture failed with the following message - '{}'", SDL_GetError());
		}
		else
		{
			if (SDL_SetTextureBlendMode(texture, blending ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE) < 0)
				Frontend::debug_log.Log("SDL_SetTextureBlendMode failed with the following message - '{}'", SDL_GetError());
		}

		return texture;
	}

	inline Texture CreateTextureWithBlending(Renderer &renderer, const SDL_TextureAccess access, const int width, const int height, const char* const scale_mode)
	{
		return CreateTexture(renderer, access, width, height, scale_mode, true);
	}
}

#endif /* SDL_WRAPPER_H */

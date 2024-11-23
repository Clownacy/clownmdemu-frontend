#ifndef SDL_WRAPPER_H
#define SDL_WRAPPER_H

#include <filesystem>
#include <string>

#include <SDL3/SDL.h>

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
	MAKE_RAII_POINTER(RWops,    SDL_IOStream, SDL_CloseIO        );

	inline RWops RWFromFile(const char* const path, const char* const mode) { return RWops(SDL_IOFromFile(path, mode)); }
	inline RWops RWFromFile(const std::string &path, const char* const mode) { return RWFromFile(path.c_str(), mode); }
	inline RWops RWFromFile(const std::filesystem::path &path, const char* const mode) { return RWFromFile(reinterpret_cast<const char*>(path.u8string().c_str()), mode); }

	inline Texture CreateTexture(Renderer &renderer, const SDL_TextureAccess access, const int width, const int height, const SDL_ScaleMode scale_mode)
	{
		Texture texture(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, access, width, height));

		if (!texture)
		{
			Frontend::debug_log.Log("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			// Disable blending, since we don't need it
			if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE))
				Frontend::debug_log.Log("SDL_SetTextureBlendMode failed with the following message - '%s'", SDL_GetError());

			if (!SDL_SetTextureScaleMode(texture, scale_mode))
				Frontend::debug_log.Log("SDL_SetTextureScaleMode failed with the following message - '%s'", SDL_GetError());
		}

		return texture;
	}
}

#endif /* SDL_WRAPPER_H */

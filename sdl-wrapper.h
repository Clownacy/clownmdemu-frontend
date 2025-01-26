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
	MAKE_RAII_POINTER(IOStream, SDL_IOStream, SDL_CloseIO        );

	template<auto Function, typename... Args>
	auto PathFunction(const char* const path, Args &&...args)
	{
		return Function(path, std::forward<Args>(args)...);
	};

	template<auto Function, typename... Args>
	auto PathFunction(const std::string &path, Args &&...args)
	{
		return Function(path.c_str(), std::forward<Args>(args)...);
	};

	template<auto Function, typename... Args>
	auto PathFunction(const std::filesystem::path &path, Args &&...args)
	{
		return Function(reinterpret_cast<const char*>(path.u8string().c_str()), std::forward<Args>(args)...);
	};

	template<typename T>
	IOStream IOFromFile(const T &path, const char* const mode) { return IOStream(PathFunction<SDL_IOFromFile>(path, mode)); }

	template<typename T>
	bool RemovePath(const T &path) { return PathFunction<SDL_RemovePath>(path); }

	template<typename T>
	bool GetPathInfo(const T &path, SDL_PathInfo* const info) { return PathFunction<SDL_GetPathInfo>(path, info); }

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

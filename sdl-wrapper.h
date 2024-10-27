#ifndef SDL_WRAPPER_H
#define SDL_WRAPPER_H

#include <filesystem>
#include <string>

#include <SDL3/SDL.h>

#include "raii-wrapper.h"

namespace SDL {

struct FreeFunctor
{
	void operator()(void* const pointer) { return SDL_free(pointer); }
};

template<typename T>
using Pointer = std::unique_ptr<T, FreeFunctor>;

MAKE_RAII_POINTER(Window,   SDL_Window,   SDL_DestroyWindow  );
MAKE_RAII_POINTER(Renderer, SDL_Renderer, SDL_DestroyRenderer);
MAKE_RAII_POINTER(Texture,  SDL_Texture,  SDL_DestroyTexture );
MAKE_RAII_POINTER(RWops,    SDL_IOStream,    SDL_CloseIO        );

inline RWops RWFromFile(const char* const path, const char* const mode) { return RWops(SDL_IOFromFile(path, mode)); }
inline RWops RWFromFile(const std::string &path, const char* const mode) { return RWFromFile(path.c_str(), mode); }
inline RWops RWFromFile(const std::filesystem::path &path, const char* const mode) { return RWFromFile(path.string(), mode); }

}

#endif /* SDL_WRAPPER_H */

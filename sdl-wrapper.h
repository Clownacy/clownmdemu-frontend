#ifndef SDL_WRAPPER_H
#define SDL_WRAPPER_H

#include <memory>

#include "SDL.h"

#define SDL_WRAPPER_MAKE_RAII_POINTER(NAME, TYPE, DELETER) \
struct NAME##Deleter \
{ \
	void operator()(TYPE* const pointer) { DELETER(pointer); } \
}; \
 \
using NAME = std::unique_ptr<TYPE, NAME##Deleter>

namespace SDL {

SDL_WRAPPER_MAKE_RAII_POINTER(Window,   SDL_Window,   SDL_DestroyWindow  );
SDL_WRAPPER_MAKE_RAII_POINTER(Renderer, SDL_Renderer, SDL_DestroyRenderer);
SDL_WRAPPER_MAKE_RAII_POINTER(Texture,  SDL_Texture,  SDL_DestroyTexture );
SDL_WRAPPER_MAKE_RAII_POINTER(RWops,    SDL_RWops,    SDL_RWclose        );

}

#endif /* SDL_WRAPPER_H */

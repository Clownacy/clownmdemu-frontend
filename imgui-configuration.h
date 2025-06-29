#include "sdl-wrapper.h"

// Avoid a bunch of needless casting by defining this as a proper pointer.
#define ImTextureID SDL_Texture*

// Disable some deprecated junk.
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DISABLE_OBSOLETE_KEYIO

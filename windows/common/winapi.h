#ifndef WINAPI_H
#define WINAPI_H

#include "../../sdl-wrapper.h"

void SetWindowTitleBarColour(SDL_Window *window, unsigned char red, unsigned char green, unsigned char blue);
void DisableWindowRounding(SDL_Window *window);

#endif /* WINAPI_H */

#ifndef WINAPI_H
#define WINAPI_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

void SetWindowTitleBarColour(SDL_Window *window, unsigned char red, unsigned char green, unsigned char blue);

#ifdef __cplusplus
}
#endif

#endif /* WINAPI_H */

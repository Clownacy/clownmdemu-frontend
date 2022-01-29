#ifndef VIDEO_H
#define VIDEO_H

#include "SDL.h"

extern SDL_Window *window;
extern SDL_Renderer *renderer;

bool InitVideo(void);
void DeinitVideo(void);

#endif /* VIDEO_H */

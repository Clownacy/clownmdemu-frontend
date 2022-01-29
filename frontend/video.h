#ifndef VIDEO_H
#define VIDEO_H

#include "SDL.h"

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern float dpi_scale;

bool InitVideo(void);
void DeinitVideo(void);

#endif /* VIDEO_H */

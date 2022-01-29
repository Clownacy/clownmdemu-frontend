#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>

#include "SDL.h"

extern SDL_AudioDeviceID audio_device;
extern size_t audio_buffer_size;
extern unsigned int native_audio_sample_rate;

bool InitAudio(void);
void DeinitAudio(void);

#endif /* AUDIO_H */

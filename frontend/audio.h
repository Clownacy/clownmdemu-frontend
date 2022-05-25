#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>

#include "SDL.h"

struct AudioDevice
{
	SDL_AudioDeviceID identifier;
	size_t audio_buffer_size;
	unsigned int native_audio_sample_rate;
};

bool CreateAudioDevice(AudioDevice *audio_device, unsigned long sample_rate, unsigned int channels);
void DestroyAudioDevice(AudioDevice *audio_device);

#endif /* AUDIO_H */

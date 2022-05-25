#include "audio.h"

#include <stddef.h>

#include "SDL.h"

#include "error.h"

bool CreateAudioDevice(AudioDevice *audio_device, unsigned long sample_rate, unsigned int channels, bool allow_frequency_change)
{
	// Initialise audio backend
	SDL_AudioSpec want, have;

	SDL_zero(want);
	want.freq = sample_rate;
	want.format = AUDIO_S16;
	want.channels = channels;
	// We want a 25ms buffer (this value must be a power of two)
	want.samples = 1;
	while (want.samples < (want.freq * want.channels) / (1000 / 25))
		want.samples <<= 1;
	want.callback = NULL;

	audio_device->identifier = SDL_OpenAudioDevice(NULL, 0, &want, &have, allow_frequency_change ? SDL_AUDIO_ALLOW_FREQUENCY_CHANGE : 0);

	if (audio_device->identifier == 0)
	{
		PrintError("SDL_OpenAudioDevice failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		audio_device->audio_buffer_size = have.size;
		audio_device->native_audio_sample_rate = (unsigned int)have.freq;

		return true;
	}

	return false;
}

void DestroyAudioDevice(AudioDevice *audio_device)
{
	SDL_CloseAudioDevice(audio_device->identifier);
}

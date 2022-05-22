#include "audio.h"

#include <stddef.h>

#include "SDL.h"

#include "error.h"

SDL_AudioDeviceID audio_device;
size_t audio_buffer_size;
unsigned int native_audio_sample_rate;

bool InitAudio(void)
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		PrintError("SDL_InitSubSystem(SDL_INIT_AUDIO) failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		// Initialise audio backend
		SDL_AudioSpec want, have;

		SDL_zero(want);
		want.freq = 53263;
		want.format = AUDIO_S16;
		want.channels = 2;
		// We want a 25ms buffer (this value must be a power of two)
		want.samples = 1;
		while (want.samples < (want.freq * want.channels) / (1000 / 25))
			want.samples <<= 1;
		want.callback = NULL;

		audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

		if (audio_device == 0)
		{
			PrintError("SDL_OpenAudioDevice failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			audio_buffer_size = have.size;
			native_audio_sample_rate = (unsigned int)have.freq;

			return true;
		}

		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}

	return false;
}

void DeinitAudio(void)
{
	SDL_CloseAudioDevice(audio_device);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

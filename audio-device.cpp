#include "audio-device.h"

#include "debug-log.h"

AudioDevice::AudioDevice(const cc_u8f channels, const cc_u32f sample_rate, const bool paused)
	: size_of_frame(channels * sizeof(cc_s16l))
{
	SDL_AudioSpec specification;
	specification.freq = sample_rate;
	specification.format = SDL_AUDIO_S16;
	specification.channels = channels;

	// Create audio stream.
	stream = SDL::AudioStream(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification, nullptr, nullptr));

	if (stream == nullptr)
		Frontend::debug_log.Log("SDL_GetAudioDeviceFormat failed with the following message - '{}'", SDL_GetError());

	SetPaused(paused);
}

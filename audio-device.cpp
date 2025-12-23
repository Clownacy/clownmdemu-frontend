#include "audio-device.h"

#include "debug-log.h"

AudioDevice::AudioDevice(const cc_u8f channels, const cc_u32f sample_rate)
	: channels(channels)
{
	SDL_AudioSpec specification;
	specification.freq = sample_rate;
	specification.format = SDL_AUDIO_S16;
	specification.channels = channels;

	// Create audio stream.
	stream = SDL::AudioStream(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification, nullptr, nullptr));

	if (stream == nullptr)
	{
		Frontend::debug_log.Log("SDL_GetAudioDeviceFormat failed with the following message - '{}'", SDL_GetError());
	}
	else
	{
		// Unpause audio device, so that playback can begin.
		SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
	}
}

void AudioDevice::QueueFrames(const cc_s16l *buffer, cc_u32f total_frames)
{
	SDL_PutAudioStreamData(stream, buffer, total_frames * SIZE_OF_FRAME);
}

cc_u32f AudioDevice::GetTotalQueuedFrames()
{
	return SDL_GetAudioStreamQueued(stream) / SIZE_OF_FRAME;
}

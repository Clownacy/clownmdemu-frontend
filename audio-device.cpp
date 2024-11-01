#include "audio-device.h"

#include <stdexcept>
#include <string>

#include "frontend.h"

static constexpr int BufferSizeFromSampleRate(const int sample_rate)
{
	// We want a 10ms buffer (this value must be a power of two).
	// TODO: Is there a C++ library function that could do this instead?
	int samples = 1;
	while (samples < sample_rate / (1000 / 10))
		samples *= 2;
	return samples;
}

AudioDevice::AudioDevice(const cc_u8f channels, cc_u32f &sample_rate, cc_u32f &total_buffer_frames)
	: channels(channels)
{
	SDL_AudioSpec specification;
	int device_buffer_size;
	if (!SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification, &device_buffer_size))
	{
		Frontend::debug_log.Log("SDL_GetAudioDeviceFormat failed with the following message - '%s'", SDL_GetError());
		// Set defaults.
		specification.freq = 48000;
		device_buffer_size = BufferSizeFromSampleRate(specification.freq);
	}

	// Set overrides.
	specification.format = SDL_AUDIO_S16;
	specification.channels = channels;

	// Give output.
	// TODO: Can't we return a struct or a tuple or something? This is very C.
	sample_rate = static_cast<cc_u32f>(specification.freq);
	total_buffer_frames = static_cast<cc_u32f>(device_buffer_size);

	// Create audio stream.
	stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification, nullptr, nullptr);

	if (stream == nullptr)
	{
		Frontend::debug_log.Log("SDL_GetAudioDeviceFormat failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		// Unpause audio device, so that playback can begin.
		SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
	}
}

AudioDevice::~AudioDevice()
{
	// TODO: std::unique_ptr.
	SDL_DestroyAudioStream(stream);
}

void AudioDevice::QueueFrames(const cc_s16l *buffer, cc_u32f total_frames)
{
	SDL_PutAudioStreamData(stream, buffer, total_frames * SIZE_OF_FRAME);
}

cc_u32f AudioDevice::GetTotalQueuedFrames()
{
	return SDL_GetAudioStreamQueued(stream) / SIZE_OF_FRAME;
}

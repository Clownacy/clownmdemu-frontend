#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include <cstddef>

#include "sdl-wrapper.h"

#include "common/core/clowncommon/clowncommon.h"

class AudioDevice
{
private:
	std::size_t size_of_frame;

	SDL::AudioStream stream;
	SDL_AudioDeviceID device;

public:
	AudioDevice(cc_u8f channels, cc_u32f sample_rate);

	void QueueFrames(const cc_s16l *buffer, cc_u32f total_frames)
	{
		SDL_PutAudioStreamData(stream, buffer, total_frames * size_of_frame);
	}

	cc_u32f GetTotalQueuedFrames()
	{
		return SDL_GetAudioStreamQueued(stream) / size_of_frame;
	}

	void SetPlaybackSpeed(const cc_u32f numerator, const cc_u32f denominator)
	{
		SDL_SetAudioStreamFrequencyRatio(stream, static_cast<float>(numerator) / denominator);
	}

	bool GetPaused() const
	{
		return SDL_AudioDevicePaused(device);
	}

	void SetPaused(const bool paused)
	{
		if (paused)
			SDL_PauseAudioDevice(device);
		else
			SDL_ResumeAudioDevice(device);
	}
};

#endif /* AUDIO_DEVICE_H */

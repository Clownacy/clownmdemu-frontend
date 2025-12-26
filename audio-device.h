#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include <cstddef>

#include "sdl-wrapper-inner.h"

#include "common/core/clowncommon/clowncommon.h"

class AudioDevice
{
private:
	std::size_t size_of_frame;

	SDL::AudioStream stream;

public:
	AudioDevice(cc_u8f channels, cc_u32f sample_rate);

	void QueueFrames(const cc_s16l *buffer, cc_u32f total_frames);
	cc_u32f GetTotalQueuedFrames();
	void SetPlaybackSpeed(const cc_u32f numerator, const cc_u32f denominator)
	{
		SDL_SetAudioStreamFrequencyRatio(stream, static_cast<float>(numerator) / denominator);
	}
};

#endif /* AUDIO_DEVICE_H */

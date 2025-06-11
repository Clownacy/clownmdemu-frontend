#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include <cstddef>

#include "sdl-wrapper-inner.h"

#include "common/core/clowncommon/clowncommon.h"

class AudioDevice
{
private:
	const cc_u8f channels;
	const std::size_t SIZE_OF_FRAME = channels * sizeof(cc_s16l);

	SDL::AudioStream stream;

public:
	AudioDevice(cc_u8f channels, cc_u32f sample_rate);
	AudioDevice(const AudioDevice&) = delete;
	AudioDevice& operator=(const AudioDevice&) = delete;

	void QueueFrames(const cc_s16l *buffer, cc_u32f total_frames);
	cc_u32f GetTotalQueuedFrames();
	void SetPlaybackSpeed(const cc_u32f numerator, const cc_u32f denominator)
	{
		SDL_SetAudioStreamFrequencyRatio(stream, static_cast<float>(numerator) / denominator);
	}
};

#endif /* AUDIO_DEVICE_H */

#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include <cstddef>

#include <SDL3/SDL.h>

#include "common/core/clowncommon/clowncommon.h"

class AudioDevice
{
private:
	const cc_u8f channels;
	const std::size_t SIZE_OF_FRAME = channels * sizeof(cc_s16l);

	SDL_AudioStream *stream;

public:
	AudioDevice(cc_u8f channels, cc_u32f &sample_rate, cc_u32f &total_buffer_frames);
	~AudioDevice();
	AudioDevice(const AudioDevice&) = delete;
	AudioDevice& operator=(const AudioDevice&) = delete;

	void QueueFrames(const cc_s16l *buffer, cc_u32f total_frames);
	cc_u32f GetTotalQueuedFrames();
};

#endif /* AUDIO_DEVICE_H */

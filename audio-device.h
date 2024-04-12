#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include "SDL.h"

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#include "debug-log.h"

class AudioDevice
{
private:
	const cc_u8f channels;
	const cc_s32f SIZE_OF_FRAME = channels * sizeof(cc_s16l);
	DebugLog &debug_log;

	SDL_AudioDeviceID device;

public:
	AudioDevice(cc_u8f channels, cc_u32f &sample_rate, cc_u32f &total_buffer_frames, DebugLog &debug_log);
	~AudioDevice();
	AudioDevice(const AudioDevice&) = delete;
	AudioDevice& operator=(const AudioDevice&) = delete;

	void QueueFrames(const cc_s16l *buffer, cc_u32f total_frames);
	cc_u32f GetTotalQueuedFrames();
};

#endif /* AUDIO_DEVICE_H */

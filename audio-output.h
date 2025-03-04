#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <algorithm>
#include <array>
#include <cstddef>

#include "common/core/clowncommon/clowncommon.h"

#define MIXER_FORMAT cc_s16l
#include "common/mixer.h"

#include "audio-device.h"

class AudioOutput
{
private:
	static Mixer_Constant mixer_constant;
	static bool mixer_constant_initialised;

	AudioDevice device;
	cc_u32f total_buffer_frames;
	cc_u32f sample_rate;

	bool pal_mode = false;
	bool mixer_update_pending = false;
	std::array<cc_u32f, 0x10>rolling_average_buffer = {0};
	cc_u8f rolling_average_buffer_index = 0;

	Mixer_State mixer_state;
	const Mixer mixer = {&mixer_constant, &mixer_state};

public:
	AudioOutput();
	~AudioOutput();
	void MixerBegin();
	void MixerEnd();
	cc_s16l* MixerAllocateFMSamples(std::size_t total_frames);
	cc_s16l* MixerAllocatePSGSamples(std::size_t total_frames);
	cc_s16l* MixerAllocatePCMSamples(std::size_t total_frames);
	cc_s16l* MixerAllocateCDDASamples(std::size_t total_frames);
	cc_u32f GetAverageFrames() const;
	cc_u32f GetTargetFrames() const { return std::max(total_buffer_frames * 2, sample_rate / 20); } // 50ms
	cc_u32f GetTotalBufferFrames() const { return total_buffer_frames; }
	cc_u32f GetSampleRate() const { return sample_rate; }

	void SetPALMode(const bool enabled)
	{
		pal_mode = enabled;
		mixer_update_pending = true;
	}

	bool GetPALMode() const { return pal_mode; }
};

#endif /* AUDIO_OUTPUT_H */

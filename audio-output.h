#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <algorithm>
#include <array>
#include <cstddef>

#include "common/core/clowncommon/clowncommon.h"

#include "common/mixer.h"

#include "audio-device.h"

class AudioOutput
{
private:
	AudioDevice device;
	cc_u32f total_buffer_frames;

	bool pal_mode = false;
	std::array<cc_u32f, 0x10> rolling_average_buffer = {0};
	cc_u8f rolling_average_buffer_index = 0;

	static Mixer::Constant constant;
	Mixer mixer = Mixer(constant, pal_mode);

public:
	AudioOutput();
	void MixerBegin();
	void MixerEnd();
	cc_s16l* MixerAllocateFMSamples(std::size_t total_frames);
	cc_s16l* MixerAllocatePSGSamples(std::size_t total_frames);
	cc_s16l* MixerAllocatePCMSamples(std::size_t total_frames);
	cc_s16l* MixerAllocateCDDASamples(std::size_t total_frames);
	cc_u32f GetAverageFrames() const;
	cc_u32f GetTargetFrames() const { return std::max<cc_u32f>(total_buffer_frames * 2, MIXER_OUTPUT_SAMPLE_RATE / 20); } // 50ms
	cc_u32f GetTotalBufferFrames() const { return total_buffer_frames; }
	cc_u32f GetSampleRate() const { return MIXER_OUTPUT_SAMPLE_RATE; }

	void SetPALMode(bool enabled);
	bool GetPALMode() const { return pal_mode; }
};

#endif /* AUDIO_OUTPUT_H */

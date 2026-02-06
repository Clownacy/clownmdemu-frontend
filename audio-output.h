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
	cc_u32f sample_rate, total_buffer_frames;
	AudioDevice device;
	Mixer mixer;

	std::array<cc_u32f, 0x10> rolling_average_buffer = {0};
	cc_u8f rolling_average_buffer_index = 0;

public:
	AudioOutput(bool pal_mode);

	void MixerBegin()
	{
		mixer.Begin();
	}

	void MixerEnd();

	cc_s16l* MixerAllocateFMSamples(const std::size_t total_frames)
	{
		return mixer.AllocateFMSamples(total_frames);
	}

	cc_s16l* MixerAllocatePSGSamples(const std::size_t total_frames)
	{
		return mixer.AllocatePSGSamples(total_frames);
	}

	cc_s16l* MixerAllocatePCMSamples(const std::size_t total_frames)
	{
		return mixer.AllocatePCMSamples(total_frames);
	}

	cc_s16l* MixerAllocateCDDASamples(const std::size_t total_frames)
	{
		return mixer.AllocateCDDASamples(total_frames);
	}

	cc_u32f GetAverageFrames() const;
	cc_u32f GetTargetFrames() const { return std::max<cc_u32f>(total_buffer_frames * 2, sample_rate / 20); } // 50ms
	cc_u32f GetTotalBufferFrames() const { return total_buffer_frames; }
	cc_u32f GetSampleRate() const { return sample_rate; }

	bool GetPaused() { return device.GetPaused(); }
	void SetPaused(const bool paused) { device.SetPaused(paused); }
};

#endif /* AUDIO_OUTPUT_H */

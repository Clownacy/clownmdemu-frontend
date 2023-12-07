#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <cstddef>

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#define MIXER_FORMAT cc_s16l
#include "clownmdemu-frontend-common/mixer.h"

#include "audio-device.h"
#include "debug-log.h"

class AudioOutputInner
{
private:
	static Mixer_Constant mixer_constant;
	static bool mixer_constant_initialised;

	AudioDevice device;
	cc_u32f total_buffer_frames;
	cc_u32f sample_rate;

	bool pal_mode = false;
	bool low_pass_filter = true;
	bool mixer_update_pending = true;
	cc_u32f rolling_average_buffer[0x10] = {0};
	cc_u8f rolling_average_buffer_index = 0;

	Mixer_State mixer_state;
	const Mixer mixer = {&mixer_constant, &mixer_state};

public:
	AudioOutputInner();
	~AudioOutputInner();
	void MixerBegin();
	void MixerEnd();
	cc_s16l* MixerAllocateFMSamples(std::size_t total_frames);
	cc_s16l* MixerAllocatePSGSamples(std::size_t total_frames);
	cc_u32f GetAverageFrames() const;
	cc_u32f GetTargetFrames() const { return CC_MAX(total_buffer_frames * 2, sample_rate / 20); } // 50ms
	cc_u32f GetTotalBufferFrames() const { return total_buffer_frames; }
	cc_u32f GetSampleRate() const { return sample_rate; }

	void SetPALMode(const bool enabled)
	{
		pal_mode = enabled;
		mixer_update_pending = true;
	}

	bool GetPALMode() const { return pal_mode; }

	void SetLowPassFilter(const bool enabled)
	{
		low_pass_filter = enabled;
		mixer_update_pending = true;
	}

	bool GetLowPassFilter() const { return low_pass_filter; }
};

class AudioOutput
{
private:
	AudioOutputInner *inner = nullptr;

public:
	AudioOutput(DebugLog &/*debug_log*/) {}
	bool Initialise()
	{
		inner = new AudioOutputInner();
		return inner != nullptr;
	}
	void Deinitialise()
	{
		delete inner;
		inner = nullptr;
	}
	void MixerBegin() { inner->MixerBegin(); };
	void MixerEnd() { inner->MixerEnd(); };
	cc_s16l* MixerAllocateFMSamples(std::size_t total_samples) { return inner->MixerAllocateFMSamples(total_samples); };
	cc_s16l* MixerAllocatePSGSamples(std::size_t total_samples) { return inner->MixerAllocatePSGSamples(total_samples); };
	cc_u32f GetAverageFrames() const { return inner->GetAverageFrames(); };
	cc_u32f GetTargetFrames() const { return inner->GetTargetFrames(); } // 50ms
	cc_u32f GetTotalBufferFrames() const { return inner->GetTotalBufferFrames(); }
	cc_u32f GetSampleRate() const { return inner->GetSampleRate(); }

	void SetPALMode(const bool enabled)
	{
		inner->SetPALMode(enabled);
	}

	bool GetPALMode() const { return inner->GetPALMode(); }

	void SetLowPassFilter(const bool enabled)
	{
		inner->SetLowPassFilter(enabled);
	}

	bool GetLowPassFilter() const { return inner->GetLowPassFilter(); }
};

#endif /* AUDIO_OUTPUT_H */

#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <cstddef>

#include "SDL.h"

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#define MIXER_FORMAT Sint16
#include "clownmdemu-frontend-common/mixer.h"

#include "debug-log.h"

class AudioOutput
{
private:
	static Mixer_Constant mixer_constant;
	static bool mixer_constant_initialised;

	DebugLog &debug_log;
	SDL_AudioDeviceID device;
	cc_u32f buffer_size;
	cc_u32f sample_rate;
	bool pal_mode;
	bool low_pass_filter = true;
	bool mixer_update_pending;
	Uint32 rolling_average_buffer[0x10];
	cc_u8f rolling_average_buffer_index;

	Mixer_State mixer_state;
	const Mixer mixer = {&mixer_constant, &mixer_state};

public:
	AudioOutput(DebugLog &debug_log) : debug_log(debug_log) {}
	bool Initialise();
	void Deinitialise();
	void MixerBegin();
	void MixerEnd();
	cc_s16l* MixerAllocateFMSamples(std::size_t total_samples);
	cc_s16l* MixerAllocatePSGSamples(std::size_t total_samples);
	cc_u32f GetAverageFrames() const;
	cc_u32f GetTargetFrames() const { return sample_rate / 20; } // 50ms
	cc_u32f GetBufferSize() const { return buffer_size; }
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

#endif /* AUDIO_OUTPUT_H */

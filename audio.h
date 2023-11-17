#ifndef AUDIO_H
#define AUDIO_H

#include <cstddef>

#include "SDL.h"

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#define MIXER_FORMAT Sint16
#include "clownmdemu-frontend-common/mixer.h"

#include "debug_log.h"

class Audio
{
private:
	static Mixer_Constant mixer_constant;
	static bool mixer_constant_initialised;

	DebugLog &debug_log;
	SDL_AudioDeviceID audio_device;
	Uint32 audio_device_buffer_size;
	unsigned long audio_device_sample_rate;
	bool pal_mode;
	bool low_pass_filter = true;

	Mixer_State mixer_state;
	const Mixer mixer = {&mixer_constant, &mixer_state};

public:

	Audio(DebugLog &debug_log) : debug_log(debug_log) {}
	bool Initialise();
	void Deinitialise();
	void MixerBegin();
	void MixerEnd();
	cc_s16l* MixerAllocateFMSamples(std::size_t total_samples);
	cc_s16l* MixerAllocatePSGSamples(std::size_t total_samples);
	void SetPALMode(bool enabled);
	bool GetPALMode() { return pal_mode; }
	void SetLowPassFilter(bool enabled);
	bool GetLowPassfilter() { return low_pass_filter; }
};

#endif /* AUDIO_H */

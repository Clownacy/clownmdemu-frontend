#include "audio-output.h"

#include <numeric>

#include <SDL3/SDL.h>

#define CLOWNRESAMPLER_ASSERT SDL_assert
#define CLOWNRESAMPLER_FABS SDL_fabs
#define CLOWNRESAMPLER_SIN SDL_sin
#define CLOWNRESAMPLER_ZERO(buffer, size) SDL_memset(buffer, 0, size)
#define CLOWNRESAMPLER_MEMMOVE SDL_memmove
#define MIXER_IMPLEMENTATION
#define MIXER_ASSERT SDL_assert
#define MIXER_FREE SDL_free
#define MIXER_CALLOC SDL_calloc
#define MIXER_MEMMOVE SDL_memmove
#define MIXER_MEMSET SDL_memset
#include "clownmdemu-frontend-common/mixer.h"

Mixer_Constant AudioOutput::mixer_constant;
bool AudioOutput::mixer_constant_initialised;

AudioOutput::AudioOutput()
	: device(MIXER_FM_CHANNEL_COUNT, sample_rate, total_buffer_frames)
{
	// Initialise the mixer.
	if (!mixer_constant_initialised)
	{
		mixer_constant_initialised = true;
		Mixer_Constant_Initialise(&mixer_constant);
	}

	Mixer_State_Initialise(&mixer_state, sample_rate, pal_mode, low_pass_filter);
}

AudioOutput::~AudioOutput()
{
	Mixer_State_Deinitialise(&mixer_state);
}

void AudioOutput::MixerBegin()
{
	if (mixer_update_pending)
	{
		mixer_update_pending = false;
		Mixer_State_Deinitialise(&mixer_state);
		Mixer_State_Initialise(&mixer_state, sample_rate, pal_mode, low_pass_filter);
	}

	Mixer_Begin(&mixer);
}

void AudioOutput::MixerEnd()
{
	const cc_u32f target_frames = GetTargetFrames();
	const cc_u32f queued_frames = device.GetTotalQueuedFrames();

	rolling_average_buffer[rolling_average_buffer_index] = queued_frames;
	rolling_average_buffer_index = (rolling_average_buffer_index + 1) % rolling_average_buffer.size();

	// If there is too much audio, just drop it because the dynamic rate control will be unable to handle it.
	if (queued_frames < target_frames * 2)
	{
		const auto callback = [](void* const user_data, const cc_s16l* const audio_samples, const std::size_t total_frames)
		{
			AudioOutput *audio_output = static_cast<AudioOutput*>(user_data);

			audio_output->device.QueueFrames(audio_samples, total_frames);
		};

		// Hans-Kristian Arntzen's Dynamic Rate Control formula.
		// https://github.com/libretro/docs/blob/master/archive/ratecontrol.pdf
		const cc_u32f divisor = target_frames * 0x100; // The number here is the inverse of the formula's 'd' value.
		Mixer_End(&mixer, queued_frames - target_frames + divisor, divisor, callback, this);
	}
}

cc_s16l* AudioOutput::MixerAllocateFMSamples(const std::size_t total_frames)
{
	return Mixer_AllocateFMSamples(&mixer, total_frames);
}

cc_s16l* AudioOutput::MixerAllocatePSGSamples(const std::size_t total_frames)
{
	return Mixer_AllocatePSGSamples(&mixer, total_frames);
}

cc_s16l* AudioOutput::MixerAllocatePCMSamples(const std::size_t total_frames)
{
	return Mixer_AllocatePCMSamples(&mixer, total_frames);
}

cc_s16l* AudioOutput::MixerAllocateCDDASamples(const std::size_t total_frames)
{
	return Mixer_AllocateCDDASamples(&mixer, total_frames);
}

cc_u32f AudioOutput::GetAverageFrames() const
{
	return std::accumulate(rolling_average_buffer.cbegin(), rolling_average_buffer.cend(), cc_u32f(0)) / rolling_average_buffer.size();
}

#include "audio-output.h"

#include "SDL.h"

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

Mixer_Constant AudioOutputInner::mixer_constant;
bool AudioOutputInner::mixer_constant_initialised;

AudioOutputInner::AudioOutputInner()
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

AudioOutputInner::~AudioOutputInner()
{
	Mixer_State_Deinitialise(&mixer_state);
}

void AudioOutputInner::MixerBegin()
{
	if (mixer_update_pending)
	{
		mixer_update_pending = false;
		Mixer_State_Deinitialise(&mixer_state);
		Mixer_State_Initialise(&mixer_state, sample_rate, pal_mode, low_pass_filter);
	}

	Mixer_Begin(&mixer);
}

void AudioOutputInner::MixerEnd()
{
	const cc_u32f target_frames = GetTargetFrames();
	const cc_u32f queued_frames = device.GetTotalQueuedFrames();

	rolling_average_buffer[rolling_average_buffer_index] = queued_frames;
	rolling_average_buffer_index = (rolling_average_buffer_index + 1) % CC_COUNT_OF(rolling_average_buffer);

	// If there is too much audio, just drop it because the dynamic rate control will be unable to handle it.
	if (queued_frames < target_frames * 2)
	{
		const auto callback = [](void* const user_data, Sint16* const audio_samples, const std::size_t total_frames)
		{
			AudioOutputInner *audio_output = static_cast<AudioOutputInner*>(user_data);

			audio_output->device.QueueFrames(audio_samples, total_frames);
		};

		// Hans-Kristian Arntzen's Dynamic Rate Control formula.
		// https://github.com/libretro/docs/blob/master/archive/ratecontrol.pdf
		const cc_u32f divisor = target_frames * 0x100; // The number here is the inverse of the formula's 'd' value.
		Mixer_End(&mixer, queued_frames - target_frames + divisor, divisor, callback, this);
	}
}

cc_s16l* AudioOutputInner::MixerAllocateFMSamples(const std::size_t total_frames)
{
	return Mixer_AllocateFMSamples(&mixer, total_frames);
}

cc_s16l* AudioOutputInner::MixerAllocatePSGSamples(const std::size_t total_frames)
{
	return Mixer_AllocatePSGSamples(&mixer, total_frames);
}

cc_u32f AudioOutputInner::GetAverageFrames() const
{
	cc_u32f average_queued_frames = 0;

	for (const auto value : rolling_average_buffer)
		average_queued_frames += value;

	average_queued_frames /= CC_COUNT_OF(rolling_average_buffer);

	return average_queued_frames;
}

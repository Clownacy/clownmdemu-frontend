#include "audio-output.h"

#include <numeric>

#include <SDL3/SDL.h>

#define MIXER_IMPLEMENTATION
#define MIXER_ASSERT SDL_assert
#define MIXER_FREE SDL_free
#define MIXER_CALLOC SDL_calloc
#define MIXER_MEMMOVE SDL_memmove
#define MIXER_MEMSET SDL_memset
#include "common/mixer.h"

static constexpr cc_u32f BufferSizeFromSampleRate(const cc_u32f sample_rate)
{
	// We want a 10ms buffer (this value must be a power of two).
	// TODO: Use `std::bit_ceil` for this instead.
	cc_u32f samples = 1;
	while (samples < sample_rate / (1000 / 10))
		samples *= 2;
	return samples;
}

AudioOutput::AudioOutput()
	: device(MIXER_CHANNEL_COUNT, MIXER_OUTPUT_SAMPLE_RATE)
	, total_buffer_frames(BufferSizeFromSampleRate(MIXER_OUTPUT_SAMPLE_RATE))
{
	Mixer_Initialise(&mixer, pal_mode);
}

AudioOutput::~AudioOutput()
{
	Mixer_Deinitialise(&mixer);
}

void AudioOutput::MixerBegin()
{
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

		Mixer_End(&mixer, callback, this);

		// Hans-Kristian Arntzen's Dynamic Rate Control formula.
		// https://github.com/libretro/docs/blob/master/archive/ratecontrol.pdf
		const cc_u32f divisor = target_frames * 0x100; // The number here is the inverse of the formula's 'd' value.
		device.SetPlaybackSpeed(queued_frames - target_frames + divisor, divisor);
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

void AudioOutput::SetPALMode(const bool enabled)
{
	pal_mode = enabled;
	Mixer_Deinitialise(&mixer);
	Mixer_Initialise(&mixer, pal_mode);
}

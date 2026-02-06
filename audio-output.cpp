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
	return std::bit_ceil(sample_rate / (1000 / 10));
}

AudioOutput::AudioOutput(const bool pal_mode, const bool paused)
	: sample_rate(pal_mode ? MIXER_OUTPUT_SAMPLE_RATE_PAL : MIXER_OUTPUT_SAMPLE_RATE_NTSC)
	, total_buffer_frames(BufferSizeFromSampleRate(sample_rate))
	, device(MIXER_CHANNEL_COUNT, sample_rate, paused)
	, mixer(pal_mode)
{}

void AudioOutput::MixerEnd()
{
	const cc_u32f target_frames = GetTargetFrames();
	const cc_u32f queued_frames = device.GetTotalQueuedFrames();

	rolling_average_buffer[rolling_average_buffer_index] = queued_frames;
	rolling_average_buffer_index = (rolling_average_buffer_index + 1) % rolling_average_buffer.size();

	// If there is too much audio, just drop it because the dynamic rate control will be unable to handle it.
	if (queued_frames < target_frames * 2)
	{
		mixer.End(
			[&](const cc_s16l* const audio_samples, const std::size_t total_frames)
			{
				device.QueueFrames(audio_samples, total_frames);
			}
		);

		// Hans-Kristian Arntzen's Dynamic Rate Control formula.
		// https://github.com/libretro/docs/blob/master/archive/ratecontrol.pdf
		const cc_u32f divisor = target_frames * 0x100; // The number here is the inverse of the formula's 'd' value.
		device.SetPlaybackSpeed(queued_frames - target_frames + divisor, divisor);
	}
}

cc_u32f AudioOutput::GetAverageFrames() const
{
	return std::accumulate(rolling_average_buffer.cbegin(), rolling_average_buffer.cend(), cc_u32f(0)) / rolling_average_buffer.size();
}

#include "mixer.h"

#include <stddef.h>
#include <string.h>

#include "../clowncommon.h"

#define CLOWNRESAMPLER_IMPLEMENTATION
#define CLOWNRESAMPLER_STATIC
#include "libraries/clownresampler/clownresampler.h"

static size_t FMResamplerCallback(const void *user_data, short *buffer, size_t buffer_size)
{
	Mixer* const mixer = (Mixer*)user_data;

	/* TODO: Zero-copy version of this. */
	const size_t frames_to_do = CC_MIN(buffer_size, (mixer->state->fm_sample_buffer_write_index - mixer->state->fm_sample_buffer_read_index) / MIXER_FM_CHANNEL_COUNT);

	memcpy(buffer, &mixer->state->fm_sample_buffer[mixer->state->fm_sample_buffer_read_index], frames_to_do * sizeof(*mixer->state->fm_sample_buffer) * MIXER_FM_CHANNEL_COUNT);

	mixer->state->fm_sample_buffer_read_index += frames_to_do * MIXER_FM_CHANNEL_COUNT;

	return frames_to_do;
}

static size_t PSGResamplerCallback(const void *user_data, short *buffer, size_t buffer_size)
{
	Mixer* const mixer = (Mixer*)user_data;

	/* TODO: Zero-copy version of this. */
	const size_t frames_to_do = CC_MIN(buffer_size, (mixer->state->psg_sample_buffer_write_index - mixer->state->psg_sample_buffer_read_index) / MIXER_PSG_CHANNEL_COUNT);

	memcpy(buffer, &mixer->state->psg_sample_buffer[mixer->state->psg_sample_buffer_read_index], frames_to_do * sizeof(*mixer->state->psg_sample_buffer) * MIXER_PSG_CHANNEL_COUNT);

	mixer->state->psg_sample_buffer_read_index += frames_to_do * MIXER_PSG_CHANNEL_COUNT;

	return frames_to_do;
}

void Mixer_Constant_Initialise(Mixer_Constant *constant)
{
	/* Compute clownresampler's lookup tables.*/
	ClownResampler_Precompute(&constant->resampler_precomputed);
}

void Mixer_State_Initialise(Mixer_State *state, unsigned long sample_rate)
{
	state->output_sample_rate = sample_rate;
}

void Mixer_SetPALMode(const Mixer *mixer, cc_bool enabled)
{
	/* Reinitialise the resamplers to support the current region's sample rate. */

	/* Divide and multiple by the sample to try to make the sample rate closer to the emulator's output. */
	const unsigned int pal_fm_sample_rate = CLOWNMDEMU_MULTIPLY_BY_PAL_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(CLOWNMDEMU_FM_SAMPLE_RATE_PAL));
	const unsigned int ntsc_fm_sample_rate = CLOWNMDEMU_MULTIPLY_BY_NTSC_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(CLOWNMDEMU_FM_SAMPLE_RATE_NTSC));

	const unsigned int pal_psg_sample_rate = CLOWNMDEMU_MULTIPLY_BY_PAL_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(CLOWNMDEMU_PSG_SAMPLE_RATE_PAL));
	const unsigned int ntsc_psg_sample_rate = CLOWNMDEMU_MULTIPLY_BY_NTSC_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(CLOWNMDEMU_PSG_SAMPLE_RATE_NTSC));

	ClownResampler_HighLevel_Init(&mixer->state->fm_resampler, MIXER_FM_CHANNEL_COUNT, enabled ? pal_fm_sample_rate : ntsc_fm_sample_rate, mixer->state->output_sample_rate);
	ClownResampler_HighLevel_Init(&mixer->state->psg_resampler, MIXER_PSG_CHANNEL_COUNT, enabled ? pal_psg_sample_rate : ntsc_psg_sample_rate, mixer->state->output_sample_rate);
}

void Mixer_Begin(const Mixer *mixer)
{
	/* Reset the audio buffers so that they can be mixed into. */
	memset(mixer->state->fm_sample_buffer, 0, sizeof(mixer->state->fm_sample_buffer));
	memset(mixer->state->psg_sample_buffer, 0, sizeof(mixer->state->psg_sample_buffer));
	mixer->state->fm_sample_buffer_write_index = 0;
	mixer->state->psg_sample_buffer_write_index = 0;
}

short* Mixer_AllocateFMSamples(const Mixer *mixer, size_t total_frames)
{
	short* const allocated_samples = &mixer->state->fm_sample_buffer[mixer->state->fm_sample_buffer_write_index];

	mixer->state->fm_sample_buffer_write_index += total_frames * MIXER_FM_CHANNEL_COUNT;

	return allocated_samples;
}

short* Mixer_AllocatePSGSamples(const Mixer *mixer, size_t total_frames)
{
	short* const allocated_samples = &mixer->state->psg_sample_buffer[mixer->state->psg_sample_buffer_write_index];

	mixer->state->psg_sample_buffer_write_index += total_frames * MIXER_PSG_CHANNEL_COUNT;

	return allocated_samples;
}

void Mixer_End(const Mixer *mixer, void (*callback)(const void *user_data, short *audio_samples, size_t total_frames), const void *user_data)
{
	/* Resample, mix, and output the audio for this frame. */
	mixer->state->fm_sample_buffer_read_index = 0;
	mixer->state->psg_sample_buffer_read_index = 0;

	for (;;)
	{
		/* Resample the FM and PSG outputs into their respective buffers. */
		const size_t total_resampled_fm_frames = ClownResampler_HighLevel_Resample(&mixer->state->fm_resampler, &mixer->constant->resampler_precomputed, mixer->state->fm_resampler_buffer, CC_COUNT_OF(mixer->state->fm_resampler_buffer) / MIXER_FM_CHANNEL_COUNT, FMResamplerCallback, mixer);
		const size_t total_resampled_psg_frames = ClownResampler_HighLevel_Resample(&mixer->state->psg_resampler, &mixer->constant->resampler_precomputed, mixer->state->psg_resampler_buffer, CC_COUNT_OF(mixer->state->psg_resampler_buffer) / MIXER_PSG_CHANNEL_COUNT, PSGResamplerCallback, mixer);

		const size_t frames_to_output = CC_MIN(total_resampled_fm_frames, total_resampled_psg_frames);

		size_t i;

		/* Mix the resampled PSG output into the resampled FM output. */
		/* There is no need for clamping because the samples are output low enough to never exceed the 16-bit limit. */
		for (i = 0; i < frames_to_output; ++i)
		{
			mixer->state->fm_resampler_buffer[i * 2 + 0] += mixer->state->psg_resampler_buffer[i];
			mixer->state->fm_resampler_buffer[i * 2 + 1] += mixer->state->psg_resampler_buffer[i];
		}

		/* Push the resampled, mixed audio to the device for playback. */
		callback(user_data, mixer->state->fm_resampler_buffer, frames_to_output);

		/* If the resampler has run out of data, then we're free to exit this loop. */
		if (frames_to_output != CC_COUNT_OF(mixer->state->fm_resampler_buffer) / MIXER_FM_CHANNEL_COUNT)
			break;
	}
}

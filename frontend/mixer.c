#include "mixer.h"

#include <stddef.h>
#include <string.h>

#include "../clowncommon.h"

#define CLOWNRESAMPLER_IMPLEMENTATION
#define CLOWNRESAMPLER_STATIC
#include "libraries/clownresampler/clownresampler.h"

static size_t FMResamplerInputCallback(const void *user_data, short *buffer, size_t buffer_size)
{
	Mixer* const mixer = (Mixer*)user_data;

	const size_t frames_to_do = CC_MIN(buffer_size, (mixer->state->fm_input_buffer_write_index - mixer->state->fm_input_buffer_read_index) / MIXER_FM_CHANNEL_COUNT);

	memcpy(buffer, &mixer->state->fm_input_buffer[mixer->state->fm_input_buffer_read_index], frames_to_do * sizeof(*mixer->state->fm_input_buffer) * MIXER_FM_CHANNEL_COUNT);

	mixer->state->fm_input_buffer_read_index += frames_to_do * MIXER_FM_CHANNEL_COUNT;

	return frames_to_do;
}

static size_t PSGResamplerInputCallback(const void *user_data, short *buffer, size_t buffer_size)
{
	Mixer* const mixer = (Mixer*)user_data;

	const size_t frames_to_do = CC_MIN(buffer_size, (mixer->state->psg_input_buffer_write_index - mixer->state->psg_input_buffer_read_index) / MIXER_PSG_CHANNEL_COUNT);

	memcpy(buffer, &mixer->state->psg_input_buffer[mixer->state->psg_input_buffer_read_index], frames_to_do * sizeof(*mixer->state->psg_input_buffer) * MIXER_PSG_CHANNEL_COUNT);

	mixer->state->psg_input_buffer_read_index += frames_to_do * MIXER_PSG_CHANNEL_COUNT;

	return frames_to_do;
}

/* There is no need for clamping in either of these callbacks because the
   samples are output low enough to never exceed the 16-bit limit. */

static char FMResamplerOutputCallback(const void *user_data, const long *frame, unsigned int channels)
{
	Mixer* const mixer = (Mixer*)user_data;

	unsigned int i;

	(void)channels;

	/* Copy the samples directly into the output buffer. */
	for (i = 0; i < MIXER_FM_CHANNEL_COUNT; ++i)
		*mixer->state->output_buffer_pointer++ = (short)*frame++;

	return mixer->state->output_buffer_pointer != &mixer->state->output_buffer[CC_COUNT_OF(mixer->state->output_buffer)];
}

static char PSGResamplerOutputCallback(const void *user_data, const long *frame, unsigned int channels)
{
	Mixer* const mixer = (Mixer*)user_data;
	const short sample = (short)*frame;

	unsigned int i;

	(void)channels;

	/* Upsample from mono to stereo and mix with the FM samples that are already in the output buffer. */
	for (i = 0; i < MIXER_FM_CHANNEL_COUNT; ++i)
		*mixer->state->output_buffer_pointer++ += sample;

	return mixer->state->output_buffer_pointer != &mixer->state->output_buffer[CC_COUNT_OF(mixer->state->output_buffer)];
}

void Mixer_Constant_Initialise(Mixer_Constant *constant)
{
	/* Compute clownresampler's lookup tables.*/
	ClownResampler_Precompute(&constant->resampler_precomputed);
}

void Mixer_State_Initialise(Mixer_State *state, unsigned long sample_rate, cc_bool pal_mode, cc_bool low_pass_filter)
{
	/* Divide and multiply by the frame rate to try to make the sample rate closer to the emulator's output. */
	const unsigned int pal_fm_sample_rate = CLOWNMDEMU_MULTIPLY_BY_PAL_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(CLOWNMDEMU_FM_SAMPLE_RATE_PAL));
	const unsigned int ntsc_fm_sample_rate = CLOWNMDEMU_MULTIPLY_BY_NTSC_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(CLOWNMDEMU_FM_SAMPLE_RATE_NTSC));

	const unsigned int pal_psg_sample_rate = CLOWNMDEMU_MULTIPLY_BY_PAL_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(CLOWNMDEMU_PSG_SAMPLE_RATE_PAL));
	const unsigned int ntsc_psg_sample_rate = CLOWNMDEMU_MULTIPLY_BY_NTSC_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(CLOWNMDEMU_PSG_SAMPLE_RATE_NTSC));

	const unsigned long low_pass_filter_sample_rate = low_pass_filter ? 22000 : sample_rate;

	ClownResampler_HighLevel_Init(&state->fm_resampler, MIXER_FM_CHANNEL_COUNT, pal_mode ? pal_fm_sample_rate : ntsc_fm_sample_rate, sample_rate, low_pass_filter_sample_rate);
	ClownResampler_HighLevel_Init(&state->psg_resampler, MIXER_PSG_CHANNEL_COUNT, pal_mode ? pal_psg_sample_rate : ntsc_psg_sample_rate, sample_rate, low_pass_filter_sample_rate);
}

void Mixer_Begin(const Mixer *mixer)
{
	/* Reset the audio buffers so that they can be mixed into. */
	memset(mixer->state->fm_input_buffer, 0, sizeof(mixer->state->fm_input_buffer));
	memset(mixer->state->psg_input_buffer, 0, sizeof(mixer->state->psg_input_buffer));
	mixer->state->fm_input_buffer_write_index = 0;
	mixer->state->psg_input_buffer_write_index = 0;
}

short* Mixer_AllocateFMSamples(const Mixer *mixer, size_t total_frames)
{
	short* const allocated_samples = &mixer->state->fm_input_buffer[mixer->state->fm_input_buffer_write_index];

	mixer->state->fm_input_buffer_write_index += total_frames * MIXER_FM_CHANNEL_COUNT;

	return allocated_samples;
}

short* Mixer_AllocatePSGSamples(const Mixer *mixer, size_t total_frames)
{
	short* const allocated_samples = &mixer->state->psg_input_buffer[mixer->state->psg_input_buffer_write_index];

	mixer->state->psg_input_buffer_write_index += total_frames * MIXER_PSG_CHANNEL_COUNT;

	return allocated_samples;
}

void Mixer_End(const Mixer *mixer, void (*callback)(const void *user_data, short *audio_samples, size_t total_frames), const void *user_data)
{
	size_t frames_to_output;

	/* Resample, mix, and output the audio for this frame. */
	mixer->state->fm_input_buffer_read_index = 0;
	mixer->state->psg_input_buffer_read_index = 0;

	do
	{
		size_t total_resampled_fm_frames;
		size_t total_resampled_psg_frames;

		/* Resample the FM and PSG outputs and mix them together into a single buffer. */
		mixer->state->output_buffer_pointer = mixer->state->output_buffer;
		ClownResampler_HighLevel_Resample(&mixer->state->fm_resampler, &mixer->constant->resampler_precomputed, FMResamplerInputCallback, FMResamplerOutputCallback, mixer);
		total_resampled_fm_frames = (mixer->state->output_buffer_pointer - mixer->state->output_buffer) / MIXER_FM_CHANNEL_COUNT;

		mixer->state->output_buffer_pointer = mixer->state->output_buffer;
		ClownResampler_HighLevel_Resample(&mixer->state->psg_resampler, &mixer->constant->resampler_precomputed, PSGResamplerInputCallback, PSGResamplerOutputCallback, mixer);
		total_resampled_psg_frames = (mixer->state->output_buffer_pointer - mixer->state->output_buffer) / MIXER_FM_CHANNEL_COUNT;

		/* In case there's a mismatch between the number of FM and PSG frames, output the smaller of the two. */
		frames_to_output = CC_MIN(total_resampled_fm_frames, total_resampled_psg_frames);

		/* Push the resampled, mixed audio to the device for playback. */
		callback(user_data, mixer->state->output_buffer, frames_to_output);

		/* If the resampler has run out of data, then we're free to exit this loop. */
	} while (frames_to_output == CC_COUNT_OF(mixer->state->output_buffer) / MIXER_FM_CHANNEL_COUNT);
}

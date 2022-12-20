#include <stddef.h>
#include <string.h>

#include "../clowncommon.h"

#define CLOWNRESAMPLER_IMPLEMENTATION
#define CLOWNRESAMPLER_STATIC
#include "libraries/clownresampler/clownresampler.h"

#define MIXER_FM_CHANNEL_COUNT 2
#define MIXER_PSG_CHANNEL_COUNT 1

#ifndef MIXER_FORMAT
#error "You need to define MIXER_FORMAT before including `mixer.c`."
#endif

typedef struct Mixer_Constant
{
	ClownResampler_Precomputed resampler_precomputed;
} Mixer_Constant;

typedef struct Mixer_State
{
	cc_s16l fm_input_buffer[MIXER_FM_CHANNEL_COUNT * CC_MAX(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(CLOWNMDEMU_FM_SAMPLE_RATE_NTSC), CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(CLOWNMDEMU_FM_SAMPLE_RATE_PAL))];
	size_t fm_input_buffer_write_index;
	size_t fm_input_buffer_read_index;
	ClownResampler_HighLevel_State fm_resampler;

	cc_s16l psg_input_buffer[MIXER_PSG_CHANNEL_COUNT * CC_MAX(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(CLOWNMDEMU_PSG_SAMPLE_RATE_NTSC), CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(CLOWNMDEMU_PSG_SAMPLE_RATE_PAL))];
	size_t psg_input_buffer_write_index;
	size_t psg_input_buffer_read_index;
	ClownResampler_HighLevel_State psg_resampler;

	MIXER_FORMAT output_buffer[0x400 * MIXER_FM_CHANNEL_COUNT];
	MIXER_FORMAT *output_buffer_pointer;
} Mixer_State;

typedef struct Mixer
{
	const Mixer_Constant *constant;
	Mixer_State *state;
} Mixer;

static size_t FMResamplerInputCallback(const void *user_data, cc_s16l *buffer, size_t buffer_size)
{
	const Mixer* const mixer = (const Mixer*)user_data;

	const size_t frames_to_do = CC_MIN(buffer_size, (mixer->state->fm_input_buffer_write_index - mixer->state->fm_input_buffer_read_index) / MIXER_FM_CHANNEL_COUNT);

	memcpy(buffer, &mixer->state->fm_input_buffer[mixer->state->fm_input_buffer_read_index], frames_to_do * sizeof(*mixer->state->fm_input_buffer) * MIXER_FM_CHANNEL_COUNT);

	mixer->state->fm_input_buffer_read_index += frames_to_do * MIXER_FM_CHANNEL_COUNT;

	return frames_to_do;
}

static size_t PSGResamplerInputCallback(const void *user_data, cc_s16l *buffer, size_t buffer_size)
{
	const Mixer* const mixer = (const Mixer*)user_data;

	const size_t frames_to_do = CC_MIN(buffer_size, (mixer->state->psg_input_buffer_write_index - mixer->state->psg_input_buffer_read_index) / MIXER_PSG_CHANNEL_COUNT);

	memcpy(buffer, &mixer->state->psg_input_buffer[mixer->state->psg_input_buffer_read_index], frames_to_do * sizeof(*mixer->state->psg_input_buffer) * MIXER_PSG_CHANNEL_COUNT);

	mixer->state->psg_input_buffer_read_index += frames_to_do * MIXER_PSG_CHANNEL_COUNT;

	return frames_to_do;
}

/* There is no need for clamping in either of these callbacks because the
   samples are output low enough to never exceed the 16-bit limit. */

static cc_bool FMResamplerOutputCallback(const void *user_data, const cc_s32f *frame, cc_u8f channels)
{
	const Mixer* const mixer = (const Mixer*)user_data;

	cc_u8f i;

	(void)channels;

	/* Copy the samples directly into the output buffer. */
	for (i = 0; i < MIXER_FM_CHANNEL_COUNT; ++i)
		*mixer->state->output_buffer_pointer++ = (MIXER_FORMAT)*frame++;

	return mixer->state->output_buffer_pointer != &mixer->state->output_buffer[CC_COUNT_OF(mixer->state->output_buffer)];
}

static cc_bool PSGResamplerOutputCallback(const void *user_data, const cc_s32f *frame, cc_u8f channels)
{
	const Mixer* const mixer = (const Mixer*)user_data;
	const MIXER_FORMAT sample = (MIXER_FORMAT)*frame;

	cc_u8f i;

	(void)channels;

	/* Upsample from mono to stereo and mix with the FM samples that are already in the output buffer. */
	for (i = 0; i < MIXER_FM_CHANNEL_COUNT; ++i)
		*mixer->state->output_buffer_pointer++ += sample;

	return mixer->state->output_buffer_pointer != &mixer->state->output_buffer[CC_COUNT_OF(mixer->state->output_buffer)];
}

static void Mixer_Constant_Initialise(Mixer_Constant *constant)
{
	/* Compute clownresampler's lookup tables.*/
	ClownResampler_Precompute(&constant->resampler_precomputed);
}

static void Mixer_State_Initialise(Mixer_State *state, cc_u32f sample_rate, cc_bool pal_mode, cc_bool low_pass_filter)
{
	/* Divide and multiply by the frame rate to try to make the sample rate closer to the emulator's output. */
	const cc_u32f pal_fm_sample_rate = CLOWNMDEMU_MULTIPLY_BY_PAL_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(CLOWNMDEMU_FM_SAMPLE_RATE_PAL));
	const cc_u32f ntsc_fm_sample_rate = CLOWNMDEMU_MULTIPLY_BY_NTSC_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(CLOWNMDEMU_FM_SAMPLE_RATE_NTSC));

	const cc_u32f pal_psg_sample_rate = CLOWNMDEMU_MULTIPLY_BY_PAL_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(CLOWNMDEMU_PSG_SAMPLE_RATE_PAL));
	const cc_u32f ntsc_psg_sample_rate = CLOWNMDEMU_MULTIPLY_BY_NTSC_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(CLOWNMDEMU_PSG_SAMPLE_RATE_NTSC));

	const cc_u32f low_pass_filter_sample_rate = low_pass_filter ? 22000 : sample_rate;

	ClownResampler_HighLevel_Init(&state->fm_resampler, MIXER_FM_CHANNEL_COUNT, pal_mode ? pal_fm_sample_rate : ntsc_fm_sample_rate, sample_rate, low_pass_filter_sample_rate);
	ClownResampler_HighLevel_Init(&state->psg_resampler, MIXER_PSG_CHANNEL_COUNT, pal_mode ? pal_psg_sample_rate : ntsc_psg_sample_rate, sample_rate, low_pass_filter_sample_rate);
}

static void Mixer_Begin(const Mixer *mixer)
{
	/* Reset the audio buffers so that they can be mixed into. */
	memset(mixer->state->fm_input_buffer, 0, sizeof(mixer->state->fm_input_buffer));
	memset(mixer->state->psg_input_buffer, 0, sizeof(mixer->state->psg_input_buffer));
	mixer->state->fm_input_buffer_write_index = 0;
	mixer->state->psg_input_buffer_write_index = 0;
}

static cc_s16l* Mixer_AllocateFMSamples(const Mixer *mixer, size_t total_frames)
{
	cc_s16l* const allocated_samples = &mixer->state->fm_input_buffer[mixer->state->fm_input_buffer_write_index];

	mixer->state->fm_input_buffer_write_index += total_frames * MIXER_FM_CHANNEL_COUNT;

	return allocated_samples;
}

static cc_s16l* Mixer_AllocatePSGSamples(const Mixer *mixer, size_t total_frames)
{
	cc_s16l* const allocated_samples = &mixer->state->psg_input_buffer[mixer->state->psg_input_buffer_write_index];

	mixer->state->psg_input_buffer_write_index += total_frames * MIXER_PSG_CHANNEL_COUNT;

	return allocated_samples;
}

static void Mixer_End(const Mixer *mixer, void (*callback)(const void *user_data, MIXER_FORMAT *audio_samples, size_t total_frames), const void *user_data)
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

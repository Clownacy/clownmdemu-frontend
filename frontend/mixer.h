#ifndef MIXER_H
#define MIXER_H

#include <stddef.h>

#include "../clowncommon/clowncommon.h"

#define CLOWNRESAMPLER_STATIC
#include "libraries/clownresampler/clownresampler.h"

#include "../clownmdemu.h"

#define MIXER_FM_CHANNEL_COUNT 2
#define MIXER_PSG_CHANNEL_COUNT 1

#ifdef __cplusplus
extern "C" {
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

	cc_s16l output_buffer[0x400 * MIXER_FM_CHANNEL_COUNT];
	cc_s16l *output_buffer_pointer;
} Mixer_State;

typedef struct Mixer
{
	const Mixer_Constant *constant;
	Mixer_State *state;
} Mixer;

void Mixer_Constant_Initialise(Mixer_Constant *constant);
void Mixer_State_Initialise(Mixer_State *state, cc_u32f sample_rate, cc_bool pal_mode, cc_bool low_pass_filter);
void Mixer_Begin(const Mixer *mixer);
cc_s16l* Mixer_AllocateFMSamples(const Mixer *mixer, size_t total_frames);
cc_s16l* Mixer_AllocatePSGSamples(const Mixer *mixer, size_t total_frames);
void Mixer_End(const Mixer *mixer, void (*callback)(const void *user_data, cc_s16l *audio_samples, size_t total_frames), const void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* MIXER_H */

#ifndef MIXER_H
#define MIXER_H

#include <stddef.h>

#include "../clowncommon.h"

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
	unsigned int output_sample_rate;

	short fm_sample_buffer[MIXER_FM_CHANNEL_COUNT * CC_MAX(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(CLOWNMDEMU_FM_SAMPLE_RATE_NTSC), CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(CLOWNMDEMU_FM_SAMPLE_RATE_PAL))];
	size_t fm_sample_buffer_write_index;
	size_t fm_sample_buffer_read_index;
	ClownResampler_HighLevel_State fm_resampler;
	short fm_resampler_buffer[0x400 * MIXER_FM_CHANNEL_COUNT];

	short psg_sample_buffer[MIXER_PSG_CHANNEL_COUNT * CC_MAX(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(CLOWNMDEMU_PSG_SAMPLE_RATE_NTSC), CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(CLOWNMDEMU_PSG_SAMPLE_RATE_PAL))];
	size_t psg_sample_buffer_write_index;
	size_t psg_sample_buffer_read_index;
	ClownResampler_HighLevel_State psg_resampler;
	short psg_resampler_buffer[0x400 * MIXER_PSG_CHANNEL_COUNT];
} Mixer_State;

typedef struct Mixer
{
	const Mixer_Constant *constant;
	Mixer_State *state;
} Mixer;

void Mixer_Constant_Initialise(Mixer_Constant *constant);
void Mixer_State_Initialise(Mixer_State *state, unsigned long sample_rate);
void Mixer_SetPALMode(const Mixer *mixer, cc_bool enabled);
void Mixer_Begin(const Mixer *mixer);
short* Mixer_AllocateFMSamples(const Mixer *mixer, size_t total_frames);
short* Mixer_AllocatePSGSamples(const Mixer *mixer, size_t total_frames);
void Mixer_End(const Mixer *mixer, void (*callback)(const void *user_data, short *audio_samples, size_t total_frames), const void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* MIXER_H */

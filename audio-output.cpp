#include "audio-output.h"

#include <stdexcept>

#define CLOWNRESAMPLER_ASSERT SDL_assert
#define CLOWNRESAMPLER_FABS SDL_fabs
#define CLOWNRESAMPLER_SIN SDL_sin
#define CLOWNRESAMPLER_ZERO(buffer, size) SDL_memset(buffer, 0, size)
#define CLOWNRESAMPLER_MEMMOVE SDL_memmove
#define MIXER_IMPLEMENTATION
#define MIXER_FORMAT Sint16
#define MIXER_ASSERT SDL_assert
#define MIXER_FREE SDL_free
#define MIXER_CALLOC SDL_calloc
#define MIXER_MEMMOVE SDL_memmove
#define MIXER_MEMSET SDL_memset
#include "clownmdemu-frontend-common/mixer.h"

#define SIZE_OF_FRAME (sizeof(Sint16) * MIXER_FM_CHANNEL_COUNT)

Mixer_Constant AudioOutputInner::mixer_constant;
bool AudioOutputInner::mixer_constant_initialised;

AudioOutputInner::AudioOutputInner()
{
	SDL_AudioSpec want, have;

	SDL_zero(want);
	want.freq = 48000;
	want.format = AUDIO_S16;
	want.channels = MIXER_FM_CHANNEL_COUNT;
	// We want a 10ms buffer (this value must be a power of two).
	want.samples = 1;
	while (want.samples < (want.freq * want.channels) / (1000 / 10))
		want.samples <<= 1;
	want.callback = nullptr;

	device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);

	if (device == 0)
		throw std::runtime_error(std::string("SDL_OpenAudioDevice failed with the following message - '") + SDL_GetError() + "'");

	total_buffer_frames = static_cast<cc_u32f>(have.samples / have.channels);
	sample_rate = static_cast<cc_u32f>(have.freq);

	// Initialise the mixer.
	if (!mixer_constant_initialised)
	{
		mixer_constant_initialised = true;
		Mixer_Constant_Initialise(&mixer_constant);
	}
	Mixer_State_Initialise(&mixer_state, sample_rate, pal_mode, low_pass_filter);

	// Unpause audio device, so that playback can begin.
	SDL_PauseAudioDevice(device, 0);
}

AudioOutputInner::~AudioOutputInner()
{
	SDL_PauseAudioDevice(device, 1);
	Mixer_State_Deinitialise(&mixer_state);
	SDL_CloseAudioDevice(device);
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
	const unsigned long target_frames = GetTargetFrames();
	const Uint32 queued_frames = SDL_GetQueuedAudioSize(device) / SIZE_OF_FRAME;

	rolling_average_buffer[rolling_average_buffer_index] = queued_frames;
	rolling_average_buffer_index = (rolling_average_buffer_index + 1) % CC_COUNT_OF(rolling_average_buffer);

	// If there is too much audio, just drop it because the dynamic rate control will be unable to handle it.
	if (queued_frames < target_frames * 2)
	{
		const auto callback = [](void* const user_data, Sint16* const audio_samples, const std::size_t total_frames)
		{
			const AudioOutputInner *audio_output = static_cast<const AudioOutputInner*>(user_data);

			SDL_QueueAudio(audio_output->device, audio_samples, static_cast<Uint32>(total_frames * SIZE_OF_FRAME));
		};

		// Hans-Kristian Arntzen's Dynamic Rate Control formula.
		// https://github.com/libretro/docs/blob/master/archive/ratecontrol.pdf
		const cc_u32f divisor = target_frames * 0x100; // The number here is the inverse of the formula's 'd' value.
		Mixer_End(&mixer, queued_frames - target_frames + divisor, divisor, callback, this);
	}
}

cc_s16l* AudioOutputInner::MixerAllocateFMSamples(const std::size_t total_samples)
{
	return Mixer_AllocateFMSamples(&mixer, total_samples);
}

cc_s16l* AudioOutputInner::MixerAllocatePSGSamples(const std::size_t total_samples)
{
	return Mixer_AllocatePSGSamples(&mixer, total_samples);
}

cc_u32f AudioOutputInner::GetAverageFrames() const
{
	Uint32 average_queued_frames = 0;

	for (const auto value : rolling_average_buffer)
	average_queued_frames += value;

	average_queued_frames /= CC_COUNT_OF(rolling_average_buffer);

	return average_queued_frames;
}

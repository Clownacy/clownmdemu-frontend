#include "audio-output.h"

#define MIXER_IMPLEMENTATION
#define MIXER_FORMAT Sint16
#include "clownmdemu-frontend-common/mixer.h"

#define SIZE_OF_FRAME (sizeof(Sint16) * MIXER_FM_CHANNEL_COUNT)

Mixer_Constant AudioOutput::mixer_constant;
bool AudioOutput::mixer_constant_initialised;

bool AudioOutput::Initialise()
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
	{
		debug_log.Log("SDL_OpenAudioDevice failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		buffer_size = have.size;
		sample_rate = static_cast<unsigned long>(have.freq);

		// Initialise the mixer.
		if (!mixer_constant_initialised)
		{
			mixer_constant_initialised = true;
			Mixer_Constant_Initialise(&mixer_constant);
		}
		Mixer_State_Initialise(&mixer_state, sample_rate, pal_mode, low_pass_filter);

		// Unpause audio device, so that playback can begin.
		SDL_PauseAudioDevice(device, 0);

		return true;
	}

	return false;
}

void AudioOutput::Deinitialise()
{
	if (device != 0)
		SDL_CloseAudioDevice(device);
}

void AudioOutput::MixerBegin()
{
	if (mixer_update_pending)
	{
		mixer_update_pending = false;
		Mixer_State_Initialise(&mixer_state, sample_rate, pal_mode, low_pass_filter);
	}

	Mixer_Begin(&mixer);
}

void AudioOutput::MixerEnd()
{
	if (device != 0)
	{
		const unsigned long target_frames = sample_rate / 40; // 25ms
		const Uint32 queued_frames = SDL_GetQueuedAudioSize(device) / SIZE_OF_FRAME;

		// If there is too much audio, just drop it because the dynamic rate control will be unable to handle it.
		if (queued_frames < target_frames * 2)
		{
			const auto callback = [](const void* const user_data, Sint16* const audio_samples, const std::size_t total_frames)
			{
				const AudioOutput *audio_output = static_cast<const AudioOutput*>(user_data);

				SDL_QueueAudio(audio_output->device, audio_samples, static_cast<Uint32>(total_frames * SIZE_OF_FRAME));
			};

			// Hans-Kristian Arntzen's Dynamic Rate Control formula.
			// https://github.com/libretro/docs/blob/master/archive/ratecontrol.pdf
			const cc_u32f divisor = target_frames * 0x100; // The number here is the inverse of the formula's 'd' value.
			Mixer_End(&mixer, queued_frames - target_frames + divisor, divisor, callback, this);
		}
	}
}

cc_s16l* AudioOutput::MixerAllocateFMSamples(const std::size_t total_samples)
{
	return Mixer_AllocateFMSamples(&mixer, total_samples);
}

cc_s16l* AudioOutput::MixerAllocatePSGSamples(const std::size_t total_samples)
{
	return Mixer_AllocatePSGSamples(&mixer, total_samples);
}

#include "audio.h"

#define MIXER_IMPLEMENTATION
#define MIXER_FORMAT Sint16
#include "clownmdemu-frontend-common/mixer.h"

Mixer_Constant AudioStatic::mixer_constant;
bool AudioStatic::mixer_constant_initialised;

bool Audio::Initialise()
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

	audio_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);

	if (audio_device == 0)
	{
		debug_log.Log("SDL_OpenAudioDevice failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		audio_device_buffer_size = have.size;
		audio_device_sample_rate = static_cast<unsigned long>(have.freq);

		// Initialise the mixer.
		if (!AudioStatic::mixer_constant_initialised)
		{
			AudioStatic::mixer_constant_initialised = true;
			Mixer_Constant_Initialise(&AudioStatic::mixer_constant);
		}
		Mixer_State_Initialise(&mixer_state, audio_device_sample_rate, pal_mode, low_pass_filter);

		// Unpause audio device, so that playback can begin.
		SDL_PauseAudioDevice(audio_device, 0);

		return true;
	}

	return false;
}

void Audio::Deinitialise()
{
	if (audio_device != 0)
		SDL_CloseAudioDevice(audio_device);
}

void Audio::MixerBegin()
{
	Mixer_Begin(&mixer);
}

void Audio::MixerEnd()
{
	// If there's a lot of audio queued, then don't queue any more.
	if (SDL_GetQueuedAudioSize(audio_device) < audio_device_buffer_size * 4)
	{
		const auto callback = [](const void* const user_data, Sint16* const audio_samples, const size_t total_frames)
		{
			const Audio *audio = reinterpret_cast<const Audio*>(user_data);

			if (audio->audio_device != 0)
				SDL_QueueAudio(audio->audio_device, audio_samples, static_cast<Uint32>(total_frames * sizeof(Sint16) * MIXER_FM_CHANNEL_COUNT));
		};

		Mixer_End(&mixer, callback, this);
	}
}

cc_s16l* Audio::MixerAllocateFMSamples(const std::size_t total_samples)
{
	return Mixer_AllocateFMSamples(&mixer, total_samples);
}

cc_s16l* Audio::MixerAllocatePSGSamples(const std::size_t total_samples)
{
	return Mixer_AllocatePSGSamples(&mixer, total_samples);
}

void Audio::SetPALMode(const bool enabled)
{
	pal_mode = enabled;

	if (audio_device != 0)
		Mixer_State_Initialise(&mixer_state, audio_device_sample_rate, pal_mode, low_pass_filter);
}

void Audio::SetLowPassFilter(const bool enabled)
{
	low_pass_filter = enabled;

	if (audio_device != 0)
		Mixer_State_Initialise(&mixer_state, audio_device_sample_rate, pal_mode, low_pass_filter);
}
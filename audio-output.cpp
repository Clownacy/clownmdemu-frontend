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
		Mixer_State_Initialise(&mixer_state, sample_rate, frame_rate, pal_mode, low_pass_filter);

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
		Mixer_State_Initialise(&mixer_state, sample_rate, frame_rate, pal_mode, low_pass_filter);
	}

	Mixer_Begin(&mixer);
}

void AudioOutput::MixerEnd()
{
	if (device != 0)
	{
		const auto callback = [](const void* const user_data, Sint16* const audio_samples, const std::size_t total_frames)
		{
			const AudioOutput *audio_output = static_cast<const AudioOutput*>(user_data);

			// If there's a lot of audio queued, then don't queue any more.
			// This is to keep audio latency to a fixed minimum.
			const std::size_t maximum_frames = audio_output->sample_rate / 20; // 50ms
			const std::size_t queued_frames = SDL_GetQueuedAudioSize(audio_output->device) / SIZE_OF_FRAME;
			const std::size_t frames_remaining = queued_frames >= maximum_frames ? 0 : maximum_frames - queued_frames;
			const std::size_t frames_to_send = CC_MIN(total_frames, frames_remaining);

			//if (frames_to_send != total_frames)
			//	audio_output->debug_log.Log("%zd audio frames discarded.", total_frames - frames_to_send);

			SDL_QueueAudio(audio_output->device, audio_samples, static_cast<Uint32>(frames_to_send * SIZE_OF_FRAME));
		};

		Mixer_End(&mixer, callback, this);
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

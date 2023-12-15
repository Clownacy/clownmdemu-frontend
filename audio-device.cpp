#include "audio-device.h"

#include <stdexcept>
#include <string>

AudioDevice::AudioDevice(const cc_u8f channels, cc_u32f &sample_rate, cc_u32f &total_buffer_frames)
	: channels(channels)
{
	SDL_AudioSpec want, have;

	SDL_zero(want);
	want.freq = 48000;
	want.format = AUDIO_S16SYS;
	want.channels = channels;
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

	// Unpause audio device, so that playback can begin.
	SDL_PauseAudioDevice(device, 0);
}

AudioDevice::~AudioDevice()
{
	SDL_CloseAudioDevice(device);
}

void AudioDevice::QueueFrames(const cc_s16l *buffer, cc_u32f total_frames)
{
	SDL_QueueAudio(device, buffer, total_frames * SIZE_OF_FRAME);
}

cc_u32f AudioDevice::GetTotalQueuedFrames()
{
	return SDL_GetQueuedAudioSize(device) / SIZE_OF_FRAME;
}


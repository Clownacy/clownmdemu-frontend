#include "emulator-with-cd-reader.h"

#include <cassert>

void EmulatorWithCDReader::FMAudioToBeGenerated(const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	generate_fm_audio(clownmdemu, audio_output.MixerAllocateFMSamples(total_frames), total_frames);
}

void EmulatorWithCDReader::PSGAudioToBeGenerated(const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	generate_psg_audio(clownmdemu, audio_output.MixerAllocatePSGSamples(total_frames), total_frames);
}

void EmulatorWithCDReader::PCMAudioToBeGenerated(const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	generate_pcm_audio(clownmdemu, audio_output.MixerAllocatePCMSamples(total_frames), total_frames);
}

void EmulatorWithCDReader::CDDAAudioToBeGenerated(const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	generate_cdda_audio(clownmdemu, audio_output.MixerAllocateCDDASamples(total_frames), total_frames);
}

void EmulatorWithCDReader::CDSeeked(const cc_u32f sector_index)
{
	cd_reader.SeekToSector(sector_index);
}

void EmulatorWithCDReader::CDSectorRead(cc_u16l* const buffer)
{
	cd_reader.ReadSector(buffer);
}

cc_bool EmulatorWithCDReader::CDTrackSeeked(const cc_u16f track_index, const ClownMDEmu_CDDAMode mode)
{
	CDReader::PlaybackSetting playback_setting;

	switch (mode)
	{
		default:
			assert(false);
			return cc_false;

		case CLOWNMDEMU_CDDA_PLAY_ALL:
			playback_setting = CDReader::PlaybackSetting::ALL;
			break;

		case CLOWNMDEMU_CDDA_PLAY_ONCE:
			playback_setting = CDReader::PlaybackSetting::ONCE;
			break;

		case CLOWNMDEMU_CDDA_PLAY_REPEAT:
			playback_setting = CDReader::PlaybackSetting::REPEAT;
			break;
	}

	return cd_reader.PlayAudio(track_index, playback_setting);
}

std::size_t EmulatorWithCDReader::CDAudioRead(cc_s16l* const sample_buffer, const std::size_t total_frames)
{
	return cd_reader.ReadAudio(sample_buffer, total_frames);
}

void EmulatorWithCDReader::Iterate()
{
	// Reset the audio buffers so that they can be mixed into.
	audio_output.MixerBegin();

	Emulator::Iterate();

	// Resample, mix, and output the audio for this frame.
	audio_output.MixerEnd();
}

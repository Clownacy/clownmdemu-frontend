#include "emulator-with-cd-reader.h"

#include <cassert>

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

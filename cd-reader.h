#ifndef CD_READER_H
#define CD_READER_H

#include <array>
#include <cstddef>
#include <filesystem>

#include "libraries/clowncd/clowncd.h"

#include "sdl-wrapper.h"

class CDReader
{
public:
	enum class PlaybackSetting
	{
		ALL,
		ONCE,
		REPEAT
	};

	using SectorIndex = cc_u32f;
	using TrackIndex = cc_u16f;
	using FrameIndex = std::size_t;

private:
	ClownCD clowncd;
	bool open = false;
	PlaybackSetting playback_setting = PlaybackSetting::ALL;
	bool audio_playing = false;

public:
	struct State
	{
		CDReader::TrackIndex track_index;
		CDReader::SectorIndex sector_index;
		CDReader::FrameIndex frame_index;
		PlaybackSetting playback_setting;
		bool audio_playing;
	};

	static constexpr cc_u16f SECTOR_SIZE = 2048;
	using Sector = std::array<cc_u8l, SECTOR_SIZE>;

	CDReader() = default;
	~CDReader()
	{
		Close();
	}
	CDReader(const CDReader &other) = delete;
	CDReader(CDReader &&other) = delete;
	CDReader& operator=(const CDReader &other) = delete;
	CDReader& operator=(CDReader &&other) = delete;
	void Open(SDL::RWops &&stream, const std::filesystem::path &path);
	void Close()
	{
		if (!IsOpen())
			return;

		ClownCD_Close(&clowncd);
		open = false;
	}
	bool IsOpen() const { return open; }
	bool SeekToSector(SectorIndex sector_index);
	bool SeekToFrame(FrameIndex frame_index);
	Sector ReadSector();
	Sector ReadSector(SectorIndex sector_index);
	bool PlayAudio(TrackIndex track_index, PlaybackSetting setting);
	cc_u32f ReadAudio(cc_s16l *sample_buffer, cc_u32f total_frames);

	State GetState();
	bool SetState(const State &state);
};

#endif // CD_READER_H

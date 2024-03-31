#ifndef CD_READER_H
#define CD_READER_H

#include <cstddef>
#include <array>

#include "libraries/clowncd/clowncd.h"

#include "sdl-wrapper.h"

class CDReader
{
public:
	using SectorIndex = cc_u32f;
	using TrackIndex = cc_u16f;
	using FrameIndex = std::size_t;

private:
	class ClownCDWrapper
	{
	private:
		bool open = false;

	public:
		ClownCD data;

		ClownCDWrapper() {}
		ClownCDWrapper(ClownCD clowncd) { Open(clowncd); }
		~ClownCDWrapper() { Close(); }
		ClownCDWrapper(const ClownCDWrapper &other) = delete;
		ClownCDWrapper& operator=(const ClownCDWrapper &other) = delete;

		void Open(const ClownCD clowncd)
		{
			if (IsOpen())
				Close();

			data = clowncd;
			open = true;
		}

		void Close()
		{
			if (!IsOpen())
				return;

			ClownCD_Close(&data); 
			open = false;
		}

		bool IsOpen() const
		{
			return open;
		}
	};

	ClownCDWrapper clowncd;
	TrackIndex current_track_index = 0;
	SectorIndex current_sector_index = 0;
	FrameIndex current_frame_index = 0;

public:
	struct State
	{
		CDReader::TrackIndex track_index;
		CDReader::SectorIndex sector_index;
		CDReader::FrameIndex frame_index;
	};

	static constexpr cc_u16f SECTOR_SIZE = 2048;
	using Sector = std::array<cc_u8l, SECTOR_SIZE>;

	CDReader() = default;
	void Open(SDL::RWops &&stream, const char* filename);
	void Close() { clowncd.Close(); }
	bool IsOpen() const { return clowncd.IsOpen(); }
	void SeekToSector(SectorIndex sector_index);
	Sector ReadSector();
	Sector ReadSector(SectorIndex sector_index);
	SectorIndex GetCurrentSector() { return current_sector_index; }
	ClownCD_CueTrackType SeekToTrack(TrackIndex track_index);
	cc_u32f ReadAudio(cc_s16l *sample_buffer, cc_u32f total_frames);

	State GetState();
	void SetState(const State &state);
};

#endif // CD_READER_H

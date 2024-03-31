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
	SectorIndex current_sector_index = 0;

public:
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
	bool SeekToTrack(TrackIndex track_index);
	cc_u32f ReadAudio(cc_s16l *sample_buffer, cc_u32f total_frames);
};

#endif // CD_READER_H

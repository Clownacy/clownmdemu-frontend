#ifndef CD_READER_H
#define CD_READER_H

#include <stddef.h>

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#include "sdl-wrapper.h"

class CDReader
{
public:
	using SectorIndex = cc_u32f;
	using TrackIndex = cc_u16f;

private:
	enum class Format
	{
		RAW,
		CLOWNCD
	};

	SDL::RWops stream;
	Format format;
	TrackIndex total_tracks, header_size;
	bool sector_size_2352;

	SectorIndex current_sector_index;
	cc_u32f remaining_frames_in_track;
	static constexpr cc_u16f EXTENDED_SECTOR_SIZE = 2352;

	static cc_u32f ReadUIntBE(SDL::RWops &stream, cc_u8f total_bytes);
	static cc_u16f ReadU16BE(SDL::RWops &stream) { return ReadUIntBE(stream, 2); }
	static cc_u32f ReadU32BE(SDL::RWops &stream) { return ReadUIntBE(stream, 4); }
	static cc_u16f ReadS16BE(SDL::RWops &stream);
	static cc_u32f ReadS32BE(SDL::RWops &stream);
	Format DetermineFormat();
	TrackIndex DetermineTotalTracks();
	cc_u16f DetermineHeaderSize();
	bool DetermineSectorSize();

public:
	static constexpr cc_u16f SECTOR_SIZE = 2048;
	using Sector = std::array<cc_u8l, SECTOR_SIZE>;

	CDReader() = default;
	CDReader(SDL::RWops &&stream);
	void Close() { stream = nullptr; }
	bool IsOpen() const { return stream.get() != nullptr; }
	void SeekToSector(SectorIndex sector_index);
	Sector ReadSector();
	Sector ReadSector(SectorIndex sector_index);
	SectorIndex GetCurrentSector() { return current_sector_index; }
	void SeekToTrack(TrackIndex track_index);
	cc_u32f ReadAudio(cc_s16l *sample_buffer, cc_u32f total_frames);
};

#endif // CD_READER_H

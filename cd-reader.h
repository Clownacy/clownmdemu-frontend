#ifndef CD_READER_H
#define CD_READER_H

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#include "sdl-wrapper.h"

class CDReader
{
public:
	using SectorIndex = cc_u32f;

private:
	SDL::RWops stream;
	bool sector_size_2352;
	SectorIndex current_sector_index;
	static constexpr cc_u16f EXTENDED_SECTOR_SIZE = 2352;

	static bool DetermineSectorSize(SDL::RWops &stream);

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
};

#endif // CD_READER_H

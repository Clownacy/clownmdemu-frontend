#ifndef CD_READER_H
#define CD_READER_H

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#include "sdl-wrapper.h"

class CDReader
{
private:
	SDL::RWops stream;
	bool sector_size_2352;
	static constexpr cc_u16f EXTENDED_SECTOR_SIZE = 2352;

	static bool DetermineSectorSize(SDL::RWops &stream);

public:
	static constexpr cc_u16f SECTOR_SIZE = 2048;
	using Sector = std::array<cc_u8l, SECTOR_SIZE>;

	CDReader() = default;
	CDReader(SDL::RWops &&stream);
	void CDReader::Close() { stream = nullptr; }
	bool CDReader::IsOpen() const { return stream.get() != nullptr; }
	void CDReader::SeekToSector(cc_u32f sector_index);
	Sector CDReader::ReadSector();
};

#endif // CD_READER_H

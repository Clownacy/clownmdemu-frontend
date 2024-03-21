#include "cd-reader.h"

#include <array>
#include <ios>
#include <utility>

CDReader::CDReader(SDL::RWops &&stream) : stream(std::move(stream)), sector_size_2352(DetermineSectorSize(this->stream))
{
	SeekToSector(0);
}

bool CDReader::DetermineSectorSize(SDL::RWops &stream)
{
	// Detect if the sector size is 2048 bytes or 2352 bytes.
	SDL_RWseek(stream.get(), 0, RW_SEEK_SET);

	std::array<unsigned char, 0x10> buffer;
	SDL_RWread(stream.get(), buffer.data(), buffer.size(), 1);

	constexpr std::array<unsigned char, 0x10> sector_header = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x02, 0x00, 0x01};

	return buffer == sector_header;
}

void CDReader::SeekToSector(const SectorIndex sector_index)
{
	current_sector_index = sector_index;

	if (stream)
		SDL_RWseek(stream.get(), sector_size_2352 ? sector_index * EXTENDED_SECTOR_SIZE + 0x10 : sector_index * SECTOR_SIZE, RW_SEEK_SET);
}

CDReader::Sector CDReader::ReadSector()
{
	Sector sector_buffer;
	SDL_RWread(stream.get(), sector_buffer.data(), sector_buffer.size(), 1);

	if (sector_size_2352)
		SDL_RWseek(stream.get(), EXTENDED_SECTOR_SIZE - SECTOR_SIZE, SEEK_CUR);

	++current_sector_index;

	return sector_buffer;
}

CDReader::Sector CDReader::ReadSector(const SectorIndex sector_index)
{
	const auto sector_backup = GetCurrentSector();
	SeekToSector(sector_index);
	const Sector sector = ReadSector();
	SeekToSector(sector_backup);
	return sector;
}
#include "cd-reader.h"

#include <array>
#include <ios>
#include <utility>

CDReader::CDReader(SDL::RWops &&stream)
	: stream(std::move(stream))
	, format(DetermineFormat())
	, total_tracks(DetermineTotalTracks())
	, header_size(DetermineHeaderSize())
	, sector_size_2352(DetermineSectorSize())
{
	SeekToSector(0);
}

cc_u32f CDReader::ReadUIntBE(SDL::RWops &stream, const cc_u8f total_bytes)
{
	std::array<unsigned char, 4> bytes;

	SDL_assert(total_bytes <= bytes.size());

	if (SDL_RWread(stream.get(), bytes.data(), total_bytes, 1) < 1)
		return 0;

	cc_u32f value = 0;

	for (cc_u8f i = 0; i < total_bytes; ++i)
	{
		value <<= 8;
		value |= bytes[i];
	}

	return value;
}

cc_u16f CDReader::ReadS16BE(SDL::RWops &stream)
{
	const cc_u16f value = ReadU16BE(stream);

	return CC_SIGN_EXTEND(cc_u16f, 16 - 1, value);
}

cc_u32f CDReader::ReadS32BE(SDL::RWops &stream)
{
	const cc_u32f value = ReadU32BE(stream);

	return CC_SIGN_EXTEND(cc_u32f, 32 - 1, value);
}

cc_u32f CDReader::ReadUIntLE(SDL::RWops &stream, const cc_u8f total_bytes)
{
	std::array<unsigned char, 4> bytes;

	SDL_assert(total_bytes <= bytes.size());

	if (SDL_RWread(stream.get(), bytes.data(), total_bytes, 1) < 1)
		return 0;

	cc_u32f value = 0;

	for (cc_u8f i = 0; i < total_bytes; ++i)
	{
		value <<= 8;
		value |= bytes[total_bytes - i - 1];
	}

	return value;
}

cc_u16f CDReader::ReadS16LE(SDL::RWops &stream)
{
	const cc_u16f value = ReadU16LE(stream);

	return CC_SIGN_EXTEND(cc_u16f, 16 - 1, value);
}

cc_u32f CDReader::ReadS32LE(SDL::RWops &stream)
{
	const cc_u32f value = ReadU32LE(stream);

	return CC_SIGN_EXTEND(cc_u32f, 32 - 1, value);
}

CDReader::Format CDReader::DetermineFormat()
{
	SDL_RWseek(stream.get(), 0, RW_SEEK_SET);

	std::array<char, 8> identifier_buffer;
	if (SDL_RWread(stream.get(), identifier_buffer.data(), identifier_buffer.size(), 1) < 1)
		return Format::RAW;

	constexpr std::array<char, 8> identifier = {'c', 'l', 'o', 'w', 'n', 'c', 'd', '\0'};

	if (identifier_buffer != identifier)
		return Format::RAW;

	const cc_u16f version = ReadU16BE(stream);

	if (version != 0)
		return Format::RAW;

	return Format::CLOWNCD;
}

CDReader::TrackIndex CDReader::DetermineTotalTracks()
{
	if (format == Format::RAW)
		return 1;

	SDL_RWseek(stream.get(), 8 + 2, RW_SEEK_SET);

	const TrackIndex total_tracks = ReadU16BE(stream);

	return total_tracks;
}

cc_u16f CDReader::DetermineHeaderSize()
{
	return format == Format::RAW ? 0 : 12 + total_tracks * 10;
}

bool CDReader::DetermineSectorSize()
{
	// Detect if the sector size is 2048 bytes or 2352 bytes.
	SDL_RWseek(stream.get(), header_size, RW_SEEK_SET);

	std::array<unsigned char, 0x10> buffer;
	SDL_RWread(stream.get(), buffer.data(), buffer.size(), 1);

	constexpr std::array<unsigned char, 0x10> sector_header = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x02, 0x00, 0x01};

	return buffer == sector_header;
}

void CDReader::SeekToSector(const SectorIndex sector_index)
{
	current_sector_index = sector_index;
	remaining_frames_in_track = 0; // Prevent playing music after trying to read data.

	if (stream)
		SDL_RWseek(stream.get(), header_size + (sector_size_2352 ? sector_index * EXTENDED_SECTOR_SIZE + 0x10 : sector_index * SECTOR_SIZE), RW_SEEK_SET);
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

cc_bool CDReader::SeekToTrack(const TrackIndex track_index)
{
	if (track_index >= total_tracks)
	{
		remaining_frames_in_track = 0;
		return cc_false;
	}

	SDL_RWseek(stream.get(), 12 + track_index * 10, SEEK_SET);

	ReadU16BE(stream);
	const SectorIndex start_sector = ReadU32BE(stream);
	const cc_u32f total_sectors = ReadU32BE(stream);

	SeekToSector(start_sector);
	remaining_frames_in_track = total_sectors * (EXTENDED_SECTOR_SIZE / (2 * 2));

	return cc_true;
}

cc_u32f CDReader::ReadAudio(cc_s16l* const sample_buffer, const cc_u32f total_frames)
{
	const cc_u32f frames_to_do = std::min(remaining_frames_in_track, total_frames);

	remaining_frames_in_track -= frames_to_do;

	for (cc_u32f i = 0; i < frames_to_do * 2; ++i)
		sample_buffer[i] = ReadS16LE(stream);

	return frames_to_do;
}

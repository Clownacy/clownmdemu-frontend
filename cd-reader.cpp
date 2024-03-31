#include "cd-reader.h"

#include <climits>

static void* FileOpenCallback(const char* const filename, const ClownCD_FileMode mode)
{
	const char *mode_string;

	switch (mode)
	{
		case CLOWNCD_RB:
			mode_string = "rb";
			break;

		case CLOWNCD_WB:
			mode_string = "wb";
			break;

		default:
			return nullptr;
	}

	return SDL_RWFromFile(filename, mode_string);
}

static int FileCloseCallback(void* const stream)
{
	return SDL_RWclose((SDL_RWops*)stream);
}

static size_t FileReadCallback(void* const buffer, const std::size_t size, const std::size_t count, void* const stream)
{
	return SDL_RWread((SDL_RWops*)stream, buffer, size, count);
}

static size_t FileWriteCallback(const void* const buffer, const std::size_t size, const std::size_t count, void* const stream)
{
	return SDL_RWwrite((SDL_RWops*)stream, buffer, size, count);
}

static long FileTellCallback(void* const stream)
{
	const auto position = SDL_RWtell((SDL_RWops*)stream);

	if (position < LONG_MIN || position > LONG_MAX)
		return -1L;

	return position;
}

static int FileSeekCallback(void* const stream, const long position, const ClownCD_FileOrigin origin)
{
	int whence;

	switch (origin)
	{
		case CLOWNCD_SEEK_SET:
			whence = RW_SEEK_SET;
			break;

		case CLOWNCD_SEEK_CUR:
			whence = RW_SEEK_CUR;
			break;

		case CLOWNCD_SEEK_END:
			whence = RW_SEEK_END;
			break;

		default:
			return -1;
	}

	return SDL_RWseek((SDL_RWops*)stream, position, whence) == -1 ? -1 : 0;
}

static const ClownCD_FileCallbacks callbacks = {FileOpenCallback, FileCloseCallback, FileReadCallback, FileWriteCallback, FileTellCallback, FileSeekCallback};

void CDReader::Open(SDL::RWops &&stream, const char* const filename)
{
	// Transfer ownership of the stream to ClownCD.
	clowncd.Open(ClownCD_OpenAlreadyOpen(stream.release(), filename, &callbacks));
}

bool CDReader::SeekToSector(const SectorIndex sector_index)
{
	if (!IsOpen())
		return false;

	SeekToTrack(1);

	if (/*current_track_type != CLOWNCD_CUE_TRACK_MODE1_2048 &&*/ current_track_type != CLOWNCD_CUE_TRACK_MODE1_2352)
		return false;

	current_sector_index = sector_index;

	return ClownCD_SeekSector(&clowncd.data, sector_index);
}

CDReader::Sector CDReader::ReadSector()
{
	Sector sector;

	if (!IsOpen())
	{
		sector.fill(0);
		return sector;
	}

	++current_sector_index;
	ClownCD_ReadSector(&clowncd.data, sector.data());
	return sector;
}

CDReader::Sector CDReader::ReadSector(const SectorIndex sector_index)
{
	Sector sector;

	if (!IsOpen())
	{
		sector.fill(0);
		return sector;
	}

	SeekToSector(sector_index);
	return ReadSector();
}

bool CDReader::SeekToTrack(const TrackIndex track_index)
{
	if (!IsOpen())
		return false;

	current_track_index = track_index;
	current_sector_index = 0;
	current_frame_index = 0;
	current_track_type = ClownCD_SeekTrackIndex(&clowncd.data, track_index, 1);

	return current_track_type != CLOWNCD_CUE_TRACK_INVALID;
}

bool CDReader::SeekToFrame(const FrameIndex frame_index)
{
	current_frame_index = frame_index;
	return ClownCD_SeekAudioFrame(&clowncd.data, frame_index);
}

cc_u32f CDReader::ReadAudio(cc_s16l* const sample_buffer, const cc_u32f total_frames)
{
	if (!IsOpen())
		return 0;

	const auto frames_read = ClownCD_ReadFrames(&clowncd.data, sample_buffer, total_frames);

	current_frame_index += frames_read;

	return frames_read;
}

CDReader::State CDReader::GetState()
{
	State state;
	state.track_index = current_track_index;
	state.sector_index = current_sector_index;
	state.frame_index = current_frame_index;
	return state;
}

bool CDReader::SetState(const State &state)
{
	if (!IsOpen())
		return false;

	if (!SeekToTrack(state.track_index))
		return false;

	switch (current_track_type)
	{
		case CLOWNCD_CUE_TRACK_INVALID:
			break;

		case CLOWNCD_CUE_TRACK_MODE1_2048:
		case CLOWNCD_CUE_TRACK_MODE1_2352:
			if (!SeekToSector(state.sector_index))
				return false;

			break;

		case CLOWNCD_CUE_TRACK_AUDIO:
			if (!SeekToFrame(state.frame_index))
				return false;

			break;
	}

	return true;
}

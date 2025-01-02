#ifndef CD_READER_H
#define CD_READER_H

#include <array>
#include <cstddef>
#include <filesystem>

#include "sdl-wrapper.h"

#include "common/cd-reader.h"

class CDReader
{
public:
	enum class PlaybackSetting
	{
		ALL    = CDREADER_PLAYBACK_ALL,
		ONCE   = CDREADER_PLAYBACK_ONCE,
		REPEAT = CDREADER_PLAYBACK_REPEAT
	};

	using SectorIndex = CDReader_SectorIndex;
	using TrackIndex  = CDReader_TrackIndex;
	using FrameIndex  = CDReader_FrameIndex;

private:
	CDReader_State state;

	static void* FileOpenCallback(const char *filename, ClownCD_FileMode mode);
	static int FileCloseCallback(void *stream);
	static std::size_t FileReadCallback(void *buffer, std::size_t size, std::size_t count, void *stream);
	static std::size_t FileWriteCallback(const void *buffer, std::size_t size, std::size_t count, void *stream);
	static long FileTellCallback(void *stream);
	static int FileSeekCallback(void *stream, long position, ClownCD_FileOrigin origin);

	static constexpr ClownCD_FileCallbacks callbacks = {FileOpenCallback, FileCloseCallback, FileReadCallback, FileWriteCallback, FileTellCallback, FileSeekCallback};

public:
	using State = CDReader_StateBackup;

	static constexpr cc_u16f SECTOR_SIZE = CDREADER_SECTOR_SIZE;

	CDReader()
	{
		CDReader_Initialise(&state);
	}
	~CDReader()
	{
		CDReader_Deinitialise(&state);
	}
	CDReader(const CDReader &other) = delete;
	CDReader(CDReader &&other) = delete;
	CDReader& operator=(const CDReader &other) = delete;
	CDReader& operator=(CDReader &&other) = delete;
	void Open(void* const stream, const std::filesystem::path &path)
	{
		CDReader_Open(&state, stream, reinterpret_cast<const char*>(path.u8string().c_str()), &callbacks);
	}
	void Open(SDL::RWops &&stream, const std::filesystem::path &path)
	{
		// Transfer ownership of the stream to ClownCD.
		Open(stream.release(), path);
	}
	void Close()
	{
		CDReader_Close(&state);
	}
	bool IsOpen() const
	{
		return CDReader_IsOpen(&state);
	}
	bool SeekToSector(const SectorIndex sector_index)
	{
		return CDReader_SeekToSector(&state, sector_index);
	}
	bool SeekToFrame(const FrameIndex frame_index)
	{
		return CDReader_SeekToFrame(&state, frame_index);
	}
	void ReadSector(cc_u16l* const buffer)
	{
		CDReader_ReadSector(&state, buffer);
	}
	bool PlayAudio(const TrackIndex track_index, const PlaybackSetting setting)
	{
		return CDReader_PlayAudio(&state, track_index, static_cast<CDReader_PlaybackSetting>(setting));
	}
	cc_u32f ReadAudio(cc_s16l* const sample_buffer, const cc_u32f total_frames)
	{
		return CDReader_ReadAudio(&state, sample_buffer, total_frames);
	}

	State GetState()
	{
		CDReader_StateBackup backup;
		CDReader_GetStateBackup(&state, &backup);
		return backup;
	}
	bool SetState(const State &state)
	{
		return CDReader_SetStateBackup(&this->state, &state);
	}

	bool ReadMegaCDHeaderSector(unsigned char* const buffer)
	{
		return CDReader_ReadMegaCDHeaderSector(&state, buffer);
	}
	bool IsMegaCDGame()
	{
		return CDReader_IsMegaCDGame(&state);
	}
	static bool IsMegaCDGame(const std::filesystem::path &path)
	{
		CDReader cd_reader;
		cd_reader.Open(nullptr, path);
		const bool is_mega_cd_game = cd_reader.IsMegaCDGame();
		cd_reader.Close();
		return is_mega_cd_game;
	}
};

#endif /* CD_READER_H */

#ifndef CD_READER_H
#define CD_READER_H

#include <cstddef>
#include <filesystem>

#include <SDL3/SDL.h>

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
	class StateBackup : private CDReader_StateBackup
	{
	public:
		StateBackup(const CDReader &cd_reader)
		{
			CDReader_GetStateBackup(&cd_reader.state, this);
		}

		void Apply(CDReader &cd_reader) const
		{
			CDReader_SetStateBackup(&cd_reader.state, this);
		}
	};

	static constexpr cc_u16f SECTOR_SIZE = CDREADER_SECTOR_SIZE;

	CDReader()
	{
		CDReader_Initialise(&state);
	}
	CDReader(const std::filesystem::path &path, SDL_IOStream* const stream = nullptr)
		: CDReader()
	{
		Open(path, stream);
	}
	~CDReader()
	{
		CDReader_Deinitialise(&state);
	}
	CDReader(const CDReader &other) = delete;
	CDReader(CDReader &&other) = delete;
	CDReader& operator=(const CDReader &other) = delete;
	CDReader& operator=(CDReader &&other) = delete;
	void Open(const std::filesystem::path &path, SDL_IOStream* const stream = nullptr)
	{
		CDReader_Open(&state, stream, reinterpret_cast<const char*>(path.u8string().c_str()), &callbacks);
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

	bool ReadMegaCDHeaderSector(unsigned char* const buffer)
	{
		return CDReader_ReadMegaCDHeaderSector(&state, buffer);
	}
	bool IsMegaCDGame()
	{
		return CDReader_IsMegaCDGame(&state);
	}
	bool IsDefinitelyACD()
	{
		return CDReader_IsDefinitelyACD(&state);
	}
	static bool IsMegaCDGame(const std::filesystem::path &path)
	{
		return CDReader(path).IsMegaCDGame();
	}
	static bool IsDefinitelyACD(const std::filesystem::path &path)
	{
		return CDReader(path).IsDefinitelyACD();
	}
};

#endif /* CD_READER_H */

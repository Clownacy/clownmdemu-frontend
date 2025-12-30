#ifndef CD_READER_H
#define CD_READER_H

#include <cstddef>
#include <filesystem>

#include <SDL3/SDL.h>

#include "common/cd-reader.h"

class CDReader : private CDReader_State
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
			CDReader_GetStateBackup(&cd_reader, this);
		}

		void Apply(CDReader &cd_reader) const
		{
			CDReader_SetStateBackup(&cd_reader, this);
		}
	};

	static constexpr cc_u16f SECTOR_SIZE = CDREADER_SECTOR_SIZE;

	CDReader()
	{
		CDReader_Initialise(this);
	}
	CDReader(const std::filesystem::path &path, SDL_IOStream* const stream = nullptr)
		: CDReader()
	{
		Open(path, stream);
	}
	~CDReader()
	{
		CDReader_Deinitialise(this);
	}
	CDReader(const CDReader &other) = delete;
	CDReader(CDReader &&other) = delete;
	CDReader& operator=(const CDReader &other) = delete;
	CDReader& operator=(CDReader &&other) = delete;
	void Open(const std::filesystem::path &path, SDL_IOStream* const stream = nullptr)
	{
		CDReader_Open(this, stream, reinterpret_cast<const char*>(path.u8string().c_str()), &callbacks);
	}
	void Close()
	{
		CDReader_Close(this);
	}
	bool IsOpen() const
	{
		return CDReader_IsOpen(this);
	}
	bool SeekToSector(const SectorIndex sector_index)
	{
		return CDReader_SeekToSector(this, sector_index);
	}
	bool SeekToFrame(const FrameIndex frame_index)
	{
		return CDReader_SeekToFrame(this, frame_index);
	}
	void ReadSector(cc_u16l* const buffer)
	{
		CDReader_ReadSector(this, buffer);
	}
	bool PlayAudio(const TrackIndex track_index, const PlaybackSetting setting)
	{
		return CDReader_PlayAudio(this, track_index, static_cast<CDReader_PlaybackSetting>(setting));
	}
	cc_u32f ReadAudio(cc_s16l* const sample_buffer, const cc_u32f total_frames)
	{
		return CDReader_ReadAudio(this, sample_buffer, total_frames);
	}

	bool ReadMegaCDHeaderSector(unsigned char* const buffer)
	{
		return CDReader_ReadMegaCDHeaderSector(this, buffer);
	}
	bool IsMegaCDGame()
	{
		return CDReader_IsMegaCDGame(this);
	}
	bool IsDefinitelyACD()
	{
		return CDReader_IsDefinitelyACD(this);
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

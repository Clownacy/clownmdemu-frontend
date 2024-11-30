#ifndef CD_READER_H
#define CD_READER_H

#include <stddef.h>

#ifdef __cplusplus
	#include <array>
	#include <cstddef>
	#include <filesystem>

	#include "sdl-wrapper.h"
#endif

#include "libraries/clowncd/clowncd.h"

#define CDREADER_SECTOR_SIZE 2048

typedef cc_u32f CDReader_SectorIndex;
typedef cc_u16f CDReader_TrackIndex;
typedef size_t CDReader_FrameIndex;
typedef cc_u8l CDReader_Sector[CDREADER_SECTOR_SIZE];

typedef enum CDReader_PlaybackSetting
{
	CDREADER_PLAYBACK_ALL,
	CDREADER_PLAYBACK_ONCE,
	CDREADER_PLAYBACK_REPEAT
} CDReader_PlaybackSetting;

typedef struct CDReader_State
{
	ClownCD clowncd;
	cc_bool open;
	CDReader_PlaybackSetting playback_setting;
	cc_bool audio_playing;
} CDReader_State;

typedef struct CDReader_StateBackup
{
	CDReader_TrackIndex track_index;
	CDReader_SectorIndex sector_index;
	CDReader_FrameIndex frame_index;
	CDReader_PlaybackSetting playback_setting;
	cc_bool audio_playing;
} CDReader_StateBackup;

#ifdef __cplusplus
extern "C" {
#endif

void CDReader_Initialise(CDReader_State *state);
void CDReader_Deinitialise(CDReader_State *state);
void CDReader_Open(CDReader_State *state, void *stream, const char *path);
void CDReader_Close(CDReader_State *state);
cc_bool CDReader_IsOpen(const CDReader_State *state);
cc_bool CDReader_SeekToSector(CDReader_State *state, CDReader_SectorIndex sector_index);
cc_bool CDReader_SeekToFrame(CDReader_State *state, CDReader_FrameIndex frame_index);
void CDReader_ReadSector(CDReader_State *state, CDReader_Sector *sector);
void CDReader_ReadSectorAt(CDReader_State *state, CDReader_Sector *sector, CDReader_SectorIndex sector_index);
cc_bool CDReader_PlayAudio(CDReader_State *state, CDReader_TrackIndex track_index, CDReader_PlaybackSetting setting);
cc_u32f CDReader_ReadAudio(CDReader_State *state, cc_s16l *sample_buffer, cc_u32f total_frames);
void CDReader_GetStateBackup(CDReader_State *state, CDReader_StateBackup *backup);
cc_bool CDReader_SetStateBackup(CDReader_State *state, const CDReader_StateBackup *backup);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
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
	using TrackIndex = CDReader_TrackIndex;
	using FrameIndex = CDReader_FrameIndex;

private:
	CDReader_State state;

public:
	using State = CDReader_StateBackup;

	static constexpr cc_u16f SECTOR_SIZE = CDREADER_SECTOR_SIZE;
	using Sector = std::array<cc_u8l, SECTOR_SIZE>;

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
	void Open(SDL::RWops &&stream, const std::filesystem::path &path)
	{
		// Transfer ownership of the stream to ClownCD.
		CDReader_Open(&state, stream.release(), reinterpret_cast<const char*>(path.u8string().c_str()));
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
	Sector ReadSector()
	{
		Sector sector;
		CDReader_ReadSector(&state, reinterpret_cast<CDReader_Sector*>(sector.data()));
		return sector;
	}
	Sector ReadSector(const SectorIndex sector_index)
	{
		Sector sector;
		CDReader_ReadSectorAt(&state, reinterpret_cast<CDReader_Sector*>(sector.data()), sector_index);
		return sector;
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
};
#endif

#endif /* CD_READER_H */

#include "cd-reader.h"

#include <climits>
#include <string.h>

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
	return SDL_RWclose(static_cast<SDL_RWops*>(stream));
}

static size_t FileReadCallback(void* const buffer, const std::size_t size, const std::size_t count, void* const stream)
{
	return SDL_RWread(static_cast<SDL_RWops*>(stream), buffer, size, count);
}

static size_t FileWriteCallback(const void* const buffer, const std::size_t size, const std::size_t count, void* const stream)
{
	return SDL_RWwrite(static_cast<SDL_RWops*>(stream), buffer, size, count);
}

static long FileTellCallback(void* const stream)
{
	const auto position = SDL_RWtell(static_cast<SDL_RWops*>(stream));

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

	return SDL_RWseek(static_cast<SDL_RWops*>(stream), position, whence) == -1 ? -1 : 0;
}

static const ClownCD_FileCallbacks callbacks = {FileOpenCallback, FileCloseCallback, FileReadCallback, FileWriteCallback, FileTellCallback, FileSeekCallback};

void CDReader_Initialise(CDReader_State* const state)
{
	state->open = cc_false;
	state->playback_setting = CDREADER_PLAYBACK_ALL;
	state->audio_playing = cc_false;
}

void CDReader_Deinitialise(CDReader_State* const state)
{
	CDReader_Close(state);
}

void CDReader_Open(CDReader_State* const state, void* const stream, const char* const path)
{
	if (CDReader_IsOpen(state))
		CDReader_Close(state);

	state->clowncd = ClownCD_OpenAlreadyOpen(stream, path, &callbacks);
	state->open = cc_true;
	state->audio_playing = cc_false;
}

void CDReader_Close(CDReader_State* const state)
{
	if (!CDReader_IsOpen(state))
		return;

	ClownCD_Close(&state->clowncd);
	state->open = cc_false;
}

cc_bool CDReader_IsOpen(const CDReader_State* const state)
{
	return state->open;
}

cc_bool CDReader_SeekToSector(CDReader_State* const state, const CDReader_SectorIndex sector_index)
{
	ClownCD_CueTrackType track_type;

	if (!CDReader_IsOpen(state))
		return cc_false;

	track_type = ClownCD_SeekTrackIndex(&state->clowncd, 1, 1);

	if (track_type != CLOWNCD_CUE_TRACK_MODE1_2048 && track_type != CLOWNCD_CUE_TRACK_MODE1_2352)
		return cc_false;

	return ClownCD_SeekSector(&state->clowncd, sector_index);
}

void CDReader_ReadSector(CDReader_State* const state, CDReader_Sector* const sector)
{
	if (!CDReader_IsOpen(state))
	{
		memset(sector, 0, sizeof(*sector));
		return;
	}

	ClownCD_ReadSector(&state->clowncd, *sector);
}

void CDReader_ReadSectorAt(CDReader_State* const state, CDReader_Sector* const sector, const CDReader_SectorIndex sector_index)
{
	if (!CDReader_IsOpen(state))
	{
		memset(sector, 0, sizeof(*sector));
		return;
	}

	CDReader_SeekToSector(state, sector_index);
	ClownCD_ReadSector(&state->clowncd, *sector);
}

cc_bool CDReader_PlayAudio(CDReader_State* const state, const CDReader_TrackIndex track_index, const CDReader_PlaybackSetting setting)
{
	if (!CDReader_IsOpen(state))
		return cc_false;

	state->audio_playing = cc_false;

	if (ClownCD_SeekTrackIndex(&state->clowncd, track_index, 1) != CLOWNCD_CUE_TRACK_AUDIO)
		return cc_false;

	state->audio_playing = cc_true;
	state->playback_setting = setting;

	return cc_true;
}

cc_bool CDReader_SeekToFrame(CDReader_State* const state, const CDReader_FrameIndex frame_index)
{
	if (!ClownCD_SeekAudioFrame(&state->clowncd, frame_index))
	{
		state->audio_playing = cc_false;
		return cc_false;
	}

	return cc_true;
}

cc_u32f CDReader_ReadAudio(CDReader_State* const state, cc_s16l* const sample_buffer, const cc_u32f total_frames)
{
	cc_u32f frames_read = 0;

	if (!CDReader_IsOpen(state))
		return 0;

	if (!state->audio_playing)
		return 0;

	while (frames_read != total_frames)
	{
		frames_read += ClownCD_ReadFrames(&state->clowncd, &sample_buffer[frames_read * 2], total_frames - frames_read);

		if (frames_read != total_frames)
		{
			switch (state->playback_setting)
			{
				case CDREADER_PLAYBACK_ALL:
					if (!CDReader_PlayAudio(state, state->clowncd.track.current_track + 1, state->playback_setting))
						state->audio_playing = cc_false;
					break;

				case CDREADER_PLAYBACK_ONCE:
					state->audio_playing = cc_false;
					/* Fallthrough */
				case CDREADER_PLAYBACK_REPEAT:
					if (!CDReader_SeekToFrame(state, 0))
						state->audio_playing = cc_false;
					break;
			}

			if (!state->audio_playing)
				break;
		}
	}

	return frames_read;
}

void CDReader_GetStateBackup(CDReader_State* const state, CDReader_StateBackup* const backup)
{
	backup->track_index = state->clowncd.track.current_track;
	backup->sector_index = state->clowncd.track.current_sector;
	backup->frame_index = state->clowncd.track.current_frame;
	backup->playback_setting = state->playback_setting;
	backup->audio_playing = state->audio_playing;
}

cc_bool CDReader_SetStateBackup(CDReader_State* const state, const CDReader_StateBackup* const backup)
{
	if (!CDReader_IsOpen(state))
		return cc_false;

	if (ClownCD_SetState(&state->clowncd, backup->track_index, 1, backup->sector_index, backup->frame_index) == CLOWNCD_CUE_TRACK_INVALID)
		return cc_false;

	state->playback_setting = backup->playback_setting;
	state->audio_playing = backup->audio_playing;

	return cc_true;
}

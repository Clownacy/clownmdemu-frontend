#include "emulator.h"

Emulator::Constant Emulator::constant;

void Emulator::Callback_ColourUpdated(void* const user_data, const cc_u16f index, const cc_u16f colour)
{
	static_cast<Emulator*>(user_data)->ColourUpdated(index, colour);
}

void Emulator::Callback_ScanlineRendered(void* const user_data, const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f left_boundary, const cc_u16f right_boundary, const cc_u16f screen_width, const cc_u16f screen_height)
{
	static_cast<Emulator*>(user_data)->ScanlineRendered(scanline, pixels, left_boundary, right_boundary, screen_width, screen_height);
}

cc_bool Emulator::Callback_InputRequested(void* const user_data, const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
	return static_cast<Emulator*>(user_data)->InputRequested(player_id, button_id);
}

void Emulator::Callback_FMAudioToBeGenerated(void* const user_data, ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_fm_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	static_cast<Emulator*>(user_data)->FMAudioToBeGenerated(clownmdemu, total_frames, generate_fm_audio);
}

void Emulator::Callback_PSGAudioToBeGenerated(void* const user_data, ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_psg_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	static_cast<Emulator*>(user_data)->PSGAudioToBeGenerated(clownmdemu, total_frames, generate_psg_audio);
}

void Emulator::Callback_PCMAudioToBeGenerated(void* const user_data, ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_pcm_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	static_cast<Emulator*>(user_data)->PCMAudioToBeGenerated(clownmdemu, total_frames, generate_pcm_audio);
}

void Emulator::Callback_CDDAAudioToBeGenerated(void* const user_data, ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_cdda_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	static_cast<Emulator*>(user_data)->CDDAAudioToBeGenerated(clownmdemu, total_frames, generate_cdda_audio);
}

void Emulator::Callback_CDSeeked(void* const user_data, const cc_u32f sector_index)
{
	static_cast<Emulator*>(user_data)->CDSeeked(sector_index);
}

void Emulator::Callback_CDSectorRead(void* const user_data, cc_u16l* const buffer)
{
	static_cast<Emulator*>(user_data)->CDSectorRead(buffer);
}

cc_bool Emulator::Callback_CDTrackSeeked(void* const user_data, const cc_u16f track_index, const ClownMDEmu_CDDAMode mode)
{
	return static_cast<Emulator*>(user_data)->CDTrackSeeked(track_index, mode);
}

std::size_t Emulator::Callback_CDAudioRead(void* const user_data, cc_s16l* const sample_buffer, const std::size_t total_frames)
{
	return static_cast<Emulator*>(user_data)->CDAudioRead(sample_buffer, total_frames);
}

cc_bool Emulator::Callback_SaveFileOpenedForReading(void* const user_data, const char* const filename)
{
	return static_cast<Emulator*>(user_data)->SaveFileOpenedForReading(filename);
}

cc_s16f Emulator::Callback_SaveFileRead(void* const user_data)
{
	return static_cast<Emulator*>(user_data)->SaveFileRead();
}

cc_bool Emulator::Callback_SaveFileOpenedForWriting(void* const user_data, const char* const filename)
{
	return static_cast<Emulator*>(user_data)->SaveFileOpenedForWriting(filename);
}

void Emulator::Callback_SaveFileWritten(void* const user_data, const cc_u8f byte)
{
	static_cast<Emulator*>(user_data)->SaveFileWritten(byte);
}

void Emulator::Callback_SaveFileClosed(void* const user_data)
{
	static_cast<Emulator*>(user_data)->SaveFileClosed();
}

cc_bool Emulator::Callback_SaveFileRemoved(void* const user_data, const char* const filename)
{
	return static_cast<Emulator*>(user_data)->SaveFileRemoved(filename);
}

cc_bool Emulator::Callback_SaveFileSizeObtained(void* const user_data, const char* const filename, std::size_t* const size)
{
	return static_cast<Emulator*>(user_data)->SaveFileSizeObtained(filename, size);
}

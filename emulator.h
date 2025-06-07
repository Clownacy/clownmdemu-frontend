#ifndef EMULATOR_H
#define EMULATOR_H

#include <cstdarg>
#include <cstddef>
#include <functional>

#include "common/core/clownmdemu.h"

class Emulator : protected ClownMDEmu
{
public:
	class Configuration : public ClownMDEmu_Configuration
	{
		friend Emulator;

		public:
			Configuration()
			{
				// Default-initialise the underlying struct.
				*static_cast<ClownMDEmu_Configuration*>(this) = {};
			}
	};

	class Constant : protected ClownMDEmu_Constant
	{
		friend Emulator;

		public:
			Constant()
			{
				ClownMDEmu_Constant_Initialise(this);
			}
	};

	class State : protected ClownMDEmu_State
	{
		friend Emulator;

		public:
			State()
			{
				ClownMDEmu_State_Initialise(this);
			}
	};

	using LogCallback = std::function<void(const char *format, std::va_list arg)>;

protected:
	static cc_u8f Callback_CartridgeRead(void *user_data, cc_u32f address);
	static void Callback_CartridgeWritten(void *user_data, cc_u32f address, cc_u8f value);
	static void Callback_ColourUpdated(void *user_data, cc_u16f index, cc_u16f colour);
	static void Callback_ScanlineRendered(void *user_data, cc_u16f scanline, const cc_u8l *pixels, cc_u16f left_boundary, cc_u16f right_boundary, cc_u16f screen_width, cc_u16f screen_height);
	static cc_bool Callback_InputRequested(void *user_data, cc_u8f player_id, ClownMDEmu_Button button_id);

	static void Callback_FMAudioToBeGenerated(void *user_data, const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
	static void Callback_PSGAudioToBeGenerated(void *user_data, const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
	static void Callback_PCMAudioToBeGenerated(void *user_data, const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
	static void Callback_CDDAAudioToBeGenerated(void *user_data, const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));

	static void Callback_CDSeeked(void *user_data, cc_u32f sector_index);
	static void Callback_CDSectorRead(void *user_data, cc_u16l *buffer);
	static cc_bool Callback_CDTrackSeeked(void *user_data, cc_u16f track_index, ClownMDEmu_CDDAMode mode);
	static std::size_t Callback_CDAudioRead(void *user_data, cc_s16l *sample_buffer, std::size_t total_frames);

	static cc_bool Callback_SaveFileOpenedForReading(void *user_data, const char *filename);
	static cc_s16f Callback_SaveFileRead(void *user_data);
	static cc_bool Callback_SaveFileOpenedForWriting(void *user_data, const char *filename);
	static void Callback_SaveFileWritten(void *user_data, cc_u8f byte);
	static void Callback_SaveFileClosed(void *user_data);
	static cc_bool Callback_SaveFileRemoved(void *user_data, const char *filename);
	static cc_bool Callback_SaveFileSizeObtained(void *user_data, const char *filename, std::size_t *size);

	virtual cc_u8f CartridgeRead(cc_u32f address) = 0;
	virtual void CartridgeWritten(cc_u32f address, cc_u8f value) = 0;
	virtual void ColourUpdated(cc_u16f index, cc_u16f colour) = 0;
	virtual void ScanlineRendered(cc_u16f scanline, const cc_u8l *pixels, cc_u16f left_boundary, cc_u16f right_boundary, cc_u16f screen_width, cc_u16f screen_height) = 0;
	virtual cc_bool InputRequested(cc_u8f player_id, ClownMDEmu_Button button_id) = 0;

	virtual void FMAudioToBeGenerated(std::size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) = 0;
	virtual void PSGAudioToBeGenerated(std::size_t total_frames, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) = 0;
	virtual void PCMAudioToBeGenerated(std::size_t total_frames, void (*generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) = 0;
	virtual void CDDAAudioToBeGenerated(std::size_t total_frames, void (*generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) = 0;

	virtual void CDSeeked(cc_u32f sector_index) = 0;
	virtual void CDSectorRead(cc_u16l *buffer) = 0;
	virtual cc_bool CDTrackSeeked(cc_u16f track_index, ClownMDEmu_CDDAMode mode) = 0;
	virtual std::size_t CDAudioRead(cc_s16l *sample_buffer, std::size_t total_frames) = 0;

	virtual cc_bool SaveFileOpenedForReading(const char *filename) = 0;
	virtual cc_s16f SaveFileRead() = 0;
	virtual cc_bool SaveFileOpenedForWriting(const char *filename) = 0;
	virtual void SaveFileWritten(cc_u8f byte) = 0;
	virtual void SaveFileClosed() = 0;
	virtual cc_bool SaveFileRemoved(const char *filename) = 0;
	virtual cc_bool SaveFileSizeObtained(const char *filename, std::size_t *size) = 0;

	static void LogCallbackWrapper(void* const user_data, const char *format, std::va_list arg)
	{
		(*static_cast<const LogCallback*>(user_data))(format, arg);
	}

	const ClownMDEmu_Callbacks callbacks = {
		this,
		Callback_CartridgeRead,
		Callback_CartridgeWritten,
		Callback_ColourUpdated,
		Callback_ScanlineRendered,
		Callback_InputRequested,
		Callback_FMAudioToBeGenerated,
		Callback_PSGAudioToBeGenerated,
		Callback_PCMAudioToBeGenerated,
		Callback_CDDAAudioToBeGenerated,
		Callback_CDSeeked,
		Callback_CDSectorRead,
		Callback_CDTrackSeeked,
		Callback_CDAudioRead,
		Callback_SaveFileOpenedForReading,
		Callback_SaveFileRead,
		Callback_SaveFileOpenedForWriting,
		Callback_SaveFileWritten,
		Callback_SaveFileClosed,
		Callback_SaveFileRemoved,
		Callback_SaveFileSizeObtained
	};

public:
	Emulator(const Configuration &configuration, const Constant &constant, State &state)
	{
		*static_cast<ClownMDEmu*>(this) = CLOWNMDEMU_PARAMETERS_INITIALISE(&configuration, &constant, &state, &callbacks);
	}

	void Reset(const cc_bool cd_boot, const cc_u32f cartridge_size)
	{
		ClownMDEmu_Reset(this, cd_boot, cartridge_size);
	}

	void Iterate()
	{
		ClownMDEmu_Iterate(this);
	}

	void SetLogCallback(const LogCallback &callback)
	{
		ClownMDEmu_SetLogCallback(&LogCallbackWrapper, &callback);
	}
};

#endif // EMULATOR_H

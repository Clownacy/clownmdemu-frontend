#ifndef EMULATOR_H
#define EMULATOR_H

#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>

#include "common/core/clownmdemu.h"

class Emulator
{
private:
	class Constant
	{
	public:
		Constant()
		{
			ClownMDEmu_Constant_Initialise();
		}
	};

	static Constant constant;

public:
	class Configuration : public ClownMDEmu_Configuration
	{
	public:
		Configuration()
			: ClownMDEmu_Configuration({})
		{}
	};

	class State : public ClownMDEmu_State
	{
	public:
		State()
		{
			ClownMDEmu_State_Initialise(this);
		}
	};

	using LogCallbackFormatted = std::function<void(const char *format, std::va_list arg)>;
	using LogCallbackPlain = std::function<void(const std::string &message)>;

protected:
	class Parameters
	{
	protected:
		ClownMDEmu clownmdemu;

	public:
		Parameters(const ClownMDEmu_Configuration &configuration, ClownMDEmu_State &state, const ClownMDEmu_Callbacks &callbacks)
			: clownmdemu(CLOWNMDEMU_PARAMETERS_INITIALISE(&configuration, &state, &callbacks))
		{}

		const ClownMDEmu& operator*() const
		{
			return clownmdemu;
		}

		ClownMDEmu& operator*()
		{
			return clownmdemu;
		}

		const ClownMDEmu* operator->() const
		{
			return &**this;
		}

		ClownMDEmu* operator->()
		{
			return &**this;
		}
	};

	const ClownMDEmu_Callbacks callbacks = {
		this,
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

private:
	State state;

protected:
	Parameters parameters;
	LogCallbackFormatted log_callback;

private:
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

	virtual void ColourUpdated(cc_u16f index, cc_u16f colour) = 0;
	virtual void ScanlineRendered(cc_u16f scanline, const cc_u8l *pixels, cc_u16f left_boundary, cc_u16f right_boundary, cc_u16f screen_width, cc_u16f screen_height) = 0;
	virtual cc_bool InputRequested(cc_u8f player_id, ClownMDEmu_Button button_id) = 0;

	virtual void FMAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) = 0;
	virtual void PSGAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) = 0;
	virtual void PCMAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) = 0;
	virtual void CDDAAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) = 0;

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
		(*static_cast<const LogCallbackFormatted*>(user_data))(format, arg);
	}

public:
	Emulator(const Configuration &configuration)
		: parameters(configuration, state, callbacks)
	{}

	void InsertCartridge(const cc_u16l* const buffer, const cc_u32f buffer_length)
	{
		ClownMDEmu_SetCartridge(&*parameters, buffer, buffer_length);
	}

	void EjectCartridge()
	{
		ClownMDEmu_SetCartridge(&*parameters, nullptr, 0);
	}

	[[nodiscard]] bool IsCartridgeInserted() const
	{
		return parameters->cartridge_buffer_length != 0;
	}

	void SoftReset(const cc_bool cd_inserted)
	{
		ClownMDEmu_Reset(&*parameters, IsCartridgeInserted(), cd_inserted);
	}

	void HardReset(const cc_bool cd_inserted)
	{
		state = {};
		SoftReset(cd_inserted);
	}

	void Iterate()
	{
		ClownMDEmu_Iterate(&*parameters);
	}

	void SetLogCallback(const LogCallbackFormatted &callback)
	{
		log_callback = callback;
		ClownMDEmu_SetLogCallback(&LogCallbackWrapper, &log_callback);
	}

	void SetLogCallback(const LogCallbackPlain &callback)
	{
		SetLogCallback(
			[=](const char* const format, std::va_list arg)
			{
				// TODO: Use 'std::string::resize_and_overwrite' here when C++23 becomes the norm.
				std::va_list arg_copy;
				va_copy(arg_copy, arg);
				std::string string(std::vsnprintf(nullptr, 0, format, arg_copy), '\0');
				va_end(arg_copy);

				std::vsnprintf(std::data(string), std::size(string) + 1, format, arg);
				callback(string);
			}
		);
	}

	[[nodiscard]] const auto& GetState() const
	{
		return state;
	}

	void SetState(const State &state)
	{
		this->state = state;
	}

	[[nodiscard]] auto& GetExternalRAM()
	{
		return state.external_ram.buffer;
	}
};

#endif // EMULATOR_H

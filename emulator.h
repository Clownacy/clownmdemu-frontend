#ifndef EMULATOR_H
#define EMULATOR_H

#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>
#include <type_traits>

#include "common/core/clownmdemu.h"

class Emulator : protected ClownMDEmu
{
	////////////////
	// Subclasses //
	////////////////

private:
	class Constant
	{
	public:
		Constant()
		{
			ClownMDEmu_Constant_Initialise();
		}
	};

	static inline Constant constant;

public:
	class InitialConfiguration : private ClownMDEmu_InitialConfiguration
	{
		friend Emulator;

	public:
		InitialConfiguration()
			: ClownMDEmu_InitialConfiguration()
		{}

#define EMULATOR_CONFIGURATION_AS_IS(VALUE) VALUE
#define EMULATOR_CONFIGURATION_NOT(VALUE) !VALUE

#define EMULATOR_CONFIGURATION_GETTER_SETTER(IDENTIFIER, VALUE, OPERATION) \
	std::remove_cvref_t<decltype(VALUE)> Get##IDENTIFIER() const { return OPERATION(VALUE); } \
	void Set##IDENTIFIER(const std::remove_cvref_t<decltype(VALUE)> value){ VALUE = OPERATION(value); }

#define EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(IDENTIFIER, VALUE) \
	EMULATOR_CONFIGURATION_GETTER_SETTER(IDENTIFIER, VALUE, EMULATOR_CONFIGURATION_AS_IS)

#define EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(IDENTIFIER, VALUE) \
	EMULATOR_CONFIGURATION_GETTER_SETTER(IDENTIFIER, VALUE, EMULATOR_CONFIGURATION_NOT)

		EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(TVStandard, general.tv_standard)
		EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(Region, general.region)
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(LowPassFilterEnabled, general.low_pass_filter_disabled)
		EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(CDAddOnEnabled, general.cd_add_on_enabled)

		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(SpritePlaneEnabled, vdp.sprites_disabled)
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(WindowPlaneEnabled, vdp.window_disabled)
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(ScrollPlaneAEnabled, vdp.planes_disabled[0])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(ScrollPlaneBEnabled, vdp.planes_disabled[1])
		EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(WidescreenTilePairs, vdp.widescreen_tile_pairs)

		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(FM1Enabled, fm.fm_channels_disabled[0])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(FM2Enabled, fm.fm_channels_disabled[1])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(FM3Enabled, fm.fm_channels_disabled[2])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(FM4Enabled, fm.fm_channels_disabled[3])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(FM5Enabled, fm.fm_channels_disabled[4])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(FM6Enabled, fm.fm_channels_disabled[5])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(DACEnabled, fm.dac_channel_disabled)
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(LadderEffectEnabled, fm.ladder_effect_disabled)

		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PSG1Enabled, psg.tone_disabled[0])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PSG2Enabled, psg.tone_disabled[1])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PSG3Enabled, psg.tone_disabled[2])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PSGNoiseEnabled, psg.noise_disabled)

		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM1Enabled, pcm.channels_disabled[0])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM2Enabled, pcm.channels_disabled[1])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM3Enabled, pcm.channels_disabled[2])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM4Enabled, pcm.channels_disabled[3])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM5Enabled, pcm.channels_disabled[4])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM6Enabled, pcm.channels_disabled[5])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM7Enabled, pcm.channels_disabled[6])
		EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM8Enabled, pcm.channels_disabled[7])
	};

	class StateBackup : private ClownMDEmu_StateBackup
	{
	public:
		StateBackup(const Emulator &emulator)
		{
			ClownMDEmu_SaveState(&emulator, this);
		}

		void Apply(Emulator &emulator) const
		{
			ClownMDEmu_LoadState(&emulator, this);
		}
	};

	///////////////
	// Callbacks //
	///////////////

private:
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

	static void Callback_ColourUpdated(void* const user_data, const cc_u16f index, const cc_u16f colour)
	{
		static_cast<Emulator*>(user_data)->ColourUpdated(index, colour);
	}
	static void Callback_ScanlineRendered(void* const user_data, const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f left_boundary, const cc_u16f right_boundary, const cc_u16f screen_width, const cc_u16f screen_height)
	{
		static_cast<Emulator*>(user_data)->ScanlineRendered(scanline, pixels, left_boundary, right_boundary, screen_width, screen_height);
	}
	static cc_bool Callback_InputRequested(void* const user_data, const cc_u8f player_id, const ClownMDEmu_Button button_id)
	{
		return static_cast<Emulator*>(user_data)->InputRequested(player_id, button_id);
	}

	static void Callback_FMAudioToBeGenerated(void* const user_data, ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_fm_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
	{
		static_cast<Emulator*>(user_data)->FMAudioToBeGenerated(clownmdemu, total_frames, generate_fm_audio);
	}
	static void Callback_PSGAudioToBeGenerated(void* const user_data, ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_psg_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
	{
		static_cast<Emulator*>(user_data)->PSGAudioToBeGenerated(clownmdemu, total_frames, generate_psg_audio);
	}
	static void Callback_PCMAudioToBeGenerated(void* const user_data, ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_pcm_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
	{
		static_cast<Emulator*>(user_data)->PCMAudioToBeGenerated(clownmdemu, total_frames, generate_pcm_audio);
	}
	static void Callback_CDDAAudioToBeGenerated(void* const user_data, ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_cdda_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
	{
		static_cast<Emulator*>(user_data)->CDDAAudioToBeGenerated(clownmdemu, total_frames, generate_cdda_audio);
	}

	static void Callback_CDSeeked(void* const user_data, const cc_u32f sector_index)
	{
		static_cast<Emulator*>(user_data)->CDSeeked(sector_index);
	}
	static void Callback_CDSectorRead(void* const user_data, cc_u16l* const buffer)
	{
		static_cast<Emulator*>(user_data)->CDSectorRead(buffer);
	}
	static cc_bool Callback_CDTrackSeeked(void* const user_data, const cc_u16f track_index, const ClownMDEmu_CDDAMode mode)
	{
		return static_cast<Emulator*>(user_data)->CDTrackSeeked(track_index, mode);
	}
	static std::size_t Callback_CDAudioRead(void* const user_data, cc_s16l* const sample_buffer, const std::size_t total_frames)
	{
		return static_cast<Emulator*>(user_data)->CDAudioRead(sample_buffer, total_frames);
	}

	static cc_bool Callback_SaveFileOpenedForReading(void* const user_data, const char* const filename)
	{
		return static_cast<Emulator*>(user_data)->SaveFileOpenedForReading(filename);
	}
	static cc_s16f Callback_SaveFileRead(void* const user_data)
	{
		return static_cast<Emulator*>(user_data)->SaveFileRead();
	}
	static cc_bool Callback_SaveFileOpenedForWriting(void* const user_data, const char* const filename)
	{
		return static_cast<Emulator*>(user_data)->SaveFileOpenedForWriting(filename);
	}
	static void Callback_SaveFileWritten(void* const user_data, const cc_u8f byte)
	{
		static_cast<Emulator*>(user_data)->SaveFileWritten(byte);
	}
	static void Callback_SaveFileClosed(void* const user_data)
	{
		static_cast<Emulator*>(user_data)->SaveFileClosed();
	}
	static cc_bool Callback_SaveFileRemoved(void* const user_data, const char* const filename)
	{
		return static_cast<Emulator*>(user_data)->SaveFileRemoved(filename);
	}
	static cc_bool Callback_SaveFileSizeObtained(void* const user_data, const char* const filename, std::size_t* const size)
	{
		return static_cast<Emulator*>(user_data)->SaveFileSizeObtained(filename, size);
	}

	virtual void ColourUpdated(cc_u16f index, cc_u16f colour) = 0;
	virtual void ScanlineRendered(cc_u16f scanline, const cc_u8l *pixels, cc_u16f left_boundary, cc_u16f right_boundary, cc_u16f screen_width, cc_u16f screen_height) = 0;
	virtual cc_bool InputRequested(cc_u8f player_id, ClownMDEmu_Button button_id) = 0;

	virtual void FMAudioToBeGenerated(ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_fm_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) = 0;
	virtual void PSGAudioToBeGenerated(ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_psg_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) = 0;
	virtual void PCMAudioToBeGenerated(ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_pcm_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) = 0;
	virtual void CDDAAudioToBeGenerated(ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_cdda_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) = 0;

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

	///////////////
	// Interface //
	///////////////

public:
	Emulator(const InitialConfiguration &configuration)
	{
		ClownMDEmu_Initialise(this, &configuration, &callbacks);
	}

	void InsertCartridge(const cc_u16l* const buffer, const cc_u32f buffer_length)
	{
		ClownMDEmu_SetCartridge(this, buffer, buffer_length);
	}

	void EjectCartridge()
	{
		ClownMDEmu_SetCartridge(this, nullptr, 0);
	}

	[[nodiscard]] bool IsCartridgeInserted() const
	{
		return cartridge_buffer_length != 0;
	}

	void Reset(const cc_bool cd_inserted)
	{
		ClownMDEmu_Reset(this, IsCartridgeInserted(), cd_inserted);
	}

	void Iterate()
	{
		ClownMDEmu_Iterate(this);
	}

	//////////////////
	// Log Callback //
	//////////////////

public:
	using LogCallbackFormatted = std::function<void(const char *format, std::va_list arg)>;
	using LogCallbackPlain = std::function<void(const std::string &message)>;

private:
	static inline LogCallbackFormatted log_callback;

public:
	static void SetLogCallback(const LogCallbackFormatted &callback)
	{
		log_callback = callback;
		ClownMDEmu_SetLogCallback(
			[](void* const user_data, const char *format, std::va_list arg)
			{
				(*static_cast<const LogCallbackFormatted*>(user_data))(format, arg);
			}, &log_callback
		);
	}

	static void SetLogCallback(const LogCallbackPlain &callback)
	{
		SetLogCallback(
			[callback](const char* const format, std::va_list arg)
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

	//////////////////
	// State Access //
	//////////////////

public:
	[[nodiscard]] const auto& GetState() const
	{
		return state;
	}

	[[nodiscard]] const auto& GetM68kState() const
	{
		return m68k;
	}

	[[nodiscard]] const auto& GetZ80State() const
	{
		return z80;
	}

	[[nodiscard]] const auto& GetVDPState() const
	{
		return vdp.state;
	}

	[[nodiscard]] const auto& GetFMState() const
	{
		return fm.state;
	}

	[[nodiscard]] const auto& GetPSGState() const
	{
		return psg.state;
	}

	[[nodiscard]] const auto& GetSubM68kState() const
	{
		return mega_cd.m68k;
	}

	[[nodiscard]] const auto& GetCDCState() const
	{
		return mega_cd.cdc;
	}

	[[nodiscard]] const auto& GetCDDAState() const
	{
		return mega_cd.cdda;
	}

	[[nodiscard]] const auto& GetPCMState() const
	{
		return mega_cd.pcm.state;
	}

	[[nodiscard]] auto& GetExternalRAM()
	{
		return state.external_ram.buffer;
	}

	///////////////////
	// Configuration //
	///////////////////

public:
	EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(TVStandard, configuration.tv_standard)
	EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(Region, configuration.region)
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(LowPassFilterEnabled, configuration.low_pass_filter_disabled)
	EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(CDAddOnEnabled, configuration.cd_add_on_enabled)

	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(SpritePlaneEnabled, vdp.configuration.sprites_disabled)
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(WindowPlaneEnabled, vdp.configuration.window_disabled)
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(ScrollPlaneAEnabled, vdp.configuration.planes_disabled[0])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(ScrollPlaneBEnabled, vdp.configuration.planes_disabled[1])
	EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(WidescreenTilePairs, vdp.configuration.widescreen_tile_pairs)

	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(FM1Enabled, fm.configuration.fm_channels_disabled[0])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(FM2Enabled, fm.configuration.fm_channels_disabled[1])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(FM3Enabled, fm.configuration.fm_channels_disabled[2])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(FM4Enabled, fm.configuration.fm_channels_disabled[3])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(FM5Enabled, fm.configuration.fm_channels_disabled[4])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(FM6Enabled, fm.configuration.fm_channels_disabled[5])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(DACEnabled, fm.configuration.dac_channel_disabled)
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(LadderEffectEnabled, fm.configuration.ladder_effect_disabled)

	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PSG1Enabled, psg.configuration.tone_disabled[0])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PSG2Enabled, psg.configuration.tone_disabled[1])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PSG3Enabled, psg.configuration.tone_disabled[2])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PSGNoiseEnabled, psg.configuration.noise_disabled)

	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM1Enabled, mega_cd.pcm.configuration.channels_disabled[0])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM2Enabled, mega_cd.pcm.configuration.channels_disabled[1])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM3Enabled, mega_cd.pcm.configuration.channels_disabled[2])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM4Enabled, mega_cd.pcm.configuration.channels_disabled[3])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM5Enabled, mega_cd.pcm.configuration.channels_disabled[4])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM6Enabled, mega_cd.pcm.configuration.channels_disabled[5])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM7Enabled, mega_cd.pcm.configuration.channels_disabled[6])
	EMULATOR_CONFIGURATION_GETTER_SETTER_NOT(PCM8Enabled, mega_cd.pcm.configuration.channels_disabled[7])
};

#endif // EMULATOR_H

#ifndef EMULATOR_INSTANCE_H
#define EMULATOR_INSTANCE_H

#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include "common/core/clownmdemu.h"

#include "emulator-extended.h"
#include "sdl-wrapper.h"
#include "state-ring-buffer.h"

class EmulatorInstance : public EmulatorExtended
{
public:
	using InputCallback = std::function<bool(cc_u8f player_id, ClownMDEmu_Button button_id)>;

	struct FrontendState
	{
		static constexpr unsigned int total_brightnesses = 3;
		static constexpr unsigned int total_palette_lines = 4;
		static constexpr unsigned int total_colours_in_palette_line = 16;

		std::array<SDL::Pixel, total_colours_in_palette_line * total_palette_lines * total_brightnesses> colours;

		auto GetPaletteLine(const cc_u8f brightness, const cc_u8f palette_line) const
		{
			return &colours[brightness * total_palette_lines * total_colours_in_palette_line + palette_line * total_colours_in_palette_line];
		}
		auto GetColour(const cc_u8f brightness, const cc_u8f palette_line, const cc_u8f colour_index) const
		{
			return GetPaletteLine(brightness, palette_line)[colour_index];
		}
	};

	struct State
	{
		EmulatorExtended::State emulator;
		FrontendState frontend;
	};

	// Ensure that this is safe to save to (and read from) a file.
	static_assert(std::is_trivially_copyable_v<State>);

private:
	class Cartridge
	{
	private:
		std::vector<cc_u16l> rom_file_buffer;
		std::filesystem::path save_data_path;
		EmulatorInstance &emulator;

	public:
		Cartridge(EmulatorInstance &emulator)
			: emulator(emulator)
		{}

		const std::vector<cc_u16l>& GetROMBuffer() const { return rom_file_buffer; }
		bool IsInserted() const { return !rom_file_buffer.empty(); }
		void Insert(const std::vector<cc_u16l> &rom_file_buffer, const std::filesystem::path &save_data_path);
		void Eject();
	};

	SDL::Texture &texture;
	const InputCallback input_callback;

	Emulator::Configuration emulator_configuration;

	Cartridge cartridge = {*this};


	SDL::Pixel *framebuffer_texture_pixels = nullptr;
	int framebuffer_texture_pitch = 0;

	class Rewind : public StateRingBuffer<State>
	{
	private:
		using Base = StateRingBuffer<State>;

		bool in_progress = false;

	public:
		using Base::Base;

		bool Enabled() const
		{
			return Exists();
		}

		bool InProgress() const { return Enabled() ? in_progress : false; }
		void InProgress(const bool active) { in_progress = Enabled() ? active : false; }
		// We need at least two frames and the frame before it, because rewinding pops one frame and then samples the frame below the head.
		bool Exhausted() const { return Enabled() ? in_progress && Base::Exhausted() : false; }
	};

	Rewind rewind;

	FrontendState frontend_state;

	unsigned int current_screen_width = 0;
	unsigned int current_screen_height = 0;

	SDL::IOStream save_data_stream;

	virtual void ColourUpdated(cc_u16f index, cc_u16f colour) override;
	virtual void ScanlineRendered(cc_u16f scanline, const cc_u8l *pixels, cc_u16f left_boundary, cc_u16f right_boundary, cc_u16f screen_width, cc_u16f screen_height) override;
	virtual cc_bool InputRequested(cc_u8f player_id, ClownMDEmu_Button button_id) override;

	virtual cc_bool SaveFileOpenedForReading(const char *filename) override;
	virtual cc_s16f SaveFileRead() override;
	virtual cc_bool SaveFileOpenedForWriting(const char *filename) override;
	virtual void SaveFileWritten(cc_u8f byte) override;
	virtual void SaveFileClosed() override;
	virtual cc_bool SaveFileRemoved(const char *filename) override;
	virtual cc_bool SaveFileSizeObtained(const char *filename, std::size_t *size) override;

public:
	EmulatorInstance(SDL::Texture &texture, const InputCallback &input_callback);
	~EmulatorInstance()
	{
		cartridge.Eject();
	}

	void Update(cc_bool fast_forward);
	void SoftResetConsole();
	void HardResetConsole();
	void LoadCartridgeFile(std::vector<cc_u16l> &&file_buffer, const std::filesystem::path &path);
	void UnloadCartridgeFile();
	bool LoadCDFile(SDL::IOStream &&stream, const std::filesystem::path &path);
	void UnloadCDFile();

	bool ValidateSaveStateFile(const std::vector<unsigned char> &file_buffer);
	bool LoadSaveStateFile(const std::vector<unsigned char> &file_buffer);
	std::size_t GetSaveStateFileSize();
	bool WriteSaveStateFile(SDL::IOStream &file);

	bool RewindingEnabled() const { return rewind.Enabled(); }
	void EnableRewinding(const bool enabled)
	{
		rewind = Rewind(enabled);
	}
	bool IsRewinding() const { return rewind.InProgress(); }
	void SetRewinding(const bool active) { rewind.InProgress(active); }
	bool RewindingExhausted() const { return rewind.Exhausted(); }

	unsigned int GetCurrentScreenWidth() const { return current_screen_width; }
	unsigned int GetCurrentScreenHeight() const { return current_screen_height; }

	const FrontendState& GetFrontendState() const { return frontend_state; }

	[[nodiscard]] State CreateSaveState() const
	{
		return {SaveState(), GetFrontendState()};
	}

	void ApplySaveState(const State &state)
	{
		LoadState(state.emulator);
		this->frontend_state = state.frontend;
	}

	const std::vector<cc_u16l>& GetROMBuffer() const { return cartridge.GetROMBuffer(); }

	bool GetPALMode() const { return emulator_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_PAL; }

	void SetPALMode(const bool enabled)
	{
		emulator_configuration.general.tv_standard = enabled ? CLOWNMDEMU_TV_STANDARD_PAL : CLOWNMDEMU_TV_STANDARD_NTSC;
		SetAudioPALMode(enabled);
	}

	bool GetDomestic() const { return emulator_configuration.general.region == CLOWNMDEMU_REGION_DOMESTIC; }
	void SetDomestic(const bool enabled) { emulator_configuration.general.region = enabled ? CLOWNMDEMU_REGION_DOMESTIC : CLOWNMDEMU_REGION_OVERSEAS; }
	bool GetLowPassFilter() const { return !emulator_configuration.general.low_pass_filter_disabled; }
	void SetLowPassFilter(const bool enabled) { emulator_configuration.general.low_pass_filter_disabled = !enabled; }
	bool GetCDAddOnEnabled() const { return emulator_configuration.general.cd_add_on_enabled; }
	void SetCDAddOnEnabled(const bool enabled) { emulator_configuration.general.cd_add_on_enabled = enabled; }
	VDP_Configuration& GetConfigurationVDP() { return emulator_configuration.vdp; }
	FM_Configuration& GetConfigurationFM() { return emulator_configuration.fm; }
	PSG_Configuration& GetConfigurationPSG() { return emulator_configuration.psg; }
	PCM_Configuration& GetConfigurationPCM() { return emulator_configuration.pcm; }

	std::string GetSoftwareName();
};

#endif /* EMULATOR_INSTANCE_H */

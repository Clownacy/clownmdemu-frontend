#ifndef EMULATOR_INSTANCE_H
#define EMULATOR_INSTANCE_H

#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "SDL.h"

#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#include "audio-output.h"
#include "cd-reader.h"
#include "debug-log.h"
#include "window.h"

class EmulatorInstance
{
public:
	using InputCallback = std::function<bool(cc_u8f player_id, ClownMDEmu_Button button_id)>;

	struct State
	{
		ClownMDEmu_State clownmdemu;
		std::array<Uint32, 3 * 4 * 16> colours;
	};

private:
	static bool clownmdemu_initialised;
	static ClownMDEmu_Constant clownmdemu_constant;

	AudioOutput audio_output;
	Window &window;
	const InputCallback input_callback;
	ClownMDEmu_Callbacks callbacks;

	ClownMDEmu_Configuration clownmdemu_configuration = {};
	ClownMDEmu clownmdemu;

	std::vector<unsigned char> rom_buffer;
	CDReader cd_file;
		
	Uint32 *framebuffer_texture_pixels = nullptr;
	int framebuffer_texture_pitch = 0;

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	std::array<State, 60 * 10> state_rewind_buffer; // Roughly 10 seconds of rewinding at 60FPS
	std::size_t state_rewind_index = 0;
	std::size_t state_rewind_remaining = 0;
	bool rewind_in_progress = false;
#else
	std::array<State, 1> state_rewind_buffer;
#endif

	State *state = &state_rewind_buffer[0];

	unsigned int current_screen_width = 0;
	unsigned int current_screen_height = 0;

	static cc_u8f CartridgeReadCallback(void *user_data, cc_u32f address);
	static void CartridgeWrittenCallback(void *user_data, cc_u32f address, cc_u8f value);
	static void ColourUpdatedCallback(void *user_data, cc_u16f index, cc_u16f colour);
	static void ScanlineRenderedCallback(void *user_data, cc_u16f scanline, const cc_u8l *pixels, cc_u16f screen_width, cc_u16f screen_height);
	static cc_bool ReadInputCallback(void *user_data, cc_u8f player_id, ClownMDEmu_Button button_id);
	static void FMAudioCallback(void *user_data, const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
	static void PSGAudioCallback(void *user_data, const ClownMDEmu *clownmdemu, std::size_t total_samples, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_samples));
	static void PCMAudioCallback(void *user_data, const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
	static void CDDAAudioCallback(void *user_data, const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
	static void CDSeekCallback(void *user_data, cc_u32f sector_index);
	static const cc_u8l* CDSectorReadCallback(void *user_data);
	static void CDSeekTrackCallback(void *user_data, cc_u16f track_index);
	static std::size_t CDAudioReadCallback(void *user_data, cc_s16l *sample_buffer, std::size_t total_frames);

public:
	EmulatorInstance(DebugLog &debug_log, Window &window, const InputCallback &input_callback);
	void Update();
	void SoftResetConsole();
	void HardResetConsole();
	void LoadCartridgeFile(const std::vector<unsigned char> &&file_buffer);
	void UnloadCartridgeFile();
	void LoadCDFile(CDReader &&cd_file);
	void UnloadCDFile();
	bool ValidateSaveState(const std::vector<unsigned char> &file_buffer);
	bool LoadSaveState(const std::vector<unsigned char> &file_buffer);
	std::size_t GetSaveStateSize();
	bool CreateSaveState(const SDL::RWops &file);

	bool IsCartridgeFileLoaded() const { return !rom_buffer.empty(); }
	bool IsCDFileLoaded() const { return cd_file.IsOpen(); }

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	bool IsRewinding() const { return rewind_in_progress; }
	void Rewind(const bool active) { rewind_in_progress = active; }
	bool RewindingExhausted() const { return IsRewinding() && state_rewind_remaining == 0; }
#endif

	unsigned int GetCurrentScreenWidth() const { return current_screen_width; }
	unsigned int GetCurrentScreenHeight() const { return current_screen_height; }
	const State& CurrentState() const { return *state; }
	void OverwriteCurrentState(const State &new_state) { *state = new_state; }
	const std::vector<unsigned char>& GetROMBuffer() const { return rom_buffer; }

	bool GetPALMode() const { return clownmdemu_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_PAL; }

	void SetPALMode(const bool enabled)
	{
		clownmdemu_configuration.general.tv_standard = enabled ? CLOWNMDEMU_TV_STANDARD_PAL : CLOWNMDEMU_TV_STANDARD_NTSC;
		audio_output.SetPALMode(enabled);
	}

	bool GetDomestic() const { return clownmdemu_configuration.general.region == CLOWNMDEMU_REGION_DOMESTIC; }
	void SetDomestic(const bool enabled) { clownmdemu_configuration.general.region = enabled ? CLOWNMDEMU_REGION_DOMESTIC : CLOWNMDEMU_REGION_OVERSEAS; }
	bool GetLowPassFilter() const { return audio_output.GetLowPassFilter(); }
	void SetLowPassFilter(const bool enabled) { audio_output.SetLowPassFilter(enabled); }
	VDP_Configuration& GetConfigurationVDP() { return clownmdemu_configuration.vdp; }
	FM_Configuration& GetConfigurationFM() { return clownmdemu_configuration.fm; }
	PSG_Configuration& GetConfigurationPSG() { return clownmdemu_configuration.psg; }
	PCM_Configuration& GetConfigurationPCM() { return clownmdemu_configuration.pcm; }

	cc_u32f GetAudioAverageFrames() const { return audio_output.GetAverageFrames(); }
	cc_u32f GetAudioTargetFrames() const { return audio_output.GetTargetFrames(); }
	cc_u32f GetAudioTotalBufferFrames() const { return audio_output.GetTotalBufferFrames(); }
	cc_u32f GetAudioSampleRate() const { return audio_output.GetSampleRate(); }

	std::string GetSoftwareName();
};

#endif /* EMULATOR_INSTANCE_H */

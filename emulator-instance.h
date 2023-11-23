#ifndef EMULATOR_INSTANCE_H
#define EMULATOR_INSTANCE_H

#include <cstddef>
#include <functional>

#include "SDL.h"

#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#include "audio-output.h"
#include "debug-log.h"
#include "utilities.h"
#include "window.h"

class EmulatorInstance
{
public:
	struct State
	{
		ClownMDEmu_State clownmdemu;
		Uint32 colours[3 * 4 * 16];
	};

	unsigned char *rom_buffer;
	std::size_t rom_buffer_size;

private:
	static bool clownmdemu_initialised;
	static ClownMDEmu_Constant clownmdemu_constant;

	AudioOutput &audio_output;
	Utilities &utilities;
	Window &window;
	const std::function<bool(cc_u8f player_id, ClownMDEmu_Button button_id)> input_callback;
	ClownMDEmu_Callbacks callbacks;

	ClownMDEmu clownmdemu;

	SDL_RWops *cd_file;
	bool sector_size_2352;
		
	Uint32 *framebuffer_texture_pixels;
	int framebuffer_texture_pitch;

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	State state_rewind_buffer[60 * 10]; // Roughly 30 seconds of rewinding at 60FPS
	std::size_t state_rewind_index;
	std::size_t state_rewind_remaining;
#else
	EmulationState state_rewind_buffer[1];
#endif

	cc_u8f CartridgeReadCallback(cc_u32f address);
	static cc_u8f CartridgeReadCallback(void *user_data, cc_u32f address);
	void CartridgeWrittenCallback(cc_u32f address, cc_u8f value);
	static void CartridgeWrittenCallback(void *user_data, cc_u32f address, cc_u8f value);
	void ColourUpdatedCallback(cc_u16f index, cc_u16f colour);
	static void ColourUpdatedCallback(void *user_data, cc_u16f index, cc_u16f colour);
	void ScanlineRenderedCallback(cc_u16f scanline, const cc_u8l *pixels, cc_u16f screen_width, cc_u16f screen_height);
	static void ScanlineRenderedCallback(void *user_data, cc_u16f scanline, const cc_u8l *pixels, cc_u16f screen_width, cc_u16f screen_height);
	cc_bool ReadInputCallback(cc_u8f player_id, ClownMDEmu_Button button_id);
	static cc_bool ReadInputCallback(void *user_data, cc_u8f player_id, ClownMDEmu_Button button_id);
	void FMAudioCallback(std::size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
	static void FMAudioCallback(void *user_data, std::size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
	void PSGAudioCallback(std::size_t total_samples, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_samples));
	static void PSGAudioCallback(void *user_data, std::size_t total_samples, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_samples));
	void CDSeekCallback(cc_u32f sector_index);
	static void CDSeekCallback(void *user_data, cc_u32f sector_index);
	const cc_u8l* CDSectorReadCallback();
	static const cc_u8l* CDSectorReadCallback(void *user_data);

public:
	ClownMDEmu_Configuration clownmdemu_configuration;
	unsigned int current_screen_width;
	unsigned int current_screen_height;
	bool rewind_in_progress;

	State *state;

	EmulatorInstance(AudioOutput &audio_output, DebugLog &debug_log, Utilities &utilities, Window &window, std::function<bool(cc_u8f player_id, ClownMDEmu_Button button_id)> input_callback);
	~EmulatorInstance();
	void Update();
	void SoftResetConsole();
	void HardResetConsole();
	void LoadCartridgeFile(unsigned char *file_buffer, std::size_t file_size);
	bool LoadCartridgeFile(const char *path);
	void UnloadCartridgeFile();
	bool LoadCDFile(const char *path);
	void UnloadCDFile();
	bool ValidateSaveState(const unsigned char *file_buffer, std::size_t file_size);
	bool ValidateSaveState(const char *save_state_path);
	bool LoadSaveState(const unsigned char *file_buffer, std::size_t file_size);
	bool LoadSaveState(const char *save_state_path);
	bool CreateSaveState(const char *save_state_path);

	bool IsCartridgeFileLoaded() const { return rom_buffer != nullptr; }
	bool IsCDFileLoaded() const { return cd_file != nullptr; }

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	bool RewindingExhausted() const { return rewind_in_progress && state_rewind_remaining == 0; }
#endif
}
;

#endif /* EMULATOR_INSTANCE_H */

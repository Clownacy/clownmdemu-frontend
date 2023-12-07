#ifndef EMULATOR_INSTANCE_H
#define EMULATOR_INSTANCE_H

#include <cstddef>
#include <functional>

#include "SDL.h"

#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#include "audio-output.h"
#include "debug-log.h"
#include "window.h"

class EmulatorInstance
{
public:
	typedef std::function<bool(cc_u8f player_id, ClownMDEmu_Button button_id)> InputCallback;

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
	Window &window;
	const InputCallback input_callback;
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
	bool rewind_in_progress;
#else
	State state_rewind_buffer[1];
#endif

	unsigned int current_screen_width;
	unsigned int current_screen_height;

	static cc_u8f CartridgeReadCallback(void *user_data, cc_u32f address);
	static void CartridgeWrittenCallback(void *user_data, cc_u32f address, cc_u8f value);
	static void ColourUpdatedCallback(void *user_data, cc_u16f index, cc_u16f colour);
	static void ScanlineRenderedCallback(void *user_data, cc_u16f scanline, const cc_u8l *pixels, cc_u16f screen_width, cc_u16f screen_height);
	static cc_bool ReadInputCallback(void *user_data, cc_u8f player_id, ClownMDEmu_Button button_id);
	static void FMAudioCallback(void *user_data, std::size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
	static void PSGAudioCallback(void *user_data, std::size_t total_samples, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_samples));
	static void CDSeekCallback(void *user_data, cc_u32f sector_index);
	static const cc_u8l* CDSectorReadCallback(void *user_data);

public:
	ClownMDEmu_Configuration clownmdemu_configuration;

	State *state;

	EmulatorInstance(AudioOutput &audio_output, DebugLog &debug_log, Window &window, const InputCallback &input_callback);
	~EmulatorInstance();
	void Update();
	void SoftResetConsole();
	void HardResetConsole();
	void LoadCartridgeFile(unsigned char *file_buffer, std::size_t file_size);
	void UnloadCartridgeFile();
	void LoadCDFile(SDL_RWops *file);
	void UnloadCDFile();
	bool ValidateSaveState(const unsigned char *file_buffer, std::size_t file_size);
	bool LoadSaveState(const unsigned char *file_buffer, std::size_t file_size);
	std::size_t GetSaveStateSize();
	bool CreateSaveState(SDL_RWops *file);

	bool IsCartridgeFileLoaded() const { return rom_buffer != nullptr; }
	bool IsCDFileLoaded() const { return cd_file != nullptr; }

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	bool IsRewinding() const { return rewind_in_progress; }
	void Rewind(const bool active) { rewind_in_progress = active; }
	bool RewindingExhausted() const { return IsRewinding() && state_rewind_remaining == 0; }
#endif

	unsigned int GetCurrentScreenWidth() const { return current_screen_width; }
	unsigned int GetCurrentScreenHeight() const { return current_screen_height; }
}
;

#endif /* EMULATOR_INSTANCE_H */

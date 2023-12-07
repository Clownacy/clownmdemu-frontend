#ifndef FRONTEND_H
#define FRONTEND_H

#include <cstddef>
#include <functional>

#include "SDL.h"

#include "libraries/doubly-linked-list/doubly-linked-list.h"
#include "libraries/imgui/imgui.h"

#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#include "audio-output.h"
#include "debug-fm.h"
#include "debug-frontend.h"
#include "debug-log.h"
#include "debug-m68k.h"
#include "debug-memory.h"
#include "debug-psg.h"
#include "debug-vdp.h"
#include "debug-z80.h"
#include "emulator-instance.h"
#include "file-utilities.h"
#include "window.h"

#ifndef __EMSCRIPTEN__
#define FILE_PATH_SUPPORT
#endif

class Frontend
{
public:
	typedef std::function<void(bool pal_mode)> FrameRateCallback;

private:
	struct Input
	{
		unsigned int bound_joypad;
		unsigned char buttons[CLOWNMDEMU_BUTTON_MAX];
		unsigned char fast_forward;
		unsigned char rewind;
	};

	struct ControllerInput
	{
		SDL_JoystickID joystick_instance_id;
		Sint16 left_stick_x;
		Sint16 left_stick_y;
		bool left_stick[4];
		bool dpad[4];
		bool left_trigger;
		bool right_trigger;
		Input input;

		struct ControllerInput *next;
	};

#ifdef FILE_PATH_SUPPORT
	struct RecentSoftware
	{
		DoublyLinkedList_Entry list;

		bool is_cd_file;
		char path[1];
	};
#endif

	enum InputBinding
	{
		INPUT_BINDING_NONE,
		INPUT_BINDING_CONTROLLER_UP,
		INPUT_BINDING_CONTROLLER_DOWN,
		INPUT_BINDING_CONTROLLER_LEFT,
		INPUT_BINDING_CONTROLLER_RIGHT,
		INPUT_BINDING_CONTROLLER_A,
		INPUT_BINDING_CONTROLLER_B,
		INPUT_BINDING_CONTROLLER_C,
		INPUT_BINDING_CONTROLLER_START,
		INPUT_BINDING_PAUSE,
		INPUT_BINDING_RESET,
		INPUT_BINDING_FAST_FORWARD,
	#ifdef CLOWNMDEMU_FRONTEND_REWINDING
		INPUT_BINDING_REWIND,
	#endif
		INPUT_BINDING_QUICK_SAVE_STATE,
		INPUT_BINDING_QUICK_LOAD_STATE,
		INPUT_BINDING_TOGGLE_FULLSCREEN,
		INPUT_BINDING_TOGGLE_CONTROL_PAD,
		INPUT_BINDING__TOTAL
	};

	Input keyboard_input;

	ControllerInput *controller_input_list_head;

	InputBinding keyboard_bindings[SDL_NUM_SCANCODES]; // TODO: `SDL_NUM_SCANCODES` is an internal macro, so use something standard!
	InputBinding keyboard_bindings_cached[SDL_NUM_SCANCODES]; // TODO: `SDL_NUM_SCANCODES` is an internal macro, so use something standard!
	bool key_pressed[SDL_NUM_SCANCODES]; // TODO: `SDL_NUM_SCANCODES` is an internal macro, so use something standard!

	DoublyLinkedList recent_software_list;
	char *drag_and_drop_filename;

	bool emulator_has_focus; // Used for deciding when to pass inputs to the emulator.
	bool emulator_paused;
	bool emulator_frame_advance;

	bool quick_save_exists;
	EmulatorInstance::State quick_save_state;

	float dpi_scale;
	unsigned int frame_counter;
	ImFont *monospace_font;

	bool use_vsync;
	bool integer_screen_scaling;
	bool tall_double_resolution_mode;

	ImGuiStyle style_backup;

	FrameRateCallback frame_rate_callback;

	DebugLog debug_log;
	AudioOutput audio_output;
	Window window;
	FileUtilities file_utilities;

	EmulatorInstance emulator;

	DebugFM debug_fm;
	DebugFrontend debug_frontend;
	DebugM68k debug_m68k;
	DebugMemory debug_memory;
	DebugPSG debug_psg;
	DebugVDP debug_vdp;
	DebugZ80 debug_z80;

	// Manages whether the program exits or not.
	bool quit;

	// Used for tracking when to pop the emulation display out into its own little window.
	bool pop_out;

	bool debug_log_active;
	bool debug_frontend_active;
	bool m68k_status;
	bool mcd_m68k_status;
	bool z80_status;
	bool m68k_ram_viewer;
	bool z80_ram_viewer;
	bool word_ram_viewer;
	bool prg_ram_viewer;
	bool vdp_registers;
	bool window_plane_viewer;
	bool plane_a_viewer;
	bool plane_b_viewer;
	bool vram_viewer;
	bool cram_viewer;
	bool fm_status;
	bool psg_status;
	bool other_status;
	bool debugging_toggles_menu;
	bool disassembler;
	bool options_menu;
	bool about_menu;

#ifndef NDEBUG
	bool dear_imgui_demo_window;
#endif

	bool emulator_on, emulator_running;

	// Manages whether the emulator runs at a higher speed or not.
	bool fast_forward_in_progress;

	SDL_Texture *framebuffer_texture_upscaled;
	unsigned int framebuffer_texture_upscaled_width;
	unsigned int framebuffer_texture_upscaled_height;

	unsigned int CalculateFontSize();
	void ReloadFonts(unsigned int font_size);
	cc_bool ReadInputCallback(cc_u8f player_id, ClownMDEmu_Button button_id);
#ifdef FILE_PATH_SUPPORT
	void AddToRecentSoftware(const char *path, bool is_cd_file, bool add_to_end);
#endif
	void UpdateFastForwardStatus();
#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	void UpdateRewindStatus();
#endif
	void RecreateUpscaledFramebuffer(unsigned int display_width, unsigned int display_height);
	bool GetUpscaledFramebufferSize(unsigned int &width, unsigned int &height);
	void LoadCartridgeFile(unsigned char *file_buffer, std::size_t file_size);
	bool LoadCartridgeFile(const char *path, SDL_RWops *file);
	bool LoadCartridgeFile(const char *path);
	bool LoadCDFile(const char *path, SDL_RWops *file);
#ifdef FILE_PATH_SUPPORT
	bool LoadCDFile(const char *path);
#endif
	bool LoadSaveState(const unsigned char *file_buffer, std::size_t file_size);
	bool LoadSaveState(SDL_RWops *file);
#ifdef FILE_PATH_SUPPORT
	bool CreateSaveState(const char *save_state_path);
#endif
	void SetAudioPALMode(bool enabled);
	static void DoToolTip(const char *text);
	static char* INIReadCallback(char *buffer, int length, void *user);
	static int INIParseCallback(void *user, const char *section, const char *name, const char *value);
	void LoadConfiguration();
	void SaveConfiguration();
	void PreEventStuff();

	Frontend(const int argc, char** const argv, const FrameRateCallback &frame_rate_callback);
	~Frontend();
	Frontend(const Frontend&) = delete;
	Frontend& operator=(const Frontend&) = delete;

public:
	static Frontend* Singleton(const int argc, char** const argv, const FrameRateCallback &frame_rate_callback)
	{
		static Frontend frontend(argc, argv, frame_rate_callback);
		return &frontend;
	}

	void HandleEvent(const SDL_Event &event);
	void Update();
	bool WantsToQuit();
	bool IsFastForwarding();
	template<typename T>
	T DivideByPALFramerate(T value) { return CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(value); }
	template<typename T>
	T DivideByNTSCFramerate(T value) { return CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(value); }
};

#endif /* FRONTEND_H */

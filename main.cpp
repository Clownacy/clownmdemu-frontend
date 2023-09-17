#include <errno.h>
#include <limits.h> // For INT_MAX.
#include <stdarg.h>
#include <stddef.h>

#include <functional>

#include "SDL.h"

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#include "libraries/imgui/imgui.h"
#include "libraries/imgui/backends/imgui_impl_sdl2.h"
#include "libraries/imgui/backends/imgui_impl_sdlrenderer2.h"

#include "libraries/inih/ini.h"

#include "inconsolata_regular.h"
#include "karla_regular.h"

#include "error.h"
#include "debug_m68k.h"
#include "debug_memory.h"
#include "debug_fm.h"
#include "debug_psg.h"
#include "debug_vdp.h"
#include "debug_z80.h"

extern "C" {
#include "doubly-linked-list.h"
}

#define MIXER_FORMAT Sint16
#include "clownmdemu-frontend-common/mixer.h"

#define CONFIG_FILENAME "clownmdemu-frontend.ini"

#define VERSION "v0.4.4"

typedef struct Input
{
	unsigned int bound_joypad;
	unsigned char buttons[CLOWNMDEMU_BUTTON_MAX];
	unsigned char fast_forward;
	unsigned char rewind;
} Input;

typedef struct ControllerInput
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
} ControllerInput;

typedef struct EmulationState
{
	ClownMDEmu_State clownmdemu;
	Uint32 colours[3 * 4 * 16];
} EmulationState;

typedef struct RecentSoftware
{
	DoublyLinkedList_Entry list;

	char path[1];
} RecentSoftware;

typedef enum InputBinding
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
	INPUT_BINDING_REWIND,
	INPUT_BINDING_QUICK_SAVE_STATE,
	INPUT_BINDING_QUICK_LOAD_STATE,
	INPUT_BINDING_TOGGLE_FULLSCREEN,
	INPUT_BINDING_TOGGLE_CONTROL_PAD
} InputBinding;

#define INPUT_BINDING__TOTAL (INPUT_BINDING_TOGGLE_CONTROL_PAD + 1)

static Input keyboard_input;

static ControllerInput *controller_input_list_head;

static InputBinding keyboard_bindings[SDL_NUM_SCANCODES]; // TODO: `SDL_NUM_SCANCODES` is an internal macro, so use something standard!
static InputBinding keyboard_bindings_cached[SDL_NUM_SCANCODES]; // TODO: `SDL_NUM_SCANCODES` is an internal macro, so use something standard!
static bool key_pressed[SDL_NUM_SCANCODES]; // TODO: `SDL_NUM_SCANCODES` is an internal macro, so use something standard!

static DoublyLinkedList recent_software_list;
static char *drag_and_drop_filename;

static bool emulator_has_focus; // Used for deciding when to pass inputs to the emulator.
static bool emulator_paused;
static bool emulator_frame_advance;

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
static EmulationState state_rewind_buffer[60 * 10]; // Roughly ten seconds of rewinding at 60FPS
static size_t state_rewind_index;
static size_t state_rewind_remaining;
#else
static EmulationState state_rewind_buffer[1];
#endif
static EmulationState *emulation_state = state_rewind_buffer;

static bool quick_save_exists;
static EmulationState quick_save_state;

static unsigned char *rom_buffer;
static size_t rom_buffer_size;

static ClownMDEmu_Configuration clownmdemu_configuration;
static ClownMDEmu_Constant clownmdemu_constant;
static ClownMDEmu clownmdemu;

static bool FileExists(const char *filename)
{
	SDL_RWops* const file = SDL_RWFromFile(filename, "rb");

	if (file != NULL)
	{
		SDL_RWclose(file);
		return true;
	}

	return false;
}

static void LoadFileToBuffer(const char *filename, unsigned char **file_buffer, size_t *file_size)
{
	*file_buffer = NULL;
	*file_size = 0;

	SDL_RWops *file = SDL_RWFromFile(filename, "rb");

	if (file == NULL)
	{
		PrintError("SDL_RWFromFile failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		const Sint64 size_s64 = SDL_RWsize(file);

		if (size_s64 < 0)
		{
			PrintError("SDL_RWsize failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			const size_t size = (size_t)size_s64;

			*file_buffer = (unsigned char*)SDL_malloc(size);

			if (*file_buffer == NULL)
			{
				PrintError("Could not allocate memory for file");
			}
			else
			{
				*file_size = size;

				SDL_RWread(file, *file_buffer, 1, size);
			}
		}

		if (SDL_RWclose(file) < 0)
			PrintError("SDL_RWclose failed with the following message - '%s'", SDL_GetError());
	}
}


///////////
// Video //
///////////

#define FRAMEBUFFER_WIDTH 320
#define FRAMEBUFFER_HEIGHT (240*2) // *2 because of double-resolution mode.

static SDL_Window *window;
static SDL_Renderer *renderer;

static SDL_Texture *framebuffer_texture;
static Uint32 *framebuffer_texture_pixels;
static int framebuffer_texture_pitch;
static SDL_Texture *framebuffer_texture_upscaled;
static unsigned int framebuffer_texture_upscaled_width;
static unsigned int framebuffer_texture_upscaled_height;

static unsigned int current_screen_width;
static unsigned int current_screen_height;

static bool use_vsync;
static bool fullscreen;
static bool integer_screen_scaling;
static bool tall_double_resolution_mode;
static float dpi_scale;
static unsigned int frame_counter;

static float GetNewDPIScale(void)
{
	float dpi_scale = 1.0f;

#ifdef _WIN32
	/* This doesn't work right on Linux nor macOS. */
	float ddpi;
	if (SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(window), &ddpi, NULL, NULL) == 0)
		dpi_scale = ddpi / 96.0f;
#endif

	return dpi_scale;
}

static bool InitialiseVideo(void)
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		PrintError("SDL_InitSubSystem(SDL_INIT_VIDEO) failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		// Create window
		window = SDL_CreateWindow("clownmdemu-frontend " VERSION, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320 * 2, 224 * 2, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);

		if (window == NULL)
		{
			PrintError("SDL_CreateWindow failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			// Use batching even if the user forces a specific rendering backend (wtf SDL).
			//
			// Personally, I like to force SDL2 to use D3D11 instead of D3D9 by setting an environment
			// variable, but it turns out that, if I do that, then SDL2 will disable its render batching
			// for backwards compatibility reasons. Setting this hint prevents that.
			//
			// Normally if a user is setting an environment variable to force a specific rendering
			// backend, then they're expected to set another environment variable to set this hint too,
			// but I might as well just do it myself and save everyone else the hassle.
			SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

			// Create renderer
			renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE);

			if (renderer == NULL)
			{
				PrintError("SDL_CreateRenderer failed with the following message - '%s'", SDL_GetError());
			}
			else
			{
				dpi_scale = GetNewDPIScale();

				return true;
			}

			SDL_DestroyWindow(window);
		}

		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

	return false;
}

static void DeinitialiseVideo(void)
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void SetFullscreen(bool enabled)
{
	SDL_SetWindowFullscreen(window, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

static void ToggleFullscreen(void)
{
	fullscreen = !fullscreen;
	SetFullscreen(fullscreen);
}

static bool InitialiseFramebuffer(void)
{
	// Create framebuffer texture
	// We're using ARGB8888 because it's more likely to be supported natively by the GPU, avoiding the need for constant conversions
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	framebuffer_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);

	if (framebuffer_texture == NULL)
	{
		PrintError("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		// Disable blending, since we don't need it
		if (SDL_SetTextureBlendMode(framebuffer_texture, SDL_BLENDMODE_NONE) < 0)
			PrintError("SDL_SetTextureBlendMode failed with the following message - '%s'", SDL_GetError());

		return true;
	}

	return false;
}

static void DeinitialiseFramebuffer(void)
{
	SDL_DestroyTexture(framebuffer_texture);
}

static void RecreateUpscaledFramebuffer(unsigned int display_width, unsigned int display_height)
{
	static unsigned int previous_framebuffer_size_factor = 0;

	const unsigned int framebuffer_size_factor = CC_MAX(1, CC_MIN(CC_DIVIDE_CEILING(display_width, 640), CC_DIVIDE_CEILING(display_height, 480)));

	if (framebuffer_texture_upscaled == NULL || framebuffer_size_factor != previous_framebuffer_size_factor)
	{
		previous_framebuffer_size_factor = framebuffer_size_factor;

		framebuffer_texture_upscaled_width = 640 * framebuffer_size_factor;
		framebuffer_texture_upscaled_height = 480 * framebuffer_size_factor;

		SDL_DestroyTexture(framebuffer_texture_upscaled); // It should be safe to pass NULL to this
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		framebuffer_texture_upscaled = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, framebuffer_texture_upscaled_width, framebuffer_texture_upscaled_height);

		if (framebuffer_texture_upscaled == NULL)
		{
			PrintError("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			// Disable blending, since we don't need it
			if (SDL_SetTextureBlendMode(framebuffer_texture_upscaled, SDL_BLENDMODE_NONE) < 0)
				PrintError("SDL_SetTextureBlendMode failed with the following message - '%s'", SDL_GetError());
		}
	}
}


///////////
// Audio //
///////////

static SDL_AudioDeviceID audio_device;
static Uint32 audio_device_buffer_size;
static unsigned long audio_device_sample_rate;
static bool pal_mode;
static bool low_pass_filter;

static Mixer_Constant mixer_constant;
static Mixer_State mixer_state;
static const Mixer mixer = {&mixer_constant, &mixer_state};

static bool InitialiseAudio(void)
{
	SDL_AudioSpec want, have;

	SDL_zero(want);
	want.freq = 48000;
	want.format = AUDIO_S16;
	want.channels = MIXER_FM_CHANNEL_COUNT;
	// We want a 10ms buffer (this value must be a power of two).
	want.samples = 1;
	while (want.samples < (want.freq * want.channels) / (1000 / 10))
		want.samples <<= 1;
	want.callback = NULL;

	audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);

	if (audio_device == 0)
	{
		PrintError("SDL_OpenAudioDevice failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		audio_device_buffer_size = have.size;
		audio_device_sample_rate = (unsigned long)have.freq;

		// Initialise the mixer.
		Mixer_Constant_Initialise(&mixer_constant);
		Mixer_State_Initialise(&mixer_state, audio_device_sample_rate, pal_mode, low_pass_filter);

		// Unpause audio device, so that playback can begin.
		SDL_PauseAudioDevice(audio_device, 0);

		return true;
	}

	return false;
}

static void DeinitialiseAudio(void)
{
	if (audio_device != 0)
		SDL_CloseAudioDevice(audio_device);
}

static void SetAudioPALMode(bool enabled)
{
	pal_mode = enabled;

	if (audio_device != 0)
		Mixer_State_Initialise(&mixer_state, audio_device_sample_rate, pal_mode, low_pass_filter);
}

static void AudioPushCallback(const void *user_data, Sint16 *audio_samples, size_t total_frames)
{
	(void)user_data;

	if (audio_device != 0)
		SDL_QueueAudio(audio_device, audio_samples, (Uint32)(total_frames * sizeof(Sint16) * MIXER_FM_CHANNEL_COUNT));
}


///////////
// Fonts //
///////////

static ImFont *monospace_font;

static unsigned int CalculateFontSize(void)
{
	// Note that we are purposefully flooring, as Dear ImGui's docs recommend.
	return (unsigned int)(15.0f * dpi_scale);
}

static void ReloadFonts(unsigned int font_size)
{
	ImGuiIO &io = ImGui::GetIO();

	io.Fonts->Clear();
	ImGui_ImplSDLRenderer2_DestroyFontsTexture();

	ImFontConfig font_cfg = ImFontConfig();
	SDL_snprintf(font_cfg.Name, IM_ARRAYSIZE(font_cfg.Name), "Karla Regular, %upx", font_size);
	io.Fonts->AddFontFromMemoryCompressedTTF(karla_regular_compressed_data, karla_regular_compressed_size, (float)font_size, &font_cfg);
	SDL_snprintf(font_cfg.Name, IM_ARRAYSIZE(font_cfg.Name), "Inconsolata Regular, %upx", font_size);
	monospace_font = io.Fonts->AddFontFromMemoryCompressedTTF(inconsolata_regular_compressed_data, inconsolata_regular_compressed_size, (float)font_size, &font_cfg);
}


////////////////////////////
// Emulator Functionality //
////////////////////////////

static cc_u8f CartridgeReadCallback(const void *user_data, cc_u32f address)
{
	(void)user_data;

	if (address >= rom_buffer_size)
		return 0;

	return rom_buffer[address];
}

static void CartridgeWrittenCallback(const void *user_data, cc_u32f address, cc_u8f value)
{
	(void)user_data;

	// For now, let's pretend that the cartridge is read-only, like retail cartridges are.
	(void)address;
	(void)value;

	/*
	if (address >= rom_buffer_size)
		return;

	rom_buffer[address] = value;
	*/
}

static void ColourUpdatedCallback(const void *user_data, cc_u16f index, cc_u16f colour)
{
	(void)user_data;

	// Decompose XBGR4444 into individual colour channels
	const cc_u32f red = (colour >> 4 * 0) & 0xF;
	const cc_u32f green = (colour >> 4 * 1) & 0xF;
	const cc_u32f blue = (colour >> 4 * 2) & 0xF;

	// Reassemble into ARGB8888
	emulation_state->colours[index] = (Uint32)((blue << 4 * 0) | (blue << 4 * 1) | (green << 4 * 2) | (green << 4 * 3) | (red << 4 * 4) | (red << 4 * 5));
}

static void ScanlineRenderedCallback(const void *user_data, cc_u16f scanline, const cc_u8l *pixels, cc_u16f screen_width, cc_u16f screen_height)
{
	(void)user_data;

	current_screen_width = screen_width;
	current_screen_height = screen_height;

	if (framebuffer_texture_pixels != NULL)
		for (cc_u16f i = 0; i < screen_width; ++i)
			framebuffer_texture_pixels[scanline * framebuffer_texture_pitch + i] = emulation_state->colours[pixels[i]];
}

static cc_bool ReadInputCallback(const void *user_data, cc_u8f player_id, ClownMDEmu_Button button_id)
{
	(void)user_data;

	SDL_assert(player_id < 2);

	cc_bool value = cc_false;

	// Don't use inputs that are for Dear ImGui
	if (emulator_has_focus)
	{
		// First, try to obtain the input from the keyboard.
		if (keyboard_input.bound_joypad == player_id)
			value |= keyboard_input.buttons[button_id] != 0 ? cc_true : cc_false;
	}

	if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
	{
		// Then, try to obtain the input from the controllers.
		for (ControllerInput *controller_input = controller_input_list_head; controller_input != NULL; controller_input = controller_input->next)
			if (controller_input->input.bound_joypad == player_id)
				value |= controller_input->input.buttons[button_id] != 0 ? cc_true : cc_false;
	}

	return value;
}

static void FMAudioCallback(const void *user_data, size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_frames))
{
	(void)user_data;

	generate_fm_audio(&clownmdemu, Mixer_AllocateFMSamples(&mixer, total_frames), total_frames);
}

static void PSGAudioCallback(const void *user_data, size_t total_samples, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_samples))
{
	(void)user_data;

	generate_psg_audio(&clownmdemu, Mixer_AllocatePSGSamples(&mixer, total_samples), total_samples);
}

// Construct our big list of callbacks for clownmdemu.
static ClownMDEmu_Callbacks callbacks = {NULL, CartridgeReadCallback, CartridgeWrittenCallback, ColourUpdatedCallback, ScanlineRenderedCallback, ReadInputCallback, FMAudioCallback, PSGAudioCallback};

static void AddToRecentSoftware(const char* const path, const bool add_to_end)
{
	// If the path already exists in the list, then move it to the start of the list.
	for (DoublyLinkedList_Entry *entry = recent_software_list.head; entry != NULL; entry = entry->next)
	{
		RecentSoftware* const recent_software = CC_STRUCT_POINTER_FROM_MEMBER_POINTER(RecentSoftware, list, entry);

		if (SDL_strcmp(recent_software->path, path) == 0)
		{
			DoublyLinkedList_Remove(&recent_software_list, entry);
			DoublyLinkedList_PushFront(&recent_software_list, entry);
			return;
		}
	}

	// If the list is 10 items long, then discard the last item before we add a new one.
	unsigned int total_items = 0;

	for (DoublyLinkedList_Entry *entry = recent_software_list.head; entry != NULL; entry = entry->next)
		++total_items;

	if (total_items == 10)
	{
		RecentSoftware* const last_item = CC_STRUCT_POINTER_FROM_MEMBER_POINTER(RecentSoftware, list, recent_software_list.tail);

		DoublyLinkedList_Remove(&recent_software_list, &last_item->list);
		SDL_free(last_item);
	}

	// Add the file to the list of recent software.
	const size_t path_length = SDL_strlen(path);
	RecentSoftware* const new_entry = (RecentSoftware*)SDL_malloc(sizeof(RecentSoftware) + path_length);

	if (new_entry != NULL)
	{
		SDL_memcpy(new_entry->path, path, path_length + 1);
		(add_to_end ? DoublyLinkedList_PushBack : DoublyLinkedList_PushFront)(&recent_software_list, &new_entry->list);
	}
}

static void OpenSoftwareFromMemory(unsigned char *rom_buffer_parameter, size_t rom_buffer_size_parameter, const ClownMDEmu_Callbacks *callbacks)
{
	quick_save_exists = false;

	// Unload the previous ROM in memory.
	SDL_free(rom_buffer);

	rom_buffer = rom_buffer_parameter;
	rom_buffer_size = rom_buffer_size_parameter;

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	state_rewind_remaining = 0;
	state_rewind_index = 0;
#endif
	emulation_state = &state_rewind_buffer[0];

	ClownMDEmu_State_Initialise(&emulation_state->clownmdemu);
	ClownMDEmu_Parameters_Initialise(&clownmdemu, &clownmdemu_configuration, &clownmdemu_constant, &emulation_state->clownmdemu);
	ClownMDEmu_Reset(&clownmdemu, callbacks);
}

static bool OpenSoftwareFromFile(const char *path, const ClownMDEmu_Callbacks *callbacks)
{
	unsigned char *temp_rom_buffer;
	size_t temp_rom_buffer_size;

	// Load ROM to memory.
	LoadFileToBuffer(path, &temp_rom_buffer, &temp_rom_buffer_size);

	if (temp_rom_buffer == NULL)
	{
		PrintError("Could not load the software");
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to load the software.", window);
		return false;
	}
	else
	{
		AddToRecentSoftware(path, false);

		OpenSoftwareFromMemory(temp_rom_buffer, temp_rom_buffer_size, callbacks);
		return true;
	}
}

static const char save_state_magic[8] = "CMDEFSS"; // Clownacy Mega Drive Emulator Frontend Save State

static bool LoadSaveStateFromMemory(const unsigned char* const file_buffer, const size_t file_size)
{
	bool success = false;

	if (file_buffer != NULL)
	{
		if (file_size != sizeof(save_state_magic) + sizeof(EmulationState))
		{
			PrintError("Invalid save state size");
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Save state file is incompatible.", window);
		}
		else
		{
			if (SDL_memcmp(file_buffer, save_state_magic, sizeof(save_state_magic)) != 0)
			{
				PrintError("Invalid save state magic");
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "The file was not a valid save state.", window);
			}
			else
			{
				SDL_memcpy(emulation_state, &file_buffer[sizeof(save_state_magic)], sizeof(EmulationState));

				emulator_paused = false;

				success = true;
			}
		}
	}

	return success;
}

static bool LoadSaveStateFromFile(const char* const save_state_path)
{
	unsigned char *file_buffer;
	size_t file_size;
	LoadFileToBuffer(save_state_path, &file_buffer, &file_size);

	bool success = false;

	if (file_buffer != NULL)
	{
		success = LoadSaveStateFromMemory(file_buffer, file_size);
		SDL_free(file_buffer);
	}

	return success;
}

// Manages whether the emulator runs at a higher speed or not.
static bool fast_forward_in_progress;

static void UpdateFastForwardStatus(void)
{
	const bool previous_fast_forward_in_progress = fast_forward_in_progress;

	fast_forward_in_progress = keyboard_input.fast_forward;

	for (ControllerInput *controller_input = controller_input_list_head; controller_input != NULL; controller_input = controller_input->next)
		fast_forward_in_progress |= controller_input->input.fast_forward != 0;

	if (previous_fast_forward_in_progress != fast_forward_in_progress)
	{
		// Disable V-sync so that 60Hz displays aren't locked to 1x speed while fast-forwarding
		if (use_vsync)
			SDL_RenderSetVSync(renderer, !fast_forward_in_progress);
	}
}

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
static bool rewind_in_progress;

static void UpdateRewindStatus(void)
{
	rewind_in_progress = keyboard_input.rewind;

	for (ControllerInput *controller_input = controller_input_list_head; controller_input != NULL; controller_input = controller_input->next)
		rewind_in_progress |= controller_input->input.rewind != 0;
}
#endif


/////////////////
// File Picker //
/////////////////

static const char *active_file_picker_popup;
static std::function<bool (const char *path)> popup_callback;
static bool is_save_dialog;

#ifdef _WIN32
 #include <windows.h>
 #include "SDL_syswm.h"
#elif defined(__unix__)
 #include <unistd.h>

 #if defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L
  #define POSIX

  #include <stdio.h>
  #include <sys/wait.h>

  static char *last_file_dialog_directory;
  static bool prefer_kdialog;
 #endif
#endif

#if defined(_WIN32) || defined(POSIX)
#define HAS_NATIVE_FILE_DIALOGS
#endif

#ifdef HAS_NATIVE_FILE_DIALOGS
static bool use_native_file_dialogs = true;
#endif

static void FileDialog(char const* const title, const std::function<bool (const char *path)> callback, bool save)
{
#ifdef _WIN32
	if (use_native_file_dialogs)
	{
		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);

		char path_buffer[MAX_PATH];
		path_buffer[0] = '\0';

		OPENFILENAME ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = SDL_GetWindowWMInfo(window, &info) ? info.info.win.window : NULL;
		ofn.lpstrFile = path_buffer;
		ofn.nMaxFile = CC_COUNT_OF(path_buffer);
		ofn.lpstrTitle = title;

		char *working_directory_buffer = NULL;
		const DWORD working_directory_buffer_size = GetCurrentDirectory(0, NULL);

		if (working_directory_buffer_size != 0)
		{
			working_directory_buffer = (char*)SDL_malloc(working_directory_buffer_size);

			if (working_directory_buffer != NULL)
			{
				if (GetCurrentDirectory(working_directory_buffer_size, working_directory_buffer) == 0)
				{
					SDL_free(working_directory_buffer);
					working_directory_buffer = NULL;
				}
			}
		}

		if (save)
		{
			ofn.Flags = OFN_OVERWRITEPROMPT;
			if (GetSaveFileName(&ofn))
				callback(path_buffer);
		}
		else
		{
			ofn.Flags = OFN_FILEMUSTEXIST;
			if (GetOpenFileName(&ofn))
				callback(path_buffer);
		}

		if (working_directory_buffer != NULL)
		{
			SetCurrentDirectory(working_directory_buffer);
			SDL_free(working_directory_buffer);
		}
	}
	else
#elif defined(POSIX)
	bool done = false;

	if (use_native_file_dialogs)
	{
		for (unsigned int i = 0; i < 2; ++i)
		{
			if (!done)
			{
				char *command;

				// Construct the command to invoke Zenity/kdialog.
				const int bytes_printed = (i == 0) != prefer_kdialog ?
					SDL_asprintf(&command, "zenity --file-selection %s --title=\"%s\" --filename=\"%s\"",
						save ? "--save" : "",
						title,
						last_file_dialog_directory == NULL ? "" : last_file_dialog_directory)
					:
					SDL_asprintf(&command, "kdialog --get%sfilename --title \"%s\" \"%s\"",
						save ? "save" : "open",
						title,
						last_file_dialog_directory == NULL ? "" : last_file_dialog_directory)
					;

				if (bytes_printed >= 0)
				{
					// Invoke Zenity/kdialog.
					FILE *path_stream = popen(command, "r");

					SDL_free(command);

					if (path_stream != NULL)
					{
					#define GROW_SIZE 0x100
						// Read the whole path returned by Zenity/kdialog.
						// This is very complicated due to handling arbitrarily long paths.
						size_t path_buffer_length = 0;
						char *path_buffer = (char*)SDL_malloc(GROW_SIZE + 1); // '+1' for the null character.

						if (path_buffer != NULL)
						{
							for (;;)
							{
								const size_t path_length = fread(&path_buffer[path_buffer_length], 1, GROW_SIZE, path_stream);
								path_buffer_length += path_length;

								if (path_length != GROW_SIZE)
									break;

								char* const new_path_buffer = (char*)SDL_realloc(path_buffer, path_buffer_length + GROW_SIZE + 1);

								if (new_path_buffer == NULL)
								{
									SDL_free(path_buffer);
									path_buffer = NULL;
									break;
								}

								path_buffer = new_path_buffer;
							}
						#undef GROW_SIZE

							if (path_buffer != NULL)
							{
								// Handle Zenity's/kdialog's return value.
								const int exit_status = pclose(path_stream);
								path_stream = NULL;

								if (exit_status != -1 && WIFEXITED(exit_status))
								{
									switch (WEXITSTATUS(exit_status))
									{
										case 0: // Success.
										{
											done = true;

											path_buffer[path_buffer_length - (path_buffer[path_buffer_length - 1] == '\n')] = '\0';
											callback(path_buffer);

											char* const directory_separator = SDL_strrchr(path_buffer, '/');

											if (directory_separator == NULL)
												path_buffer[0] = '\0';
											else
												directory_separator[1] = '\0';

											if (last_file_dialog_directory != NULL)
												SDL_free(last_file_dialog_directory);

											last_file_dialog_directory = path_buffer;
											path_buffer = NULL;

											break;
										}

										case 1: // No file selected.
											done = true;
											break;
									}
								}
							}

							SDL_free(path_buffer);
						}

						if (path_stream != NULL)
							pclose(path_stream);
					}
				}
			}
		}
	}

	if (!done)
#endif
	{
		active_file_picker_popup = title;
		popup_callback = callback;
		is_save_dialog = save;
	}
}

void OpenFileDialog(char const* const title, const std::function<bool (const char *path)> callback)
{
	FileDialog(title, callback, false);
}

void SaveFileDialog(char const* const title, const std::function<bool (const char *path)> callback)
{
	FileDialog(title, callback, true);
}

static void DoFilePicker(void)
{
	if (active_file_picker_popup != NULL)
	{
		if (!ImGui::IsPopupOpen(active_file_picker_popup))
			ImGui::OpenPopup(active_file_picker_popup);

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(active_file_picker_popup, NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			static int text_buffer_size;
			static char *text_buffer = NULL;

			if (text_buffer == NULL)
			{
				text_buffer_size = 0x40;
				text_buffer = (char*)SDL_malloc(text_buffer_size);
				text_buffer[0] = '\0';
			}

			const ImGuiInputTextCallback callback = [](ImGuiInputTextCallbackData* const data)
			{
				if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
				{
					if (data->BufSize > text_buffer_size)
					{
						int new_text_buffer_size = (INT_MAX >> 1) + 1; // Largest power of 2.

						// Find the first power of two that is larger than the requested buffer size.
						while (data->BufSize < new_text_buffer_size >> 1)
							new_text_buffer_size >>= 1;

						char* const new_text_buffer = (char*)SDL_realloc(text_buffer, new_text_buffer_size);

						if (new_text_buffer != NULL)
						{
							data->BufSize = text_buffer_size = new_text_buffer_size;
							data->Buf = text_buffer = new_text_buffer;
						}
					}
				}

				return 0;
			};

			/* Make it so that the text box is selected by default,
			   so that the user doesn't have to click on it first.
			   If a file is dropped onto the dialog, focus on the
			   'open' button instead or else the text box won't show
			   the dropped file's path. */
			if (drag_and_drop_filename != NULL)
				ImGui::SetKeyboardFocusHere(1);
			else if (ImGui::IsWindowAppearing())
				ImGui::SetKeyboardFocusHere();

			ImGui::TextUnformatted("Filename:");
			const bool enter_pressed = ImGui::InputText("##filename", text_buffer, text_buffer_size, ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_EnterReturnsTrue, callback);

			// Set the text box's contents to the dropped file's path.
			if (drag_and_drop_filename != NULL)
			{
				SDL_free(text_buffer);
				text_buffer = drag_and_drop_filename;
				drag_and_drop_filename = NULL;

				text_buffer_size = SDL_strlen(text_buffer) + 1;
			}

			ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x + (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x - ImGui::GetStyle().FramePadding.x * 4 - ImGui::CalcTextSize(is_save_dialog ? "Save" : "Open").x - ImGui::CalcTextSize("Cancel").x) / 2);
			const bool ok_pressed = ImGui::Button(is_save_dialog ? "Save" : "Open");
			ImGui::SameLine();
			bool exit = ImGui::Button("Cancel");

			bool submit = false;

			if (enter_pressed || ok_pressed)
			{
				if (!is_save_dialog || !FileExists(text_buffer))
					submit = true;
				else
					ImGui::OpenPopup("File Already Exists");
			}

			ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

			if (ImGui::BeginPopupModal("File Already Exists", NULL, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::TextUnformatted("A file with that name already exists. Overwrite it?");

				if (ImGui::Button("Yes"))
				{
					submit = true;
					ImGui::CloseCurrentPopup();
				}

				ImGui::SameLine();

				if (ImGui::Button("No"))
					ImGui::CloseCurrentPopup();

				ImGui::EndPopup();
			}

			if (submit)
				if (popup_callback(text_buffer))
					exit = true;

			if (exit)
			{
				ImGui::CloseCurrentPopup();
				SDL_free(text_buffer);
				text_buffer = NULL;
				active_file_picker_popup = NULL;
			}

			ImGui::EndPopup();
		}
	}
}


/////////////
// Tooltip //
/////////////

static void DoToolTip(const char* const text)
{
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(text);
		ImGui::EndTooltip();
	}
}


/////////
// INI //
/////////

static char* INIReadCallback(char* const buffer, const int length, void* const user)
{
	SDL_RWops* const sdl_rwops = (SDL_RWops*)user;

	int i = 0;

	while (i < length - 1)
	{
		char character;
		if (SDL_RWread(sdl_rwops, &character, 1, 1) == 0)
		{
			if (i == 0)
				return 0;

			break;
		}

		buffer[i] = character;

		++i;

		if (character == '\n')
			break;
	}

	buffer[i] = '\0';
	return buffer;
}

static int INIParseCallback(void* const user, const char* const section, const char* const name, const char* const value)
{
	(void)user;

	if (SDL_strcmp(section, "Miscellaneous") == 0)
	{
		const bool state = SDL_strcmp(value, "on") == 0;

		if (SDL_strcmp(name, "vsync") == 0)
			use_vsync = state;
		else if (SDL_strcmp(name, "integer-screen-scaling") == 0)
			integer_screen_scaling = state;
		else if (SDL_strcmp(name, "tall-interlace-mode-2") == 0)
			tall_double_resolution_mode = state;
		else if (SDL_strcmp(name, "low-pass-filter") == 0)
			low_pass_filter = state;
		else if (SDL_strcmp(name, "pal") == 0)
			clownmdemu_configuration.general.tv_standard = state ? CLOWNMDEMU_TV_STANDARD_PAL : CLOWNMDEMU_TV_STANDARD_NTSC;
		else if (SDL_strcmp(name, "japanese") == 0)
			clownmdemu_configuration.general.region = state ? CLOWNMDEMU_REGION_DOMESTIC : CLOWNMDEMU_REGION_OVERSEAS;
	#ifdef POSIX
		else if (SDL_strcmp(name, "last-directory") == 0)
			last_file_dialog_directory = SDL_strdup(value);
		else if (SDL_strcmp(name, "prefer-kdialog") == 0)
			prefer_kdialog = state;
	#endif
	}
	else if (SDL_strcmp(section, "Keyboard Bindings") == 0)
	{
		char *string_end;

		errno = 0;
		const SDL_Scancode scancode = (SDL_Scancode)SDL_strtoul(name, &string_end, 0);

		if (errno != ERANGE && string_end >= SDL_strchr(name, '\0') && scancode < SDL_NUM_SCANCODES)
		{
			errno = 0;
			const InputBinding input_binding = (InputBinding)SDL_strtoul(value, &string_end, 0);

			if (errno != ERANGE && string_end >= SDL_strchr(name, '\0'))
				keyboard_bindings[scancode] = input_binding;
		}

	}
	else if (SDL_strcmp(section, "Recent Software") == 0)
	{
		if (SDL_strcmp(name, "path") == 0)
			if (value[0] != '\0')
				AddToRecentSoftware(value, true);
	}

	return 1;
}


///////////////////
// Configuration //
///////////////////

static void LoadConfiguration(void)
{
	// Set default settings.

	// Default V-sync.
	const int display_index = SDL_GetWindowDisplayIndex(window);

	if (display_index >= 0)
	{
		SDL_DisplayMode display_mode;

		if (SDL_GetCurrentDisplayMode(display_index, &display_mode) == 0)
		{
			// Enable V-sync on displays with an FPS of a multiple of 60.
			use_vsync = display_mode.refresh_rate % 60 == 0;
		}
	}

	// Default other settings.
	integer_screen_scaling = false;
	tall_double_resolution_mode = false;
	low_pass_filter = true;

	clownmdemu_configuration.general.region = CLOWNMDEMU_REGION_OVERSEAS;
	clownmdemu_configuration.general.tv_standard = CLOWNMDEMU_TV_STANDARD_NTSC;

	SDL_RWops* const file = SDL_RWFromFile(CONFIG_FILENAME, "r");

	// Load the configuration file, overwriting the above settings.
	if (file == NULL || ini_parse_stream(INIReadCallback, file, INIParseCallback, NULL) != 0)
	{
		// Failed to read configuration file: set defaults key bindings.
		for (size_t i = 0; i < CC_COUNT_OF(keyboard_bindings); ++i)
			keyboard_bindings[i] = INPUT_BINDING_NONE;

		keyboard_bindings[SDL_SCANCODE_UP] = INPUT_BINDING_CONTROLLER_UP;
		keyboard_bindings[SDL_SCANCODE_DOWN] = INPUT_BINDING_CONTROLLER_DOWN;
		keyboard_bindings[SDL_SCANCODE_LEFT] = INPUT_BINDING_CONTROLLER_LEFT;
		keyboard_bindings[SDL_SCANCODE_RIGHT] = INPUT_BINDING_CONTROLLER_RIGHT;
		keyboard_bindings[SDL_SCANCODE_Z] = INPUT_BINDING_CONTROLLER_A;
		keyboard_bindings[SDL_SCANCODE_X] = INPUT_BINDING_CONTROLLER_B;
		keyboard_bindings[SDL_SCANCODE_C] = INPUT_BINDING_CONTROLLER_C;
		keyboard_bindings[SDL_SCANCODE_RETURN] = INPUT_BINDING_CONTROLLER_START;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_PAUSE)] = INPUT_BINDING_PAUSE;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F11)] = INPUT_BINDING_TOGGLE_FULLSCREEN;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_TAB)] = INPUT_BINDING_RESET;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F1)] = INPUT_BINDING_TOGGLE_CONTROL_PAD;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F5)] = INPUT_BINDING_QUICK_SAVE_STATE;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F9)] = INPUT_BINDING_QUICK_LOAD_STATE;
		keyboard_bindings[SDL_SCANCODE_SPACE] = INPUT_BINDING_FAST_FORWARD;
		keyboard_bindings[SDL_SCANCODE_R] = INPUT_BINDING_REWIND;
	}

	if (file != NULL)
		SDL_RWclose(file);

	// Apply the V-sync setting, now that it's been decided.
	SDL_RenderSetVSync(renderer, use_vsync);
}

static void SaveConfiguration(void)
{
	// Save configuration file:
	SDL_RWops *file = SDL_RWFromFile(CONFIG_FILENAME, "w");

	if (file == NULL)
	{
		PrintError("Could not open configuration file for writing.");
	}
	else
	{
	#define PRINT_STRING(FILE, STRING) SDL_RWwrite(FILE, STRING, sizeof(STRING) - 1, 1)
		// Save keyboard bindings.
		PRINT_STRING(file, "[Miscellaneous]\n");

	#define PRINT_BOOLEAN_OPTION(FILE, NAME, VARIABLE) \
		PRINT_STRING(FILE, NAME " = "); \
		if (VARIABLE) \
			PRINT_STRING(FILE, "on\n"); \
		else \
			PRINT_STRING(FILE, "off\n");

		PRINT_BOOLEAN_OPTION(file, "vsync", use_vsync);
		PRINT_BOOLEAN_OPTION(file, "integer-screen-scaling", integer_screen_scaling);
		PRINT_BOOLEAN_OPTION(file, "tall-interlace-mode-2", tall_double_resolution_mode);
		PRINT_BOOLEAN_OPTION(file, "low-pass-filter", low_pass_filter);
		PRINT_BOOLEAN_OPTION(file, "pal", clownmdemu_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_PAL);
		PRINT_BOOLEAN_OPTION(file, "japanese", clownmdemu_configuration.general.region == CLOWNMDEMU_REGION_DOMESTIC);

	#ifdef POSIX
		if (last_file_dialog_directory != NULL)
		{
			PRINT_STRING(file, "last-directory = ");
			SDL_RWwrite(file, last_file_dialog_directory, SDL_strlen(last_file_dialog_directory), 1);
			PRINT_STRING(file, "\n");
		}
		PRINT_BOOLEAN_OPTION(file, "prefer-kdialog", prefer_kdialog);
	#endif

		// Save keyboard bindings.
		PRINT_STRING(file, "\n[Keyboard Bindings]\n");

		for (size_t i = 0; i < CC_COUNT_OF(keyboard_bindings); ++i)
		{
			if (keyboard_bindings[i] != INPUT_BINDING_NONE)
			{
				char buffer[0x20];
				SDL_snprintf(buffer, sizeof(buffer), "%u = %u\n", (unsigned int)i, keyboard_bindings[i]);
				SDL_RWwrite(file, buffer, SDL_strlen(buffer), 1);
			}
		}

		// Save recent software paths.
		PRINT_STRING(file, "\n[Recent Software]\n");

		for (DoublyLinkedList_Entry *entry = recent_software_list.head; entry != NULL; entry = entry->next)
		{
			RecentSoftware* const recent_software = CC_STRUCT_POINTER_FROM_MEMBER_POINTER(RecentSoftware, list, entry);

			PRINT_STRING(file, "path = ");
			SDL_RWwrite(file, recent_software->path, 1, SDL_strlen(recent_software->path));
			PRINT_STRING(file, "\n");
		}

		SDL_RWclose(file);
	}
}


///////////////////
// Main function //
///////////////////

int main(int argc, char **argv)
{
	InitError();

	// Initialise SDL2
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) < 0)
	{
		PrintError("SDL_Init failed with the following message - '%s'", SDL_GetError());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to initialise SDL2. The program will now close.", NULL);
	}
	else
	{
		if (!InitialiseVideo())
		{
			PrintError("InitVideo failed");
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to initialise video subsystem. The program will now close.", NULL);
		}
		else
		{
			DoublyLinkedList_Initialise(&recent_software_list);
			LoadConfiguration();

			if (!InitialiseFramebuffer())
			{
				PrintError("CreateFramebuffer failed");
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to initialise framebuffer. The program will now close.", NULL);
			}
			else
			{
				// Setup Dear ImGui context
				IMGUI_CHECKVERSION();
				ImGui::CreateContext();
				ImGuiIO &io = ImGui::GetIO();
				ImGuiStyle &style = ImGui::GetStyle();
				io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
				io.IniFilename = "clownmdemu-frontend-imgui.ini";

				// Setup Dear ImGui style
				ImGui::StyleColorsDark();
				//ImGui::StyleColorsLight();
				//ImGui::StyleColorsClassic();

				style.WindowBorderSize = 0.0f;
				style.PopupBorderSize = 0.0f;
				style.ChildBorderSize = 0.0f;
				style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
				style.TabRounding = 0.0f;
				style.ScrollbarRounding = 0.0f;

				ImVec4* colors = style.Colors;
				colors[ImGuiCol_Text] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
				colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.13f, 0.13f, 0.94f);
				colors[ImGuiCol_FrameBg] = ImVec4(0.35f, 0.35f, 0.35f, 0.54f);
				colors[ImGuiCol_FrameBgHovered] = ImVec4(0.69f, 0.69f, 0.69f, 0.40f);
				colors[ImGuiCol_FrameBgActive] = ImVec4(0.69f, 0.69f, 0.69f, 0.67f);
				colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.43f, 0.50f);
				colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
				colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
				colors[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
				colors[ImGuiCol_CheckMark] = ImVec4(0.63f, 0.63f, 0.63f, 1.00f);
				colors[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
				colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
				colors[ImGuiCol_Button] = ImVec4(0.41f, 0.41f, 0.41f, 0.63f);
				colors[ImGuiCol_ButtonHovered] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
				colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
				colors[ImGuiCol_Header] = ImVec4(0.38f, 0.38f, 0.38f, 0.39f);
				colors[ImGuiCol_HeaderHovered] = ImVec4(0.38f, 0.38f, 0.38f, 0.80f);
				colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
				colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.43f, 0.50f);
				colors[ImGuiCol_SeparatorHovered] = ImVec4(0.56f, 0.56f, 0.56f, 0.78f);
				colors[ImGuiCol_SeparatorActive] = ImVec4(0.63f, 0.63f, 0.63f, 1.00f);
				colors[ImGuiCol_ResizeGrip] = ImVec4(0.51f, 0.51f, 0.51f, 0.43f);
				colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.51f, 0.51f, 0.51f, 0.79f);
				colors[ImGuiCol_ResizeGripActive] = ImVec4(0.51f, 0.51f, 0.51f, 0.95f);
				colors[ImGuiCol_Tab] = ImVec4(0.31f, 0.31f, 0.31f, 0.86f);
				colors[ImGuiCol_TabHovered] = ImVec4(0.51f, 0.51f, 0.51f, 0.80f);
				colors[ImGuiCol_TabActive] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);

				// Apply DPI scale (also resize the window so that there's room for the menu bar).
				style.ScaleAllSizes(dpi_scale);
				const unsigned int font_size = CalculateFontSize();
				const float menu_bar_size = (float)font_size + style.FramePadding.y * 2.0f; // An inlined ImGui::GetFrameHeight that actually works
				SDL_SetWindowSize(window, (int)(320.0f * 2.0f * dpi_scale), (int)(224.0f * 2.0f * dpi_scale + menu_bar_size));

				// Setup Platform/Renderer backends
				ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
				ImGui_ImplSDLRenderer2_Init(renderer);

				// Load fonts
				ReloadFonts(font_size);

				// This should be called before any other clownmdemu functions are called!
				ClownMDEmu_SetErrorCallback(PrintErrorInternal);

				// Initialise the clownmdemu configuration struct.
				clownmdemu_configuration.vdp.sprites_disabled = cc_false;
				clownmdemu_configuration.vdp.window_disabled = cc_false;
				clownmdemu_configuration.vdp.planes_disabled[0] = cc_false;
				clownmdemu_configuration.vdp.planes_disabled[1] = cc_false;
				clownmdemu_configuration.fm.fm_channels_disabled[0] = cc_false;
				clownmdemu_configuration.fm.fm_channels_disabled[1] = cc_false;
				clownmdemu_configuration.fm.fm_channels_disabled[2] = cc_false;
				clownmdemu_configuration.fm.fm_channels_disabled[3] = cc_false;
				clownmdemu_configuration.fm.fm_channels_disabled[4] = cc_false;
				clownmdemu_configuration.fm.fm_channels_disabled[5] = cc_false;
				clownmdemu_configuration.fm.dac_channel_disabled = cc_false;
				clownmdemu_configuration.psg.tone_disabled[0] = cc_false;
				clownmdemu_configuration.psg.tone_disabled[1] = cc_false;
				clownmdemu_configuration.psg.tone_disabled[2] = cc_false;
				clownmdemu_configuration.psg.noise_disabled = cc_false;

				// Initialise persistent data such as lookup tables.
				ClownMDEmu_Constant_Initialise(&clownmdemu_constant);

				// Intiialise audio if we can (but it's okay if it fails).
				if (!InitialiseAudio())
				{
					PrintError("InitialiseAudio failed");
					SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning", "Unable to initialise audio subsystem: the program will not output audio!", window);
				}
				else
				{
					// Initialise resamplers.
					SetAudioPALMode(clownmdemu_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_PAL);
				}

				// If the user passed the path to the software on the command line, then load it here, automatically.
				if (argc > 1)
					OpenSoftwareFromFile(argv[1], &callbacks);
				else
					OpenSoftwareFromMemory(NULL, 0, &callbacks);

				// We are now ready to show the window
				SDL_ShowWindow(window);

				// Manages whether the program exits or not.
				bool quit = false;

				// Used for tracking when to pop the emulation display out into its own little window.
				bool pop_out = false;

				bool m68k_status = false;
				bool z80_status = false;
				bool m68k_ram_viewer = false;
				bool z80_ram_viewer = false;
				bool vdp_registers = false;
				bool window_plane_viewer = false;
				bool plane_a_viewer = false;
				bool plane_b_viewer = false;
				bool vram_viewer = false;
				bool cram_viewer = false;
				bool fm_status = false;
				bool psg_status = false;
				bool other_status = false;
				bool debugging_toggles_menu = false;
				bool options_menu = false;
				bool about_menu = false;

			#ifndef NDEBUG
				bool dear_imgui_demo_window = false;
			#endif

				while (!quit)
				{
					const bool emulator_on = rom_buffer != NULL;
					const bool emulator_running = emulator_on && (!emulator_paused || emulator_frame_advance) && active_file_picker_popup == NULL
					#ifdef CLOWNMDEMU_FRONTEND_REWINDING
						&& !(rewind_in_progress && state_rewind_remaining == 0)
					#endif
						;

					emulator_frame_advance = false;

				#ifdef CLOWNMDEMU_FRONTEND_REWINDING
					// Handle rewinding.
					if (emulator_running)
					{
						// We maintain a ring buffer of emulator states:
						// when rewinding, we go backwards through this buffer,
						// and when not rewinding, we go forwards through it.
						size_t from_index, to_index;

						if (rewind_in_progress)
						{
							--state_rewind_remaining;

							from_index = to_index = state_rewind_index;

							if (from_index == 0)
								from_index = CC_COUNT_OF(state_rewind_buffer) - 1;
							else
								--from_index;

							state_rewind_index = from_index;
						}
						else
						{
							if (state_rewind_remaining < CC_COUNT_OF(state_rewind_buffer) - 1)
								++state_rewind_remaining;

							from_index = to_index = state_rewind_index;

							if (to_index == CC_COUNT_OF(state_rewind_buffer) - 1)
								to_index = 0;
							else
								++to_index;

							state_rewind_index = to_index;
						}

						SDL_memcpy(&state_rewind_buffer[to_index], &state_rewind_buffer[from_index], sizeof(state_rewind_buffer[0]));

						emulation_state = &state_rewind_buffer[to_index];
						ClownMDEmu_Parameters_Initialise(&clownmdemu, &clownmdemu_configuration, &clownmdemu_constant, &emulation_state->clownmdemu);
					}
				#endif

					// This loop processes events and manages the framerate.
					for (;;)
					{
						// 300 is the magic number that prevents these calculations from ever dipping into numbers smaller than 1
						// (technically it's only required by the NTSC framerate: PAL doesn't need it).
						#define MULTIPLIER 300

						static Uint64 next_time;
						const Uint64 current_time = SDL_GetTicks64() * MULTIPLIER;

						int timeout = 0;

						if (current_time < next_time)
							timeout = (next_time - current_time) / MULTIPLIER;
						else if (current_time > next_time + 100 * MULTIPLIER) // If we're way past our deadline, then resync to the current tick instead of fast-forwarding
							next_time = current_time;

						// Obtain an event
						SDL_Event event;
						if (!SDL_WaitEventTimeout(&event, timeout)) // If the timeout has expired and there are no more events, exit this loop
						{
							// Calculate when the next frame will be
							Uint32 delta;

							switch (clownmdemu_configuration.general.tv_standard)
							{
								default:
								case CLOWNMDEMU_TV_STANDARD_NTSC:
									// Run at roughly 59.94FPS (60 divided by 1.001)
									delta = CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(1000ul * MULTIPLIER);
									break;

								case CLOWNMDEMU_TV_STANDARD_PAL:
									// Run at 50FPS
									delta = CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(1000ul * MULTIPLIER);
									break;
							}

							next_time += delta >> (fast_forward_in_progress ? 2 : 0);

							break;
						}

						#undef MULTIPLIER

						bool give_event_to_imgui = true;
						static bool anything_pressed_during_alt;

						// Process the event
						switch (event.type)
						{
							case SDL_QUIT:
								quit = true;
								break;

							case SDL_KEYDOWN:
								// Ignore repeated key inputs caused by holding the key down
								if (event.key.repeat)
									break;

								anything_pressed_during_alt = true;

								// Ignore CTRL+TAB (used by Dear ImGui for cycling between windows).
								if (event.key.keysym.sym == SDLK_TAB && (SDL_GetModState() & KMOD_CTRL) != 0)
									break;

								if (event.key.keysym.sym == SDLK_RETURN && (SDL_GetModState() & KMOD_ALT) != 0)
								{
									ToggleFullscreen();
									break;
								}

								if (event.key.keysym.sym == SDLK_ESCAPE)
								{
									// Exit fullscreen
									if (fullscreen)
									{
										fullscreen = false;
										SetFullscreen(fullscreen);
									}
								}

								// Prevent invalid memory accesses due to future API expansions.
								// TODO: Yet another reason to not use `SDL_NUM_SCANCODES`.
								if (event.key.keysym.scancode >= CC_COUNT_OF(keyboard_bindings))
									break;

								switch (keyboard_bindings[event.key.keysym.scancode])
								{
									case INPUT_BINDING_TOGGLE_FULLSCREEN:
										ToggleFullscreen();
										break;

									case INPUT_BINDING_TOGGLE_CONTROL_PAD:
										// Toggle which joypad the keyboard controls
										keyboard_input.bound_joypad ^= 1;
										break;

									default:
										break;
								}

								// Many inputs should not be acted upon while the emulator is not running.
								if (emulator_on && emulator_has_focus)
								{
									switch (keyboard_bindings[event.key.keysym.scancode])
									{
										case INPUT_BINDING_PAUSE:
											emulator_paused = !emulator_paused;
											break;

										case INPUT_BINDING_RESET:
											// Soft-reset console
											ClownMDEmu_Reset(&clownmdemu, &callbacks);

											emulator_paused = false;

											break;

										case INPUT_BINDING_QUICK_SAVE_STATE:
											// Save save state
											quick_save_exists = true;
											quick_save_state = *emulation_state;
											break;

										case INPUT_BINDING_QUICK_LOAD_STATE:
											// Load save state
											if (quick_save_exists)
											{
												*emulation_state = quick_save_state;

												emulator_paused = false;
											}

											break;

										default:
											break;
									}
								}
								// Fallthrough
							case SDL_KEYUP:
							{
								// Prevent invalid memory accesses due to future API expansions.
								// TODO: Yet another reason to not use `SDL_NUM_SCANCODES`.
								if (event.key.keysym.scancode >= CC_COUNT_OF(keyboard_bindings))
									break;

								// When a key-down is processed, cache the binding so that the corresponding key-up
								// affects the same input. This is to prevent phantom inputs when a key is unbinded
								// whilst it is being held.
								const InputBinding binding = (event.type == SDL_KEYUP ? keyboard_bindings_cached : keyboard_bindings)[event.key.keysym.scancode];
								keyboard_bindings_cached[event.key.keysym.scancode] = keyboard_bindings[event.key.keysym.scancode];

								const bool pressed = event.key.state == SDL_PRESSED;

								if (key_pressed[event.key.keysym.scancode] != pressed)
								{
									key_pressed[event.key.keysym.scancode] = pressed;

									// This chunk of code prevents ALT-ENTER from causing ImGui to enter the menu bar.
									// TODO: Remove this when Dear ImGui stops being dumb.
									if (event.key.keysym.scancode == SDL_SCANCODE_LALT)
									{
										if (pressed)
										{
											anything_pressed_during_alt = false;
										}
										else
										{
											if (anything_pressed_during_alt)
												give_event_to_imgui = false;
										}
									}

									const int delta = pressed ? 1 : -1;

									switch (binding)
									{
										#define DO_KEY(state, code) case code: keyboard_input.buttons[state] += delta; break

										DO_KEY(CLOWNMDEMU_BUTTON_UP, INPUT_BINDING_CONTROLLER_UP);
										DO_KEY(CLOWNMDEMU_BUTTON_DOWN, INPUT_BINDING_CONTROLLER_DOWN);
										DO_KEY(CLOWNMDEMU_BUTTON_LEFT, INPUT_BINDING_CONTROLLER_LEFT);
										DO_KEY(CLOWNMDEMU_BUTTON_RIGHT, INPUT_BINDING_CONTROLLER_RIGHT);
										DO_KEY(CLOWNMDEMU_BUTTON_A, INPUT_BINDING_CONTROLLER_A);
										DO_KEY(CLOWNMDEMU_BUTTON_B, INPUT_BINDING_CONTROLLER_B);
										DO_KEY(CLOWNMDEMU_BUTTON_C, INPUT_BINDING_CONTROLLER_C);
										DO_KEY(CLOWNMDEMU_BUTTON_START, INPUT_BINDING_CONTROLLER_START);

										#undef DO_KEY

										case INPUT_BINDING_FAST_FORWARD:
											// Toggle fast-forward
											keyboard_input.fast_forward += delta;

											if ((!emulator_paused && emulator_has_focus) || !pressed)
												UpdateFastForwardStatus();

											if (pressed && emulator_paused)
												emulator_frame_advance = true;

											break;

										#ifdef CLOWNMDEMU_FRONTEND_REWINDING
										case INPUT_BINDING_REWIND:
											keyboard_input.rewind += delta;

											if (emulator_has_focus || !pressed)
												UpdateRewindStatus();

											break;
										#endif

										default:
											break;
									}
								}

								break;
							}

							case SDL_CONTROLLERDEVICEADDED:
							{
								// Open the controller, and create an entry for it in the controller list.
								SDL_GameController *controller = SDL_GameControllerOpen(event.cdevice.which);

								if (controller == NULL)
								{
									PrintError("SDL_GameControllerOpen failed with the following message - '%s'", SDL_GetError());
								}
								else
								{
									const SDL_JoystickID joystick_instance_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));

									if (joystick_instance_id < 0)
									{
										PrintError("SDL_JoystickInstanceID failed with the following message - '%s'", SDL_GetError());
									}
									else
									{
										ControllerInput *controller_input = (ControllerInput*)SDL_calloc(sizeof(ControllerInput), 1);

										if (controller_input == NULL)
										{
											PrintError("Could not allocate memory for the new ControllerInput struct");
										}
										else
										{
											controller_input->joystick_instance_id = joystick_instance_id;

											controller_input->next = controller_input_list_head;
											controller_input_list_head = controller_input;

											break;
										}
									}

									SDL_GameControllerClose(controller);
								}

								break;
							}

							case SDL_CONTROLLERDEVICEREMOVED:
							{
								// Close the controller, and remove it from the controller list.
								SDL_GameController *controller = SDL_GameControllerFromInstanceID(event.cdevice.which);

								if (controller == NULL)
								{
									PrintError("SDL_GameControllerFromInstanceID failed with the following message - '%s'", SDL_GetError());
								}
								else
								{
									SDL_GameControllerClose(controller);
								}

								for (ControllerInput **controller_input_pointer = &controller_input_list_head; ; controller_input_pointer = &(*controller_input_pointer)->next)
								{
									if ((*controller_input_pointer) == NULL)
									{
										PrintError("Received an SDL_CONTROLLERDEVICEREMOVED event for an unrecognised controller");
										break;
									}

									ControllerInput *controller_input = *controller_input_pointer;

									if (controller_input->joystick_instance_id == event.cdevice.which)
									{
										*controller_input_pointer = controller_input->next;
										SDL_free(controller_input);
										break;
									}
								}

								break;
							}

							case SDL_CONTROLLERBUTTONDOWN:
								if (event.cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSTICK)
								{
									// Toggle Dear ImGui gamepad controls.
									io.ConfigFlags ^= ImGuiConfigFlags_NavEnableGamepad;
								}

								// Don't use inputs that are for Dear ImGui.
								if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0)
									break;

								switch (event.cbutton.button)
								{
									case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
										// Load save state
										if (quick_save_exists)
										{
											*emulation_state = quick_save_state;

											emulator_paused = false;

											SDL_GameControllerRumble(SDL_GameControllerFromInstanceID(event.cbutton.which), 0xFFFF / 2, 0, 1000 / 8);
										}

										break;

									case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
										// Save save state
										quick_save_exists = true;
										quick_save_state = *emulation_state;

										SDL_GameControllerRumble(SDL_GameControllerFromInstanceID(event.cbutton.which), 0xFFFF * 3 / 4, 0, 1000 / 8);
										break;
								}

								// Fallthrough
							case SDL_CONTROLLERBUTTONUP:
							{
								const bool pressed = event.cbutton.state == SDL_PRESSED;

								// Look for the controller that this event belongs to.
								for (ControllerInput *controller_input = controller_input_list_head; ; controller_input = controller_input->next)
								{
									// If we've reached the end of the list, then somehow we've received an event for a controller that we haven't registered.
									if (controller_input == NULL)
									{
										PrintError("Received an SDL_CONTROLLERBUTTONDOWN/SDL_CONTROLLERBUTTONUP event for an unrecognised controller");
										break;
									}

									// Check if the current controller is the one that matches this event.
									if (controller_input->joystick_instance_id == event.cbutton.which)
									{
										switch (event.cbutton.button)
										{
											#define DO_BUTTON(state, code) case code: controller_input->input.buttons[state] = pressed; break

											DO_BUTTON(CLOWNMDEMU_BUTTON_A, SDL_CONTROLLER_BUTTON_X);
											DO_BUTTON(CLOWNMDEMU_BUTTON_B, SDL_CONTROLLER_BUTTON_Y);
											DO_BUTTON(CLOWNMDEMU_BUTTON_C, SDL_CONTROLLER_BUTTON_B);
											DO_BUTTON(CLOWNMDEMU_BUTTON_B, SDL_CONTROLLER_BUTTON_A);
											DO_BUTTON(CLOWNMDEMU_BUTTON_START, SDL_CONTROLLER_BUTTON_START);

											#undef DO_BUTTON

											case SDL_CONTROLLER_BUTTON_DPAD_UP:
											case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
											case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
											case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
											{
												unsigned int direction;
												unsigned int button;

												switch (event.cbutton.button)
												{
													default:
													case SDL_CONTROLLER_BUTTON_DPAD_UP:
														direction = 0;
														button = CLOWNMDEMU_BUTTON_UP;
														break;

													case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
														direction = 1;
														button = CLOWNMDEMU_BUTTON_DOWN;
														break;

													case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
														direction = 2;
														button = CLOWNMDEMU_BUTTON_LEFT;
														break;

													case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
														direction = 3;
														button = CLOWNMDEMU_BUTTON_RIGHT;
														break;
												}

												controller_input->dpad[direction] = pressed;

												// Combine D-pad and left stick values into final joypad D-pad inputs.
												controller_input->input.buttons[button] = controller_input->left_stick[direction] || controller_input->dpad[direction];

												break;
											}

											case SDL_CONTROLLER_BUTTON_BACK:
												// Toggle which joypad the controller is bound to.
												if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
													if (pressed)
														controller_input->input.bound_joypad ^= 1;

												break;

											default:
												break;
										}

										break;
									}
								}

								break;
							}

							case SDL_CONTROLLERAXISMOTION:
								// Look for the controller that this event belongs to.
								for (ControllerInput *controller_input = controller_input_list_head; ; controller_input = controller_input->next)
								{
									// If we've reached the end of the list, then somehow we've received an event for a controller that we haven't registered.
									if (controller_input == NULL)
									{
										PrintError("Received an SDL_CONTROLLERAXISMOTION event for an unrecognised controller");
										break;
									}

									// Check if the current controller is the one that matches this event.
									if (controller_input->joystick_instance_id == event.caxis.which)
									{
										switch (event.caxis.axis)
										{
											case SDL_CONTROLLER_AXIS_LEFTX:
											case SDL_CONTROLLER_AXIS_LEFTY:
											{
												if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
													controller_input->left_stick_x = event.caxis.value;
												else //if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
													controller_input->left_stick_y = event.caxis.value;

												// Now that we have the left stick's X and Y values, let's do some trigonometry to figure out which direction(s) it's pointing in.

												// To start with, let's treat the X and Y values as a vector, and turn it into a unit vector.
												const float magnitude = SDL_sqrtf((float)(controller_input->left_stick_x * controller_input->left_stick_x + controller_input->left_stick_y * controller_input->left_stick_y));

												const float left_stick_x_unit = controller_input->left_stick_x / magnitude;
												const float left_stick_y_unit = controller_input->left_stick_y / magnitude;

												// Now that we have the stick's direction in the form of a unit vector,
												// we can create a dot product of it with another directional unit vector
												// to determine the angle between them.
												for (unsigned int i = 0; i < 4; ++i)
												{
													// Apply a deadzone.
													if (magnitude < 32768.0f / 4.0f)
													{
														controller_input->left_stick[i] = false;
													}
													else
													{
														// This is a list of directions expressed as unit vectors.
														static const float directions[4][2] = {
															{ 0.0f, -1.0f}, // Up
															{ 0.0f,  1.0f}, // Down
															{-1.0f,  0.0f}, // Left
															{ 1.0f,  0.0f}  // Right
														};

														// Perform dot product of stick's direction vector with other direction vector.
														const float delta_angle = SDL_acosf(left_stick_x_unit * directions[i][0] + left_stick_y_unit * directions[i][1]);

														// If the stick is within 67.5 degrees of the specified direction, then this will be true.
														controller_input->left_stick[i] = (delta_angle < (360.0f * 3.0f / 8.0f / 2.0f) * ((float)CC_PI / 180.0f)); // Half of 3/8 of 360 degrees converted to radians
													}

													static const unsigned int buttons[4] = {
														CLOWNMDEMU_BUTTON_UP,
														CLOWNMDEMU_BUTTON_DOWN,
														CLOWNMDEMU_BUTTON_LEFT,
														CLOWNMDEMU_BUTTON_RIGHT
													};

													// Combine D-pad and left stick values into final joypad D-pad inputs.
													controller_input->input.buttons[buttons[i]] = controller_input->left_stick[i] || controller_input->dpad[i];
												}

												break;
											}

										#ifdef CLOWNMDEMU_FRONTEND_REWINDING
											case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
										#endif
											case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
											{
												if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
												{
													const bool held = event.caxis.value > 0x7FFF / 8;

												#ifdef CLOWNMDEMU_FRONTEND_REWINDING
													if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
													{
														if (controller_input->left_trigger != held)
														{
															controller_input->input.rewind = held;
															UpdateRewindStatus();
														}

														controller_input->left_trigger = held;
													}
													else
												#endif
													{
														if (controller_input->right_trigger != held)
														{
															controller_input->input.fast_forward = held;
															UpdateFastForwardStatus();
														}

														controller_input->right_trigger = held;
													}
												}

												break;
											}

											default:
												break;
										}

										break;
									}
								}

								break;

							case SDL_DROPFILE:
								drag_and_drop_filename = event.drop.file;
								break;

							default:
								break;
						}

						if (give_event_to_imgui)
							ImGui_ImplSDL2_ProcessEvent(&event);
					}

					// Handle dynamic DPI support
					const float new_dpi = GetNewDPIScale();
					if (dpi_scale != new_dpi) // 96 DPI appears to be the "normal" DPI
					{
						style.ScaleAllSizes(new_dpi / dpi_scale);

						dpi_scale = new_dpi;

						ReloadFonts(CalculateFontSize());
					}

					if (emulator_running)
					{
						// Reset the audio buffers so that they can be mixed into.
						Mixer_Begin(&mixer);

						// Lock the texture so that we can write to its pixels later
						if (SDL_LockTexture(framebuffer_texture, NULL, (void**)&framebuffer_texture_pixels, &framebuffer_texture_pitch) < 0)
							framebuffer_texture_pixels = NULL;

						framebuffer_texture_pitch /= sizeof(Uint32);

						// Run the emulator for a frame
						ClownMDEmu_Iterate(&clownmdemu, &callbacks);

						// Unlock the texture so that we can draw it
						SDL_UnlockTexture(framebuffer_texture);

						// Resample, mix, and output the audio for this frame.
						// If there's a lot of audio queued, then don't queue any more.
						if (SDL_GetQueuedAudioSize(audio_device) < audio_device_buffer_size * 4)
							Mixer_End(&mixer, AudioPushCallback, NULL);

						++frame_counter;
					}

					// Start the Dear ImGui frame
					ImGui_ImplSDLRenderer2_NewFrame();
					ImGui_ImplSDL2_NewFrame();
					ImGui::NewFrame();

					// Handle drag-and-drop event.
					if (active_file_picker_popup == NULL && drag_and_drop_filename != NULL)
					{
						unsigned char *file_buffer;
						size_t file_size;
						LoadFileToBuffer(drag_and_drop_filename, &file_buffer, &file_size);

						if (file_buffer != NULL)
						{
							if (file_size >= sizeof(save_state_magic) && SDL_memcmp(file_buffer, save_state_magic, sizeof(save_state_magic)) == 0)
							{
								if (rom_buffer != NULL)
									LoadSaveStateFromMemory(file_buffer, file_size);

								SDL_free(file_buffer);
							}
							else
							{
								AddToRecentSoftware(drag_and_drop_filename, false);
								OpenSoftwareFromMemory(file_buffer, file_size, &callbacks);
								emulator_paused = false;
							}
						}

						SDL_free(drag_and_drop_filename);
						drag_and_drop_filename = NULL;
					}

				#ifndef NDEBUG
					if (dear_imgui_demo_window)
						ImGui::ShowDemoWindow(&dear_imgui_demo_window);
				#endif

					const ImGuiViewport *viewport = ImGui::GetMainViewport();

					// Maximise the window if needed.
					if (!pop_out)
					{
						ImGui::SetNextWindowPos(viewport->WorkPos);
						ImGui::SetNextWindowSize(viewport->WorkSize);
					}

					// Prevent the window from getting too small or we'll get division by zero errors later on.
					ImGui::SetNextWindowSizeConstraints(ImVec2(100.0f * dpi_scale, 100.0f * dpi_scale), ImVec2(FLT_MAX, FLT_MAX)); // Width > 100, Height > 100

					const bool show_menu_bar = !fullscreen
					                        || pop_out
					                        || m68k_status
					                        || z80_status
					                        || m68k_ram_viewer
					                        || z80_ram_viewer
					                        || vdp_registers
					                        || window_plane_viewer
					                        || plane_a_viewer
					                        || plane_b_viewer
					                        || vram_viewer
					                        || cram_viewer
					                        || fm_status
					                        || psg_status
					                        || other_status
					                        || debugging_toggles_menu
					                        || options_menu
					                        || about_menu
					                        || (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0;

					// Hide mouse when the user just wants a fullscreen display window
					if (!show_menu_bar)
						ImGui::SetMouseCursor(ImGuiMouseCursor_None);

					// We don't want the emulator window overlapping the others while it's maximised.
					ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBringToFrontOnFocus;

					// Hide as much of the window as possible when maximised.
					if (!pop_out)
						window_flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

					// Hide the menu bar when maximised in fullscreen.
					if (show_menu_bar)
						window_flags |= ImGuiWindowFlags_MenuBar;

					// Tweak the style so that the display fill the window.
					ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
					const bool not_collapsed = ImGui::Begin("Display", NULL, window_flags);
					ImGui::PopStyleVar();

					if (not_collapsed)
					{
						if (show_menu_bar && ImGui::BeginMenuBar())
						{
							if (ImGui::BeginMenu("Console"))
							{
								if (ImGui::MenuItem("Open Software..."))
								{
									OpenFileDialog("Open Software", [](const char* const path)
									{
										if (OpenSoftwareFromFile(path, &callbacks))
										{
											emulator_paused = false;
											return true;
										}
										else
										{
											return false;
										}
									});
								}

								if (ImGui::MenuItem("Close Software", NULL, false, emulator_on))
								{
									SDL_free(rom_buffer);
									rom_buffer = NULL;
									rom_buffer_size = 0;

									emulator_paused = false;
								}

								ImGui::Separator();

								ImGui::MenuItem("Pause", NULL, &emulator_paused, emulator_on);

								if (ImGui::MenuItem("Reset", NULL, false, emulator_on))
								{
									// Soft-reset console
									ClownMDEmu_Reset(&clownmdemu, &callbacks);

									emulator_paused = false;
								}

								ImGui::SeparatorText("Recent Software");

								if (recent_software_list.head == NULL)
								{
									ImGui::MenuItem("None", NULL, false, false);
								}
								else
								{
									for (DoublyLinkedList_Entry *entry = recent_software_list.head; entry != NULL; entry = entry->next)
									{
										RecentSoftware* const recent_software = CC_STRUCT_POINTER_FROM_MEMBER_POINTER(RecentSoftware, list, entry);

										// Display only the filename.
										const char* const forward_slash = SDL_strrchr(recent_software->path, '/');
									#ifdef _WIN32
										const char* const backward_slash = SDL_strrchr(recent_software->path, '\\');
										const char* const filename = CC_MAX(CC_MAX(forward_slash, backward_slash) + 1, recent_software->path);
									#else
										const char* const filename = CC_MAX(forward_slash + 1, recent_software->path);
									#endif

										if (ImGui::MenuItem(filename))
											if (OpenSoftwareFromFile(recent_software->path, &callbacks))
												emulator_paused = false;

										// Show the full path as a tooltip.
										DoToolTip(recent_software->path);
									}
								}

								ImGui::EndMenu();
							}

							if (ImGui::BeginMenu("Save States"))
							{
								if (ImGui::MenuItem("Quick Save", NULL, false, emulator_on))
								{
									quick_save_exists = true;
									quick_save_state = *emulation_state;
								}

								if (ImGui::MenuItem("Quick Load", NULL, false, emulator_on && quick_save_exists))
								{
									*emulation_state = quick_save_state;

									emulator_paused = false;
								}

								ImGui::Separator();

								if (ImGui::MenuItem("Save to File...", NULL, false, emulator_on))
								{
									// Obtain a filename and path from the user.
									SaveFileDialog("Create Save State", [](const char* const save_state_path)
									{
										bool success = false;

										// Save the current state to the specified file.
										SDL_RWops *file = SDL_RWFromFile(save_state_path, "wb");

										if (file == NULL)
										{
											PrintError("Could not open save state file for writing");
											SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not create save state file.", window);
										}
										else
										{
											if (SDL_RWwrite(file, save_state_magic, sizeof(save_state_magic), 1) != 1 || SDL_RWwrite(file, emulation_state, sizeof(*emulation_state), 1) != 1)
											{
												PrintError("Could not write save state file");
												SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not create save state file.", window);
											}
											else
											{
												success = true;
											}

											SDL_RWclose(file);
										}

										return success;
									});
								}

								if (ImGui::MenuItem("Load from File...", NULL, false, emulator_on))
									OpenFileDialog("Load Save State", LoadSaveStateFromFile);

								ImGui::EndMenu();
							}

							if (ImGui::BeginMenu("Debugging"))
							{
								if (ImGui::BeginMenu("68000"))
								{
									ImGui::MenuItem("Registers", NULL, &m68k_status);
									ImGui::MenuItem("RAM", NULL, &m68k_ram_viewer);

									ImGui::EndMenu();
								}

								if (ImGui::BeginMenu("Z80"))
								{
									ImGui::MenuItem("Registers", NULL, &z80_status);
									ImGui::MenuItem("RAM", NULL, &z80_ram_viewer);

									ImGui::EndMenu();
								}

								if (ImGui::BeginMenu("VDP"))
								{
									ImGui::MenuItem("Registers", NULL, &vdp_registers);

									ImGui::SeparatorText("Visualisers");

									ImGui::MenuItem("Window Plane", NULL, &window_plane_viewer);

									ImGui::MenuItem("Plane A", NULL, &plane_a_viewer);

									ImGui::MenuItem("Plane B", NULL, &plane_b_viewer);

									ImGui::MenuItem("VRAM", NULL, &vram_viewer);

									ImGui::MenuItem("CRAM", NULL, &cram_viewer);

									ImGui::EndMenu();
								}

								ImGui::MenuItem("FM", NULL, &fm_status);

								ImGui::MenuItem("PSG", NULL, &psg_status);

								ImGui::MenuItem("Other", NULL, &other_status);

								ImGui::Separator();

								ImGui::MenuItem("Toggles", NULL, &debugging_toggles_menu);

								ImGui::EndMenu();
							}

							if (ImGui::BeginMenu("Misc."))
							{
								if (ImGui::MenuItem("Fullscreen", NULL, &fullscreen))
									SetFullscreen(fullscreen);

								ImGui::MenuItem("Display Window", NULL, &pop_out);

							#ifndef NDEBUG
								ImGui::Separator();

								ImGui::MenuItem("Dear ImGui Demo Window", NULL, &dear_imgui_demo_window);

							#ifdef HAS_NATIVE_FILE_DIALOGS
								ImGui::MenuItem("Native File Dialogs", NULL, &use_native_file_dialogs);
							#endif
							#endif

								ImGui::Separator();

								ImGui::MenuItem("Options", NULL, &options_menu);

								ImGui::Separator();

								ImGui::MenuItem("About", NULL, &about_menu);

								ImGui::Separator();

								if (ImGui::MenuItem("Exit"))
									quit = true;

								ImGui::EndMenu();
							}

							ImGui::EndMenuBar();
						}

						// We need this block of logic to be outside of the below condition so that the emulator
						// has keyboard focus on startup without needing the window to be clicked first.
						// Weird behaviour - I know.
						const ImVec2 size_of_display_region = ImGui::GetContentRegionAvail();

						// Create an invisible button which detects when input is intended for the emulator.
						// We do this cursor stuff so that the framebuffer is drawn over the button.
						const ImVec2 cursor = ImGui::GetCursorPos();
						ImGui::InvisibleButton("Magical emulator focus detector", size_of_display_region);

						emulator_has_focus = ImGui::IsItemFocused();

						if (emulator_on)
						{
							ImGui::SetCursorPos(cursor);

							SDL_Texture *selected_framebuffer_texture = framebuffer_texture;

							const unsigned int work_width = (unsigned int)size_of_display_region.x;
							const unsigned int work_height = (unsigned int)size_of_display_region.y;

							unsigned int destination_width;
							unsigned int destination_height;

							switch (current_screen_height)
							{
								default:
									assert(false);
									// Fallthrough
								case 224:
									destination_width = 320;
									destination_height = 224;
									break;

								case 240:
									destination_width = 320;
									destination_height = 240;
									break;

								case 448:
									destination_width = tall_double_resolution_mode ? 320 : 640;
									destination_height = 448;
									break;

								case 480:
									destination_width = tall_double_resolution_mode ? 320 : 640;
									destination_height = 480;
									break;
							}

							unsigned int destination_width_scaled;
							unsigned int destination_height_scaled;

							ImVec2 uv1 = {(float)current_screen_width / (float)FRAMEBUFFER_WIDTH, (float)current_screen_height / (float)FRAMEBUFFER_HEIGHT};

							if (integer_screen_scaling)
							{
								// Round down to the nearest multiple of 'destination_width' and 'destination_height'.
								const unsigned int framebuffer_upscale_factor = CC_MAX(1, CC_MIN(work_width / destination_width, work_height / destination_height));

								destination_width_scaled = destination_width * framebuffer_upscale_factor;
								destination_height_scaled = destination_height * framebuffer_upscale_factor;
							}
							else
							{
								// Round to the nearest multiple of 'destination_width' and 'destination_height'.
								const unsigned int framebuffer_upscale_factor = CC_MAX(1, CC_MIN(CC_DIVIDE_ROUND(work_width, destination_width), CC_DIVIDE_ROUND(work_height, destination_height)));

								RecreateUpscaledFramebuffer(work_width, work_height);

								// Correct the aspect ratio of the rendered frame.
								// (256x224 and 320x240 should be the same width, but 320x224 and 320x240 should be different heights - this matches the behaviour of a real Mega Drive).
								if (work_width > work_height * destination_width / destination_height)
								{
									destination_width_scaled = work_height * destination_width / destination_height;
									destination_height_scaled = work_height;
								}
								else
								{
									destination_width_scaled = work_width;
									destination_height_scaled = work_width * destination_height / destination_width;
								}

								// Avoid blurring if...
								// 1. The upscale texture failed to be created.
								// 2. Blurring is unnecessary because the texture will be upscaled by an integer multiple.
								if (framebuffer_texture_upscaled != NULL && (destination_width_scaled % destination_width != 0 || destination_height_scaled % destination_height != 0))
								{
									// Render the upscaled framebuffer to the screen.
									selected_framebuffer_texture = framebuffer_texture_upscaled;

									// Before we can do that though, we have to actually render the upscaled framebuffer.
									SDL_Rect framebuffer_rect;
									framebuffer_rect.x = 0;
									framebuffer_rect.y = 0;
									framebuffer_rect.w = current_screen_width;
									framebuffer_rect.h = current_screen_height;

									SDL_Rect upscaled_framebuffer_rect;
									upscaled_framebuffer_rect.x = 0;
									upscaled_framebuffer_rect.y = 0;
									upscaled_framebuffer_rect.w = destination_width * framebuffer_upscale_factor;
									upscaled_framebuffer_rect.h = destination_height * framebuffer_upscale_factor;

									// Render to the upscaled framebuffer.
									SDL_SetRenderTarget(renderer, framebuffer_texture_upscaled);

									// Render.
									SDL_RenderCopy(renderer, framebuffer_texture, &framebuffer_rect, &upscaled_framebuffer_rect);

									// Switch back to actually rendering to the screen.
									SDL_SetRenderTarget(renderer, NULL);

									// Update the texture UV to suit the upscaled framebuffer.
									uv1.x = (float)upscaled_framebuffer_rect.w / (float)framebuffer_texture_upscaled_width;
									uv1.y = (float)upscaled_framebuffer_rect.h / (float)framebuffer_texture_upscaled_height;
								}
							}

							// Center the framebuffer in the available region.
							ImGui::SetCursorPosX((float)((int)ImGui::GetCursorPosX() + ((int)size_of_display_region.x - destination_width_scaled) / 2));
							ImGui::SetCursorPosY((float)((int)ImGui::GetCursorPosY() + ((int)size_of_display_region.y - destination_height_scaled) / 2));

							// Draw the upscaled framebuffer in the window.
							ImGui::Image(selected_framebuffer_texture, ImVec2((float)destination_width_scaled, (float)destination_height_scaled), ImVec2(0, 0), uv1);
						}
					}

					ImGui::End();

					if (m68k_status)
						Debug_M68k(&m68k_status, &clownmdemu.state->m68k, monospace_font);

					if (z80_status)
						Debug_Z80(&z80_status, &clownmdemu.state->z80, monospace_font);

					if (m68k_ram_viewer)
						Debug_Memory(&m68k_ram_viewer, monospace_font, "68000 RAM", clownmdemu.state->m68k_ram, CC_COUNT_OF(clownmdemu.state->m68k_ram));

					if (z80_ram_viewer)
						Debug_Memory(&z80_ram_viewer, monospace_font, "Z80 RAM", clownmdemu.state->z80_ram, CC_COUNT_OF(clownmdemu.state->z80_ram));

					const Debug_VDP_Data debug_vdp_data = {emulation_state->colours, renderer, window, dpi_scale, frame_counter};

					if (vdp_registers)
						Debug_VDP(&vdp_registers, &clownmdemu, monospace_font);

					if (window_plane_viewer)
						Debug_WindowPlane(&window_plane_viewer, &clownmdemu, &debug_vdp_data);

					if (plane_a_viewer)
						Debug_PlaneA(&plane_a_viewer, &clownmdemu, &debug_vdp_data);

					if (plane_b_viewer)
						Debug_PlaneB(&plane_b_viewer, &clownmdemu, &debug_vdp_data);

					if (vram_viewer)
						Debug_VRAM(&vram_viewer, &clownmdemu, &debug_vdp_data);

					if (cram_viewer)
						Debug_CRAM(&cram_viewer, &clownmdemu, &debug_vdp_data, monospace_font);

					if (fm_status)
						Debug_FM(&fm_status, &clownmdemu, monospace_font);

					if (psg_status)
						Debug_PSG(&psg_status, &clownmdemu, monospace_font);

					if (other_status)
					{
						if (ImGui::Begin("Other", &other_status, ImGuiWindowFlags_AlwaysAutoResize))
						{
							if (ImGui::BeginTable("Other", 2, ImGuiTableFlags_Borders))
							{
								ImGui::TableSetupColumn("Property");
								ImGui::TableSetupColumn("Value");
								ImGui::TableHeadersRow();

								ImGui::TableNextColumn();
								ImGui::TextUnformatted("Z80 Bank");
								ImGui::TableNextColumn();
								ImGui::PushFont(monospace_font);
								ImGui::Text("0x%06" CC_PRIXFAST16 "-0x%06" CC_PRIXFAST16, clownmdemu.state->z80_bank * 0x8000, (clownmdemu.state->z80_bank + 1) * 0x8000 - 1);
								ImGui::PopFont();

								ImGui::TableNextColumn();
								ImGui::TextUnformatted("68000 Has Z80 Bus");
								ImGui::TableNextColumn();
								ImGui::TextUnformatted(clownmdemu.state->m68k_has_z80_bus ? "Yes" : "No");

								ImGui::TableNextColumn();
								ImGui::TextUnformatted("Z80 Reset");
								ImGui::TableNextColumn();
								ImGui::TextUnformatted(clownmdemu.state->z80_reset ? "Yes" : "No");

								ImGui::EndTable();
							}
						}

						ImGui::End();
					}

					if (debugging_toggles_menu)
					{
						if (ImGui::Begin("Toggles", &debugging_toggles_menu, ImGuiWindowFlags_AlwaysAutoResize))
						{
							bool temp;

							ImGui::SeparatorText("VDP");

							if (ImGui::BeginTable("VDP Options", 2, ImGuiTableFlags_SizingStretchSame))
							{
								ImGui::TableNextColumn();
								temp = !clownmdemu_configuration.vdp.sprites_disabled;
								if (ImGui::Checkbox("Sprite Plane", &temp))
									clownmdemu_configuration.vdp.sprites_disabled = !clownmdemu_configuration.vdp.sprites_disabled;

								ImGui::TableNextColumn();
								temp = !clownmdemu_configuration.vdp.window_disabled;
								if (ImGui::Checkbox("Window Plane", &temp))
									clownmdemu_configuration.vdp.window_disabled = !clownmdemu_configuration.vdp.window_disabled;

								ImGui::TableNextColumn();
								temp = !clownmdemu_configuration.vdp.planes_disabled[0];
								if (ImGui::Checkbox("Plane A", &temp))
									clownmdemu_configuration.vdp.planes_disabled[0] = !clownmdemu_configuration.vdp.planes_disabled[0];

								ImGui::TableNextColumn();
								temp = !clownmdemu_configuration.vdp.planes_disabled[1];
								if (ImGui::Checkbox("Plane B", &temp))
									clownmdemu_configuration.vdp.planes_disabled[1] = !clownmdemu_configuration.vdp.planes_disabled[1];

								ImGui::EndTable();
							}

							ImGui::SeparatorText("FM");

							if (ImGui::BeginTable("FM Options", 2, ImGuiTableFlags_SizingStretchSame))
							{
								char buffer[] = "FM1";

								for (size_t i = 0; i < CC_COUNT_OF(clownmdemu_configuration.fm.fm_channels_disabled); ++i)
								{
									buffer[2] = '1' + i;
									ImGui::TableNextColumn();
									temp = !clownmdemu_configuration.fm.fm_channels_disabled[i];
									if (ImGui::Checkbox(buffer, &temp))
										clownmdemu_configuration.fm.fm_channels_disabled[i] = !clownmdemu_configuration.fm.fm_channels_disabled[i];
								}

								ImGui::TableNextColumn();
								temp = !clownmdemu_configuration.fm.dac_channel_disabled;
								if (ImGui::Checkbox("DAC", &temp))
									clownmdemu_configuration.fm.dac_channel_disabled = !clownmdemu_configuration.fm.dac_channel_disabled;

								ImGui::EndTable();
							}

							ImGui::SeparatorText("PSG");

							if (ImGui::BeginTable("PSG Options", 2, ImGuiTableFlags_SizingStretchSame))
							{
								char buffer[] = "PSG1";

								for (size_t i = 0; i < CC_COUNT_OF(clownmdemu_configuration.psg.tone_disabled); ++i)
								{
									buffer[3] = '1' + i;
									ImGui::TableNextColumn();
									temp = !clownmdemu_configuration.psg.tone_disabled[i];
									if (ImGui::Checkbox(buffer, &temp))
										clownmdemu_configuration.psg.tone_disabled[i] = !clownmdemu_configuration.psg.tone_disabled[i];
								}

								ImGui::TableNextColumn();
								temp = !clownmdemu_configuration.psg.noise_disabled;
								if (ImGui::Checkbox("PSG Noise", &temp))
									clownmdemu_configuration.psg.noise_disabled = !clownmdemu_configuration.psg.noise_disabled;

								ImGui::EndTable();
							}
						}

						ImGui::End();
					}

					if (options_menu)
					{
						ImGui::SetNextWindowSize(ImVec2(360.0f * dpi_scale, 360.0f * dpi_scale), ImGuiCond_FirstUseEver);

						if (ImGui::Begin("Options", &options_menu))
						{
							ImGui::SeparatorText("Console");

							if (ImGui::BeginTable("Console Options", 3))
							{
								ImGui::TableNextColumn();
								ImGui::TextUnformatted("TV Standard:");
								DoToolTip("Some games only work with a certain TV standard.");
								ImGui::TableNextColumn();
								if (ImGui::RadioButton("NTSC", clownmdemu_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_NTSC))
								{
									if (clownmdemu_configuration.general.tv_standard != CLOWNMDEMU_TV_STANDARD_NTSC)
									{
										clownmdemu_configuration.general.tv_standard = CLOWNMDEMU_TV_STANDARD_NTSC;
										SetAudioPALMode(false);
									}
								}
								DoToolTip("60 FPS");
								ImGui::TableNextColumn();
								if (ImGui::RadioButton("PAL", clownmdemu_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_PAL))
								{
									if (clownmdemu_configuration.general.tv_standard != CLOWNMDEMU_TV_STANDARD_PAL)
									{
										clownmdemu_configuration.general.tv_standard = CLOWNMDEMU_TV_STANDARD_PAL;
										SetAudioPALMode(true);
									}
								}
								DoToolTip("50 FPS");

								ImGui::TableNextColumn();
								ImGui::TextUnformatted("Region:");
								DoToolTip("Some games only work with a certain region.");
								ImGui::TableNextColumn();
								if (ImGui::RadioButton("Japan", clownmdemu_configuration.general.region == CLOWNMDEMU_REGION_DOMESTIC))
									clownmdemu_configuration.general.region = CLOWNMDEMU_REGION_DOMESTIC;
								ImGui::TableNextColumn();
								if (ImGui::RadioButton("Elsewhere", clownmdemu_configuration.general.region == CLOWNMDEMU_REGION_OVERSEAS))
									clownmdemu_configuration.general.region = CLOWNMDEMU_REGION_OVERSEAS;

								ImGui::EndTable();
							}

							ImGui::SeparatorText("Video");

							if (ImGui::BeginTable("Video Options", 2))
							{
								ImGui::TableNextColumn();
								if (ImGui::Checkbox("V-Sync", &use_vsync))
									if (!fast_forward_in_progress)
										SDL_RenderSetVSync(renderer, use_vsync);
								DoToolTip("Prevents screen tearing.");

								ImGui::TableNextColumn();
								if (ImGui::Checkbox("Integer Screen Scaling", &integer_screen_scaling) && integer_screen_scaling)
								{
									// Reclaim memory used by the upscaled framebuffer, since we won't be needing it anymore.
									SDL_DestroyTexture(framebuffer_texture_upscaled);
									framebuffer_texture_upscaled = NULL;
								}
								DoToolTip("Preserves pixel aspect ratio,\navoiding non-square pixels.");

								ImGui::TableNextColumn();
								ImGui::Checkbox("Tall Interlace Mode 2", &tall_double_resolution_mode);
								DoToolTip("Makes games that use Interlace Mode 2\nfor split-screen not appear squashed.");

								ImGui::EndTable();
							}

							ImGui::SeparatorText("Audio");

							if (ImGui::BeginTable("Audio Options", 2))
							{
								ImGui::TableNextColumn();
								if (ImGui::Checkbox("Low-Pass Filter", &low_pass_filter))
									Mixer_State_Initialise(&mixer_state, audio_device_sample_rate, pal_mode, low_pass_filter);
								DoToolTip("Makes the audio sound 'softer',\njust like on a real Mega Drive.");

								ImGui::EndTable();
							}

						#ifdef POSIX
							ImGui::SeparatorText("Preferred File Dialog");

							if (ImGui::BeginTable("Preferred File Dialog", 2))
							{
								ImGui::TableNextColumn();

								if (ImGui::RadioButton("Zenity (GTK)", !prefer_kdialog))
									prefer_kdialog = false;

								ImGui::TableNextColumn();

								if (ImGui::RadioButton("kdialog (Qt)", prefer_kdialog))
									prefer_kdialog = true;

								ImGui::EndTable();
							}
						#endif

							ImGui::SeparatorText("Keyboard Input");

							static bool sorted_scancodes_done;
							static SDL_Scancode sorted_scancodes[SDL_NUM_SCANCODES]; // TODO: `SDL_NUM_SCANCODES` is an internal macro, so use something standard!

							if (!sorted_scancodes_done)
							{
								sorted_scancodes_done = true;

								for (size_t i = 0; i < CC_COUNT_OF(sorted_scancodes); ++i)
									sorted_scancodes[i] = (SDL_Scancode)i;

								SDL_qsort(sorted_scancodes, CC_COUNT_OF(sorted_scancodes), sizeof(sorted_scancodes[0]),
									[](const void* const a, const void* const b)
								{
									const SDL_Scancode* const binding_1 = (const SDL_Scancode*)a;
									const SDL_Scancode* const binding_2 = (const SDL_Scancode*)b;

									return keyboard_bindings[*binding_1] - keyboard_bindings[*binding_2];
								}
								);
							}

							static SDL_Scancode selected_scancode;

							static const char* const binding_names[INPUT_BINDING__TOTAL] = {
								"None",
								"Control Pad Up",
								"Control Pad Down",
								"Control Pad Left",
								"Control Pad Right",
								"Control Pad A",
								"Control Pad B",
								"Control Pad C",
								"Control Pad Start",
								"Pause",
								"Reset",
								"Fast-Forward",
								"Rewind",
								"Quick Save State",
								"Quick Load State",
								"Toggle Fullscreen",
								"Toggle Control Pad"
							};

							if (ImGui::BeginTable("Keyboard Input Options", 2))
							{
								ImGui::TableNextColumn();
								if (ImGui::RadioButton("Control Pad #1", keyboard_input.bound_joypad == 0))
									keyboard_input.bound_joypad = 0;
								DoToolTip("Binds the keyboard to Control Pad #1.");


								ImGui::TableNextColumn();
								if (ImGui::RadioButton("Control Pad #2", keyboard_input.bound_joypad == 1))
									keyboard_input.bound_joypad = 1;
								DoToolTip("Binds the keyboard to Control Pad #2.");

								ImGui::EndTable();
							}

							if (ImGui::BeginTable("control pad bindings", 3, ImGuiTableFlags_Borders))
							{
								ImGui::TableSetupColumn("Key");
								ImGui::TableSetupColumn("Action");
								ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
								ImGui::TableHeadersRow();
								for (size_t i = 0; i < CC_COUNT_OF(sorted_scancodes); ++i)
								{
									if (keyboard_bindings[sorted_scancodes[i]] != INPUT_BINDING_NONE)
									{
										ImGui::TableNextColumn();
										ImGui::TextUnformatted(SDL_GetScancodeName((SDL_Scancode)sorted_scancodes[i]));

										ImGui::TableNextColumn();
										ImGui::TextUnformatted(binding_names[keyboard_bindings[sorted_scancodes[i]]]);

										ImGui::TableNextColumn();
										ImGui::PushID(i);
										if (ImGui::Button("X"))
											keyboard_bindings[sorted_scancodes[i]] = INPUT_BINDING_NONE;
										ImGui::PopID();
									}
								}
								ImGui::EndTable();
							}

							if (ImGui::Button("Add Binding"))
								ImGui::OpenPopup("Select Key");

							static bool scroll_to_add_bindings_button;

							if (scroll_to_add_bindings_button)
							{
								scroll_to_add_bindings_button = false;
								ImGui::SetScrollHereY(1.0f);
							}

							const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
							ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

							if (ImGui::BeginPopupModal("Select Key", NULL, ImGuiWindowFlags_AlwaysAutoResize))
							{
								bool next_menu = false;

								ImGui::TextUnformatted("Press the key that you want to bind, or press the Esc key to cancel...");
								ImGui::TextUnformatted("(The left Alt key cannot be bound, as it is used to access the menu bar).");

								int total_keys;
								const Uint8* const keys_pressed = SDL_GetKeyboardState(&total_keys);

								for (int i = 0; i < total_keys; ++i)
								{
									if (keys_pressed[i] && i != SDL_GetScancodeFromKey(SDLK_LALT))
									{
										ImGui::CloseCurrentPopup();

										// The 'escape' key will exit the menu without binding.
										if (i != SDL_GetScancodeFromKey(SDLK_ESCAPE))
										{
											next_menu = true;
											selected_scancode = (SDL_Scancode)i;
										}
										break;
									}
								}

								if (ImGui::Button("Cancel"))
									ImGui::CloseCurrentPopup();

								ImGui::EndPopup();

								if (next_menu)
									ImGui::OpenPopup("Select Action");
							}

							ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

							if (ImGui::BeginPopupModal("Select Action", NULL, ImGuiWindowFlags_AlwaysAutoResize))
							{
								bool previous_menu = false;

								if (ImGui::BeginListBox("##Actions"))
								{
									for (InputBinding i = (InputBinding)(INPUT_BINDING_NONE + 1); i < INPUT_BINDING__TOTAL; i = (InputBinding)(i + 1))
									{
										if (ImGui::Selectable(binding_names[i]))
										{
											ImGui::CloseCurrentPopup();
											keyboard_bindings[selected_scancode] = i;
											sorted_scancodes_done = false;
											scroll_to_add_bindings_button = true;
										}
									}
									ImGui::EndListBox();
								}

								ImGui::Text("Selected Key: %s", SDL_GetScancodeName(selected_scancode));

								if (ImGui::Button("Cancel"))
								{
									previous_menu = true;
									ImGui::CloseCurrentPopup();
								}

								ImGui::EndPopup();

								if (previous_menu)
									ImGui::OpenPopup("Select Key");
							}
						}

						ImGui::End();
					}

					if (about_menu)
					{
						ImGui::SetNextWindowSize(ImVec2(605.0f * dpi_scale, 430.0f * dpi_scale), ImGuiCond_FirstUseEver);

						if (ImGui::Begin("About", &about_menu, ImGuiWindowFlags_HorizontalScrollbar))
						{
							static const char licence_clownmdemu[] = {
								#include "licences/clownmdemu.h"
							};
							static const char licence_dear_imgui[] = {
								#include "licences/dear-imgui.h"
							};
						#ifdef IMGUI_ENABLE_FREETYPE
							static const char licence_freetype[] = {
								#include "licences/freetype.h"
							};
							static const char licence_freetype_bdf[] = {
								#include "licences/freetype-bdf.h"
							};
							static const char licence_freetype_pcf[] = {
								#include "licences/freetype-pcf.h"
							};
							static const char licence_freetype_fthash[] = {
								#include "licences/freetype-fthash.h"
							};
							static const char licence_freetype_ft_hb[] = {
								#include "licences/freetype-ft-hb.h"
							};
						#endif
							static const char licence_inih[] = {
								#include "licences/inih.h"
							};
							static const char licence_karla[] = {
								#include "licences/karla.h"
							};
							static const char licence_inconsolata[] = {
								#include "licences/inconsolata.h"
							};

							ImGui::SeparatorText("clownmdemu-frontend " VERSION);

							ImGui::TextUnformatted("This is a Sega Mega Drive (AKA Sega Genesis) emulator. Created by Clownacy.");
							const char* const url = "https://github.com/Clownacy/clownmdemu-frontend";
							if (ImGui::Button(url))
								SDL_OpenURL(url);

							ImGui::SeparatorText("Licences");

							if (ImGui::CollapsingHeader("clownmdemu"))
							{
								ImGui::PushFont(monospace_font);
								ImGui::TextUnformatted(licence_clownmdemu, licence_clownmdemu + sizeof(licence_clownmdemu));
								ImGui::PopFont();
							}

							if (ImGui::CollapsingHeader("Dear ImGui"))
							{
								ImGui::PushFont(monospace_font);
								ImGui::TextUnformatted(licence_dear_imgui, licence_dear_imgui + sizeof(licence_dear_imgui));
								ImGui::PopFont();
							}

						#ifdef IMGUI_ENABLE_FREETYPE
							if (ImGui::CollapsingHeader("FreeType"))
							{
								if (ImGui::TreeNode("General"))
								{
									ImGui::PushFont(monospace_font);
									ImGui::TextUnformatted(licence_freetype, licence_freetype + sizeof(licence_freetype));
									ImGui::PopFont();
									ImGui::TreePop();
								}

								if (ImGui::TreeNode("BDF Driver"))
								{
									ImGui::PushFont(monospace_font);
									ImGui::TextUnformatted(licence_freetype_bdf, licence_freetype_bdf + sizeof(licence_freetype_bdf));
									ImGui::PopFont();
									ImGui::TreePop();
								}

								if (ImGui::TreeNode("PCF Driver"))
								{
									ImGui::PushFont(monospace_font);
									ImGui::TextUnformatted(licence_freetype_pcf, licence_freetype_pcf + sizeof(licence_freetype_pcf));
									ImGui::PopFont();
									ImGui::TreePop();
								}

								if (ImGui::TreeNode("fthash.c & fthash.h"))
								{
									ImGui::PushFont(monospace_font);
									ImGui::TextUnformatted(licence_freetype_fthash, licence_freetype_fthash + sizeof(licence_freetype_fthash));
									ImGui::PopFont();
									ImGui::TreePop();
								}

								if (ImGui::TreeNode("ft-hb.c & ft-hb.h"))
								{
									ImGui::PushFont(monospace_font);
									ImGui::TextUnformatted(licence_freetype_ft_hb, licence_freetype_ft_hb + sizeof(licence_freetype_ft_hb));
									ImGui::PopFont();
									ImGui::TreePop();
								}
							}
						#endif

							if (ImGui::CollapsingHeader("inih"))
							{
								ImGui::PushFont(monospace_font);
								ImGui::TextUnformatted(licence_inih, licence_inih + sizeof(licence_inih));
								ImGui::PopFont();
							}

							if (ImGui::CollapsingHeader("Karla"))
							{
								ImGui::PushFont(monospace_font);
								ImGui::TextUnformatted(licence_karla, licence_karla + sizeof(licence_karla));
								ImGui::PopFont();
							}

							if (ImGui::CollapsingHeader("Inconsolata"))
							{
								ImGui::PushFont(monospace_font);
								ImGui::TextUnformatted(licence_inconsolata, licence_inconsolata + sizeof(licence_inconsolata));
								ImGui::PopFont();
							}

							ImGui::SeparatorText("SDL2 Drivers");

							if (ImGui::BeginTable("SDL2 Drivers", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
							{
								// Render
								ImGui::TableNextColumn();
								ImGui::TextUnformatted("Render");

								ImGui::TableNextColumn();

								SDL_RendererInfo info;
								if (SDL_GetRendererInfo(renderer, &info) == 0)
									ImGui::TextUnformatted(info.name);
								else
									ImGui::TextUnformatted("Unknown");

								// Video
								ImGui::TableNextColumn();
								ImGui::TextUnformatted("Video");

								ImGui::TableNextColumn();
								const char* const audio_driver_name = SDL_GetCurrentVideoDriver();
								ImGui::TextUnformatted(audio_driver_name != NULL ? audio_driver_name : "None");

								// Audio
								ImGui::TableNextColumn();
								ImGui::TextUnformatted("Audio");

								ImGui::TableNextColumn();
								const char* const video_driver_name = SDL_GetCurrentAudioDriver();
								ImGui::TextUnformatted(video_driver_name != NULL ? video_driver_name : "None");

								ImGui::EndTable();
							}
						}

						ImGui::End();
					}

					DoFilePicker();

					SDL_RenderClear(renderer);

					// Render Dear ImGui.
					ImGui::Render();
					ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

					// Finally display the rendered frame to the user.
					SDL_RenderPresent(renderer);
				}

				SDL_free(rom_buffer);

				DeinitialiseAudio();

				ImGui_ImplSDLRenderer2_Shutdown();
				ImGui_ImplSDL2_Shutdown();
				ImGui::DestroyContext();

				SDL_DestroyTexture(framebuffer_texture_upscaled);

				DeinitialiseFramebuffer();
			}

			SaveConfiguration();

		#ifdef POSIX
			SDL_free(last_file_dialog_directory);
		#endif

			// Free recent software list.
			while (recent_software_list.head != NULL)
			{
				RecentSoftware* const recent_software = CC_STRUCT_POINTER_FROM_MEMBER_POINTER(RecentSoftware, list, recent_software_list.head);

				DoublyLinkedList_Remove(&recent_software_list, recent_software_list.head);
				SDL_free(recent_software);
			}

			DeinitialiseVideo();
		}

		SDL_Quit();
	}

	return 0;
}

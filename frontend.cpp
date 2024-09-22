#include "frontend.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <climits> // For INT_MAX.
#include <cstddef>
#include <filesystem>
#include <format>
#include <forward_list>
#include <functional>
#include <iterator>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "SDL.h"

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#include "libraries/imgui/imgui.h"
#include "libraries/imgui/backends/imgui_impl_sdl2.h"
#include "libraries/imgui/backends/imgui_impl_sdlrenderer2.h"
#include "libraries/inih/ini.h"

#include "cd-reader.h"
#include "debug-fm.h"
#include "debug-frontend.h"
#include "debug-log.h"
#include "debug-m68k.h"
#include "debug-memory.h"
#include "debug-pcm.h"
#include "debug-psg.h"
#include "debug-vdp.h"
#include "debug-z80.h"
#include "disassembler.h"
#include "emulator-instance.h"
#include "file-utilities.h"
#include "window-popup.h"
#include "window-with-framebuffer.h"

#ifndef __EMSCRIPTEN__
#define FILE_PATH_SUPPORT
#endif

#define VERSION "v0.7.1"

static constexpr unsigned int INITIAL_WINDOW_WIDTH = 320 * 2;
static constexpr unsigned int INITIAL_WINDOW_HEIGHT = 224 * 2;

static constexpr unsigned int FRAMEBUFFER_WIDTH = 320;
static constexpr unsigned int FRAMEBUFFER_HEIGHT = 240 * 2; // *2 because of double-resolution mode.

static constexpr char DEFAULT_TITLE[] = "clownmdemu";

struct Input
{
	unsigned int bound_joypad = 0;
	std::array<unsigned char, CLOWNMDEMU_BUTTON_MAX> buttons = {0};
	unsigned char fast_forward = 0;
	unsigned char rewind = 0;
};

struct ControllerInput
{
	SDL_JoystickID joystick_instance_id;
	Sint16 left_stick_x = 0;
	Sint16 left_stick_y = 0;
	std::array<bool, 4> left_stick = {false};
	std::array<bool, 4> dpad = {false};
	bool left_trigger = false;
	bool right_trigger = false;
	Input input;

	ControllerInput(const SDL_JoystickID joystick_instance_id) : joystick_instance_id(joystick_instance_id) {}
};

#ifdef FILE_PATH_SUPPORT
struct RecentSoftware
{
	bool is_cd_file;
	std::filesystem::path path;
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
	INPUT_BINDING_CONTROLLER_X,
	INPUT_BINDING_CONTROLLER_Y,
	INPUT_BINDING_CONTROLLER_Z,
	INPUT_BINDING_CONTROLLER_START,
	INPUT_BINDING_CONTROLLER_MODE,
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

static Input keyboard_input;

static std::forward_list<ControllerInput> controller_input_list;

static std::array<InputBinding, SDL_NUM_SCANCODES> keyboard_bindings; // TODO: `SDL_NUM_SCANCODES` is an internal macro, so use something standard!
static std::array<InputBinding, SDL_NUM_SCANCODES> keyboard_bindings_cached; // TODO: `SDL_NUM_SCANCODES` is an internal macro, so use something standard!
static std::array<bool, SDL_NUM_SCANCODES> key_pressed; // TODO: `SDL_NUM_SCANCODES` is an internal macro, so use something standard!

#ifdef FILE_PATH_SUPPORT
static std::list<RecentSoftware> recent_software_list;
#endif
static std::filesystem::path drag_and_drop_filename;

static bool emulator_has_focus; // Used for deciding when to pass inputs to the emulator.
static bool emulator_paused;
static bool emulator_frame_advance;

static bool quick_save_exists;
static EmulatorInstance::State quick_save_state;

static float dpi_scale;
static unsigned int frame_counter;
static ImFont *monospace_font;

static bool use_vsync;
static bool integer_screen_scaling;
static bool tall_double_resolution_mode;

static Frontend::FrameRateCallback frame_rate_callback;

static DebugLog debug_log(dpi_scale, monospace_font);
static FileUtilities file_utilities(debug_log);

static std::optional<WindowWithFramebuffer> window;
static std::optional<EmulatorInstance> emulator;
static std::optional<DebugFM> debug_fm;
static std::optional<DebugFrontend> debug_frontend;
static std::optional<DebugM68k> debug_m68k;
static std::optional<DebugMemory> debug_memory;
static std::optional<DebugPCM> debug_pcm;
static std::optional<DebugPSG> debug_psg;
static std::optional<DebugVDP> debug_vdp;
static std::optional<DebugZ80> debug_z80;

static std::array<std::optional<WindowPopup>, 6> popup_windows;
static std::optional<WindowPopup> &options_window = popup_windows[0];
static std::optional<WindowPopup> &about_window = popup_windows[1];
static std::optional<WindowPopup> &debug_log_window = popup_windows[2];
static std::optional<WindowPopup> &debugging_toggles_window = popup_windows[3];
static std::optional<WindowPopup> &m68k_disassembler_window = popup_windows[4];
static std::optional<WindowPopup> &debug_frontend_window = popup_windows[5];

// Manages whether the program exits or not.
static bool quit;

// Used for tracking when to pop the emulation display out into its own little window.
static bool pop_out;

static bool m68k_status;
static bool mcd_m68k_status;
static bool z80_status;
static bool m68k_ram_viewer;
static bool z80_ram_viewer;
static bool word_ram_viewer;
static bool prg_ram_viewer;
static bool wave_ram_viewer;
static bool vdp_registers;
static bool sprite_list;
static bool sprite_viewer;
static bool window_plane_viewer;
static bool plane_a_viewer;
static bool plane_b_viewer;
static bool vram_viewer;
static bool cram_viewer;
static bool fm_status;
static bool psg_status;
static bool pcm_status;
static bool other_status;

#ifndef NDEBUG
static bool dear_imgui_demo_window;
#endif

static bool emulator_on, emulator_running;


////////////////////////////
// Emulator Functionality //
////////////////////////////

static cc_bool ReadInputCallback(const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
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
		for (const auto &controller_input : controller_input_list)
			if (controller_input.input.bound_joypad == player_id)
				value |= controller_input.input.buttons[button_id] != 0 ? cc_true : cc_false;
	}

	return value;
}

#ifdef FILE_PATH_SUPPORT
static void AddToRecentSoftware(const std::filesystem::path &path, const bool is_cd_file, const bool add_to_end)
{
	const auto absolute_path = std::filesystem::absolute(path);

	// If the path already exists in the list, then move it to the start of the list.
	for (auto recent_software = recent_software_list.begin(); recent_software != recent_software_list.end(); ++recent_software)
	{
		if (recent_software->path == absolute_path)
		{
			if (recent_software != recent_software_list.begin())
				recent_software_list.splice(recent_software_list.begin(), recent_software_list, recent_software, std::next(recent_software));

			return;
		}
	}

	// If the list is 10 items long, then discard the last item before we add a new one.
	if (recent_software_list.size() == 10)
		recent_software_list.pop_back();

	// Add the file to the list of recent software.
	if (add_to_end)
		recent_software_list.emplace_back(is_cd_file, absolute_path);
	else
		recent_software_list.emplace_front(is_cd_file, absolute_path);
}
#endif

// Manages whether the emulator runs at a higher speed or not.
static bool fast_forward_in_progress;

static void UpdateFastForwardStatus()
{
	const bool previous_fast_forward_in_progress = fast_forward_in_progress;

	fast_forward_in_progress = keyboard_input.fast_forward;

	for (const auto &controller_input : controller_input_list)
		fast_forward_in_progress |= controller_input.input.fast_forward != 0;

	if (previous_fast_forward_in_progress != fast_forward_in_progress)
	{
		// Disable V-sync so that 60Hz displays aren't locked to 1x speed while fast-forwarding
		if (use_vsync)
			SDL_RenderSetVSync(window->GetRenderer(), !fast_forward_in_progress);
	}
}

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
static void UpdateRewindStatus()
{
	bool will_rewind = keyboard_input.rewind;

	for (const auto &controller_input : controller_input_list)
		will_rewind |= controller_input.input.rewind != 0;

	emulator->Rewind(will_rewind);
}
#endif


//////////////////////////
// Upscaled Framebuffer //
//////////////////////////

static SDL_Texture *framebuffer_texture_upscaled;
static unsigned int framebuffer_texture_upscaled_width;
static unsigned int framebuffer_texture_upscaled_height;

static void RecreateUpscaledFramebuffer(const unsigned int display_width, const unsigned int display_height)
{
	static unsigned int previous_framebuffer_size_factor = 0;

	const unsigned int framebuffer_size_factor = std::max(1u, std::min(CC_DIVIDE_CEILING(display_width, 640), CC_DIVIDE_CEILING(display_height, 480)));

	if (framebuffer_texture_upscaled == nullptr || framebuffer_size_factor != previous_framebuffer_size_factor)
	{
		previous_framebuffer_size_factor = framebuffer_size_factor;

		framebuffer_texture_upscaled_width = 640 * framebuffer_size_factor;
		framebuffer_texture_upscaled_height = 480 * framebuffer_size_factor;

		SDL_DestroyTexture(framebuffer_texture_upscaled); // It should be safe to pass nullptr to this
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		framebuffer_texture_upscaled = SDL_CreateTexture(window->GetRenderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, framebuffer_texture_upscaled_width, framebuffer_texture_upscaled_height);

		if (framebuffer_texture_upscaled == nullptr)
		{
			debug_log.Log("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			// Disable blending, since we don't need it
			if (SDL_SetTextureBlendMode(framebuffer_texture_upscaled, SDL_BLENDMODE_NONE) < 0)
				debug_log.Log("SDL_SetTextureBlendMode failed with the following message - '%s'", SDL_GetError());
		}
	}
}

static bool GetUpscaledFramebufferSize(unsigned int &width, unsigned int &height)
{
	if (framebuffer_texture_upscaled == nullptr)
		return false;

	SDL_QueryTexture(framebuffer_texture_upscaled, nullptr, nullptr, reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height));
	return true;
}


///////////
// Misc. //
///////////

static void SetWindowTitleToSoftwareName()
{
	const std::string name = emulator->GetSoftwareName();

	// Use the default title if the ROM does not provide a name.
	// Micro Machines is one game that lacks a name.
	SDL_SetWindowTitle(window->GetSDLWindow(), name.empty() ? DEFAULT_TITLE : name.c_str());
}

static void LoadCartridgeFile(const std::vector<unsigned char> &&file_buffer)
{
	quick_save_exists = false;
	emulator->LoadCartridgeFile(std::move(file_buffer));

	SetWindowTitleToSoftwareName();
}

static bool LoadCartridgeFile([[maybe_unused]] const std::filesystem::path* const path, const SDL::RWops &file)
{
	std::vector<unsigned char> file_buffer;

	if (!file_utilities.LoadFileToBuffer(file_buffer, file))
	{
		debug_log.Log("Could not load the cartridge file");
		window->ShowErrorMessageBox("Failed to load the cartridge file.");
		return false;
	}

#ifdef FILE_PATH_SUPPORT
	if (path != nullptr)
		AddToRecentSoftware(*path, false, false);
#endif

	LoadCartridgeFile(std::move(file_buffer));

	return true;
}

static bool LoadCartridgeFile(const std::filesystem::path &path)
{
	const SDL::RWops file = SDL::RWFromFile(path, "rb");

	if (file == nullptr)
	{
		debug_log.Log("Could not load the cartridge file");
		window->ShowErrorMessageBox("Failed to load the cartridge file.");
		return false;
	}

	return LoadCartridgeFile(&path, file);
}

static bool LoadCDFile(const std::filesystem::path* const path, SDL::RWops &&file)
{
#ifdef FILE_PATH_SUPPORT
	if (path != nullptr)
		AddToRecentSoftware(*path, true, false);
#endif

	// Load the CD.
	if (!emulator->LoadCDFile(std::move(file), *path))
		return false;

	SetWindowTitleToSoftwareName();

	return true;
}

#ifdef FILE_PATH_SUPPORT
static bool LoadCDFile(const std::filesystem::path &path)
{
	SDL::RWops file = SDL::RWFromFile(path, "rb");

	if (file == nullptr)
	{
		debug_log.Log("Could not load the CD file");
		window->ShowErrorMessageBox("Failed to load the CD file.");
		return false;
	}

	return LoadCDFile(&path, std::move(file));
}

static bool LoadSoftwareFile(const bool is_cd_file, const std::filesystem::path &path)
{
	if (is_cd_file)
		return LoadCDFile(path);
	else
		return LoadCartridgeFile(path);
}
#endif

static bool LoadSaveState(const std::vector<unsigned char> &file_buffer)
{
	if (!emulator->LoadSaveStateFile(file_buffer))
	{
		debug_log.Log("Could not load save state file");
		window->ShowErrorMessageBox("Could not load save state file.");
		return false;
	}

	emulator_paused = false;

	return true;
}

static bool LoadSaveState(const SDL::RWops &file)
{
	std::vector<unsigned char> file_buffer;

	if (!file_utilities.LoadFileToBuffer(file_buffer, file))
	{
		debug_log.Log("Could not load save state file");
		window->ShowErrorMessageBox("Could not load save state file.");
		return false;
	}

	return LoadSaveState(file_buffer);
}

#ifdef FILE_PATH_SUPPORT
static bool CreateSaveState(const std::filesystem::path &path)
{
	bool success = true;

	const SDL::RWops file = SDL::RWFromFile(path, "wb");

	if (file == nullptr || !emulator->WriteSaveStateFile(file))
	{
		debug_log.Log("Could not create save state file");
		window->ShowErrorMessageBox("Could not create save state file.");
		success = false;
	}

	return success;
}
#endif

static void SetAudioPALMode(const bool enabled)
{
	frame_rate_callback(enabled);
	emulator->SetPALMode(enabled);
}

static std::filesystem::path ConfigPath()
{
	const auto path_cstr = SDL::Pointer<char>(SDL_GetPrefPath("clownacy", "clownmdemu-frontend"));

	if (path_cstr == nullptr)
		return {};

	const std::filesystem::path path(path_cstr.get());

	return path;
}

static std::filesystem::path GetConfigurationFilePath()
{
	return ConfigPath()/"configuration.ini";
}

static std::filesystem::path GetDearImGuiSettingsFilePath()
{
	return ConfigPath()/"dear-imgui-settings.ini";
}


/////////////
// Tooltip //
/////////////

static void DoToolTip(const std::string &text)
{
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(text.data(), text.data() + text.size());
		ImGui::EndTooltip();
	}
}


/////////
// INI //
/////////

static char* INIReadCallback(char* const buffer, const int length, void* const user)
{
	const SDL::RWops* const file = static_cast<const SDL::RWops*>(user);

	int i = 0;

	while (i < length - 1)
	{
		char character;
		if (SDL_RWread(file->get(), &character, 1, 1) == 0)
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

static int INIParseCallback([[maybe_unused]] void* const user_cstr, const char* const section_cstr, const char* const name_cstr, const char* const value_cstr)
{
	const std::string_view section(section_cstr);
	const std::string_view name(name_cstr);
	const std::string_view value(value_cstr);

	if (section == "Miscellaneous")
	{
		const bool state = value == "on";

		if (name == "vsync")
			use_vsync = state;
		else if (name == "integer-screen-scaling")
			integer_screen_scaling = state;
		else if (name == "tall-interlace-mode-2")
			tall_double_resolution_mode = state;
		else if (name == "low-pass-filter")
			emulator->SetLowPassFilter(state);
		else if (name == "low-volume-distortion")
			emulator->GetConfigurationFM().ladder_effect_disabled = !state;
		else if (name == "pal")
			SetAudioPALMode(state);
		else if (name == "japanese")
			emulator->SetDomestic(state);
	#ifdef FILE_PICKER_POSIX
		else if (name == "last-directory")
			file_utilities.last_file_dialog_directory = value;
		else if (name == "prefer-kdialog")
			file_utilities.prefer_kdialog = state;
	#endif
	}
	else if (section == "Keyboard Bindings")
	{
		unsigned int scancode_integer;
		const auto scancode_integer_result = std::from_chars(&name.front(), &name.back() + 1, scancode_integer, 10);

		if (scancode_integer_result.ec == std::errc{} && scancode_integer_result.ptr == &name.back() + 1 && scancode_integer < SDL_NUM_SCANCODES)
		{
			const SDL_Scancode scancode = static_cast<SDL_Scancode>(scancode_integer);

			unsigned int binding_index;
			const auto binding_index_result = std::from_chars(&value.front(), &value.back() + 1, binding_index, 10);

			InputBinding input_binding = INPUT_BINDING_NONE;

			if (binding_index_result.ec == std::errc() && binding_index_result.ptr == &value.back() + 1)
			{
				// Legacy numerical input bindings.
				static const std::array input_bindings = {
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
				#else
					INPUT_BINDING_NONE,
				#endif
					INPUT_BINDING_QUICK_SAVE_STATE,
					INPUT_BINDING_QUICK_LOAD_STATE,
					INPUT_BINDING_TOGGLE_FULLSCREEN,
					INPUT_BINDING_TOGGLE_CONTROL_PAD
				};

				if (binding_index < input_bindings.size())
					input_binding = input_bindings[binding_index];
			}
			else
			{
				if (value == "INPUT_BINDING_CONTROLLER_UP")
					input_binding = INPUT_BINDING_CONTROLLER_UP;
				else if (value == "INPUT_BINDING_CONTROLLER_DOWN")
					input_binding = INPUT_BINDING_CONTROLLER_DOWN;
				else if (value == "INPUT_BINDING_CONTROLLER_LEFT")
					input_binding = INPUT_BINDING_CONTROLLER_LEFT;
				else if (value == "INPUT_BINDING_CONTROLLER_RIGHT")
					input_binding = INPUT_BINDING_CONTROLLER_RIGHT;
				else if (value == "INPUT_BINDING_CONTROLLER_A")
					input_binding = INPUT_BINDING_CONTROLLER_A;
				else if (value == "INPUT_BINDING_CONTROLLER_B")
					input_binding = INPUT_BINDING_CONTROLLER_B;
				else if (value == "INPUT_BINDING_CONTROLLER_C")
					input_binding = INPUT_BINDING_CONTROLLER_C;
				else if (value == "INPUT_BINDING_CONTROLLER_X")
					input_binding = INPUT_BINDING_CONTROLLER_X;
				else if (value == "INPUT_BINDING_CONTROLLER_Y")
					input_binding = INPUT_BINDING_CONTROLLER_Y;
				else if (value == "INPUT_BINDING_CONTROLLER_Z")
					input_binding = INPUT_BINDING_CONTROLLER_Z;
				else if (value == "INPUT_BINDING_CONTROLLER_START")
					input_binding = INPUT_BINDING_CONTROLLER_START;
				else if (value == "INPUT_BINDING_CONTROLLER_MODE")
					input_binding = INPUT_BINDING_CONTROLLER_MODE;
				else if (value == "INPUT_BINDING_PAUSE")
					input_binding = INPUT_BINDING_PAUSE;
				else if (value == "INPUT_BINDING_RESET")
					input_binding = INPUT_BINDING_RESET;
				else if (value == "INPUT_BINDING_FAST_FORWARD")
					input_binding = INPUT_BINDING_FAST_FORWARD;
			#ifdef CLOWNMDEMU_FRONTEND_REWINDING
				else if (value == "INPUT_BINDING_REWIND")
					input_binding = INPUT_BINDING_REWIND;
			#endif
				else if (value == "INPUT_BINDING_QUICK_SAVE_STATE")
					input_binding = INPUT_BINDING_QUICK_SAVE_STATE;
				else if (value == "INPUT_BINDING_QUICK_LOAD_STATE")
					input_binding = INPUT_BINDING_QUICK_LOAD_STATE;
				else if (value == "INPUT_BINDING_TOGGLE_FULLSCREEN")
					input_binding = INPUT_BINDING_TOGGLE_FULLSCREEN;
				else if (value == "INPUT_BINDING_TOGGLE_CONTROL_PAD")
					input_binding = INPUT_BINDING_TOGGLE_CONTROL_PAD;
			}

			keyboard_bindings[scancode] = input_binding;
		}

	}
#ifdef FILE_PATH_SUPPORT
	else if (section == "Recent Software")
	{
		static bool is_cd_file = false;

		if (name == "cd")
		{
			is_cd_file = value == "true";
		}
		else if (name == "path")
		{
			if (!value.empty())
				AddToRecentSoftware(value, is_cd_file, true);
		}
	}
#endif

	return 1;
}


///////////////////
// Configuration //
///////////////////

static void LoadConfiguration()
{
	// Set default settings.

	// Default V-sync.
	const int display_index = SDL_GetWindowDisplayIndex(window->GetSDLWindow());

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
	emulator->SetLowPassFilter(true);
	integer_screen_scaling = false;
	tall_double_resolution_mode = false;

	emulator->SetDomestic(false);
	SetAudioPALMode(false);

	const SDL::RWops file = SDL::RWFromFile(GetConfigurationFilePath(), "r");

	// Load the configuration file, overwriting the above settings.
	if (file == nullptr || ini_parse_stream(INIReadCallback, const_cast<SDL::RWops*>(&file), INIParseCallback, nullptr) != 0)
	{
		// Failed to read configuration file: set defaults key bindings.
		for (std::size_t i = 0; i < keyboard_bindings.size(); ++i)
			keyboard_bindings[i] = INPUT_BINDING_NONE;

		keyboard_bindings[SDL_SCANCODE_UP] = INPUT_BINDING_CONTROLLER_UP;
		keyboard_bindings[SDL_SCANCODE_DOWN] = INPUT_BINDING_CONTROLLER_DOWN;
		keyboard_bindings[SDL_SCANCODE_LEFT] = INPUT_BINDING_CONTROLLER_LEFT;
		keyboard_bindings[SDL_SCANCODE_RIGHT] = INPUT_BINDING_CONTROLLER_RIGHT;
		keyboard_bindings[SDL_SCANCODE_Z] = INPUT_BINDING_CONTROLLER_A;
		keyboard_bindings[SDL_SCANCODE_X] = INPUT_BINDING_CONTROLLER_B;
		keyboard_bindings[SDL_SCANCODE_C] = INPUT_BINDING_CONTROLLER_C;
		keyboard_bindings[SDL_SCANCODE_A] = INPUT_BINDING_CONTROLLER_X;
		keyboard_bindings[SDL_SCANCODE_S] = INPUT_BINDING_CONTROLLER_Y;
		keyboard_bindings[SDL_SCANCODE_D] = INPUT_BINDING_CONTROLLER_Z;
		keyboard_bindings[SDL_SCANCODE_RETURN] = INPUT_BINDING_CONTROLLER_START;
		keyboard_bindings[SDL_SCANCODE_BACKSPACE] = INPUT_BINDING_CONTROLLER_MODE;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_PAUSE)] = INPUT_BINDING_PAUSE;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F11)] = INPUT_BINDING_TOGGLE_FULLSCREEN;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_TAB)] = INPUT_BINDING_RESET;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F1)] = INPUT_BINDING_TOGGLE_CONTROL_PAD;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F5)] = INPUT_BINDING_QUICK_SAVE_STATE;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F9)] = INPUT_BINDING_QUICK_LOAD_STATE;
		keyboard_bindings[SDL_SCANCODE_SPACE] = INPUT_BINDING_FAST_FORWARD;
#ifdef CLOWNMDEMU_FRONTEND_REWINDING
		keyboard_bindings[SDL_SCANCODE_R] = INPUT_BINDING_REWIND;
#endif
	}

	// Apply the V-sync setting, now that it's been decided.
	SDL_RenderSetVSync(window->GetRenderer(), use_vsync);
}

static void SaveConfiguration()
{
	// Save configuration file:
	const SDL::RWops file = SDL::RWFromFile(GetConfigurationFilePath(), "w");

	if (file == nullptr)
	{
		debug_log.Log("Could not open configuration file for writing.");
	}
	else
	{
	#define PRINT_STRING(FILE, STRING) SDL_RWwrite(FILE, STRING, sizeof(STRING) - 1, 1)
		// Save keyboard bindings.
		PRINT_STRING(file.get(), "[Miscellaneous]\n");

	#define PRINT_BOOLEAN_OPTION(FILE, NAME, VARIABLE) \
		PRINT_STRING(FILE, NAME " = "); \
		if (VARIABLE) \
			PRINT_STRING(FILE, "on\n"); \
		else \
			PRINT_STRING(FILE, "off\n");

		PRINT_BOOLEAN_OPTION(file.get(), "vsync", use_vsync);
		PRINT_BOOLEAN_OPTION(file.get(), "integer-screen-scaling", integer_screen_scaling);
		PRINT_BOOLEAN_OPTION(file.get(), "tall-interlace-mode-2", tall_double_resolution_mode);
		PRINT_BOOLEAN_OPTION(file.get(), "low-pass-filter", emulator->GetLowPassFilter());
		PRINT_BOOLEAN_OPTION(file.get(), "low-volume-distortion", !emulator->GetConfigurationFM().ladder_effect_disabled);
		PRINT_BOOLEAN_OPTION(file.get(), "pal", emulator->GetPALMode());
		PRINT_BOOLEAN_OPTION(file.get(), "japanese", emulator->GetDomestic());

	#ifdef FILE_PICKER_POSIX
		if (!file_utilities.last_file_dialog_directory.empty())
		{
			PRINT_STRING(file.get(), "last-directory = ");
			const std::string last_file_dialog_directory = file_utilities.last_file_dialog_directory.string();
			SDL_RWwrite(file.get(), last_file_dialog_directory.data(), last_file_dialog_directory.size(), 1);
			PRINT_STRING(file.get(), "\n");
		}
		PRINT_BOOLEAN_OPTION(file.get(), "prefer-kdialog", file_utilities.prefer_kdialog);
	#endif

		// Save keyboard bindings.
		PRINT_STRING(file.get(), "\n[Keyboard Bindings]\n");

		for (std::size_t i = 0; i < keyboard_bindings.size(); ++i)
		{
			if (keyboard_bindings[i] != INPUT_BINDING_NONE)
			{
				const char *binding_string;

				// Default, to shut up potential warnings.
				binding_string = "INPUT_BINDING_NONE";

				switch (keyboard_bindings[i])
				{
					case INPUT_BINDING_NONE:
						binding_string = "INPUT_BINDING_NONE";
						break;

					case INPUT_BINDING_CONTROLLER_UP:
						binding_string = "INPUT_BINDING_CONTROLLER_UP";
						break;

					case INPUT_BINDING_CONTROLLER_DOWN:
						binding_string = "INPUT_BINDING_CONTROLLER_DOWN";
						break;

					case INPUT_BINDING_CONTROLLER_LEFT:
						binding_string = "INPUT_BINDING_CONTROLLER_LEFT";
						break;

					case INPUT_BINDING_CONTROLLER_RIGHT:
						binding_string = "INPUT_BINDING_CONTROLLER_RIGHT";
						break;

					case INPUT_BINDING_CONTROLLER_A:
						binding_string = "INPUT_BINDING_CONTROLLER_A";
						break;

					case INPUT_BINDING_CONTROLLER_B:
						binding_string = "INPUT_BINDING_CONTROLLER_B";
						break;

					case INPUT_BINDING_CONTROLLER_C:
						binding_string = "INPUT_BINDING_CONTROLLER_C";
						break;

					case INPUT_BINDING_CONTROLLER_X:
						binding_string = "INPUT_BINDING_CONTROLLER_X";
						break;

					case INPUT_BINDING_CONTROLLER_Y:
						binding_string = "INPUT_BINDING_CONTROLLER_Y";
						break;

					case INPUT_BINDING_CONTROLLER_Z:
						binding_string = "INPUT_BINDING_CONTROLLER_Z";
						break;

					case INPUT_BINDING_CONTROLLER_START:
						binding_string = "INPUT_BINDING_CONTROLLER_START";
						break;

					case INPUT_BINDING_CONTROLLER_MODE:
						binding_string = "INPUT_BINDING_CONTROLLER_MODE";
						break;

					case INPUT_BINDING_PAUSE:
						binding_string = "INPUT_BINDING_PAUSE";
						break;

					case INPUT_BINDING_RESET:
						binding_string = "INPUT_BINDING_RESET";
						break;

					case INPUT_BINDING_FAST_FORWARD:
						binding_string = "INPUT_BINDING_FAST_FORWARD";
						break;

				#ifdef CLOWNMDEMU_FRONTEND_REWINDING
					case INPUT_BINDING_REWIND:
						binding_string = "INPUT_BINDING_REWIND";
						break;
				#endif

					case INPUT_BINDING_QUICK_SAVE_STATE:
						binding_string = "INPUT_BINDING_QUICK_SAVE_STATE";
						break;

					case INPUT_BINDING_QUICK_LOAD_STATE:
						binding_string = "INPUT_BINDING_QUICK_LOAD_STATE";
						break;

					case INPUT_BINDING_TOGGLE_FULLSCREEN:
						binding_string = "INPUT_BINDING_TOGGLE_FULLSCREEN";
						break;

					case INPUT_BINDING_TOGGLE_CONTROL_PAD:
						binding_string = "INPUT_BINDING_TOGGLE_CONTROL_PAD";
						break;

					case INPUT_BINDING__TOTAL:
						SDL_assert(false);
						break;
				}

				const std::string buffer = std::format("{} = {}\n", i, binding_string);
				SDL_RWwrite(file.get(), buffer.data(), buffer.size(), 1);
			}
		}

	#ifdef FILE_PATH_SUPPORT
		// Save recent software paths.
		PRINT_STRING(file.get(), "\n[Recent Software]\n");

		for (const auto &recent_software : recent_software_list)
		{
			PRINT_STRING(file.get(), "cd = ");
			if (recent_software.is_cd_file)
				PRINT_STRING(file.get(), "true\n");
			else
				PRINT_STRING(file.get(), "false\n");

			PRINT_STRING(file.get(), "path = ");
			const auto path_string = recent_software.path.string();
			SDL_RWwrite(file.get(), path_string.data(), 1, path_string.length());
			PRINT_STRING(file.get(), "\n");
		}
	#endif
	}
}


///////////////////
// Main function //
///////////////////

static void PreEventStuff()
{
	emulator_on = emulator->IsCartridgeFileLoaded() || emulator->IsCDFileLoaded();
	emulator_running = emulator_on && (!emulator_paused || emulator_frame_advance) && !file_utilities.IsDialogOpen()
	#ifdef CLOWNMDEMU_FRONTEND_REWINDING
		&& !emulator->RewindingExhausted()
	#endif
		;

	emulator_frame_advance = false;
}

bool Frontend::Initialise(const int argc, char** const argv, const FrameRateCallback &frame_rate_callback_param)
{
	frame_rate_callback = frame_rate_callback_param;

	// Enable high-DPI support on Windows because SDL2 is bad at being a platform abstraction library
	SDL_SetHint("SDL_WINDOWS_DPI_SCALING", "1");

#if !SDL_VERSION_ATLEAST(2,30,3)
	// Force the window icon to the proper icon group, as the default icon
	// size chosen by older versions of SDL does not suit high-DPI display.
	// https://github.com/libsdl-org/SDL/pull/9429
	SDL_SetHint("SDL_WINDOWS_INTRESOURCE_ICON", "100");
#endif

	// Initialise SDL2
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) < 0)
	{
		debug_log.Log("SDL_Init failed with the following message - '%s'", SDL_GetError());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to initialise SDL2. The program will now close.", nullptr);
	}
	else
	{
		IMGUI_CHECKVERSION();

		window.emplace(debug_log, DEFAULT_TITLE, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);
		monospace_font = window->monospace_font;
		emulator.emplace(debug_log, *window, ReadInputCallback);
		debug_fm.emplace(*emulator, monospace_font);
		debug_frontend.emplace(*emulator, GetUpscaledFramebufferSize);
		debug_m68k.emplace(monospace_font);
		debug_memory.emplace(monospace_font);
		debug_pcm.emplace(*emulator, monospace_font);
		debug_psg.emplace(*emulator, monospace_font);
		debug_vdp.emplace(debug_log, dpi_scale, *emulator, file_utilities, frame_counter, monospace_font, *window);
		debug_z80.emplace(*emulator, monospace_font);

		dpi_scale = window->GetDPIScale();

		LoadConfiguration();

		// TODO: ImGui::LoadIniSettingsFromDisk(GetDearImGuiSettingsFilePath().string().c_str());

		// We shouldn't resize the window if something is overriding its size.
		// This is needed for the Emscripen build to work correctly in a full-window HTML canvas.
		int window_width, window_height;
		SDL_GetWindowSize(window->GetSDLWindow(), &window_width, &window_height);

		if (window_width == INITIAL_WINDOW_WIDTH && window_height == INITIAL_WINDOW_HEIGHT)
		{
			// Resize the window so that there's room for the menu bar.
			SDL_SetWindowSize(window->GetSDLWindow(), INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT + window->GetMenuBarSize());
		}

		// If the user passed the path to the software on the command line, then load it here, automatically.
		if (argc > 1)
			LoadCartridgeFile(argv[1]);

		// We are now ready to show the window
		SDL_ShowWindow(window->GetSDLWindow());

		debug_log.ForceConsoleOutput(false);

		PreEventStuff();

		return true;
	}

	return false;
}

void Frontend::Deinitialise()
{
	debug_log.ForceConsoleOutput(true);

	if (framebuffer_texture_upscaled != nullptr)
		SDL_DestroyTexture(framebuffer_texture_upscaled);

	// TODO: ImGui::SaveIniSettingsToDisk(GetDearImGuiSettingsFilePath().string().c_str());

	SaveConfiguration();

#ifdef FILE_PATH_SUPPORT
	// Free recent software list.
	// TODO: Once the frontend is a class, this won't be necessary.
	recent_software_list.clear();
#endif

	// TODO: Once the frontend is a class, this won't be necessary.
	debug_z80.reset();
	debug_vdp.reset();
	debug_psg.reset();
	debug_pcm.reset();
	debug_memory.reset();
	debug_m68k.reset();
	debug_frontend.reset();
	debug_fm.reset();
	emulator.reset();
	window.reset();

	SDL_Quit();
}

static void HandleMainWindowEvent(const SDL_Event &event)
{
	window->MakeDearImGuiContextCurrent();

	ImGuiIO &io = ImGui::GetIO();

	bool give_event_to_imgui = true;
	static bool anything_pressed_during_alt;

	// Process the event
	switch (event.type)
	{
		case SDL_WINDOWEVENT:
			if (event.window.event != SDL_WINDOWEVENT_CLOSE)
				break;
			[[fallthrough]];
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
				window->ToggleFullscreen();
				break;
			}

			if (event.key.keysym.sym == SDLK_ESCAPE)
			{
				// Exit fullscreen
				window->SetFullscreen(false);
			}

			// Prevent invalid memory accesses due to future API expansions.
			// TODO: Yet another reason to not use `SDL_NUM_SCANCODES`.
			if (event.key.keysym.scancode >= keyboard_bindings.size())
				break;

			switch (keyboard_bindings[event.key.keysym.scancode])
			{
				case INPUT_BINDING_TOGGLE_FULLSCREEN:
					window->ToggleFullscreen();
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
						emulator->SoftResetConsole();
						emulator_paused = false;
						break;

					case INPUT_BINDING_QUICK_SAVE_STATE:
						// Save save state
						quick_save_exists = true;
						quick_save_state = emulator->CurrentState();
						break;

					case INPUT_BINDING_QUICK_LOAD_STATE:
						// Load save state
						if (quick_save_exists)
						{
							emulator->OverwriteCurrentState(quick_save_state);
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
			if (event.key.keysym.scancode >= keyboard_bindings.size())
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
					DO_KEY(CLOWNMDEMU_BUTTON_X, INPUT_BINDING_CONTROLLER_X);
					DO_KEY(CLOWNMDEMU_BUTTON_Y, INPUT_BINDING_CONTROLLER_Y);
					DO_KEY(CLOWNMDEMU_BUTTON_Z, INPUT_BINDING_CONTROLLER_Z);
					DO_KEY(CLOWNMDEMU_BUTTON_START, INPUT_BINDING_CONTROLLER_START);
					DO_KEY(CLOWNMDEMU_BUTTON_MODE, INPUT_BINDING_CONTROLLER_MODE);

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

			if (controller == nullptr)
			{
				debug_log.Log("SDL_GameControllerOpen failed with the following message - '%s'", SDL_GetError());
			}
			else
			{
				const SDL_JoystickID joystick_instance_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));

				if (joystick_instance_id < 0)
				{
					debug_log.Log("SDL_JoystickInstanceID failed with the following message - '%s'", SDL_GetError());
				}
				else
				{
					try
					{
						controller_input_list.emplace_front(joystick_instance_id);
						break;
					}
					catch (std::bad_alloc&)
					{
						debug_log.Log("Could not allocate memory for the new ControllerInput struct");
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

			if (controller == nullptr)
			{
				debug_log.Log("SDL_GameControllerFromInstanceID failed with the following message - '%s'", SDL_GetError());
			}
			else
			{
				SDL_GameControllerClose(controller);
			}

			controller_input_list.remove_if([&event](const ControllerInput &controller_input){ return controller_input.joystick_instance_id == event.cdevice.which;});

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
						emulator->OverwriteCurrentState(quick_save_state);

						emulator_paused = false;

						SDL_GameControllerRumble(SDL_GameControllerFromInstanceID(event.cbutton.which), 0xFFFF / 2, 0, 1000 / 8);
					}

					break;

				case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
					// Save save state
					quick_save_exists = true;
					quick_save_state = emulator->CurrentState();

					SDL_GameControllerRumble(SDL_GameControllerFromInstanceID(event.cbutton.which), 0xFFFF * 3 / 4, 0, 1000 / 8);
					break;
			}

			// Fallthrough
		case SDL_CONTROLLERBUTTONUP:
		{
			const bool pressed = event.cbutton.state == SDL_PRESSED;

			// Look for the controller that this event belongs to.
			for (auto &controller_input : controller_input_list)
			{
				// Check if the current controller is the one that matches this event.
				if (controller_input.joystick_instance_id == event.cbutton.which)
				{
					switch (event.cbutton.button)
					{
						#define DO_BUTTON(state, code) case code: controller_input.input.buttons[state] = pressed; break

						// TODO: X, Y, Z, and Mode buttons.
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

							controller_input.dpad[direction] = pressed;

							// Combine D-pad and left stick values into final joypad D-pad inputs.
							controller_input.input.buttons[button] = controller_input.left_stick[direction] || controller_input.dpad[direction];

							break;
						}

						case SDL_CONTROLLER_BUTTON_BACK:
							// Toggle which joypad the controller is bound to.
							if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
								if (pressed)
									controller_input.input.bound_joypad ^= 1;

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
			for (auto &controller_input : controller_input_list)
			{
				// Check if the current controller is the one that matches this event.
				if (controller_input.joystick_instance_id == event.caxis.which)
				{
					switch (event.caxis.axis)
					{
						case SDL_CONTROLLER_AXIS_LEFTX:
						case SDL_CONTROLLER_AXIS_LEFTY:
						{
							if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
								controller_input.left_stick_x = event.caxis.value;
							else //if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
								controller_input.left_stick_y = event.caxis.value;

							// Now that we have the left stick's X and Y values, let's do some trigonometry to figure out which direction(s) it's pointing in.

							// To start with, let's treat the X and Y values as a vector, and turn it into a unit vector.
							const float magnitude = SDL_sqrtf(static_cast<float>(controller_input.left_stick_x * controller_input.left_stick_x + controller_input.left_stick_y * controller_input.left_stick_y));

							const float left_stick_x_unit = controller_input.left_stick_x / magnitude;
							const float left_stick_y_unit = controller_input.left_stick_y / magnitude;

							// Now that we have the stick's direction in the form of a unit vector,
							// we can create a dot product of it with another directional unit vector
							// to determine the angle between them.
							for (unsigned int i = 0; i < 4; ++i)
							{
								// Apply a deadzone.
								if (magnitude < 32768.0f / 4.0f)
								{
									controller_input.left_stick[i] = false;
								}
								else
								{
									// This is a list of directions expressed as unit vectors.
									static const std::array<std::array<float, 2>, 4> directions = {{
										{ 0.0f, -1.0f}, // Up
										{ 0.0f,  1.0f}, // Down
										{-1.0f,  0.0f}, // Left
										{ 1.0f,  0.0f}  // Right
									}};

									// Perform dot product of stick's direction vector with other direction vector.
									const float delta_angle = SDL_acosf(left_stick_x_unit * directions[i][0] + left_stick_y_unit * directions[i][1]);

									// If the stick is within 67.5 degrees of the specified direction, then this will be true.
									controller_input.left_stick[i] = (delta_angle < (360.0f * 3.0f / 8.0f / 2.0f) * (static_cast<float>(CC_PI) / 180.0f)); // Half of 3/8 of 360 degrees converted to radians
								}

								static const std::array<unsigned int, 4> buttons = {
									CLOWNMDEMU_BUTTON_UP,
									CLOWNMDEMU_BUTTON_DOWN,
									CLOWNMDEMU_BUTTON_LEFT,
									CLOWNMDEMU_BUTTON_RIGHT
								};

								// Combine D-pad and left stick values into final joypad D-pad inputs.
								controller_input.input.buttons[buttons[i]] = controller_input.left_stick[i] || controller_input.dpad[i];
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
									if (controller_input.left_trigger != held)
									{
										controller_input.input.rewind = held;
										UpdateRewindStatus();
									}

									controller_input.left_trigger = held;
								}
								else
							#endif
								{
									if (controller_input.right_trigger != held)
									{
										controller_input.input.fast_forward = held;
										UpdateFastForwardStatus();
									}

									controller_input.right_trigger = held;
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
			SDL_free(event.drop.file);
			break;

		default:
			break;
	}

	if (give_event_to_imgui)
		ImGui_ImplSDL2_ProcessEvent(&event);
}

void Frontend::HandleEvent(const SDL_Event &event)
{
	const std::optional<Uint32> window_id = [&]() -> std::optional<Uint32>
	{
		switch (event.type)
		{
			case SDL_WINDOWEVENT:
				return event.window.windowID;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
				return event.key.windowID;

			case SDL_TEXTEDITING:
				return event.edit.windowID;

			case SDL_TEXTEDITING_EXT:
				return event.editExt.windowID;

			case SDL_TEXTINPUT:
				return event.text.windowID;

			case SDL_MOUSEMOTION:
				return event.motion.windowID;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				return event.button.windowID;

			case SDL_MOUSEWHEEL:
				return event.wheel.windowID;

			case SDL_USEREVENT:
				return event.user.windowID;

			case SDL_DROPFILE:
			case SDL_DROPTEXT:
			case SDL_DROPBEGIN:
			case SDL_DROPCOMPLETE:
				return event.drop.windowID;
		}

		return std::nullopt;
	}();

	if (!window_id.has_value())
	{
		HandleMainWindowEvent(event);
	}
	else if (*window_id == SDL_GetWindowID(window->GetSDLWindow()))
	{
		HandleMainWindowEvent(event);
	}
	else
	{
		for (auto &popup_window : popup_windows)
		{
			if (popup_window.has_value() && popup_window->IsWindowID(*window_id))
			{
				if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
					popup_window.reset();
				else
					popup_window->ProcessEvent(event);
			}
		}
	}
}

void Frontend::Update()
{
	ImGuiStyle &style = ImGui::GetStyle();
	ImGuiIO &io = ImGui::GetIO();

	if (emulator_running)
	{
		emulator->Update();
		++frame_counter;
	}

	window->StartDearImGuiFrame();

	// Handle drag-and-drop event.
	if (!file_utilities.IsDialogOpen() && !drag_and_drop_filename.empty())
	{
		std::vector<unsigned char> file_buffer;

		if (file_utilities.LoadFileToBuffer(file_buffer, drag_and_drop_filename))
		{
			if (emulator->ValidateSaveStateFile(file_buffer))
			{
				if (emulator_on)
					LoadSaveState(file_buffer);
			}
			else
			{
				// TODO: Handle dropping a CD file here.
			#ifdef FILE_PATH_SUPPORT
				AddToRecentSoftware(drag_and_drop_filename, false, false);
			#endif
				LoadCartridgeFile(std::move(file_buffer));
				emulator_paused = false;
			}
		}

		drag_and_drop_filename.clear();
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

	const bool show_menu_bar = !window->GetFullscreen()
							|| pop_out
							|| debug_log_window.has_value()
							|| debug_frontend_window.has_value()
							|| m68k_status
							|| mcd_m68k_status
							|| z80_status
							|| m68k_ram_viewer
							|| z80_ram_viewer
							|| prg_ram_viewer
							|| word_ram_viewer
							|| wave_ram_viewer
							|| vdp_registers
							|| sprite_list
							|| sprite_viewer
							|| window_plane_viewer
							|| plane_a_viewer
							|| plane_b_viewer
							|| vram_viewer
							|| cram_viewer
							|| fm_status
							|| psg_status
							|| pcm_status
							|| other_status
							|| m68k_disassembler_window.has_value()
							|| debugging_toggles_window.has_value()
							|| options_window.has_value()
							|| about_window.has_value()
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
	const bool not_collapsed = ImGui::Begin("Display", nullptr, window_flags);
	ImGui::PopStyleVar();

	if (not_collapsed)
	{
		if (show_menu_bar && ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("Software"))
			{
				if (ImGui::MenuItem("Load Cartridge File..."))
				{
					file_utilities.LoadFile(*window, "Load Cartridge File", [](const std::filesystem::path &path, const SDL::RWops &file)
					{
						const bool success = LoadCartridgeFile(&path, file);

						if (success)
							emulator_paused = false;

						return success;
					});
				}

				if (ImGui::MenuItem("Unload Cartridge File", nullptr, false, emulator->IsCartridgeFileLoaded()))
				{
					emulator->UnloadCartridgeFile();

					if (emulator->IsCDFileLoaded())
						emulator->HardResetConsole();

					SetWindowTitleToSoftwareName();
					emulator_paused = false;
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Load CD File..."))
				{
					file_utilities.LoadFile(*window, "Load CD File", [](const std::filesystem::path &path, SDL::RWops &&file)
					{
						if (!LoadCDFile(&path, std::move(file)))
							return false;

						emulator_paused = false;
						return true;
					});
				}

				if (ImGui::MenuItem("Unload CD File", nullptr, false, emulator->IsCDFileLoaded()))
				{
					emulator->UnloadCDFile();

					if (emulator->IsCartridgeFileLoaded())
						emulator->HardResetConsole();

					SetWindowTitleToSoftwareName();
					emulator_paused = false;
				}

				ImGui::Separator();

				ImGui::MenuItem("Pause", nullptr, &emulator_paused, emulator_on);

				if (ImGui::MenuItem("Reset", nullptr, false, emulator_on))
				{
					emulator->SoftResetConsole();
					emulator_paused = false;
				}

			#ifdef FILE_PATH_SUPPORT
				ImGui::SeparatorText("Recent Software");

				if (recent_software_list.empty())
				{
					ImGui::MenuItem("None", nullptr, false, false);
				}
				else
				{
					const RecentSoftware *selected_software = nullptr;

					for (const auto &recent_software : recent_software_list)
					{
						ImGui::PushID(&recent_software - &recent_software_list.front());

						// Display only the filename.
						if (ImGui::MenuItem(recent_software.path.filename().string().c_str()))
							selected_software = &recent_software;

						ImGui::PopID();

						// Show the full path as a tooltip.
						DoToolTip(recent_software.path.string());
					}

					if (selected_software != nullptr && LoadSoftwareFile(selected_software->is_cd_file, selected_software->path))
						emulator_paused = false;
				}
			#endif

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Save States"))
			{
				if (ImGui::MenuItem("Quick Save", nullptr, false, emulator_on))
				{
					quick_save_exists = true;
					quick_save_state = emulator->CurrentState();
				}

				if (ImGui::MenuItem("Quick Load", nullptr, false, emulator_on && quick_save_exists))
				{
					emulator->OverwriteCurrentState(quick_save_state);

					emulator_paused = false;
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Save to File...", nullptr, false, emulator_on))
				{
				#ifdef FILE_PATH_SUPPORT
					file_utilities.CreateSaveFileDialog(*window, "Create Save State", CreateSaveState);
				#else
					file_utilities.SaveFile(*window, "Create Save State", [](const FileUtilities::SaveFileInnerCallback &callback)
					{
						// Inefficient, but it's the only way...
						try
						{
							std::vector<unsigned char> save_state_buffer;
							save_state_buffer.resize(emulator->GetSaveStateFileSize());

							const SDL::RWops file = SDL::RWops(SDL_RWFromMem(save_state_buffer.data(), save_state_buffer.size()));

							if (file != nullptr)
							{
								emulator->WriteSaveStateFile(file);

								callback(save_state_buffer.data(), save_state_buffer.size());
							}
						}
						catch (const std::bad_alloc&)
						{
							return false;
						}

						return true;
					});
				#endif
				}

				if (ImGui::MenuItem("Load from File...", nullptr, false, emulator_on))
					file_utilities.LoadFile(*window, "Load Save State", []([[maybe_unused]] const std::filesystem::path &path, const SDL::RWops &&file)
					{
						LoadSaveState(file);
						return true;
					});

				ImGui::EndMenu();
			}

			const auto PopupButton = [](const char* const title, std::optional<WindowPopup> &window, const int width, const int height)
			{
				if (ImGui::MenuItem(title, nullptr, window.has_value()))
				{
					if (window.has_value())
						window.reset();
					else
						window.emplace(debug_log, title, width, height);
				}
			};

			if (ImGui::BeginMenu("Debugging"))
			{
				PopupButton("Log", debug_log_window, 400, 300);

				PopupButton("Toggles", debugging_toggles_window, 240, 436);

				PopupButton("68000 Disassembler", m68k_disassembler_window, 380, 410);

				ImGui::Separator();

				PopupButton("Frontend", debug_frontend_window, 160, 300);

				if (ImGui::BeginMenu("Main-68000"))
				{
					ImGui::MenuItem("Registers", nullptr, &m68k_status);
					ImGui::MenuItem("WORK-RAM", nullptr, &m68k_ram_viewer);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Sub-68000"))
				{
					ImGui::MenuItem("Registers", nullptr, &mcd_m68k_status);
					ImGui::MenuItem("PRG-RAM", nullptr, &prg_ram_viewer);
					ImGui::MenuItem("WORD-RAM", nullptr, &word_ram_viewer);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Z80"))
				{
					ImGui::MenuItem("Registers", nullptr, &z80_status);
					ImGui::MenuItem("SOUND-RAM", nullptr, &z80_ram_viewer);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("VDP"))
				{
					ImGui::MenuItem("Registers", nullptr, &vdp_registers);
					ImGui::MenuItem("Sprites", nullptr, &sprite_list);
					ImGui::SeparatorText("Visualisers");
					ImGui::MenuItem("Sprite Plane", nullptr, &sprite_viewer);
					ImGui::MenuItem("Window Plane", nullptr, &window_plane_viewer);
					ImGui::MenuItem("Plane A", nullptr, &plane_a_viewer);
					ImGui::MenuItem("Plane B", nullptr, &plane_b_viewer);
					ImGui::MenuItem("VRAM", nullptr, &vram_viewer);
					ImGui::MenuItem("CRAM", nullptr, &cram_viewer);
					ImGui::EndMenu();
				}

				ImGui::MenuItem("FM", nullptr, &fm_status);

				ImGui::MenuItem("PSG", nullptr, &psg_status);

				if (ImGui::BeginMenu("PCM"))
				{
					ImGui::MenuItem("Registers", nullptr, &pcm_status);
					ImGui::MenuItem("WAVE-RAM", nullptr, &wave_ram_viewer);
					ImGui::EndMenu();
				}

				ImGui::MenuItem("Other", nullptr, &other_status);

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Misc."))
			{
				if (ImGui::MenuItem("Fullscreen", nullptr, window->GetFullscreen()))
					window->ToggleFullscreen();

				ImGui::MenuItem("Display Window", nullptr, &pop_out);

			#ifndef NDEBUG
				ImGui::Separator();

				ImGui::MenuItem("Dear ImGui Demo Window", nullptr, &dear_imgui_demo_window);

			#ifdef FILE_PICKER_HAS_NATIVE_FILE_DIALOGS
				ImGui::MenuItem("Native File Dialogs", nullptr, &file_utilities.use_native_file_dialogs);
			#endif
			#endif

				ImGui::Separator();

				PopupButton("Options", options_window, 360, 360);

				ImGui::Separator();

				PopupButton("About", about_window, 605, 430);

			#ifndef __EMSCRIPTEN__
				ImGui::Separator();

				if (ImGui::MenuItem("Exit"))
					quit = true;
			#endif

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

			SDL_Texture *selected_framebuffer_texture = window->GetFramebufferTexture();

			const unsigned int work_width = static_cast<unsigned int>(size_of_display_region.x);
			const unsigned int work_height = static_cast<unsigned int>(size_of_display_region.y);

			unsigned int destination_width;
			unsigned int destination_height;

			switch (emulator->GetCurrentScreenHeight())
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

			ImVec2 uv1 = {static_cast<float>(emulator->GetCurrentScreenWidth()) / static_cast<float>(FRAMEBUFFER_WIDTH), static_cast<float>(emulator->GetCurrentScreenHeight()) / static_cast<float>(FRAMEBUFFER_HEIGHT)};

			if (integer_screen_scaling)
			{
				// Round down to the nearest multiple of 'destination_width' and 'destination_height'.
				const unsigned int framebuffer_upscale_factor = std::max(1u, std::min(work_width / destination_width, work_height / destination_height));

				destination_width_scaled = destination_width * framebuffer_upscale_factor;
				destination_height_scaled = destination_height * framebuffer_upscale_factor;
			}
			else
			{
				// Round to the nearest multiple of 'destination_width' and 'destination_height'.
				const unsigned int framebuffer_upscale_factor = std::max(1u, std::min(CC_DIVIDE_ROUND(work_width, destination_width), CC_DIVIDE_ROUND(work_height, destination_height)));

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
				if (framebuffer_texture_upscaled != nullptr && (destination_width_scaled % destination_width != 0 || destination_height_scaled % destination_height != 0))
				{
					// Render the upscaled framebuffer to the screen.
					selected_framebuffer_texture = framebuffer_texture_upscaled;

					// Before we can do that though, we have to actually render the upscaled framebuffer.
					SDL_Rect framebuffer_rect;
					framebuffer_rect.x = 0;
					framebuffer_rect.y = 0;
					framebuffer_rect.w = emulator->GetCurrentScreenWidth();
					framebuffer_rect.h = emulator->GetCurrentScreenHeight();

					SDL_Rect upscaled_framebuffer_rect;
					upscaled_framebuffer_rect.x = 0;
					upscaled_framebuffer_rect.y = 0;
					upscaled_framebuffer_rect.w = destination_width * framebuffer_upscale_factor;
					upscaled_framebuffer_rect.h = destination_height * framebuffer_upscale_factor;

					// Render to the upscaled framebuffer.
					SDL_SetRenderTarget(window->GetRenderer(), framebuffer_texture_upscaled);

					// Render.
					SDL_RenderCopy(window->GetRenderer(), window->GetFramebufferTexture(), &framebuffer_rect, &upscaled_framebuffer_rect);

					// Switch back to actually rendering to the screen.
					SDL_SetRenderTarget(window->GetRenderer(), nullptr);

					// Update the texture UV to suit the upscaled framebuffer.
					uv1.x = static_cast<float>(upscaled_framebuffer_rect.w) / static_cast<float>(framebuffer_texture_upscaled_width);
					uv1.y = static_cast<float>(upscaled_framebuffer_rect.h) / static_cast<float>(framebuffer_texture_upscaled_height);

					debug_frontend->upscale_width = upscaled_framebuffer_rect.w;
					debug_frontend->upscale_height = upscaled_framebuffer_rect.h;
				}
			}

			// Center the framebuffer in the available region.
			ImGui::SetCursorPosX(static_cast<float>(static_cast<int>(ImGui::GetCursorPosX()) + (static_cast<int>(size_of_display_region.x) - destination_width_scaled) / 2));
			ImGui::SetCursorPosY(static_cast<float>(static_cast<int>(ImGui::GetCursorPosY()) + (static_cast<int>(size_of_display_region.y) - destination_height_scaled) / 2));

			// Draw the upscaled framebuffer in the window.
			ImGui::Image(selected_framebuffer_texture, ImVec2(static_cast<float>(destination_width_scaled), static_cast<float>(destination_height_scaled)), ImVec2(0, 0), uv1);

			debug_frontend->output_width = destination_width_scaled;
			debug_frontend->output_height = destination_height_scaled;
		}
	}

	ImGui::End();

	if (debug_log_window.has_value())
		debug_log.Display(*debug_log_window);

	if (debug_frontend_window.has_value())
		debug_frontend->Display(*debug_frontend_window);

	if (m68k_status)
		debug_m68k->Display(m68k_status, "Main-68000 Registers", emulator->CurrentState().clownmdemu.m68k.state);

	if (mcd_m68k_status)
		debug_m68k->Display(mcd_m68k_status, "Sub-68000 Registers", emulator->CurrentState().clownmdemu.mega_cd.m68k.state);

	if (z80_status)
		debug_z80->Display(z80_status);

	if (m68k_ram_viewer)
		debug_memory->Display(m68k_ram_viewer, "WORK-RAM", emulator->CurrentState().clownmdemu.m68k.ram, CC_COUNT_OF(emulator->CurrentState().clownmdemu.m68k.ram));

	if (z80_ram_viewer)
		debug_memory->Display(z80_ram_viewer, "SOUND-RAM", emulator->CurrentState().clownmdemu.z80.ram, CC_COUNT_OF(emulator->CurrentState().clownmdemu.z80.ram));

	if (prg_ram_viewer)
		debug_memory->Display(prg_ram_viewer, "PRG-RAM", emulator->CurrentState().clownmdemu.mega_cd.prg_ram.buffer, CC_COUNT_OF(emulator->CurrentState().clownmdemu.mega_cd.prg_ram.buffer));

	if (word_ram_viewer)
		debug_memory->Display(word_ram_viewer, "WORD-RAM", emulator->CurrentState().clownmdemu.mega_cd.word_ram.buffer, CC_COUNT_OF(emulator->CurrentState().clownmdemu.mega_cd.word_ram.buffer));

	if (wave_ram_viewer)
		debug_memory->Display(wave_ram_viewer, "WAVE-RAM", emulator->CurrentState().clownmdemu.mega_cd.pcm.wave_ram, CC_COUNT_OF(emulator->CurrentState().clownmdemu.mega_cd.pcm.wave_ram));

	if (vdp_registers)
		debug_vdp->DisplayRegisters(vdp_registers);

	if (sprite_list)
		debug_vdp->DisplaySpriteList(sprite_list);

	if (sprite_viewer)
		debug_vdp->DisplaySpritePlane(sprite_viewer);

	if (window_plane_viewer)
		debug_vdp->DisplayWindowPlane(window_plane_viewer);

	if (plane_a_viewer)
		debug_vdp->DisplayPlaneA(plane_a_viewer);

	if (plane_b_viewer)
		debug_vdp->DisplayPlaneB(plane_b_viewer);

	if (vram_viewer)
		debug_vdp->DisplayVRAM(vram_viewer);

	if (cram_viewer)
		debug_vdp->DisplayCRAM(cram_viewer);

	if (fm_status)
		debug_fm->Display(fm_status);

	if (psg_status)
		debug_psg->Display(psg_status);

	if (pcm_status)
		debug_pcm->Display(pcm_status);

	if (other_status)
	{
		if (ImGui::Begin("Other", &other_status, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (ImGui::BeginTable("Other", 2, ImGuiTableFlags_Borders))
			{
				const ClownMDEmu_State &clownmdemu_state = emulator->CurrentState().clownmdemu;

				cc_u8f i;

				ImGui::TableSetupColumn("Property");
				ImGui::TableSetupColumn("Value");
				ImGui::TableHeadersRow();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Z80 Bank");
				ImGui::TableNextColumn();
				ImGui::PushFont(monospace_font);
				ImGui::Text("0x%06" CC_PRIXFAST16 "-0x%06" CC_PRIXFAST16, clownmdemu_state.z80.bank * 0x8000, (clownmdemu_state.z80.bank + 1) * 0x8000 - 1);
				ImGui::PopFont();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Main 68000 Has Z80 Bus");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.z80.bus_requested ? "Yes" : "No");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Z80 Reset Held");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.z80.reset_held ? "Yes" : "No");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Main 68000 Has Sub 68000 Bus");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.mega_cd.m68k.bus_requested ? "Yes" : "No");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Sub 68000 Reset");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.mega_cd.m68k.reset_held ? "Yes" : "No");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("PRG-RAM Bank");
				ImGui::TableNextColumn();
				ImGui::PushFont(monospace_font);
				ImGui::Text("0x%06" CC_PRIXFAST16 "-0x%06" CC_PRIXFAST16, clownmdemu_state.mega_cd.prg_ram.bank * 0x20000, (clownmdemu_state.mega_cd.prg_ram.bank + 1) * 0x20000 - 1);
				ImGui::PopFont();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("WORD-RAM Mode");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.mega_cd.word_ram.in_1m_mode ? "1M" : "2M");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("DMNA Bit");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.mega_cd.word_ram.dmna ? "Set" : "Clear");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("RET Bit");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.mega_cd.word_ram.ret ? "Set" : "Clear");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Boot Mode");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.mega_cd.boot_from_cd ? "CD" : "Cartridge");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("68000 Communication Flag");
				ImGui::TableNextColumn();
				ImGui::PushFont(monospace_font);
				ImGui::Text("0x%04" CC_PRIXFAST16, clownmdemu_state.mega_cd.communication.flag);
				ImGui::PopFont();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("68000 Communication Command");
				ImGui::TableNextColumn();
				ImGui::PushFont(monospace_font);
				for (i = 0; i < CC_COUNT_OF(clownmdemu_state.mega_cd.communication.command); i += 2)
					ImGui::Text("0x%04" CC_PRIXFAST16 " 0x%04" CC_PRIXFAST16, clownmdemu_state.mega_cd.communication.command[i + 0], clownmdemu_state.mega_cd.communication.command[i + 1]);
				ImGui::PopFont();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("68000 Communication Status");
				ImGui::TableNextColumn();
				ImGui::PushFont(monospace_font);
				for (i = 0; i < CC_COUNT_OF(clownmdemu_state.mega_cd.communication.status); i += 2)
					ImGui::Text("0x%04" CC_PRIXLEAST16 " 0x%04" CC_PRIXLEAST16, clownmdemu_state.mega_cd.communication.status[i + 0], clownmdemu_state.mega_cd.communication.status[i + 1]);
				ImGui::PopFont();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("SUB-CPU Graphics Interrupt");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.mega_cd.irq.enabled[0] ? "Enabled" : "Disabled");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("SUB-CPU Mega Drive Interrupt");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.mega_cd.irq.enabled[1] ? "Enabled" : "Disabled");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("SUB-CPU Timer Interrupt");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.mega_cd.irq.enabled[2] ? "Enabled" : "Disabled");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("SUB-CPU CDD Interrupt");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.mega_cd.irq.enabled[3] ? "Enabled" : "Disabled");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("SUB-CPU CDC Interrupt");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.mega_cd.irq.enabled[4] ? "Enabled" : "Disabled");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("SUB-CPU Sub-code Interrupt");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(clownmdemu_state.mega_cd.irq.enabled[5] ? "Enabled" : "Disabled");

				ImGui::EndTable();
			}
		}

		ImGui::End();
	}

	if (debugging_toggles_window.has_value())
	{
		if (debugging_toggles_window->Begin(ImGuiWindowFlags_AlwaysAutoResize))
		{
			bool temp;

			ImGui::SeparatorText("VDP");

			if (ImGui::BeginTable("VDP Options", 2, ImGuiTableFlags_SizingStretchSame))
			{
				VDP_Configuration &vdp = emulator->GetConfigurationVDP();

				ImGui::TableNextColumn();
				temp = !vdp.sprites_disabled;
				if (ImGui::Checkbox("Sprite Plane", &temp))
					vdp.sprites_disabled = !vdp.sprites_disabled;

				ImGui::TableNextColumn();
				temp = !vdp.window_disabled;
				if (ImGui::Checkbox("Window Plane", &temp))
					vdp.window_disabled = !vdp.window_disabled;

				ImGui::TableNextColumn();
				temp = !vdp.planes_disabled[0];
				if (ImGui::Checkbox("Plane A", &temp))
					vdp.planes_disabled[0] = !vdp.planes_disabled[0];

				ImGui::TableNextColumn();
				temp = !vdp.planes_disabled[1];
				if (ImGui::Checkbox("Plane B", &temp))
					vdp.planes_disabled[1] = !vdp.planes_disabled[1];

				ImGui::EndTable();
			}

			ImGui::SeparatorText("FM");

			if (ImGui::BeginTable("FM Options", 2, ImGuiTableFlags_SizingStretchSame))
			{
				FM_Configuration &fm = emulator->GetConfigurationFM();

				char buffer[] = "FM1";

				for (std::size_t i = 0; i < CC_COUNT_OF(fm.fm_channels_disabled); ++i)
				{
					buffer[2] = '1' + i;
					ImGui::TableNextColumn();
					temp = !fm.fm_channels_disabled[i];
					if (ImGui::Checkbox(buffer, &temp))
						fm.fm_channels_disabled[i] = !fm.fm_channels_disabled[i];
				}

				ImGui::TableNextColumn();
				temp = !fm.dac_channel_disabled;
				if (ImGui::Checkbox("DAC", &temp))
					fm.dac_channel_disabled = !fm.dac_channel_disabled;

				ImGui::EndTable();
			}

			ImGui::SeparatorText("PSG");

			if (ImGui::BeginTable("PSG Options", 2, ImGuiTableFlags_SizingStretchSame))
			{
				PSG_Configuration &psg = emulator->GetConfigurationPSG();

				char buffer[] = "PSG1";

				for (std::size_t i = 0; i < CC_COUNT_OF(psg.tone_disabled); ++i)
				{
					buffer[3] = '1' + i;
					ImGui::TableNextColumn();
					temp = !psg.tone_disabled[i];
					if (ImGui::Checkbox(buffer, &temp))
						psg.tone_disabled[i] = !psg.tone_disabled[i];
				}

				ImGui::TableNextColumn();
				temp = !psg.noise_disabled;
				if (ImGui::Checkbox("PSG Noise", &temp))
					psg.noise_disabled = !psg.noise_disabled;

				ImGui::EndTable();
			}

			ImGui::SeparatorText("PCM");

			if (ImGui::BeginTable("PCM Options", 2, ImGuiTableFlags_SizingStretchSame))
			{
				PCM_Configuration &pcm = emulator->GetConfigurationPCM();

				char buffer[] = "PCM1";

				for (std::size_t i = 0; i < CC_COUNT_OF(pcm.channels_disabled); ++i)
				{
					buffer[3] = '1' + i;
					ImGui::TableNextColumn();
					temp = !pcm.channels_disabled[i];
					if (ImGui::Checkbox(buffer, &temp))
						pcm.channels_disabled[i] = !pcm.channels_disabled[i];
				}

				ImGui::EndTable();
			}
		}

		debugging_toggles_window->End();
	}

	if (m68k_disassembler_window.has_value())
		Disassembler(*m68k_disassembler_window, *emulator, monospace_font);

	if (options_window.has_value())
	{
		if (options_window->Begin())
		{
			ImGui::SeparatorText("Console");

			if (ImGui::BeginTable("Console Options", 3))
			{
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("TV Standard:");
				DoToolTip("Some games only work with a certain TV standard.");
				ImGui::TableNextColumn();
				if (ImGui::RadioButton("NTSC", !emulator->GetPALMode()))
					if (emulator->GetPALMode())
						SetAudioPALMode(false);
				DoToolTip("60 FPS");
				ImGui::TableNextColumn();
				if (ImGui::RadioButton("PAL", emulator->GetPALMode()))
					if (!emulator->GetPALMode())
						SetAudioPALMode(true);
				DoToolTip("50 FPS");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Region:");
				DoToolTip("Some games only work with a certain region.");
				ImGui::TableNextColumn();
				if (ImGui::RadioButton("Japan", emulator->GetDomestic()))
					emulator->SetDomestic(true);
				DoToolTip("Games may show Japanese text.");
				ImGui::TableNextColumn();
				if (ImGui::RadioButton("Elsewhere", !emulator->GetDomestic()))
					emulator->SetDomestic(false);
				DoToolTip("Games may show English text.");

				ImGui::EndTable();
			}

			ImGui::SeparatorText("Video");

			if (ImGui::BeginTable("Video Options", 2))
			{
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("V-Sync", &use_vsync))
					if (!fast_forward_in_progress)
						SDL_RenderSetVSync(window->GetRenderer(), use_vsync);
				DoToolTip("Prevents screen tearing.");

				ImGui::TableNextColumn();
				if (ImGui::Checkbox("Integer Screen Scaling", &integer_screen_scaling) && integer_screen_scaling)
				{
					// Reclaim memory used by the upscaled framebuffer, since we won't be needing it anymore.
					SDL_DestroyTexture(framebuffer_texture_upscaled);
					framebuffer_texture_upscaled = nullptr;
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
				bool low_pass_filter = emulator->GetLowPassFilter();
				if (ImGui::Checkbox("Low-Pass Filter", &low_pass_filter))
					emulator->SetLowPassFilter(low_pass_filter);
				DoToolTip("Makes the audio sound 'softer',\njust like on a real Mega Drive.");

				ImGui::TableNextColumn();
				bool ladder_effect = !emulator->GetConfigurationFM().ladder_effect_disabled;
				if (ImGui::Checkbox("Low-Volume Distortion", &ladder_effect))
					emulator->GetConfigurationFM().ladder_effect_disabled = !ladder_effect;
				DoToolTip("Approximates the so-called 'ladder effect' that\nis present in early Mega Drives. Without this,\ncertain sounds in some games will be too quiet.");

				ImGui::EndTable();
			}

		#ifdef FILE_PICKER_POSIX
			ImGui::SeparatorText("Preferred File Dialog");

			if (ImGui::BeginTable("Preferred File Dialog", 2))
			{
				ImGui::TableNextColumn();

				if (ImGui::RadioButton("Zenity (GTK)", !file_utilities.prefer_kdialog))
					file_utilities.prefer_kdialog = false;
				DoToolTip("Best with GNOME, Xfce, LXDE, MATE, Cinnamon, etc.");

				ImGui::TableNextColumn();

				if (ImGui::RadioButton("kdialog (Qt)", file_utilities.prefer_kdialog))
					file_utilities.prefer_kdialog = true;
				DoToolTip("Best with KDE, LXQt, Deepin, etc.");

				ImGui::EndTable();
			}
		#endif

			ImGui::SeparatorText("Keyboard Input");

			static bool sorted_scancodes_done;
			static std::array<SDL_Scancode, SDL_NUM_SCANCODES> sorted_scancodes; // TODO: `SDL_NUM_SCANCODES` is an internal macro, so use something standard!

			if (!sorted_scancodes_done)
			{
				sorted_scancodes_done = true;

				for (std::size_t i = 0; i < sorted_scancodes.size(); ++i)
					sorted_scancodes[i] = static_cast<SDL_Scancode>(i);

				SDL_qsort(&sorted_scancodes, sorted_scancodes.size(), sizeof(sorted_scancodes[0]),
					[](const void* const a, const void* const b)
				{
					const SDL_Scancode* const binding_1 = static_cast<const SDL_Scancode*>(a);
					const SDL_Scancode* const binding_2 = static_cast<const SDL_Scancode*>(b);

					return keyboard_bindings[*binding_1] - keyboard_bindings[*binding_2];
				}
				);
			}

			static SDL_Scancode selected_scancode;

			static const std::array<const char*, INPUT_BINDING__TOTAL> binding_names = {
				"None",
				"Control Pad Up",
				"Control Pad Down",
				"Control Pad Left",
				"Control Pad Right",
				"Control Pad A",
				"Control Pad B",
				"Control Pad C",
				"Control Pad X",
				"Control Pad Y",
				"Control Pad Z",
				"Control Pad Start",
				"Control Pad Mode",
				"Pause",
				"Reset",
				"Fast-Forward",
#ifdef CLOWNMDEMU_FRONTEND_REWINDING
				"Rewind",
#endif
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
				for (std::size_t i = 0; i < sorted_scancodes.size(); ++i)
				{
					if (keyboard_bindings[sorted_scancodes[i]] != INPUT_BINDING_NONE)
					{
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(SDL_GetScancodeName(static_cast<SDL_Scancode>(sorted_scancodes[i])));

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

			if (ImGui::BeginPopupModal("Select Key", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
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
							selected_scancode = static_cast<SDL_Scancode>(i);
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

			if (ImGui::BeginPopupModal("Select Action", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				bool previous_menu = false;

				ImGui::Text("Selected Key: %s", SDL_GetScancodeName(selected_scancode));

				if (ImGui::BeginListBox("##Actions"))
				{
					for (unsigned int i = INPUT_BINDING_NONE + 1; i < INPUT_BINDING__TOTAL; i = i + 1)
					{
						if (ImGui::Selectable(binding_names[i]))
						{
							ImGui::CloseCurrentPopup();
							keyboard_bindings[selected_scancode] = static_cast<InputBinding>(i);
							sorted_scancodes_done = false;
							scroll_to_add_bindings_button = true;
						}
					}
					ImGui::EndListBox();
				}

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

		options_window->End();
	}

	if (about_window.has_value())
	{
		if (about_window->Begin(ImGuiWindowFlags_HorizontalScrollbar))
		{
			static const char licence_clownmdemu[] = {
				#include "licences/clownmdemu.h"
			};
			static const char licence_dear_imgui[] = {
				#include "licences/dear-imgui.h"
			};
		#ifdef __EMSCRIPTEN__
			static const char licence_emscripten_browser_file[] = {
				#include "licences/emscripten-browser-file.h"
			};
		#endif
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
			static const char licence_noto_sans[] = {
				#include "licences/noto-sans.h"
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
				ImGui::PushFont(about_window->GetMonospaceFont());
				ImGui::TextUnformatted(licence_clownmdemu, licence_clownmdemu + sizeof(licence_clownmdemu));
				ImGui::PopFont();
			}

			if (ImGui::CollapsingHeader("Dear ImGui"))
			{
				ImGui::PushFont(about_window->GetMonospaceFont());
				ImGui::TextUnformatted(licence_dear_imgui, licence_dear_imgui + sizeof(licence_dear_imgui));
				ImGui::PopFont();
			}

		#ifdef __EMSCRIPTEN__
			if (ImGui::CollapsingHeader("Emscripten Browser File Library"))
			{
				ImGui::PushFont(about_window->GetMonospaceFont());
				ImGui::TextUnformatted(licence_emscripten_browser_file, licence_emscripten_browser_file + sizeof(licence_emscripten_browser_file));
				ImGui::PopFont();
			}
		#endif

		#ifdef IMGUI_ENABLE_FREETYPE
			if (ImGui::CollapsingHeader("FreeType"))
			{
				if (ImGui::TreeNode("General"))
				{
					ImGui::PushFont(about_window->GetMonospaceFont());
					ImGui::TextUnformatted(licence_freetype, licence_freetype + sizeof(licence_freetype));
					ImGui::PopFont();
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("BDF Driver"))
				{
					ImGui::PushFont(about_window->GetMonospaceFont());
					ImGui::TextUnformatted(licence_freetype_bdf, licence_freetype_bdf + sizeof(licence_freetype_bdf));
					ImGui::PopFont();
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("PCF Driver"))
				{
					ImGui::PushFont(about_window->GetMonospaceFont());
					ImGui::TextUnformatted(licence_freetype_pcf, licence_freetype_pcf + sizeof(licence_freetype_pcf));
					ImGui::PopFont();
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("fthash.c & fthash.h"))
				{
					ImGui::PushFont(about_window->GetMonospaceFont());
					ImGui::TextUnformatted(licence_freetype_fthash, licence_freetype_fthash + sizeof(licence_freetype_fthash));
					ImGui::PopFont();
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("ft-hb.c & ft-hb.h"))
				{
					ImGui::PushFont(about_window->GetMonospaceFont());
					ImGui::TextUnformatted(licence_freetype_ft_hb, licence_freetype_ft_hb + sizeof(licence_freetype_ft_hb));
					ImGui::PopFont();
					ImGui::TreePop();
				}
			}
		#endif

			if (ImGui::CollapsingHeader("inih"))
			{
				ImGui::PushFont(about_window->GetMonospaceFont());
				ImGui::TextUnformatted(licence_inih, licence_inih + sizeof(licence_inih));
				ImGui::PopFont();
			}

			if (ImGui::CollapsingHeader("Noto Sans"))
			{
				ImGui::PushFont(about_window->GetMonospaceFont());
				ImGui::TextUnformatted(licence_noto_sans, licence_noto_sans + sizeof(licence_noto_sans));
				ImGui::PopFont();
			}

			if (ImGui::CollapsingHeader("Inconsolata"))
			{
				ImGui::PushFont(about_window->GetMonospaceFont());
				ImGui::TextUnformatted(licence_inconsolata, licence_inconsolata + sizeof(licence_inconsolata));
				ImGui::PopFont();
			}
		}

		about_window->End();
	}

	file_utilities.DisplayFileDialog(drag_and_drop_filename);

	window->FinishDearImGuiFrame();

	PreEventStuff();
}

bool Frontend::WantsToQuit()
{
	return quit;
}

bool Frontend::IsFastForwarding()
{
	return fast_forward_in_progress;
}

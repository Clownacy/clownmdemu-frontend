#include <cerrno>
#include <climits> // For INT_MAX.
#include <cstddef>
#include <functional>

#include "SDL.h"

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#include "libraries/imgui/imgui.h"
#include "libraries/imgui/backends/imgui_impl_sdl2.h"
#include "libraries/imgui/backends/imgui_impl_sdlrenderer2.h"

#include "libraries/inih/ini.h"

#include "inconsolata-regular.h"
#include "karla-regular.h"

#include "audio-output.h"
#include "common.h"
#include "debug-fm.h"
#include "debug-log.h"
#include "debug-m68k.h"
#include "debug-memory.h"
#include "debug-psg.h"
#include "debug-vdp.h"
#include "debug-z80.h"
#include "doubly-linked-list.h"
#include "emulator-instance.h"
#include "file-picker.h"
#include "utilities.h"

#define CONFIG_FILENAME "clownmdemu-frontend.ini"

#define VERSION "v0.4.4"

#define FRAMEBUFFER_WIDTH 320
#define FRAMEBUFFER_HEIGHT (240*2) // *2 because of double-resolution mode.

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

typedef struct RecentSoftware
{
	DoublyLinkedList_Entry list;

	bool is_cd_file;
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

static bool quick_save_exists;
static EmulatorInstance::State quick_save_state;

static AudioOutput audio_output(debug_log);

static bool use_vsync;
static bool integer_screen_scaling;
static bool tall_double_resolution_mode;

static cc_bool ReadInputCallback(const cc_u8f player_id, const ClownMDEmu_Button button_id);
static EmulatorInstance emulator(audio_output, debug_log, ReadInputCallback);


///////////
// Fonts //
///////////

static unsigned int CalculateFontSize()
{
	// Note that we are purposefully flooring, as Dear ImGui's docs recommend.
	return static_cast<unsigned int>(15.0f * dpi_scale);
}

static void ReloadFonts(unsigned int font_size)
{
	ImGuiIO &io = ImGui::GetIO();

	io.Fonts->Clear();
	ImGui_ImplSDLRenderer2_DestroyFontsTexture();

	ImFontConfig font_cfg = ImFontConfig();
	SDL_snprintf(font_cfg.Name, IM_ARRAYSIZE(font_cfg.Name), "Karla Regular, %upx", font_size);
	io.Fonts->AddFontFromMemoryCompressedTTF(karla_regular_compressed_data, karla_regular_compressed_size, static_cast<float>(font_size), &font_cfg);
	SDL_snprintf(font_cfg.Name, IM_ARRAYSIZE(font_cfg.Name), "Inconsolata Regular, %upx", font_size);
	monospace_font = io.Fonts->AddFontFromMemoryCompressedTTF(inconsolata_regular_compressed_data, inconsolata_regular_compressed_size, static_cast<float>(font_size), &font_cfg);
}


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
		for (ControllerInput *controller_input = controller_input_list_head; controller_input != nullptr; controller_input = controller_input->next)
			if (controller_input->input.bound_joypad == player_id)
				value |= controller_input->input.buttons[button_id] != 0 ? cc_true : cc_false;
	}

	return value;
}

static void AddToRecentSoftware(const char* const path, const bool is_cd_file, const bool add_to_end)
{
	// If the path already exists in the list, then move it to the start of the list.
	for (DoublyLinkedList_Entry *entry = recent_software_list.head; entry != nullptr; entry = entry->next)
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

	for (DoublyLinkedList_Entry *entry = recent_software_list.head; entry != nullptr; entry = entry->next)
		++total_items;

	if (total_items == 10)
	{
		RecentSoftware* const last_item = CC_STRUCT_POINTER_FROM_MEMBER_POINTER(RecentSoftware, list, recent_software_list.tail);

		DoublyLinkedList_Remove(&recent_software_list, &last_item->list);
		SDL_free(last_item);
	}

	// Add the file to the list of recent software.
	const std::size_t path_length = SDL_strlen(path);
	RecentSoftware* const new_entry = static_cast<RecentSoftware*>(SDL_malloc(sizeof(RecentSoftware) + path_length));

	if (new_entry != nullptr)
	{
		new_entry->is_cd_file = is_cd_file;
		SDL_memcpy(new_entry->path, path, path_length + 1);
		(add_to_end ? DoublyLinkedList_PushBack : DoublyLinkedList_PushFront)(&recent_software_list, &new_entry->list);
	}
}

// Manages whether the emulator runs at a higher speed or not.
static bool fast_forward_in_progress;

static void UpdateFastForwardStatus()
{
	const bool previous_fast_forward_in_progress = fast_forward_in_progress;

	fast_forward_in_progress = keyboard_input.fast_forward;

	for (ControllerInput *controller_input = controller_input_list_head; controller_input != nullptr; controller_input = controller_input->next)
		fast_forward_in_progress |= controller_input->input.fast_forward != 0;

	if (previous_fast_forward_in_progress != fast_forward_in_progress)
	{
		// Disable V-sync so that 60Hz displays aren't locked to 1x speed while fast-forwarding
		if (use_vsync)
			SDL_RenderSetVSync(window.renderer, !fast_forward_in_progress);
	}
}

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
static void UpdateRewindStatus()
{
	emulator.rewind_in_progress = keyboard_input.rewind;

	for (ControllerInput *controller_input = controller_input_list_head; controller_input != nullptr; controller_input = controller_input->next)
		emulator.rewind_in_progress |= controller_input->input.rewind != 0;
}
#endif


///////////
// Misc. //
///////////

static void LoadCartridgeFile(unsigned char* const file_buffer, const std::size_t file_size)
{
	quick_save_exists = false;
	emulator.LoadCartridgeFileFromMemory(file_buffer, file_size);
}

static bool LoadCartridgeFile(const char* const path)
{
	if (!emulator.LoadCartridgeFileFromFile(path))
	{
		debug_log.Log("Could not load the cartridge file");
		window.ShowErrorMessageBox("Failed to load the cartridge file.");
		return false;
	}

	quick_save_exists = false;
	AddToRecentSoftware(path, false, false);
	return true;
}

static bool LoadCDFile(const char* const path)
{
	if (!emulator.LoadCDFile(path))
	{
		debug_log.Log("Could not load the CD file");
		window.ShowErrorMessageBox("Failed to load the CD file.");
		return false;
	}

	AddToRecentSoftware(path, true, false);
	return true;
}

static bool LoadSaveState(const unsigned char* const file_buffer, const std::size_t file_size)
{
	if (!emulator.LoadSaveStateFromMemory(file_buffer, file_size))
	{
		debug_log.Log("Could not load save state file");
		window.ShowErrorMessageBox("Could not load save state file.");
		return false;
	}

	emulator_paused = false;

	return true;
}

static bool LoadSaveState(const char* const save_state_path)
{
	if (!emulator.LoadSaveStateFromFile(save_state_path))
	{
		debug_log.Log("Could not load save state file");
		window.ShowErrorMessageBox("Could not load save state file.");
		return false;
	}

	emulator_paused = false;

	return true;
}

static bool CreateSaveState(const char* const save_state_path)
{
	if (!emulator.CreateSaveState(save_state_path))
	{
		debug_log.Log("Could not create save state file");
		window.ShowErrorMessageBox("Could not create save state file.");
		return false;
	}

	return true;
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
	SDL_RWops* const sdl_rwops = static_cast<SDL_RWops*>(user);

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

static int INIParseCallback(void* const /*user*/, const char* const section, const char* const name, const char* const value)
{
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
			audio_output.SetLowPassFilter(state);
		else if (SDL_strcmp(name, "pal") == 0)
			emulator.clownmdemu_configuration.general.tv_standard = state ? CLOWNMDEMU_TV_STANDARD_PAL : CLOWNMDEMU_TV_STANDARD_NTSC;
		else if (SDL_strcmp(name, "japanese") == 0)
			emulator.clownmdemu_configuration.general.region = state ? CLOWNMDEMU_REGION_DOMESTIC : CLOWNMDEMU_REGION_OVERSEAS;
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
		const SDL_Scancode scancode = static_cast<SDL_Scancode>(SDL_strtoul(name, &string_end, 0));

		if (errno != ERANGE && string_end >= SDL_strchr(name, '\0') && scancode < SDL_NUM_SCANCODES)
		{
			errno = 0;
			const InputBinding input_binding = static_cast<InputBinding>(SDL_strtoul(value, &string_end, 0));

			if (errno != ERANGE && string_end >= SDL_strchr(name, '\0'))
				keyboard_bindings[scancode] = input_binding;
		}

	}
	else if (SDL_strcmp(section, "Recent Software") == 0)
	{
		static bool is_cd_file = false;

		if (SDL_strcmp(name, "cd") == 0)
		{
			is_cd_file = SDL_strcmp(value, "true") == 0;
		}
		else if (SDL_strcmp(name, "path") == 0)
		{
			if (value[0] != '\0')
				AddToRecentSoftware(value, is_cd_file, true);
		}
	}

	return 1;
}


///////////////////
// Configuration //
///////////////////

static void LoadConfiguration()
{
	// Set default settings.

	// Default V-sync.
	const int display_index = SDL_GetWindowDisplayIndex(window.sdl);

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
	audio_output.SetLowPassFilter(true);
	integer_screen_scaling = false;
	tall_double_resolution_mode = false;

	emulator.clownmdemu_configuration.general.region = CLOWNMDEMU_REGION_OVERSEAS;
	emulator.clownmdemu_configuration.general.tv_standard = CLOWNMDEMU_TV_STANDARD_NTSC;

	SDL_RWops* const file = SDL_RWFromFile(CONFIG_FILENAME, "r");

	// Load the configuration file, overwriting the above settings.
	if (file == nullptr || ini_parse_stream(INIReadCallback, file, INIParseCallback, nullptr) != 0)
	{
		// Failed to read configuration file: set defaults key bindings.
		for (std::size_t i = 0; i < CC_COUNT_OF(keyboard_bindings); ++i)
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

	if (file != nullptr)
		SDL_RWclose(file);

	// Apply the V-sync setting, now that it's been decided.
	SDL_RenderSetVSync(window.renderer, use_vsync);
}

static void SaveConfiguration()
{
	// Save configuration file:
	SDL_RWops *file = SDL_RWFromFile(CONFIG_FILENAME, "w");

	if (file == nullptr)
	{
		debug_log.Log("Could not open configuration file for writing.");
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
		PRINT_BOOLEAN_OPTION(file, "low-pass-filter", audio_output.GetLowPassFilter());
		PRINT_BOOLEAN_OPTION(file, "pal", emulator.clownmdemu_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_PAL);
		PRINT_BOOLEAN_OPTION(file, "japanese", emulator.clownmdemu_configuration.general.region == CLOWNMDEMU_REGION_DOMESTIC);

	#ifdef POSIX
		if (last_file_dialog_directory != nullptr)
		{
			PRINT_STRING(file, "last-directory = ");
			SDL_RWwrite(file, last_file_dialog_directory, SDL_strlen(last_file_dialog_directory), 1);
			PRINT_STRING(file, "\n");
		}
		PRINT_BOOLEAN_OPTION(file, "prefer-kdialog", prefer_kdialog);
	#endif

		// Save keyboard bindings.
		PRINT_STRING(file, "\n[Keyboard Bindings]\n");

		for (std::size_t i = 0; i < CC_COUNT_OF(keyboard_bindings); ++i)
		{
			if (keyboard_bindings[i] != INPUT_BINDING_NONE)
			{
				char *buffer;
				if (SDL_asprintf(&buffer, "%u = %u\n", static_cast<unsigned int>(i), keyboard_bindings[i]) != -1)
				{
					SDL_RWwrite(file, buffer, SDL_strlen(buffer), 1);
					SDL_free(buffer);
				}
			}
		}

		// Save recent software paths.
		PRINT_STRING(file, "\n[Recent Software]\n");

		for (DoublyLinkedList_Entry *entry = recent_software_list.head; entry != nullptr; entry = entry->next)
		{
			RecentSoftware* const recent_software = CC_STRUCT_POINTER_FROM_MEMBER_POINTER(RecentSoftware, list, entry);

			PRINT_STRING(file, "cd = ");
			if (recent_software->is_cd_file)
				PRINT_STRING(file, "true\n");
			else
				PRINT_STRING(file, "false\n");

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
	// Initialise SDL2
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) < 0)
	{
		debug_log.Log("SDL_Init failed with the following message - '%s'", SDL_GetError());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to initialise SDL2. The program will now close.", nullptr);
	}
	else
	{
		if (!window.Initialise("clownmdemu-frontend " VERSION, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT))
		{
			debug_log.Log("window.Initialise failed");
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to initialise video subsystem. The program will now close.", nullptr);
		}
		else
		{
			dpi_scale = window.GetDPIScale();

			DoublyLinkedList_Initialise(&recent_software_list);
			LoadConfiguration();

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
			const float menu_bar_size = static_cast<float>(font_size) + style.FramePadding.y * 2.0f; // An inlined ImGui::GetFrameHeight that actually works
			SDL_SetWindowSize(window.sdl, static_cast<int>(320.0f * 2.0f * dpi_scale), static_cast<int>(224.0f * 2.0f * dpi_scale + menu_bar_size));

			// Setup Platform/Renderer backends
			ImGui_ImplSDL2_InitForSDLRenderer(window.sdl, window.renderer);
			ImGui_ImplSDLRenderer2_Init(window.renderer);

			// Load fonts
			ReloadFonts(font_size);

			// Intiialise audio if we can (but it's okay if it fails).
			if (!audio_output.Initialise())
			{
				debug_log.Log("InitialiseAudio failed");
				window.ShowWarningMessageBox("Unable to initialise audio subsystem: the program will not output audio!");
			}
			else
			{
				// Initialise resamplers.
				audio_output.SetPALMode(emulator.clownmdemu_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_PAL);
			}

			// If the user passed the path to the software on the command line, then load it here, automatically.
			// Otherwise, initialise the emulator state anyway in case the user opens the debuggers without loading a ROM first.
			if (argc > 1)
				LoadCartridgeFile(argv[1]);
			else
				LoadCartridgeFile(nullptr, 0);

			// We are now ready to show the window
			SDL_ShowWindow(window.sdl);

			debug_log.ForceConsoleOutput(false);

			// Manages whether the program exits or not.
			bool quit = false;

			// Used for tracking when to pop the emulation display out into its own little window.
			bool pop_out = false;

			bool debug_log_active = false;
			bool m68k_status = false;
			bool mcd_m68k_status = false;
			bool z80_status = false;
			bool m68k_ram_viewer = false;
			bool z80_ram_viewer = false;
			bool word_ram_viewer = false;
			bool prg_ram_viewer = false;
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
				const bool emulator_on = emulator.IsCartridgeFileLoaded() || emulator.IsCDFileLoaded();
				const bool emulator_running = emulator_on && (!emulator_paused || emulator_frame_advance) && !file_picker.IsDialogOpen()
				#ifdef CLOWNMDEMU_FRONTEND_REWINDING
					&& !emulator.RewindingExhausted()
				#endif
					;

				emulator_frame_advance = false;

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

						switch (emulator.clownmdemu_configuration.general.tv_standard)
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
								window.ToggleFullscreen();
								break;
							}

							if (event.key.keysym.sym == SDLK_ESCAPE)
							{
								// Exit fullscreen
								window.SetFullscreen(false);
							}

							// Prevent invalid memory accesses due to future API expansions.
							// TODO: Yet another reason to not use `SDL_NUM_SCANCODES`.
							if (event.key.keysym.scancode >= CC_COUNT_OF(keyboard_bindings))
								break;

							switch (keyboard_bindings[event.key.keysym.scancode])
							{
								case INPUT_BINDING_TOGGLE_FULLSCREEN:
									window.ToggleFullscreen();
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
										emulator.SoftResetConsole();
										emulator_paused = false;
										break;

									case INPUT_BINDING_QUICK_SAVE_STATE:
										// Save save state
										quick_save_exists = true;
										quick_save_state = *emulator.state;
										break;

									case INPUT_BINDING_QUICK_LOAD_STATE:
										// Load save state
										if (quick_save_exists)
										{
											*emulator.state = quick_save_state;

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
									ControllerInput *controller_input = static_cast<ControllerInput*>(SDL_calloc(sizeof(ControllerInput), 1));

									if (controller_input == nullptr)
									{
										debug_log.Log("Could not allocate memory for the new ControllerInput struct");
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

							if (controller == nullptr)
							{
								debug_log.Log("SDL_GameControllerFromInstanceID failed with the following message - '%s'", SDL_GetError());
							}
							else
							{
								SDL_GameControllerClose(controller);
							}

							for (ControllerInput **controller_input_pointer = &controller_input_list_head; ; controller_input_pointer = &(*controller_input_pointer)->next)
							{
								if ((*controller_input_pointer) == nullptr)
								{
									debug_log.Log("Received an SDL_CONTROLLERDEVICEREMOVED event for an unrecognised controller");
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
										*emulator.state = quick_save_state;

										emulator_paused = false;

										SDL_GameControllerRumble(SDL_GameControllerFromInstanceID(event.cbutton.which), 0xFFFF / 2, 0, 1000 / 8);
									}

									break;

								case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
									// Save save state
									quick_save_exists = true;
									quick_save_state = *emulator.state;

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
								if (controller_input == nullptr)
								{
									debug_log.Log("Received an SDL_CONTROLLERBUTTONDOWN/SDL_CONTROLLERBUTTONUP event for an unrecognised controller");
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
								if (controller_input == nullptr)
								{
									debug_log.Log("Received an SDL_CONTROLLERAXISMOTION event for an unrecognised controller");
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
											const float magnitude = SDL_sqrtf(static_cast<float>(controller_input->left_stick_x * controller_input->left_stick_x + controller_input->left_stick_y * controller_input->left_stick_y));

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
													controller_input->left_stick[i] = (delta_angle < (360.0f * 3.0f / 8.0f / 2.0f) * (static_cast<float>(CC_PI) / 180.0f)); // Half of 3/8 of 360 degrees converted to radians
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
				const float new_dpi = window.GetDPIScale();
				if (dpi_scale != new_dpi) // 96 DPI appears to be the "normal" DPI
				{
					style.ScaleAllSizes(new_dpi / dpi_scale);

					dpi_scale = new_dpi;

					ReloadFonts(CalculateFontSize());
				}

				if (emulator_running)
				{
					emulator.Update(window);
					++frame_counter;
				}

				// Start the Dear ImGui frame
				ImGui_ImplSDLRenderer2_NewFrame();
				ImGui_ImplSDL2_NewFrame();
				ImGui::NewFrame();

				// Handle drag-and-drop event.
				if (!file_picker.IsDialogOpen() && drag_and_drop_filename != nullptr)
				{
					unsigned char *file_buffer;
					std::size_t file_size;
					Utilities::LoadFileToBuffer(drag_and_drop_filename, file_buffer, file_size);

					if (file_buffer != nullptr)
					{
						if (emulator.ValidateSaveStateFromMemory(file_buffer, file_size))
						{
							if (emulator_on)
								LoadSaveState(file_buffer, file_size);

							SDL_free(file_buffer);
						}
						else
						{
							// TODO: Handle dropping a CD file here.
							AddToRecentSoftware(drag_and_drop_filename, false, false);
							LoadCartridgeFile(file_buffer, file_size);
							emulator_paused = false;
						}
					}

					SDL_free(drag_and_drop_filename);
					drag_and_drop_filename = nullptr;
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

				const bool show_menu_bar = !window.GetFullscreen()
					                    || pop_out
					                    || debug_log_active
					                    || m68k_status
					                    || mcd_m68k_status
					                    || z80_status
					                    || m68k_ram_viewer
					                    || z80_ram_viewer
					                    || prg_ram_viewer
					                    || word_ram_viewer
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
								file_picker.CreateOpenFileDialog("Load Cartridge File", [](const char* const path)
								{
									if (LoadCartridgeFile(path))
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

							if (ImGui::MenuItem("Unload Cartridge File", nullptr, false, emulator.IsCartridgeFileLoaded()))
							{
								emulator.UnloadCartridgeFile();

								if (emulator.IsCDFileLoaded())
									emulator.HardResetConsole();

								emulator_paused = false;
							}

							ImGui::Separator();

							if (ImGui::MenuItem("Load CD File..."))
							{
								file_picker.CreateOpenFileDialog("Load CD File", [](const char* const path)
								{
									if (LoadCDFile(path))
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

							if (ImGui::MenuItem("Unload CD File", nullptr, false, emulator.IsCDFileLoaded()))
							{
								emulator.UnloadCDFile();

								if (emulator.IsCartridgeFileLoaded())
									emulator.HardResetConsole();

								emulator_paused = false;
							}

							ImGui::Separator();

							ImGui::MenuItem("Pause", nullptr, &emulator_paused, emulator_on);

							if (ImGui::MenuItem("Reset", nullptr, false, emulator_on))
							{
								emulator.SoftResetConsole();
								emulator_paused = false;
							}

							ImGui::SeparatorText("Recent Software");

							if (recent_software_list.head == nullptr)
							{
								ImGui::MenuItem("None", nullptr, false, false);
							}
							else
							{
								for (DoublyLinkedList_Entry *entry = recent_software_list.head; entry != nullptr; entry = entry->next)
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
									{
										if (recent_software->is_cd_file)
										{
											if (LoadCDFile(recent_software->path))
												emulator_paused = false;
										}
										else
										{
											if (LoadCartridgeFile(recent_software->path))
												emulator_paused = false;
										}
									}

									// Show the full path as a tooltip.
									DoToolTip(recent_software->path);
								}
							}

							ImGui::EndMenu();
						}

						if (ImGui::BeginMenu("Save States"))
						{
							if (ImGui::MenuItem("Quick Save", nullptr, false, emulator_on))
							{
								quick_save_exists = true;
								quick_save_state = *emulator.state;
							}

							if (ImGui::MenuItem("Quick Load", nullptr, false, emulator_on && quick_save_exists))
							{
								*emulator.state = quick_save_state;

								emulator_paused = false;
							}

							ImGui::Separator();

							if (ImGui::MenuItem("Save to File...", nullptr, false, emulator_on))
							{
								// Obtain a filename and path from the user.
								file_picker.CreateSaveFileDialog("Create Save State", CreateSaveState);
							}

							if (ImGui::MenuItem("Load from File...", nullptr, false, emulator_on))
								file_picker.CreateOpenFileDialog("Load Save State", static_cast<bool(*)(const char*)>(LoadSaveState));

							ImGui::EndMenu();
						}

						if (ImGui::BeginMenu("Debugging"))
						{
							ImGui::MenuItem("Log", nullptr, &debug_log_active);

							ImGui::MenuItem("Toggles", nullptr, &debugging_toggles_menu);

							ImGui::Separator();

							if (ImGui::BeginMenu("CPU Registers"))
							{
								ImGui::MenuItem("Main 68000", nullptr, &m68k_status);
								ImGui::MenuItem("Sub 68000", nullptr, &mcd_m68k_status);
								ImGui::MenuItem("Z80", nullptr, &z80_status);
								ImGui::EndMenu();
							}

							if (ImGui::BeginMenu("CPU RAM"))
							{
								ImGui::MenuItem("WORK-RAM", nullptr, &m68k_ram_viewer);
								ImGui::MenuItem("PRG-RAM", nullptr, &prg_ram_viewer);
								ImGui::MenuItem("WORD-RAM", nullptr, &word_ram_viewer);
								ImGui::MenuItem("SOUND-RAM", nullptr, &z80_ram_viewer);
								ImGui::EndMenu();
							}

							if (ImGui::BeginMenu("VDP"))
							{
								ImGui::MenuItem("Registers", nullptr, &vdp_registers);
								ImGui::SeparatorText("Visualisers");
								ImGui::MenuItem("Window Plane", nullptr, &window_plane_viewer);
								ImGui::MenuItem("Plane A", nullptr, &plane_a_viewer);
								ImGui::MenuItem("Plane B", nullptr, &plane_b_viewer);
								ImGui::MenuItem("VRAM", nullptr, &vram_viewer);
								ImGui::MenuItem("CRAM", nullptr, &cram_viewer);
								ImGui::EndMenu();
							}

							ImGui::MenuItem("FM", nullptr, &fm_status);

							ImGui::MenuItem("PSG", nullptr, &psg_status);

							ImGui::MenuItem("Other", nullptr, &other_status);

							ImGui::EndMenu();
						}

						if (ImGui::BeginMenu("Misc."))
						{
							if (ImGui::MenuItem("Fullscreen", nullptr, window.GetFullscreen()))
								window.ToggleFullscreen();

							ImGui::MenuItem("Display Window", nullptr, &pop_out);

						#ifndef NDEBUG
							ImGui::Separator();

							ImGui::MenuItem("Dear ImGui Demo Window", nullptr, &dear_imgui_demo_window);

						#ifdef FILE_PICKER_HAS_NATIVE_FILE_DIALOGS
							ImGui::MenuItem("Native File Dialogs", nullptr, &file_picker.use_native_file_dialogs);
						#endif
						#endif

							ImGui::Separator();

							ImGui::MenuItem("Options", nullptr, &options_menu);

							ImGui::Separator();

							ImGui::MenuItem("About", nullptr, &about_menu);

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

						SDL_Texture *selected_framebuffer_texture = window.framebuffer_texture;

						const unsigned int work_width = static_cast<unsigned int>(size_of_display_region.x);
						const unsigned int work_height = static_cast<unsigned int>(size_of_display_region.y);

						unsigned int destination_width;
						unsigned int destination_height;

						switch (emulator.current_screen_height)
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

						ImVec2 uv1 = {static_cast<float>(emulator.current_screen_width) / static_cast<float>(FRAMEBUFFER_WIDTH), static_cast<float>(emulator.current_screen_height) / static_cast<float>(FRAMEBUFFER_HEIGHT)};

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

							window.RecreateUpscaledFramebuffer(work_width, work_height);

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
							if (window.framebuffer_texture_upscaled != nullptr && (destination_width_scaled % destination_width != 0 || destination_height_scaled % destination_height != 0))
							{
								// Render the upscaled framebuffer to the screen.
								selected_framebuffer_texture = window.framebuffer_texture_upscaled;

								// Before we can do that though, we have to actually render the upscaled framebuffer.
								SDL_Rect framebuffer_rect;
								framebuffer_rect.x = 0;
								framebuffer_rect.y = 0;
								framebuffer_rect.w = emulator.current_screen_width;
								framebuffer_rect.h = emulator.current_screen_height;

								SDL_Rect upscaled_framebuffer_rect;
								upscaled_framebuffer_rect.x = 0;
								upscaled_framebuffer_rect.y = 0;
								upscaled_framebuffer_rect.w = destination_width * framebuffer_upscale_factor;
								upscaled_framebuffer_rect.h = destination_height * framebuffer_upscale_factor;

								// Render to the upscaled framebuffer.
								SDL_SetRenderTarget(window.renderer, window.framebuffer_texture_upscaled);

								// Render.
								SDL_RenderCopy(window.renderer, window.framebuffer_texture, &framebuffer_rect, &upscaled_framebuffer_rect);

								// Switch back to actually rendering to the screen.
								SDL_SetRenderTarget(window.renderer, nullptr);

								// Update the texture UV to suit the upscaled framebuffer.
								uv1.x = static_cast<float>(upscaled_framebuffer_rect.w) / static_cast<float>(window.framebuffer_texture_upscaled_width);
								uv1.y = static_cast<float>(upscaled_framebuffer_rect.h) / static_cast<float>(window.framebuffer_texture_upscaled_height);
							}
						}

						// Center the framebuffer in the available region.
						ImGui::SetCursorPosX(static_cast<float>(static_cast<int>(ImGui::GetCursorPosX()) + (static_cast<int>(size_of_display_region.x) - destination_width_scaled) / 2));
						ImGui::SetCursorPosY(static_cast<float>(static_cast<int>(ImGui::GetCursorPosY()) + (static_cast<int>(size_of_display_region.y) - destination_height_scaled) / 2));

						// Draw the upscaled framebuffer in the window.
						ImGui::Image(selected_framebuffer_texture, ImVec2(static_cast<float>(destination_width_scaled), static_cast<float>(destination_height_scaled)), ImVec2(0, 0), uv1);
					}
				}

				ImGui::End();

				if (debug_log_active)
					debug_log.Display(debug_log_active, monospace_font);

				if (m68k_status)
					Debug_M68k(m68k_status, "Main 68000 Registers", emulator.state->clownmdemu.m68k, monospace_font);

				if (mcd_m68k_status)
					Debug_M68k(mcd_m68k_status, "Sub 68000 Registers", emulator.state->clownmdemu.mcd_m68k, monospace_font);

				if (z80_status)
					Debug_Z80(z80_status, emulator.state->clownmdemu.z80, monospace_font);

				if (m68k_ram_viewer)
					Debug_Memory(m68k_ram_viewer, monospace_font, "WORK-RAM", emulator.state->clownmdemu.m68k_ram, CC_COUNT_OF(emulator.state->clownmdemu.m68k_ram));

				if (z80_ram_viewer)
					Debug_Memory(z80_ram_viewer, monospace_font, "SOUND-RAM", emulator.state->clownmdemu.z80_ram, CC_COUNT_OF(emulator.state->clownmdemu.z80_ram));

				if (prg_ram_viewer)
					Debug_Memory(prg_ram_viewer, monospace_font, "PRG-RAM", emulator.state->clownmdemu.prg_ram, CC_COUNT_OF(emulator.state->clownmdemu.prg_ram));

				if (word_ram_viewer)
					Debug_Memory(word_ram_viewer, monospace_font, "WORD-RAM", emulator.state->clownmdemu.word_ram, CC_COUNT_OF(emulator.state->clownmdemu.word_ram));

				if (vdp_registers)
					Debug_VDP(vdp_registers, emulator);

				if (window_plane_viewer)
					Debug_WindowPlane(window_plane_viewer, emulator);

				if (plane_a_viewer)
					Debug_PlaneA(plane_a_viewer, emulator);

				if (plane_b_viewer)
					Debug_PlaneB(plane_b_viewer, emulator);

				if (vram_viewer)
					Debug_VRAM(vram_viewer, emulator);

				if (cram_viewer)
					Debug_CRAM(cram_viewer, emulator);

				if (fm_status)
					Debug_FM(fm_status, emulator, monospace_font);

				if (psg_status)
					Debug_PSG(psg_status, emulator, monospace_font);

				if (other_status)
				{
					if (ImGui::Begin("Other", &other_status, ImGuiWindowFlags_AlwaysAutoResize))
					{
						if (ImGui::BeginTable("Other", 2, ImGuiTableFlags_Borders))
						{
							const ClownMDEmu_State &clownmdemu_state = emulator.state->clownmdemu;

							cc_u8f i;

							ImGui::TableSetupColumn("Property");
							ImGui::TableSetupColumn("Value");
							ImGui::TableHeadersRow();

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("Z80 Bank");
							ImGui::TableNextColumn();
							ImGui::PushFont(monospace_font);
							ImGui::Text("0x%06" CC_PRIXFAST16 "-0x%06" CC_PRIXFAST16, clownmdemu_state.z80_bank * 0x8000, (clownmdemu_state.z80_bank + 1) * 0x8000 - 1);
							ImGui::PopFont();

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("Main 68000 Has Z80 Bus");
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(clownmdemu_state.m68k_has_z80_bus ? "Yes" : "No");

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("Z80 Reset");
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(clownmdemu_state.z80_reset ? "Yes" : "No");

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("Main 68000 Has Sub 68000 Bus");
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(clownmdemu_state.m68k_has_mcd_m68k_bus ? "Yes" : "No");

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("Sub 68000 Reset");
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(clownmdemu_state.mcd_m68k_reset ? "Yes" : "No");

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("PRG-RAM Bank");
							ImGui::TableNextColumn();
							ImGui::PushFont(monospace_font);
							ImGui::Text("0x%06" CC_PRIXFAST16 "-0x%06" CC_PRIXFAST16, clownmdemu_state.prg_ram_bank * 0x20000, (clownmdemu_state.prg_ram_bank + 1) * 0x20000 - 1);
							ImGui::PopFont();

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("WORD-RAM Mode");
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(clownmdemu_state.word_ram_1m_mode ? "1M" : "2M");

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("DMNA Bit");
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(clownmdemu_state.word_ram_dmna ? "Set" : "Clear");

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("RET Bit");
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(clownmdemu_state.word_ram_ret ? "Set" : "Clear");

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("Boot Mode");
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(clownmdemu_state.cd_boot ? "CD" : "Cartridge");

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("68000 Communication Flag");
							ImGui::TableNextColumn();
							ImGui::PushFont(monospace_font);
							ImGui::Text("0x%04" CC_PRIXFAST16, clownmdemu_state.mcd_communication_flag);
							ImGui::PopFont();

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("68000 Communication Command");
							ImGui::TableNextColumn();
							ImGui::PushFont(monospace_font);
							for (i = 0; i < CC_COUNT_OF(clownmdemu_state.mcd_communication_command); i += 2)
								ImGui::Text("0x%04" CC_PRIXFAST16 " 0x%04" CC_PRIXFAST16, clownmdemu_state.mcd_communication_command[i + 0], clownmdemu_state.mcd_communication_command[i + 1]);
							ImGui::PopFont();

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("68000 Communication Status");
							ImGui::TableNextColumn();
							ImGui::PushFont(monospace_font);
							for (i = 0; i < CC_COUNT_OF(clownmdemu_state.mcd_communication_status); i += 2)
								ImGui::Text("0x%04" CC_PRIXLEAST16 " 0x%04" CC_PRIXLEAST16, clownmdemu_state.mcd_communication_status[i + 0], clownmdemu_state.mcd_communication_status[i + 1]);
							ImGui::PopFont();

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("SUB-CPU Waiting for V-Int");
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(clownmdemu_state.mcd_waiting_for_vint ? "True" : "False");

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("SUB-CPU V-Int Enabled");
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(clownmdemu_state.mcd_vint_enabled ? "True" : "False");

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
							temp = !emulator.clownmdemu_configuration.vdp.sprites_disabled;
							if (ImGui::Checkbox("Sprite Plane", &temp))
								emulator.clownmdemu_configuration.vdp.sprites_disabled = !emulator.clownmdemu_configuration.vdp.sprites_disabled;

							ImGui::TableNextColumn();
							temp = !emulator.clownmdemu_configuration.vdp.window_disabled;
							if (ImGui::Checkbox("Window Plane", &temp))
								emulator.clownmdemu_configuration.vdp.window_disabled = !emulator.clownmdemu_configuration.vdp.window_disabled;

							ImGui::TableNextColumn();
							temp = !emulator.clownmdemu_configuration.vdp.planes_disabled[0];
							if (ImGui::Checkbox("Plane A", &temp))
								emulator.clownmdemu_configuration.vdp.planes_disabled[0] = !emulator.clownmdemu_configuration.vdp.planes_disabled[0];

							ImGui::TableNextColumn();
							temp = !emulator.clownmdemu_configuration.vdp.planes_disabled[1];
							if (ImGui::Checkbox("Plane B", &temp))
								emulator.clownmdemu_configuration.vdp.planes_disabled[1] = !emulator.clownmdemu_configuration.vdp.planes_disabled[1];

							ImGui::EndTable();
						}

						ImGui::SeparatorText("FM");

						if (ImGui::BeginTable("FM Options", 2, ImGuiTableFlags_SizingStretchSame))
						{
							char buffer[] = "FM1";

							for (std::size_t i = 0; i < CC_COUNT_OF(emulator.clownmdemu_configuration.fm.fm_channels_disabled); ++i)
							{
								buffer[2] = '1' + i;
								ImGui::TableNextColumn();
								temp = !emulator.clownmdemu_configuration.fm.fm_channels_disabled[i];
								if (ImGui::Checkbox(buffer, &temp))
									emulator.clownmdemu_configuration.fm.fm_channels_disabled[i] = !emulator.clownmdemu_configuration.fm.fm_channels_disabled[i];
							}

							ImGui::TableNextColumn();
							temp = !emulator.clownmdemu_configuration.fm.dac_channel_disabled;
							if (ImGui::Checkbox("DAC", &temp))
								emulator.clownmdemu_configuration.fm.dac_channel_disabled = !emulator.clownmdemu_configuration.fm.dac_channel_disabled;

							ImGui::EndTable();
						}

						ImGui::SeparatorText("PSG");

						if (ImGui::BeginTable("PSG Options", 2, ImGuiTableFlags_SizingStretchSame))
						{
							char buffer[] = "PSG1";

							for (std::size_t i = 0; i < CC_COUNT_OF(emulator.clownmdemu_configuration.psg.tone_disabled); ++i)
							{
								buffer[3] = '1' + i;
								ImGui::TableNextColumn();
								temp = !emulator.clownmdemu_configuration.psg.tone_disabled[i];
								if (ImGui::Checkbox(buffer, &temp))
									emulator.clownmdemu_configuration.psg.tone_disabled[i] = !emulator.clownmdemu_configuration.psg.tone_disabled[i];
							}

							ImGui::TableNextColumn();
							temp = !emulator.clownmdemu_configuration.psg.noise_disabled;
							if (ImGui::Checkbox("PSG Noise", &temp))
								emulator.clownmdemu_configuration.psg.noise_disabled = !emulator.clownmdemu_configuration.psg.noise_disabled;

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
							if (ImGui::RadioButton("NTSC", emulator.clownmdemu_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_NTSC))
							{
								if (emulator.clownmdemu_configuration.general.tv_standard != CLOWNMDEMU_TV_STANDARD_NTSC)
								{
									emulator.clownmdemu_configuration.general.tv_standard = CLOWNMDEMU_TV_STANDARD_NTSC;
									audio_output.SetPALMode(false);
								}
							}
							DoToolTip("60 FPS");
							ImGui::TableNextColumn();
							if (ImGui::RadioButton("PAL", emulator.clownmdemu_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_PAL))
							{
								if (emulator.clownmdemu_configuration.general.tv_standard != CLOWNMDEMU_TV_STANDARD_PAL)
								{
									emulator.clownmdemu_configuration.general.tv_standard = CLOWNMDEMU_TV_STANDARD_PAL;
									audio_output.SetPALMode(true);
								}
							}
							DoToolTip("50 FPS");

							ImGui::TableNextColumn();
							ImGui::TextUnformatted("Region:");
							DoToolTip("Some games only work with a certain region.");
							ImGui::TableNextColumn();
							if (ImGui::RadioButton("Japan", emulator.clownmdemu_configuration.general.region == CLOWNMDEMU_REGION_DOMESTIC))
								emulator.clownmdemu_configuration.general.region = CLOWNMDEMU_REGION_DOMESTIC;
							ImGui::TableNextColumn();
							if (ImGui::RadioButton("Elsewhere", emulator.clownmdemu_configuration.general.region == CLOWNMDEMU_REGION_OVERSEAS))
								emulator.clownmdemu_configuration.general.region = CLOWNMDEMU_REGION_OVERSEAS;

							ImGui::EndTable();
						}

						ImGui::SeparatorText("Video");

						if (ImGui::BeginTable("Video Options", 2))
						{
							ImGui::TableNextColumn();
							if (ImGui::Checkbox("V-Sync", &use_vsync))
								if (!fast_forward_in_progress)
									SDL_RenderSetVSync(window.renderer, use_vsync);
							DoToolTip("Prevents screen tearing.");

							ImGui::TableNextColumn();
							if (ImGui::Checkbox("Integer Screen Scaling", &integer_screen_scaling) && integer_screen_scaling)
							{
								// Reclaim memory used by the upscaled framebuffer, since we won't be needing it anymore.
								SDL_DestroyTexture(window.framebuffer_texture_upscaled);
								window.framebuffer_texture_upscaled = nullptr;
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
							bool low_pass_filter = audio_output.GetLowPassFilter();
							if (ImGui::Checkbox("Low-Pass Filter", &low_pass_filter))
								audio_output.SetLowPassFilter(low_pass_filter);
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

							for (std::size_t i = 0; i < CC_COUNT_OF(sorted_scancodes); ++i)
								sorted_scancodes[i] = static_cast<SDL_Scancode>(i);

							SDL_qsort(sorted_scancodes, CC_COUNT_OF(sorted_scancodes), sizeof(sorted_scancodes[0]),
								[](const void* const a, const void* const b)
							{
								const SDL_Scancode* const binding_1 = static_cast<const SDL_Scancode*>(a);
								const SDL_Scancode* const binding_2 = static_cast<const SDL_Scancode*>(b);

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
							for (std::size_t i = 0; i < CC_COUNT_OF(sorted_scancodes); ++i)
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

							if (ImGui::BeginListBox("##Actions"))
							{
								for (InputBinding i = static_cast<InputBinding>(INPUT_BINDING_NONE + 1); i < INPUT_BINDING__TOTAL; i = static_cast<InputBinding>(i + 1))
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
							if (SDL_GetRendererInfo(window.renderer, &info) == 0)
								ImGui::TextUnformatted(info.name);
							else
								ImGui::TextUnformatted("Unknown");

							// Video
							ImGui::TableNextColumn();
							ImGui::TextUnformatted("Video");

							ImGui::TableNextColumn();
							const char* const audio_driver_name = SDL_GetCurrentVideoDriver();
							ImGui::TextUnformatted(audio_driver_name != nullptr ? audio_driver_name : "None");

							// Audio
							ImGui::TableNextColumn();
							ImGui::TextUnformatted("Audio");

							ImGui::TableNextColumn();
							const char* const video_driver_name = SDL_GetCurrentAudioDriver();
							ImGui::TextUnformatted(video_driver_name != nullptr ? video_driver_name : "None");

							ImGui::EndTable();
						}
					}

					ImGui::End();
				}

				file_picker.Update(drag_and_drop_filename);

				SDL_RenderClear(window.renderer);

				// Render Dear ImGui.
				ImGui::Render();
				ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

				// Finally display the rendered frame to the user.
				SDL_RenderPresent(window.renderer);
			}

			debug_log.ForceConsoleOutput(true);

			audio_output.Deinitialise();

			ImGui_ImplSDLRenderer2_Shutdown();
			ImGui_ImplSDL2_Shutdown();
			ImGui::DestroyContext();

			SDL_DestroyTexture(window.framebuffer_texture_upscaled);

			SaveConfiguration();

		#ifdef POSIX
			SDL_free(last_file_dialog_directory);
		#endif

			// Free recent software list.
			while (recent_software_list.head != nullptr)
			{
				RecentSoftware* const recent_software = CC_STRUCT_POINTER_FROM_MEMBER_POINTER(RecentSoftware, list, recent_software_list.head);

				DoublyLinkedList_Remove(&recent_software_list, recent_software_list.head);
				SDL_free(recent_software);
			}

			window.Deinitialise();
		}

		SDL_Quit();
	}

	return 0;
}

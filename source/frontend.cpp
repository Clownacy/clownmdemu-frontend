#include "frontend.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits> // For INT_MAX.
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <iterator>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <fmt/core.h>
#include <SDL3/SDL.h>

#include "../common/core/libraries/clowncommon/clowncommon.h"
#include "../common/core/source/clownmdemu.h"

#include "../libraries/imgui/imgui.h"
#include "../libraries/imgui/backends/imgui_impl_sdl3.h"
#include "../libraries/imgui/backends/imgui_impl_sdlrenderer3.h"

#include "cd-reader.h"
#include "debug-log.h"
#include "emulator-instance.h"
#include "file-utilities.h"
#include "windows/about.h"
#include "windows/cheats.h"
#include "windows/debug-cdc.h"
#include "windows/debug-cdda.h"
#include "windows/debug-fm.h"
#include "windows/debug-frontend.h"
#include "windows/debug-log-viewer.h"
#include "windows/debug-m68k.h"
#include "windows/debug-memory.h"
#include "windows/debug-other.h"
#include "windows/debug-pcm.h"
#include "windows/debug-psg.h"
#include "windows/debug-toggles.h"
#include "windows/debug-vdp.h"
#include "windows/debug-z80.h"
#include "windows/disassembler.h"
#include "windows/common/window-with-framebuffer.h"

#ifndef __EMSCRIPTEN__
#define FILE_PATH_SUPPORT
#endif

static constexpr unsigned int INITIAL_WINDOW_SCALE = 2;
static constexpr float INITIAL_WINDOW_WIDTH = VDP_H40_SCREEN_WIDTH_IN_TILE_PAIRS * VDP_TILE_PAIR_WIDTH * INITIAL_WINDOW_SCALE;
static constexpr float INITIAL_WINDOW_HEIGHT = VDP_V28_SCANLINES_IN_TILES * VDP_STANDARD_TILE_HEIGHT * INITIAL_WINDOW_SCALE;

static constexpr unsigned int FRAMEBUFFER_WIDTH = VDP_MAX_SCANLINE_WIDTH;
static constexpr unsigned int FRAMEBUFFER_HEIGHT = VDP_MAX_SCANLINES;

static constexpr char DEFAULT_TITLE[] = "ClownMDEmu";

class Input
{
private:
	std::array<unsigned char, CLOWNMDEMU_BUTTON_MAX> buttons = {0};

public:
	const std::string name;
	unsigned char fast_forward = 0;
	unsigned char rewind = 0;

	Input(std::string name)
		: name(std::move(name))
	{}

	unsigned char GetButton(ClownMDEmu_Button button)
	{
		return buttons[button];
	}
	void SetButton(ClownMDEmu_Button button, unsigned char value);
};

struct ControllerInput
{
	static inline unsigned int controller_number;

	const SDL_JoystickID joystick_instance_id;
	Sint16 left_stick_x = 0;
	Sint16 left_stick_y = 0;
	std::array<bool, 4> left_stick = {false};
	std::array<bool, 4> dpad = {false};
	bool left_trigger = false;
	bool right_trigger = false;
	Input input;

	ControllerInput(const SDL_JoystickID joystick_instance_id)
		: joystick_instance_id(joystick_instance_id)
		, input(fmt::format("Controller {}: {}", ++controller_number, SDL_GetJoystickNameForID(joystick_instance_id)))
	{}
};

enum class InputBinding
{
	NONE,
	CONTROLLER_UP,
	CONTROLLER_DOWN,
	CONTROLLER_LEFT,
	CONTROLLER_RIGHT,
	CONTROLLER_A,
	CONTROLLER_B,
	CONTROLLER_C,
	CONTROLLER_X,
	CONTROLLER_Y,
	CONTROLLER_Z,
	CONTROLLER_START,
	CONTROLLER_MODE,
	PAUSE,
	RESET,
	FAST_FORWARD,
	REWIND,
	QUICK_SAVE_STATE,
	QUICK_LOAD_STATE,
	TOGGLE_FULLSCREEN,
	TOTAL,
};

enum class ScreenScaling
{
	FIT,
	FILL,
	PIXEL_PERFECT,
};

#ifdef FILE_PATH_SUPPORT
struct RecentSoftware
{
	bool is_cd_file;
	std::filesystem::path path;
};
#endif

static std::list<ControllerInput> controller_input_list;

static Input keyboard_input = {"Keyboard"};
static std::array<InputBinding, SDL_SCANCODE_COUNT> keyboard_bindings; // TODO: `SDL_SCANCODE_COUNT` is an internal macro, so use something standard!

static std::array<Input*, 8> bound_inputs;

static ScreenScaling screen_scaling;

#ifndef NDEBUG
static bool dear_imgui_demo_window;
#endif

void Input::SetButton(const ClownMDEmu_Button button, const unsigned char value)
{
	buttons[button] = value;

	if (!value)
		return;

	// Automatically bind this controller to the first unbound input.
	for (auto &bound_input : bound_inputs)
		if (bound_input == this)
			return;

	for (auto &bound_input : bound_inputs)
	{
		if (bound_input == nullptr)
		{
			bound_input = this;
			break;
		}
	}
}

class OptionsWindow : public WindowPopup<OptionsWindow>
{
private:
	using Base = WindowPopup<OptionsWindow>;

	static constexpr Uint32 window_flags = 0;

	bool sorted_scancodes_done;
	std::array<SDL_Scancode, SDL_SCANCODE_COUNT> sorted_scancodes; // TODO: `SDL_SCANCODE_COUNT` is an internal macro, so use something standard!

	SDL_Scancode selected_scancode;
	bool scroll_to_add_bindings_button;

	void DisplayInternal()
	{
		ImGui::SeparatorText("Console");

		// TODO: Convert these horrible macros to lambdas.
	#define DO_FORM_LAYOUT(LABEL, LABEL_TOOLTIP) \
		do { \
			const auto &start_position_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x / 3; \
			ImGui::TextUnformatted(LABEL ":"); \
			DoToolTip(LABEL_TOOLTIP); \
			ImGui::SameLine(); \
			ImGui::SetCursorPosX(start_position_x); \
			ImGui::SetNextItemWidth(-FLT_MIN); \
		} while (0)

		struct ComboItemAndToolTip
		{
			const char* label;
			const char* tooltip;
		};

		const auto &ComboWithToolTips = [](const char* const label, int &current_item, const ComboItemAndToolTip items_and_tooltip[], const int items_and_tooltip_count)
		{
			bool changed = false;

			if (ImGui::BeginCombo(label, items_and_tooltip[current_item].label))
			{
				for (int i = 0; i < items_and_tooltip_count; ++i)
				{
					const bool selected = current_item == i;
					if (ImGui::Selectable(items_and_tooltip[i].label, selected))
					{
						current_item = i;
						changed = true;
					}

					DoToolTip(items_and_tooltip[i].tooltip);

					if (selected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			DoToolTip(items_and_tooltip[current_item].tooltip);

			return changed;
		};

	#define DO_FORM_OPTION(LABEL, LABEL_TOOLTIP, OPTION_NAMES_AND_TOOLTIPS, OPTION) \
		do { \
			DO_FORM_LAYOUT(LABEL, LABEL_TOOLTIP); \
			const auto option = frontend->emulator->Get##OPTION(); \
			auto option_int = static_cast<int>(option); \
			if (ComboWithToolTips("##" LABEL, option_int, std::data(OPTION_NAMES_AND_TOOLTIPS), std::size(OPTION_NAMES_AND_TOOLTIPS))) \
				frontend->emulator->Set##OPTION(static_cast<std::remove_cv_t<decltype(option)>>(option_int)); \
		} while (0)

		if (ImGui::BeginTable("Console Options", 3))
		{
		#define DO_CONSOLE_OPTION(LABEL, LABEL_TOOLTIP, OPTION_NAMES, OPTION_TOOLTIPS, OPTION) \
			do { \
				ImGui::TableNextColumn(); \
				ImGui::TextUnformatted(LABEL ":"); \
				DoToolTip(LABEL_TOOLTIP); \
				const auto option = frontend->emulator->Get##OPTION(); \
				const auto option_int = static_cast<unsigned int>(option); \
				for (unsigned int i = 0; i < std::size(OPTION_NAMES); ++i) \
				{ \
					ImGui::TableNextColumn(); \
					if (ImGui::RadioButton(OPTION_NAMES[i], i == option_int)) \
						frontend->emulator->Set##OPTION(static_cast<std::remove_cv_t<decltype(option)>>(i)); \
					DoToolTip(OPTION_TOOLTIPS[i]); \
				} \
			} while (0)

			static const std::array<const char*, 2> tv_standard_names = {
				"NTSC",
				"PAL",
			};

			static const std::array<const char*, 2> tv_standard_tooltips = {
				"59.94Hz",
				"50Hz",
			};

			DO_CONSOLE_OPTION("TV Standard", "Some games only work with a certain TV standard.", tv_standard_names, tv_standard_tooltips, TVStandard);

			static const std::array<const char*, 2> region_names = {
				"Japan",
				"International",
			};

			static const std::array<const char*, 2> region_tooltips = {
				"Games may show Japanese text.",
				"Games may show English text.",
			};

			DO_CONSOLE_OPTION("Region", "Some games only work with a certain region.", region_names, region_tooltips, Region);

			static const std::array<const char*, 2> cd_addon_names = {
				"Disconnected",
				"Connected",
			};

			static const std::array<const char*, 2> cd_addon_tooltips = {
				"More stable, but provides fewer features.",
				"Less stable, but provides more features.",
			};

			DO_CONSOLE_OPTION(
				"CD Add-on",
				"Allow cartridge-only software to utilise features of\n"
				"the emulated Mega CD add-on, such as CD music.\n"
				"This may break some software.",
				cd_addon_names,
				cd_addon_tooltips,
				CDAddOnEnabled
			);

			ImGui::EndTable();
		}

		ImGui::SeparatorText("Video");

		static const std::array<ComboItemAndToolTip, 3> scaling_types = {{
			{
				"Fit",
				"Adds black bars around the screen\nto fit it within the window."
			},
			{
				"Fill",
				"Crops the screen to fill the entire window.\nPairs well with the widescreen hack."
			},
			{
				"Pixel-Perfect",
				"Preserves pixel aspect ratio,\navoiding non-square pixels."
			},
		}};

		DO_FORM_LAYOUT("Scaling", "How to fit the screen to the window.");

		auto scaling_int = static_cast<int>(screen_scaling);
		if (ComboWithToolTips("##Scaling", scaling_int, std::data(scaling_types), std::size(scaling_types)))
			screen_scaling = static_cast<ScreenScaling>(scaling_int);

		DO_FORM_LAYOUT(
			"Widescreen Hack",
			"Widens the display. Works well\n"
			"with some games, badly with others."
		);

		int current_widescreen_setting = frontend->emulator->GetWidescreenTiles();
		const std::string widescreen_slider_text = current_widescreen_setting == 0 ? "Disabled" : fmt::format("{} Extra Columns", current_widescreen_setting * 2);
		if (ImGui::SliderInt("##Widescreen Hack Slider", &current_widescreen_setting, 0, VDP_MAX_WIDESCREEN_TILES, widescreen_slider_text.c_str(), ImGuiSliderFlags_AlwaysClamp))
			frontend->emulator->SetWidescreenTiles(current_widescreen_setting);

		if (ImGui::BeginTable("Video Options", 2))
		{
			ImGui::TableNextColumn();
			auto vsync = frontend->window->GetVSync();
			if (ImGui::Checkbox("V-Sync", &vsync))
				frontend->window->SetVSync(vsync);
			DoToolTip("Prevents screen tearing.");

			ImGui::TableNextColumn();
			ImGui::Checkbox("Tall Interlace Mode 2", &frontend->tall_double_resolution_mode);
			DoToolTip(
				"Makes games that use Interlace Mode 2\n"
				"for split-screen not appear squashed.");

			ImGui::EndTable();
		}

		ImGui::SeparatorText("Audio");

		if (ImGui::BeginTable("Audio Options", 2))
		{
			ImGui::TableNextColumn();
			bool low_pass_filter = frontend->emulator->GetLowPassFilterEnabled();
			if (ImGui::Checkbox("Low-Pass Filter", &low_pass_filter))
				frontend->emulator->SetLowPassFilterEnabled(low_pass_filter);
			DoToolTip(
				"Lowers the volume of high frequencies to make\n"
				"the audio 'softer', like a real Mega Drive does.\n"
				"\n"
				"Without this, treble will become louder due to\n"
				"differences in volume balancing.");

			ImGui::TableNextColumn();
			bool ladder_effect = frontend->emulator->GetLadderEffectEnabled();
			if (ImGui::Checkbox("Low-Volume Distortion", &ladder_effect))
				frontend->emulator->SetLadderEffectEnabled(ladder_effect);
			DoToolTip(
				"Enables the so-called 'ladder effect' that\n"
				"is present in early Mega Drives.\n"
				"\n"
				"Without this, some quiet sounds will\n"
				"become inaudible.");

			ImGui::EndTable();
		}

		ImGui::SeparatorText("Miscellaneous");

		if (ImGui::BeginTable("Miscellaneous Options", 2))
		{
			ImGui::TableNextColumn();
			bool rewinding_enabled = frontend->emulator->GetRewindEnabled();
			if (ImGui::Checkbox("Rewinding", &rewinding_enabled))
				frontend->emulator->SetRewindEnabled(rewinding_enabled);
			DoToolTip(
				"Allows the emulated console to be played in\n"
				"reverse for up to 10 seconds. This uses a lot\n"
				"of RAM and increases CPU usage, so disable\n"
				"this if there is lag.");

			ImGui::EndTable();
		}

	#ifndef NDEBUG
		ImGui::SeparatorText("Development");

		if (ImGui::BeginTable("Development Options", 2))
		{
	#ifndef __EMSCRIPTEN__
			ImGui::TableNextColumn();
			ImGui::Checkbox("Native Windows", &frontend->native_windows);
			DoToolTip(
				"Use real windows instead of 'fake' windows\n"
				"that are stuck inside the main window.");
	#endif

			ImGui::TableNextColumn();
			ImGui::Checkbox("Native File Dialogs", &file_utilities.use_native_file_dialogs);

			ImGui::TableNextColumn();
			ImGui::Checkbox("Dear ImGui Demo Window", &dear_imgui_demo_window);

			ImGui::EndTable();
		}
	#endif

		ImGui::SeparatorText("Control Pads");

		static const std::array<ComboItemAndToolTip, 3> input_protocols = {{
			{
				"Standard",
				"Standard input protocol.\nShould work with all games.",
			},
			{
				"Multi",
				"Sega's multitap protocol.\nOnly works with certain games.",
			},
			{
				"Extra",
				"Electronic Arts' multitap protocol.\nOnly works with certain games.",
			}
		}};

		DO_FORM_OPTION("Protocol", "Which method to read control pad inputs.", input_protocols, ControllerProtocol);

		if (ImGui::BeginTable("Control Pad Devices", 2, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Device");
			ImGui::TableHeadersRow();

			constexpr auto GetMaximumControlPads = [](const ControllerManager_Protocol protocol) -> unsigned int
			{
				switch (protocol)
				{
					case CONTROLLER_MANAGER_PROTOCOL_STANDARD:
						return 2;

					case CONTROLLER_MANAGER_PROTOCOL_SEGA_TAP:
						return 8;

					case CONTROLLER_MANAGER_PROTOCOL_EA_4_WAY_PLAY:
						return 4;
				}

				return 0;
			};

			const auto maximum_control_pads = GetMaximumControlPads(frontend->emulator->GetControllerProtocol());

			for (unsigned int i = 0; i < maximum_control_pads; ++i)
			{
				ImGui::PushID(i);

				const auto &label = std::to_string(1 + i);

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(label);

				auto &bound_input = bound_inputs[i];

				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::BeginCombo("", bound_input == nullptr ? "None" : bound_input->name.c_str()))
				{
					const auto &DoSelectable = [&bound_input](const char* name, Input* const address)
					{
						const bool selected = bound_input == address;
						if (ImGui::Selectable(name, selected))
						{
							// If the selected device is already bound to an input, swap them.
							const auto &FindOtherBoundInput = [](const Input* const address) -> Input**
							{
								if (address != nullptr)
									for (auto &other_bound_input : bound_inputs)
										if (other_bound_input == address)
											return &other_bound_input;

								return nullptr;
							};

							Input** const other_bound_input = FindOtherBoundInput(address);

							if (other_bound_input != nullptr)
								std::swap(*other_bound_input, bound_input);
							else
								bound_input = address;
						}
						if (selected)
							ImGui::SetItemDefaultFocus();
					};

					DoSelectable("None", nullptr);
					DoSelectable(keyboard_input.name.c_str(), &keyboard_input);

					for (auto &controller_input : controller_input_list)
						DoSelectable(controller_input.input.name.c_str(), &controller_input.input);

					ImGui::EndCombo();
				}

				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		ImGui::SeparatorText("Keyboard");

		static const std::array<const char*, static_cast<std::size_t>(InputBinding::TOTAL)> binding_names = {
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
			"Rewind",
			"Quick Save State",
			"Quick Load State",
			"Toggle Fullscreen"
		};

		if (!sorted_scancodes_done)
		{
			sorted_scancodes_done = true;

			for (std::size_t i = 0; i != std::size(sorted_scancodes); ++i)
				sorted_scancodes[i] = static_cast<SDL_Scancode>(i);

			std::sort(sorted_scancodes.begin(), sorted_scancodes.end(),
				[](const auto &binding_1, const auto &binding_2)
				{
					return keyboard_bindings[binding_1] < keyboard_bindings[binding_2];
				}
			);
		}

		if (ImGui::BeginTable("Keyboard Bindings", 3, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Key");
			ImGui::TableSetupColumn("Action");
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableHeadersRow();
			for (const auto &scancode : sorted_scancodes)
			{
				if (keyboard_bindings[scancode] != InputBinding::NONE)
				{
					ImGui::PushID(&scancode);

					ImGui::TableNextColumn();
					ImGui::TextUnformatted(SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode)));

					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					int index = static_cast<int>(keyboard_bindings[scancode]) - 1;
					if (ImGui::Combo("##action", &index, std::data(binding_names) + 1, std::size(binding_names) - 1))
					{
						keyboard_bindings[scancode] = static_cast<InputBinding>(index + 1);
						sorted_scancodes_done = false;
					}

					ImGui::TableNextColumn();
					if (ImGui::Button("X"))
						keyboard_bindings[scancode] = InputBinding::NONE;

					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}

		if (ImGui::Button("Add Binding"))
			ImGui::OpenPopup("Select Key");

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

			ImGui::TextUnformatted("Press the key that you want to bind...");

			int total_keys;
			const bool* const keys_pressed = SDL_GetKeyboardState(&total_keys);

			for (int i = 0; i < total_keys; ++i)
			{
				if (keys_pressed[i] && i != SDL_GetScancodeFromKey(SDLK_LALT, nullptr))
				{
					// The 'escape' key will exit the menu without binding.
					if (i != SDL_GetScancodeFromKey(SDLK_ESCAPE, nullptr))
					{
						next_menu = true;
						selected_scancode = static_cast<SDL_Scancode>(i);
					}
					break;
				}
			}

			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();

			if (next_menu)
				ImGui::OpenPopup("Select Action");

			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

			if (ImGui::BeginPopupModal("Select Action", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				bool exit = false;

				ImGui::TextFormatted("Selected Key: {}", SDL_GetScancodeName(selected_scancode));

				if (ImGui::BeginListBox("##Actions"))
				{
					for (unsigned int i = static_cast<unsigned int>(InputBinding::NONE) + 1; i < static_cast<unsigned int>(InputBinding::TOTAL); i = i + 1)
					{
						if (ImGui::Selectable(binding_names[i]))
						{
							ImGui::CloseCurrentPopup();
							exit = true;
							keyboard_bindings[selected_scancode] = static_cast<InputBinding>(i);
							sorted_scancodes_done = false;
							scroll_to_add_bindings_button = true;
						}
					}
					ImGui::EndListBox();
				}

				if (ImGui::Button("Cancel"))
					ImGui::CloseCurrentPopup();

				ImGui::EndPopup();

				if (exit)
					ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

public:
	using Base::WindowPopup;

	friend Base;
};

static std::array<InputBinding, SDL_SCANCODE_COUNT> keyboard_bindings_cached; // TODO: `SDL_SCANCODE_COUNT` is an internal macro, so use something standard!
static std::array<bool, SDL_SCANCODE_COUNT> key_pressed; // TODO: `SDL_SCANCODE_COUNT` is an internal macro, so use something standard!

#ifdef FILE_PATH_SUPPORT
static std::list<RecentSoftware> recent_software_list;
#endif
static std::filesystem::path drag_and_drop_filename;

static bool emulator_has_focus; // Used for deciding when to pass inputs to the emulator.
static bool emulator_frame_advance;

static std::optional<EmulatorInstance::StateBackup> quick_save_state;

static std::optional<Cheats> cheats_window;
static std::optional<DebugLogViewer> debug_log_window;
static std::optional<DebugToggles> debugging_toggles_window;
static std::optional<Disassembler> disassembler_window;
static std::optional<DebugFrontend> debug_frontend_window;
static std::optional<DebugM68k::Registers> m68k_status_window;
static std::optional<DebugM68k::Registers> mcd_m68k_status_window;
static std::optional<DebugZ80::Registers> z80_status_window;
static std::optional<DebugMemory> m68k_ram_viewer_window;
static std::optional<DebugMemory> external_ram_viewer_window;
static std::optional<DebugMemory> z80_ram_viewer_window;
static std::optional<DebugMemory> word_ram_viewer_window;
static std::optional<DebugMemory> prg_ram_viewer_window;
static std::optional<DebugMemory> wave_ram_viewer_window;
static std::optional<DebugVDP::Registers> vdp_registers_window;
static std::optional<DebugVDP::SpriteList> sprite_list_window;
static std::optional<DebugMemory> vram_viewer_window;
static std::optional<DebugMemory> cram_viewer_window;
static std::optional<DebugMemory> vsram_viewer_window;
static std::optional<DebugVDP::SpriteViewer> sprite_plane_visualiser_window;
static std::optional<DebugVDP::PlaneViewer> window_plane_visualiser_window;
static std::optional<DebugVDP::PlaneViewer> plane_a_visualiser_window;
static std::optional<DebugVDP::PlaneViewer> plane_b_visualiser_window;
static std::optional<DebugVDP::VRAMViewer> tile_visualiser_window;
static std::optional<DebugVDP::CRAMViewer> colour_visualiser_window;
static std::optional<DebugVDP::StampViewer> stamp_visualiser_window;
static std::optional<DebugVDP::StampMapViewer> stamp_map_visualiser_window;
static std::optional<DebugFM::Registers> fm_status_window;
static std::optional<DebugPSG::Registers> psg_status_window;
static std::optional<DebugPCM::Registers> pcm_status_window;
static std::optional<DebugCDDA> cdda_status_window;
static std::optional<DebugCDC> cdc_status_window;
static std::optional<DebugOther> other_status_window;
static std::optional<OptionsWindow> options_window;
static std::optional<AboutWindow> about_window;

static constexpr auto popup_windows = std::make_tuple(
	&cheats_window,
	&debug_log_window,
	&debugging_toggles_window,
	&disassembler_window,
	&debug_frontend_window,
	&m68k_status_window,
	&mcd_m68k_status_window,
	&z80_status_window,
	&m68k_ram_viewer_window,
	&external_ram_viewer_window,
	&z80_ram_viewer_window,
	&word_ram_viewer_window,
	&prg_ram_viewer_window,
	&wave_ram_viewer_window,
	&vdp_registers_window,
	&sprite_list_window,
	&vram_viewer_window,
	&cram_viewer_window,
	&vsram_viewer_window,
	&sprite_plane_visualiser_window,
	&window_plane_visualiser_window,
	&plane_a_visualiser_window,
	&plane_b_visualiser_window,
	&tile_visualiser_window,
	&colour_visualiser_window,
	&stamp_visualiser_window,
	&stamp_map_visualiser_window,
	&fm_status_window,
	&psg_status_window,
	&pcm_status_window,
	&cdda_status_window,
	&cdc_status_window,
	&other_status_window,
	&options_window,
	&about_window
);

// Manages whether the program exits or not.
static bool quit;

// Used for tracking when to pop the emulation display out into its own little window.
static bool screen_subwindow;

static bool emulator_on;

static bool forced_fullscreen;


////////////////////////////
// Emulator Functionality //
////////////////////////////

static cc_bool ReadInputCallback(const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
	SDL_assert(player_id < std::size(bound_inputs));

	Input* const input = bound_inputs[player_id];

	if (input == nullptr)
		return cc_false;

	// Don't use inputs that are for Dear ImGui
	if (input == &keyboard_input)
	{
		if (!emulator_has_focus)
			return cc_false;
	}
	else
	{
		if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0)
			return cc_false;
	}

	return input->GetButton(button_id) != 0;
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

			recent_software_list.front().is_cd_file = is_cd_file;
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

void Frontend::UpdateFastForwardStatus()
{
	unsigned int speed = keyboard_input.fast_forward;

	for (const auto &controller_input : controller_input_list)
		speed += controller_input.input.fast_forward;

	emulator->SetFastForwarding(speed);
}

void Frontend::UpdateRewindStatus()
{
	bool will_rewind = keyboard_input.rewind;

	for (const auto &controller_input : controller_input_list)
		will_rewind |= controller_input.input.rewind != 0;

	emulator->rewinding = will_rewind;
}


///////////
// Misc. //
///////////

static std::filesystem::path configuration_directory_path;

bool Frontend::IsFileCD(const std::filesystem::path& path)
{
	return CDReader::IsDefinitelyACD(path);
}

static std::filesystem::path GetDefaultConfigurationDirectoryPath()
{
	// Standard configuration directory.
	std::filesystem::path path = SDL::GetPrefPath("clownacy", "clownmdemu-frontend");

	if (path.empty())
	{
		// Use working directory as fallback.
		path = ".";
	}

	return path;
}

static void InitialiseConfigurationDirectoryPath(const std::filesystem::path &user_data_path)
{
	// User-specified directory.
	configuration_directory_path = user_data_path;

	if (configuration_directory_path.empty())
		configuration_directory_path = GetDefaultConfigurationDirectoryPath();
}

const std::filesystem::path& Frontend::GetConfigurationDirectoryPath()
{
	return configuration_directory_path;
}

std::filesystem::path Frontend::GetSaveDataDirectoryPath()
{
	return GetConfigurationDirectoryPath() / "Save Data";
}

std::filesystem::path Frontend::GetConfigurationFilePath()
{
	return GetConfigurationDirectoryPath() / "configuration.ini";
}

std::filesystem::path Frontend::GetDearImGuiSettingsFilePath()
{
	return GetConfigurationDirectoryPath() / "dear-imgui-settings.ini";
}

void Frontend::LoadCartridgeFile(const std::filesystem::path &path, std::vector<cc_u16l> &&file_buffer)
{
#ifdef FILE_PATH_SUPPORT
	AddToRecentSoftware(path, false, false);
#endif

	quick_save_state = std::nullopt;
	emulator->LoadCartridgeFile(std::move(file_buffer), path);
}

bool Frontend::LoadCartridgeFile(const std::filesystem::path &path, SDL::IOStream &file)
{
	if (file)
	{
		std::optional<std::vector<cc_u16l>> file_buffer;

		// First try loading the file as a ZIP file.
		file_buffer = FileUtilities::LoadZIPFileToBuffer(file, 0); // Assume that it's the first file in the archive.

		// Failing that, just load it as a raw binary.
		if (!file_buffer.has_value())
			file_buffer = FileUtilities::LoadFileToBuffer<cc_u16l, 2>(file);

		if (file_buffer.has_value())
		{
			LoadCartridgeFile(path, std::move(*file_buffer));
			return true;
		}
	}

	debug_log.Log("Could not load the cartridge file");
	window->ShowErrorMessageBox("Failed to load the cartridge file.");
	return false;
}

bool Frontend::LoadCartridgeFile(const std::filesystem::path &path)
{
	SDL::IOStream file(path, "rb");
	return LoadCartridgeFile(path, file);
}

bool Frontend::LoadCDFile(const std::filesystem::path &path, SDL::IOStream &&file)
{
#ifdef FILE_PATH_SUPPORT
	AddToRecentSoftware(path, true, false);
#endif

	// Load the CD.
	if (!emulator->LoadCDFile(std::move(file), path))
		return false;

	return true;
}

bool Frontend::LoadCDFile(const std::filesystem::path &path)
{
	SDL::IOStream file(path, "rb");

	if (!file)
	{
		debug_log.Log("Could not load the CD file");
		window->ShowErrorMessageBox("Failed to load the CD file.");
		return false;
	}

	return LoadCDFile(path, std::move(file));
}

#ifdef FILE_PATH_SUPPORT
bool Frontend::LoadSoftwareFile(const bool is_cd_file, const std::filesystem::path &path)
{
	if (is_cd_file)
		return LoadCDFile(path);
	else
		return LoadCartridgeFile(path);
}
#endif

bool Frontend::LoadSaveState(SDL::IOStream &file)
{
	if (!file || !emulator->LoadSaveStateFile(file))
	{
		debug_log.Log("Could not load save state file");
		window->ShowErrorMessageBox("Could not load save state file.");
		return false;
	}

	emulator->SetPaused(false);
	return true;
}

bool Frontend::LoadSaveState(const std::filesystem::path &path)
{
	SDL::IOStream file(path, "rb");
	return LoadSaveState(file);
}

#ifdef FILE_PATH_SUPPORT
bool Frontend::SaveState(const std::filesystem::path &path)
{
	SDL::IOStream file(path, "wb");

	if (!file || !emulator->WriteSaveStateFile(file))
	{
		debug_log.Log("Could not create save state file");
		window->ShowErrorMessageBox("Could not create save state file.");
		return false;
	}

	return true;
}
#endif

bool Frontend::ShouldBeInFullscreenMode()
{
	return forced_fullscreen || window->GetFullscreen();
}

bool Frontend::NativeWindowsActive()
{
	return native_windows && !ShouldBeInFullscreenMode();
}


/////////
// INI //
/////////

namespace INI
{
	using Callback = std::function<void(const std::string_view &section, const std::string_view &key, const std::string_view &value)>;

	void ProcessFile(SDL::IOStream &stream, const Callback &callback)
	{
		if (!stream)
			return;

		std::string line, section;

		const auto &ProcessLine = [&]()
		{
			if (line.empty())
				return;

			if (line.front() == '[' && line.back() == ']')
			{
				// Section.
				section = line.substr(1, line.length() - 2);
			}
			else
			{
				// Key/value pair.
				const std::string_view line_view(line);
				const std::string_view separator = " = ";
				const auto separator_position = line_view.find(separator);
				const auto key = line_view.substr(0, separator_position);
				const auto value = line_view.substr(separator_position + separator.length());
				callback(section, key, value);
			}

			line.clear();
		};

		for (;;)
		{
			char character;
			if (SDL_ReadIO(stream, &character, 1) == 0)
			{
				ProcessLine();
				return;
			}

			if (character == '\r' || character == '\n')
				ProcessLine();
			else
				line += character;
		}
	}
}


///////////////////
// Configuration //
///////////////////

void Frontend::LoadConfiguration()
{
	// Set default settings.
#ifndef __EMSCRIPTEN__
	bool fullscreen = false;
#endif
	bool vsync = false;
	screen_scaling = ScreenScaling::FIT;
	tall_double_resolution_mode = false;
	unsigned int widescreen_tiles = 0;
#ifdef __EMSCRIPTEN__
	native_windows = false;
#else
	native_windows = true;
#endif
	bool rewinding = true;
	bool low_pass_filter = true;
	bool cd_add_on = false;
	ControllerManager_Protocol input_protocol = CONTROLLER_MANAGER_PROTOCOL_STANDARD;
	bool ladder_effect = true;
	bool pal_mode = false;
	bool domestic = false;

	// Default V-sync.
	const SDL_DisplayID display_index = SDL_GetDisplayForWindow(window->GetSDLWindow());

	if (display_index == 0)
	{
		debug_log.SDLError("SDL_GetDisplayForWindow");
	}
	else
	{
		const SDL_DisplayMode* const display_mode = SDL_GetCurrentDisplayMode(display_index);

		if (display_mode == nullptr)
		{
			debug_log.SDLError("SDL_GetCurrentDisplayMode");
		}
		else
		{
			// Enable V-sync on displays with an FPS of a multiple of 60.
			// TODO: ...But what about PAL50?
			vsync = std::lround(display_mode->refresh_rate) % 60 == 0;
		}
	}

	// Load the configuration file, overwriting the above settings.
	SDL::IOStream file(GetConfigurationFilePath(), "r");

	if (!file)
	{
		// Failed to read configuration file: set defaults key bindings.
		keyboard_bindings.fill(InputBinding::NONE);

		keyboard_bindings[SDL_SCANCODE_UP] = InputBinding::CONTROLLER_UP;
		keyboard_bindings[SDL_SCANCODE_DOWN] = InputBinding::CONTROLLER_DOWN;
		keyboard_bindings[SDL_SCANCODE_LEFT] = InputBinding::CONTROLLER_LEFT;
		keyboard_bindings[SDL_SCANCODE_RIGHT] = InputBinding::CONTROLLER_RIGHT;
		keyboard_bindings[SDL_SCANCODE_A] = InputBinding::CONTROLLER_A;
		keyboard_bindings[SDL_SCANCODE_S] = InputBinding::CONTROLLER_B;
		keyboard_bindings[SDL_SCANCODE_D] = InputBinding::CONTROLLER_C;
		keyboard_bindings[SDL_SCANCODE_Q] = InputBinding::CONTROLLER_X;
		keyboard_bindings[SDL_SCANCODE_W] = InputBinding::CONTROLLER_Y;
		keyboard_bindings[SDL_SCANCODE_E] = InputBinding::CONTROLLER_Z;
		keyboard_bindings[SDL_SCANCODE_RETURN] = InputBinding::CONTROLLER_START;
		keyboard_bindings[SDL_SCANCODE_BACKSPACE] = InputBinding::CONTROLLER_MODE;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_PAUSE, nullptr)] = InputBinding::PAUSE;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F11, nullptr)] = InputBinding::TOGGLE_FULLSCREEN;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_TAB, nullptr)] = InputBinding::RESET;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F5, nullptr)] = InputBinding::QUICK_SAVE_STATE;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F9, nullptr)] = InputBinding::QUICK_LOAD_STATE;
		keyboard_bindings[SDL_SCANCODE_SPACE] = InputBinding::FAST_FORWARD;
		keyboard_bindings[SDL_SCANCODE_F] = InputBinding::FAST_FORWARD;
		keyboard_bindings[SDL_SCANCODE_R] = InputBinding::REWIND;
	}
	else
	{
		const auto &Callback = [&](const std::string_view &section, const std::string_view &name, const std::string_view &value)
		{
			const bool value_boolean = value == "on" || value == "true";
			const auto &value_integer = FileUtilities::StringToInteger<unsigned int>(value);

			if (section == "Miscellaneous")
			{
			#ifndef __EMSCRIPTEN__
				if (name == "fullscreen")
					fullscreen = value_boolean;
				else
			#endif
				if (name == "vsync")
					vsync = value_boolean;
				else if (name == "screen-scaling")
					screen_scaling = value_integer.has_value() ? static_cast<ScreenScaling>(*value_integer) : ScreenScaling::FIT;
				else if (name == "tall-interlace-mode-2")
					tall_double_resolution_mode = value_boolean;
				else if (name == "widescreen-tiles")
					widescreen_tiles = value_integer.value_or(0);
			#ifndef __EMSCRIPTEN__
				else if (name == "native-windows")
					native_windows = value_boolean;
			#endif
				else if (name == "rewinding")
					rewinding = value_boolean;
				else if (name == "low-pass-filter")
					low_pass_filter = value_boolean;
				else if (name == "cd-add-on")
					cd_add_on = value_boolean;
				else if (name == "input-protocol")
					input_protocol = static_cast<ControllerManager_Protocol>(value_integer.value_or(CONTROLLER_MANAGER_PROTOCOL_STANDARD));
				else if (name == "low-volume-distortion")
					ladder_effect = value_boolean;
				else if (name == "pal")
					pal_mode = value_boolean;
				else if (name == "japanese")
					domestic = value_boolean;
			}
			else if (section == "Keyboard Bindings")
			{
				const auto scancode_integer = FileUtilities::StringToInteger<unsigned int>(name);

				if (scancode_integer.has_value() && *scancode_integer < SDL_SCANCODE_COUNT)
				{
					const SDL_Scancode scancode = static_cast<SDL_Scancode>(*scancode_integer);

					const auto binding_index = FileUtilities::StringToInteger<unsigned int>(value);

					keyboard_bindings[scancode] = [&]()
					{
						if (binding_index.has_value())
						{
							// Legacy numerical input bindings.
							static constexpr auto input_bindings = std::to_array({
								InputBinding::NONE,
								InputBinding::CONTROLLER_UP,
								InputBinding::CONTROLLER_DOWN,
								InputBinding::CONTROLLER_LEFT,
								InputBinding::CONTROLLER_RIGHT,
								InputBinding::CONTROLLER_A,
								InputBinding::CONTROLLER_B,
								InputBinding::CONTROLLER_C,
								InputBinding::CONTROLLER_START,
								InputBinding::PAUSE,
								InputBinding::RESET,
								InputBinding::FAST_FORWARD,
								InputBinding::REWIND,
								InputBinding::QUICK_SAVE_STATE,
								InputBinding::QUICK_LOAD_STATE,
								InputBinding::TOGGLE_FULLSCREEN
							});

							if (*binding_index < std::size(input_bindings))
								return input_bindings[*binding_index];
						}
						else
						{
							if (value == "INPUT_BINDING_CONTROLLER_UP")
								return InputBinding::CONTROLLER_UP;
							else if (value == "INPUT_BINDING_CONTROLLER_DOWN")
								return InputBinding::CONTROLLER_DOWN;
							else if (value == "INPUT_BINDING_CONTROLLER_LEFT")
								return InputBinding::CONTROLLER_LEFT;
							else if (value == "INPUT_BINDING_CONTROLLER_RIGHT")
								return InputBinding::CONTROLLER_RIGHT;
							else if (value == "INPUT_BINDING_CONTROLLER_A")
								return InputBinding::CONTROLLER_A;
							else if (value == "INPUT_BINDING_CONTROLLER_B")
								return InputBinding::CONTROLLER_B;
							else if (value == "INPUT_BINDING_CONTROLLER_C")
								return InputBinding::CONTROLLER_C;
							else if (value == "INPUT_BINDING_CONTROLLER_X")
								return InputBinding::CONTROLLER_X;
							else if (value == "INPUT_BINDING_CONTROLLER_Y")
								return InputBinding::CONTROLLER_Y;
							else if (value == "INPUT_BINDING_CONTROLLER_Z")
								return InputBinding::CONTROLLER_Z;
							else if (value == "INPUT_BINDING_CONTROLLER_START")
								return InputBinding::CONTROLLER_START;
							else if (value == "INPUT_BINDING_CONTROLLER_MODE")
								return InputBinding::CONTROLLER_MODE;
							else if (value == "INPUT_BINDING_PAUSE")
								return InputBinding::PAUSE;
							else if (value == "INPUT_BINDING_RESET")
								return InputBinding::RESET;
							else if (value == "INPUT_BINDING_FAST_FORWARD")
								return InputBinding::FAST_FORWARD;
							else if (value == "INPUT_BINDING_REWIND")
								return InputBinding::REWIND;
							else if (value == "INPUT_BINDING_QUICK_SAVE_STATE")
								return InputBinding::QUICK_SAVE_STATE;
							else if (value == "INPUT_BINDING_QUICK_LOAD_STATE")
								return InputBinding::QUICK_LOAD_STATE;
							else if (value == "INPUT_BINDING_TOGGLE_FULLSCREEN")
								return InputBinding::TOGGLE_FULLSCREEN;
						}

						return InputBinding::NONE;
					}();
				}
			}
		#ifdef FILE_PATH_SUPPORT
			else if (section == "Recent Software")
			{
				static bool is_cd_file = false;

				if (name == "cd")
				{
					is_cd_file = value_boolean;
				}
				else if (name == "path")
				{
					if (!value.empty())
						AddToRecentSoftware(FileUtilities::U8Path(value), is_cd_file, true);
				}
			}
		#endif
			else if (section == "Window Positions")
			{
				auto value_stream = std::stringstream(std::string(value));

				int x, y;
				value_stream >> x;
				value_stream >> y;

				Window::positions[std::string(name)] = {x, y};
			}
			else if (section == "Window Sizes")
			{
				auto value_stream = std::stringstream(std::string(value));

				int x, y;
				value_stream >> x;
				value_stream >> y;

				Window::sizes[std::string(name)] = {x, y};
			}
			else if (section == "Maximised Windows")
			{
				if (value_boolean)
					Window::maximisations.emplace(std::string(name));
			}
		};

		INI::ProcessFile(file, Callback);
	}

	// Apply settings now that they have been decided.
	window->LoadPosition();
#ifndef __EMSCRIPTEN__
	window->SetFullscreen(fullscreen);
#endif
	window->SetVSync(vsync);
	emulator->SetWidescreenTiles(widescreen_tiles);
	emulator->SetRewindEnabled(rewinding);
	emulator->SetLowPassFilterEnabled(low_pass_filter);
	emulator->SetCDAddOnEnabled(cd_add_on);
	emulator->SetControllerProtocol(input_protocol);
	emulator->SetLadderEffectEnabled(ladder_effect);

	emulator->SetTVStandard(pal_mode ? CLOWNMDEMU_TV_STANDARD_PAL : CLOWNMDEMU_TV_STANDARD_NTSC);
	emulator->SetRegion(domestic ? CLOWNMDEMU_REGION_DOMESTIC : CLOWNMDEMU_REGION_OVERSEAS);
}

void Frontend::SaveConfiguration()
{
#ifdef SDL_PLATFORM_WIN32
#define ENDL "\r\n"
#else
#define ENDL "\n"
#endif
	// Save configuration file:
	SDL::IOStream file(GetConfigurationFilePath(), "w");

	if (!file)
	{
		debug_log.Log("Could not open configuration file for writing.");
	}
	else
	{
	#define PRINT_STRING(FILE, STRING) SDL_WriteIO(FILE, STRING, sizeof(STRING) - 1)
	#define PRINT_LINE(FILE, STRING) PRINT_STRING(FILE, STRING ENDL)
	#define PRINT_NEWLINE(FILE) PRINT_LINE(FILE, "")
	#define PRINT_HEADER(FILE, STRING) PRINT_LINE(FILE, "[" STRING "]")
	#define PRINT_KEY(FILE, KEY) PRINT_STRING(FILE, KEY " = ")
		const auto &PRINT_BOOLEAN_VALUE = [](SDL::IOStream &file, const bool variable)
		{
			if (variable)
				PRINT_LINE(file, "on");
			else
				PRINT_LINE(file, "off");
		};
	#define PRINT_INTEGER_VALUE(FILE, VARIABLE) SDL_IOprintf(FILE, "%d\n", VARIABLE)
	#define PRINT_BOOLEAN_OPTION(FILE, NAME, VARIABLE) \
		PRINT_KEY(FILE, NAME); \
		PRINT_BOOLEAN_VALUE(FILE, VARIABLE)
	#define PRINT_INTEGER_OPTION(FILE, NAME, VARIABLE) \
		PRINT_KEY(FILE, NAME); \
		PRINT_INTEGER_VALUE(FILE, VARIABLE)

		// Save settings.
		PRINT_HEADER(file, "Miscellaneous");

	#ifndef __EMSCRIPTEN__
		PRINT_BOOLEAN_OPTION(file, "fullscreen", window->GetFullscreen());
	#endif
		PRINT_BOOLEAN_OPTION(file, "vsync", window->GetVSync());
		PRINT_INTEGER_OPTION(file, "screen-scaling", static_cast<int>(screen_scaling));
		PRINT_BOOLEAN_OPTION(file, "tall-interlace-mode-2", tall_double_resolution_mode);
		PRINT_INTEGER_OPTION(file, "widescreen-tiles", emulator->GetWidescreenTiles());
	#ifndef __EMSCRIPTEN__
		PRINT_BOOLEAN_OPTION(file, "native-windows", native_windows);
	#endif
		PRINT_BOOLEAN_OPTION(file, "rewinding", emulator->GetRewindEnabled());
		PRINT_BOOLEAN_OPTION(file, "low-pass-filter", emulator->GetLowPassFilterEnabled());
		PRINT_BOOLEAN_OPTION(file, "cd-add-on", emulator->GetCDAddOnEnabled());
		PRINT_INTEGER_OPTION(file, "input-protocol", emulator->GetControllerProtocol());
		PRINT_BOOLEAN_OPTION(file, "low-volume-distortion", emulator->GetLadderEffectEnabled());
		PRINT_BOOLEAN_OPTION(file, "pal", emulator->GetTVStandard() == CLOWNMDEMU_TV_STANDARD_PAL);
		PRINT_BOOLEAN_OPTION(file, "japanese", emulator->GetRegion() == CLOWNMDEMU_REGION_DOMESTIC);
		PRINT_NEWLINE(file);

		// Save keyboard bindings.
		PRINT_HEADER(file, "Keyboard Bindings");

		for (std::size_t i = 0; i != std::size(keyboard_bindings); ++i)
		{
			const auto &keyboard_binding = keyboard_bindings[i];

			if (keyboard_binding != InputBinding::NONE)
			{
				const auto &binding_string = [&]()
				{
					switch (keyboard_binding)
					{
						case InputBinding::NONE:
							return "INPUT_BINDING_NONE";

						case InputBinding::CONTROLLER_UP:
							return "INPUT_BINDING_CONTROLLER_UP";

						case InputBinding::CONTROLLER_DOWN:
							return "INPUT_BINDING_CONTROLLER_DOWN";

						case InputBinding::CONTROLLER_LEFT:
							return "INPUT_BINDING_CONTROLLER_LEFT";

						case InputBinding::CONTROLLER_RIGHT:
							return "INPUT_BINDING_CONTROLLER_RIGHT";

						case InputBinding::CONTROLLER_A:
							return "INPUT_BINDING_CONTROLLER_A";

						case InputBinding::CONTROLLER_B:
							return "INPUT_BINDING_CONTROLLER_B";

						case InputBinding::CONTROLLER_C:
							return "INPUT_BINDING_CONTROLLER_C";

						case InputBinding::CONTROLLER_X:
							return "INPUT_BINDING_CONTROLLER_X";

						case InputBinding::CONTROLLER_Y:
							return "INPUT_BINDING_CONTROLLER_Y";

						case InputBinding::CONTROLLER_Z:
							return "INPUT_BINDING_CONTROLLER_Z";

						case InputBinding::CONTROLLER_START:
							return "INPUT_BINDING_CONTROLLER_START";

						case InputBinding::CONTROLLER_MODE:
							return "INPUT_BINDING_CONTROLLER_MODE";

						case InputBinding::PAUSE:
							return "INPUT_BINDING_PAUSE";

						case InputBinding::RESET:
							return "INPUT_BINDING_RESET";

						case InputBinding::FAST_FORWARD:
							return "INPUT_BINDING_FAST_FORWARD";

						case InputBinding::REWIND:
							return "INPUT_BINDING_REWIND";

						case InputBinding::QUICK_SAVE_STATE:
							return "INPUT_BINDING_QUICK_SAVE_STATE";

						case InputBinding::QUICK_LOAD_STATE:
							return "INPUT_BINDING_QUICK_LOAD_STATE";

						case InputBinding::TOGGLE_FULLSCREEN:
							return "INPUT_BINDING_TOGGLE_FULLSCREEN";

						case InputBinding::TOTAL:
							SDL_assert(false);
							break;
					}

					return "INPUT_BINDING_NONE";
				}();

				const std::string buffer = fmt::format("{} = {}" ENDL, i, binding_string);
				SDL_WriteIO(file, buffer.data(), buffer.size());
			}
		}

	#ifdef FILE_PATH_SUPPORT
		PRINT_NEWLINE(file);

		// Save recent software paths.
		PRINT_HEADER(file, "Recent Software");

		for (const auto &recent_software : recent_software_list)
		{
			PRINT_KEY(file, "cd");
			if (recent_software.is_cd_file)
				PRINT_LINE(file, "true");
			else
				PRINT_LINE(file, "false");

			PRINT_KEY(file, "path");
			FileUtilities::PathToStringView(recent_software.path,
				[&](const std::string_view &string_view)
				{
					SDL_WriteIO(file, std::data(string_view), std::size(string_view));
				}
			);
			PRINT_NEWLINE(file);
		}
	#endif

		PRINT_NEWLINE(file);

		// Save window positions.
		PRINT_HEADER(file, "Window Positions");

		for (const auto &position : Window::positions)
		{
			const std::string buffer = fmt::format("{} = {} {}" ENDL, position.first, position.second.first, position.second.second);
			SDL_WriteIO(file, buffer.data(), buffer.size());
		}

		PRINT_NEWLINE(file);

		// Save window sizes.
		PRINT_HEADER(file, "Window Sizes");

		for (const auto &size : Window::sizes)
		{
			const std::string buffer = fmt::format("{} = {} {}" ENDL, size.first, size.second.first, size.second.second);
			SDL_WriteIO(file, buffer.data(), buffer.size());
		}

		PRINT_NEWLINE(file);

		// Save maximised windows.
		PRINT_HEADER(file, "Maximised Windows");

		for (const auto &maximised : Window::maximisations)
		{
			const std::string buffer = fmt::format("{} = true" ENDL, maximised);
			SDL_WriteIO(file, buffer.data(), buffer.size());
		}
	}
#undef ENDL
}


///////////////////
// Main function //
///////////////////

void Frontend::PreEventStuff()
{
	emulator_on = emulator->IsCartridgeInserted() || emulator->IsCDInserted();

	emulator_frame_advance = false;
}

Frontend::Frontend(const EmulatorInstance::FramerateCallback &framerate_callback, const bool fullscreen, const std::filesystem::path &user_data_path, const std::filesystem::path &cartridge_path, const std::filesystem::path &cd_path)
{
	forced_fullscreen = fullscreen;

	IMGUI_CHECKVERSION();

	InitialiseConfigurationDirectoryPath(user_data_path);

	window.emplace(DEFAULT_TITLE, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, true, std::nullopt, SDL_WINDOW_FILL_DOCUMENT);
	emulator.emplace(window->framebuffer_texture, ReadInputCallback,
		[this](const std::string &title)
		{
			// Use the default title if the ROM does not provide a name.
			// Micro Machines is one game that lacks a name.
			SDL_SetWindowTitle(window->GetSDLWindow(), title.empty() ? DEFAULT_TITLE : title.c_str());
		},
		framerate_callback
	);

	LoadConfiguration();

	FileUtilities::PathToCString(GetDearImGuiSettingsFilePath(),
		[&](const char* const string)
		{
			ImGui::LoadIniSettingsFromDisk(string);
		}
	);

	if (fullscreen)
		window->SetFullscreen(true);

	// We shouldn't resize the window if something is overriding its size.
	// This is needed for the Emscripen build to work correctly in a full-window HTML canvas.
	int window_width, window_height;
	SDL_GetWindowSize(window->GetSDLWindow(), &window_width, &window_height);
	const float scale = window->GetSizeScale();

	if (window_width == static_cast<int>(INITIAL_WINDOW_WIDTH * scale) && window_height == static_cast<int>(INITIAL_WINDOW_HEIGHT * scale))
	{
		// Resize the window so that there's room for the menu bar.
		// Also adjust for widescreen if the user has the option enabled.
		const auto desired_width = ((VDP_H40_SCREEN_WIDTH_IN_TILES + emulator->GetWidescreenTiles() * 2) * VDP_TILE_WIDTH) * INITIAL_WINDOW_SCALE;

		SDL_SetWindowSize(window->GetSDLWindow(), static_cast<int>(desired_width * scale), static_cast<int>((INITIAL_WINDOW_HEIGHT + window->GetMenuBarSize()) * scale));
	}

	// If the user passed the path to the software on the command line, then load it here, automatically.
	if (!cartridge_path.empty())
		LoadCartridgeFile(cartridge_path);
	if (!cd_path.empty())
		LoadCDFile(cd_path);

	// We are now ready to show the window
	SDL_ShowWindow(window->GetSDLWindow());

	debug_log.ForceConsoleOutput(false);

	PreEventStuff();
}

Frontend::~Frontend()
{
	debug_log.ForceConsoleOutput(true);

	// Destroy windows BEFORE we shut-down SDL.
	std::apply(
		[]<typename... Ts>(const Ts&... windows)
		{
			(windows->reset(), ...);
		}, popup_windows
	);

	WriteSaveData();
}

void Frontend::WriteSaveData()
{
	FileUtilities::PathToCString(GetDearImGuiSettingsFilePath(),
		[&](const char* const string)
		{
			ImGui::SaveIniSettingsToDisk(string);
		}
	);

	SaveConfiguration();

	emulator->SaveCartridgeSaveData();
}

void Frontend::HandleMainWindowEvent(const SDL_Event &event)
{
	window->MakeDearImGuiContextCurrent();

	ImGuiIO &io = ImGui::GetIO();

	bool give_event_to_imgui = true;
	static bool anything_pressed_during_alt;

	// Process the event
	switch (event.type)
	{
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		case SDL_EVENT_QUIT:
			quit = true;
			break;

		case SDL_EVENT_KEY_DOWN:
			// Ignore repeated key inputs caused by holding the key down
			if (event.key.repeat)
				break;

			anything_pressed_during_alt = true;

			// Ignore CTRL+TAB (used by Dear ImGui for cycling between windows).
			if (event.key.key == SDLK_TAB && (SDL_GetModState() & SDL_KMOD_CTRL) != 0)
				break;

			if (event.key.key == SDLK_RETURN && (SDL_GetModState() & SDL_KMOD_ALT) != 0)
			{
				window->ToggleFullscreen();
				break;
			}

			if (event.key.key == SDLK_ESCAPE)
			{
				// Exit fullscreen
				window->SetFullscreen(false);
			}

			// Prevent invalid memory accesses due to future API expansions.
			// TODO: Yet another reason to not use `SDL_SCANCODE_COUNT`.
			if (event.key.scancode >= keyboard_bindings.size())
				break;

			switch (keyboard_bindings[event.key.scancode])
			{
				case InputBinding::TOGGLE_FULLSCREEN:
					window->ToggleFullscreen();
					break;

				default:
					break;
			}

			// Many inputs should not be acted upon while the emulator is not running.
			if (emulator_on && emulator_has_focus)
			{
				switch (keyboard_bindings[event.key.scancode])
				{
					case InputBinding::PAUSE:
						emulator->SetPaused(!emulator->IsPaused());
						break;

					case InputBinding::RESET:
						// Soft-reset console
						emulator->SoftReset();
						emulator->SetPaused(false);
						break;

					case InputBinding::QUICK_SAVE_STATE:
						// Save save state
						quick_save_state.emplace(*emulator);
						break;

					case InputBinding::QUICK_LOAD_STATE:
						// Load save state
						if (quick_save_state)
						{
							quick_save_state->Apply(*emulator);
							emulator->SetPaused(false);
						}

						break;

					default:
						break;
				}
			}
			[[fallthrough]];
		case SDL_EVENT_KEY_UP:
		{
			// Prevent invalid memory accesses due to future API expansions.
			// TODO: Yet another reason to not use `SDL_SCANCODE_COUNT`.
			if (event.key.scancode >= keyboard_bindings.size())
				break;

			// When a key-down is processed, cache the binding so that the corresponding key-up
			// affects the same input. This is to prevent phantom inputs when a key is unbinded
			// whilst it is being held.
			const InputBinding binding = (event.type == SDL_EVENT_KEY_UP ? keyboard_bindings_cached : keyboard_bindings)[event.key.scancode];
			keyboard_bindings_cached[event.key.scancode] = keyboard_bindings[event.key.scancode];

			const bool pressed = event.key.down;

			if (key_pressed[event.key.scancode] != pressed)
			{
				key_pressed[event.key.scancode] = pressed;

				// This chunk of code prevents ALT-ENTER from causing ImGui to enter the menu bar.
				// TODO: Remove this when Dear ImGui stops being dumb.
				if (event.key.scancode == SDL_SCANCODE_LALT)
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
					#define DO_KEY(state, code) case code: keyboard_input.SetButton(state, keyboard_input.GetButton(state) + delta); break

					DO_KEY(CLOWNMDEMU_BUTTON_UP, InputBinding::CONTROLLER_UP);
					DO_KEY(CLOWNMDEMU_BUTTON_DOWN, InputBinding::CONTROLLER_DOWN);
					DO_KEY(CLOWNMDEMU_BUTTON_LEFT, InputBinding::CONTROLLER_LEFT);
					DO_KEY(CLOWNMDEMU_BUTTON_RIGHT, InputBinding::CONTROLLER_RIGHT);
					DO_KEY(CLOWNMDEMU_BUTTON_A, InputBinding::CONTROLLER_A);
					DO_KEY(CLOWNMDEMU_BUTTON_B, InputBinding::CONTROLLER_B);
					DO_KEY(CLOWNMDEMU_BUTTON_C, InputBinding::CONTROLLER_C);
					DO_KEY(CLOWNMDEMU_BUTTON_X, InputBinding::CONTROLLER_X);
					DO_KEY(CLOWNMDEMU_BUTTON_Y, InputBinding::CONTROLLER_Y);
					DO_KEY(CLOWNMDEMU_BUTTON_Z, InputBinding::CONTROLLER_Z);
					DO_KEY(CLOWNMDEMU_BUTTON_START, InputBinding::CONTROLLER_START);
					DO_KEY(CLOWNMDEMU_BUTTON_MODE, InputBinding::CONTROLLER_MODE);

					#undef DO_KEY

					case InputBinding::FAST_FORWARD:
						// Toggle fast-forward
						keyboard_input.fast_forward += delta;

						if ((!emulator->IsPaused() && emulator_has_focus) || !pressed)
							UpdateFastForwardStatus();

						if (pressed && emulator->IsPaused())
							emulator_frame_advance = true;

						break;

					case InputBinding::REWIND:
						keyboard_input.rewind += delta;

						if (emulator_has_focus || !pressed)
							UpdateRewindStatus();

						break;

					default:
						break;
				}
			}

			break;
		}

		case SDL_EVENT_GAMEPAD_ADDED:
		{
			// Open the controller, and create an entry for it in the controller list.
			SDL_Gamepad *controller = SDL_OpenGamepad(event.gdevice.which);

			if (controller == nullptr)
			{
				debug_log.SDLError("SDL_OpenGamepad");
			}
			else
			{
				try
				{
					controller_input_list.emplace_back(event.gdevice.which);
				}
				catch (const std::bad_alloc&)
				{
					debug_log.Log("Could not allocate memory for the new ControllerInput struct");
					SDL_CloseGamepad(controller);
				}
			}

			break;
		}

		case SDL_EVENT_GAMEPAD_REMOVED:
		{
			// Close the controller, and remove it from the controller list.
			SDL_Gamepad *controller = SDL_GetGamepadFromID(event.gdevice.which);

			if (controller == nullptr)
				debug_log.SDLError("SDL_GetGamepadFromID");
			else
				SDL_CloseGamepad(controller);

			controller_input_list.remove_if(
				[&event](const ControllerInput &controller_input)
				{
					if (controller_input.joystick_instance_id != event.gdevice.which)
						return false;

					// Remove controller from input bindings.
					for (auto &bound_input : bound_inputs)
						if (bound_input == &controller_input.input)
							bound_input = nullptr;

					return true;
				}
			);

			break;
		}

		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			if (event.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK)
			{
				// Toggle pause.
				emulator->SetPaused(!emulator->IsPaused());
			}

			// Don't use inputs that are for Dear ImGui.
			if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0)
				break;

			[[fallthrough]];
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
		{
			const bool pressed = event.gbutton.down;

			// Look for the controller that this event belongs to.
			for (auto &controller_input : controller_input_list)
			{
				// Check if the current controller is the one that matches this event.
				if (controller_input.joystick_instance_id == event.gbutton.which)
				{
					switch (event.gbutton.button)
					{
						#define DO_BUTTON(state, code) case code: controller_input.input.SetButton(state, pressed); break

						// TODO: This is currently just Genesis Plus GX's libretro button layout,
						// but I would like something closer to a real 6-button controller's layout.
						DO_BUTTON(CLOWNMDEMU_BUTTON_A    , SDL_GAMEPAD_BUTTON_WEST          );
						DO_BUTTON(CLOWNMDEMU_BUTTON_B    , SDL_GAMEPAD_BUTTON_SOUTH         );
						DO_BUTTON(CLOWNMDEMU_BUTTON_C    , SDL_GAMEPAD_BUTTON_EAST          );
						DO_BUTTON(CLOWNMDEMU_BUTTON_X    , SDL_GAMEPAD_BUTTON_LEFT_SHOULDER );
						DO_BUTTON(CLOWNMDEMU_BUTTON_Y    , SDL_GAMEPAD_BUTTON_NORTH         );
						DO_BUTTON(CLOWNMDEMU_BUTTON_Z    , SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
						DO_BUTTON(CLOWNMDEMU_BUTTON_START, SDL_GAMEPAD_BUTTON_START         );
						DO_BUTTON(CLOWNMDEMU_BUTTON_MODE , SDL_GAMEPAD_BUTTON_BACK          );

						#undef DO_BUTTON

						case SDL_GAMEPAD_BUTTON_DPAD_UP:
						case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
						case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
						case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
						{
							unsigned int direction;
							ClownMDEmu_Button button;

							switch (event.gbutton.button)
							{
								default:
								case SDL_GAMEPAD_BUTTON_DPAD_UP:
									direction = 0;
									button = CLOWNMDEMU_BUTTON_UP;
									break;

								case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
									direction = 1;
									button = CLOWNMDEMU_BUTTON_DOWN;
									break;

								case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
									direction = 2;
									button = CLOWNMDEMU_BUTTON_LEFT;
									break;

								case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
									direction = 3;
									button = CLOWNMDEMU_BUTTON_RIGHT;
									break;
							}

							controller_input.dpad[direction] = pressed;

							// Combine D-pad and left stick values into final joypad D-pad inputs.
							controller_input.input.SetButton(button, controller_input.left_stick[direction] || controller_input.dpad[direction]);

							break;
						}

						default:
							break;
					}

					break;
				}
			}

			break;
		}

		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
			// Look for the controller that this event belongs to.
			for (auto &controller_input : controller_input_list)
			{
				// Check if the current controller is the one that matches this event.
				if (controller_input.joystick_instance_id == event.gaxis.which)
				{
					switch (event.gaxis.axis)
					{
						case SDL_GAMEPAD_AXIS_LEFTX:
						case SDL_GAMEPAD_AXIS_LEFTY:
						{
							if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX)
								controller_input.left_stick_x = event.gaxis.value;
							else //if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)
								controller_input.left_stick_y = event.gaxis.value;

							// Now that we have the left stick's X and Y values, let's do some trigonometry to figure out which direction(s) it's pointing in.

							// To start with, let's treat the X and Y values as a vector, and turn it into a unit vector.
							const float magnitude = std::sqrt(static_cast<float>(controller_input.left_stick_x * controller_input.left_stick_x + controller_input.left_stick_y * controller_input.left_stick_y));

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
									const float delta_angle = std::acos(left_stick_x_unit * directions[i][0] + left_stick_y_unit * directions[i][1]);

									// If the stick is within 67.5 degrees of the specified direction, then this will be true.
									controller_input.left_stick[i] = (delta_angle < CC_DEGREE_TO_RADIAN(360.0f * 3.0f / 8.0f / 2.0f)); // Half of 3/8 of 360 degrees (in radians)
								}

								static const std::array<ClownMDEmu_Button, 4> buttons = {
									CLOWNMDEMU_BUTTON_UP,
									CLOWNMDEMU_BUTTON_DOWN,
									CLOWNMDEMU_BUTTON_LEFT,
									CLOWNMDEMU_BUTTON_RIGHT
								};

								// Combine D-pad and left stick values into final joypad D-pad inputs.
								controller_input.input.SetButton(buttons[i], controller_input.left_stick[i] || controller_input.dpad[i]);
							}

							break;
						}

						case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
						case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
						{
							if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
							{
								const bool held = event.gaxis.value > 0x7FFF / 8;

								if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
								{
									if (controller_input.left_trigger != held)
									{
										controller_input.input.rewind = held;
										UpdateRewindStatus();
									}

									controller_input.left_trigger = held;
								}
								else
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

		case SDL_EVENT_DROP_FILE:
			drag_and_drop_filename = FileUtilities::U8Path(event.drop.data);
			break;

		default:
			break;
	}

	if (give_event_to_imgui)
		ImGui_ImplSDL3_ProcessEvent(&event);
}

void Frontend::HandleEvent(const SDL_Event &event)
{
	const auto &FilterWindowStateEvents = [&](const char* const window_title)
	{
		switch (event.type)
		{
			case SDL_EVENT_WINDOW_MOVED:
				if (!Window::maximisations.contains(window_title))
					Window::positions[window_title] = {event.window.data1, event.window.data2};
				break;

			case SDL_EVENT_WINDOW_RESIZED:
				if (!Window::maximisations.contains(window_title))
					Window::sizes[window_title] = {event.window.data1, event.window.data2};
				break;

			case SDL_EVENT_WINDOW_MAXIMIZED:
				Window::maximisations.emplace(window_title);
				break;

			case SDL_EVENT_WINDOW_MINIMIZED:
			case SDL_EVENT_WINDOW_RESTORED:
				Window::maximisations.erase(window_title);
				break;
		}
	};

	SDL_Window* const event_window = SDL_GetWindowFromEvent(&event);

	if (event_window == nullptr || event_window == window->GetSDLWindow())
	{
		FilterWindowStateEvents("ClownMDEmu");
		HandleMainWindowEvent(event);
	}
	else
	{
		const auto DoWindow = [&]<typename T>(std::optional<T> &popup_window)
		{
			if (popup_window.has_value() && popup_window->IsWindow(event_window))
			{
				if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
					popup_window.reset();
				else
					popup_window->ProcessEvent(event);
			}
		};

		FilterWindowStateEvents(SDL_GetWindowTitle(event_window));
		std::apply(
			[&]<typename... Ts>(Ts const&... windows)
			{
				(DoWindow(*windows), ...);
			}, popup_windows
		);
	}
}

void Frontend::DrawStatusIndicator(const ImVec2 &display_position, const ImVec2 &display_size)
{
	if (emulator->IsPaused() || emulator->rewinding || emulator->IsFastForwarding())
	{
		// A bunch of utility junk.
		const auto DrawOutlinedTriangle = [](ImDrawList* const draw_list, const ImVec2 &position, const float radius, const float outline_thickness, const unsigned int degree)
		{
			const auto DrawTriangle = [&](const ImVec2 &position, const unsigned int degree, const ImU32 col, const float radius)
			{
				const auto DegreeToPoint = [&](const unsigned int degree)
				{
					const auto x = +std::sin(CC_DEGREE_TO_RADIAN(degree));
					const auto y = -std::cos(CC_DEGREE_TO_RADIAN(degree));
					return ImVec2(x, y) * radius;
				};

				draw_list->AddTriangleFilled(position + DegreeToPoint(degree + 120 * 0), position + DegreeToPoint(degree + 120 * 1), position + DegreeToPoint(degree + 120 * 2), col);
			};

			DrawTriangle(position, degree, IM_COL32_BLACK, radius);
			// The cosine trick is to make the outline thickness match that of a rectangle.
			DrawTriangle(position, degree, IM_COL32_WHITE, radius - outline_thickness / std::cos(CC_DEGREE_TO_RADIAN(180 / 3)));
		};

		const auto DrawOutlinedRectangle = [](ImDrawList* const draw_list, const ImVec2 &position, const ImVec2 &radius, const float outline_thickness)
		{
			const auto DrawRectangle = [&](const ImVec2 &position, const ImVec2 &radius, const ImU32 colour)
			{
				draw_list->AddRectFilled(position - radius, position + radius, colour);
			};

			DrawRectangle(position, radius, IM_COL32_BLACK);
			DrawRectangle(position, radius - ImVec2(outline_thickness, outline_thickness), IM_COL32_WHITE);
		};

		const auto DrawCross = [](ImDrawList* const draw_list, const ImVec2 &position, const ImVec2 &radius, const float outline_thickness)
		{
			const auto DoLine = [&](const ImVec2 &first_scale, const ImVec2 &second_scale)
			{
				const auto DoCoordinate = [&](const ImVec2 &scale)
				{
					return position + radius * scale;
				};

				draw_list->AddLine(DoCoordinate(first_scale), DoCoordinate(second_scale), IM_COL32(0xFF, 0, 0, 0xFF), outline_thickness);
			};

			DoLine({-1, -1}, { 1,  1});
			DoLine({ 1, -1}, {-1,  1});
		};

		const auto DrawBar = [](ImDrawList* const draw_list, const ImVec2 &position, const float radius, const float outline_thickness, const float progress)
		{
			const auto &bar_left_position = position + ImVec2(-radius, radius);
			const auto &bar_right_position = bar_left_position + ImVec2(radius * 2, outline_thickness * 3);
			const ImVec2 outline_thickness_v(outline_thickness, outline_thickness);
			const auto &bar_inside_left_position = bar_left_position + outline_thickness_v;
			const auto &bar_inside_right_position = bar_right_position - outline_thickness_v;
			const auto &bar_inside_middle_position_x = std::lerp(bar_inside_left_position.x, bar_inside_right_position.x, progress);

			draw_list->AddRectFilled(bar_left_position, bar_right_position, IM_COL32_BLACK);
			draw_list->AddRectFilled(bar_inside_left_position, ImVec2(bar_inside_middle_position_x, bar_inside_right_position.y), IM_COL32_WHITE);
			draw_list->AddRectFilled(ImVec2(bar_inside_middle_position_x, bar_inside_left_position.y), bar_inside_right_position, IM_COL32(0x7F, 0x7F, 0x7F, 0xFF));
		};

		// The actual logic for drawing the symbols.
		const auto radius = 20.0f * window->GetDPIScale();
		const auto outline_thickness = radius / 6;

		const auto position = display_position + ImVec2(display_size.x - radius * 2, radius * 2);
		const auto offset = ImVec2(radius / 2, 0);
		const auto &left_position = position - offset;
		const auto &right_position = position + offset;

		ImDrawList* const draw_list = ImGui::GetWindowDrawList();

		if (emulator->IsPaused())
		{
			// Paused symbol.
			const ImVec2 size(radius * 2 / 5, radius);
			DrawOutlinedRectangle(draw_list, left_position, size, outline_thickness);
			DrawOutlinedRectangle(draw_list, right_position, size, outline_thickness);
		}
		else if (emulator->rewinding)
		{
			// Rewinding symbol.
			const auto angle = 270;
			DrawOutlinedTriangle(draw_list, left_position, radius, outline_thickness, angle);
			DrawOutlinedTriangle(draw_list, right_position, radius, outline_thickness, angle);

			// Show how much of the rewind buffer remains.
			DrawBar(draw_list, position, radius, outline_thickness, emulator->GetRewindAmount());

			// Cross-out the symbol when exhausted.
			if (emulator->IsRewindExhausted())
				DrawCross(draw_list, position, ImVec2(radius, radius), outline_thickness);
		}
		else if (emulator->IsFastForwarding())
		{
			// Fast-forwarding symbol.
			const auto angle = 90;
			DrawOutlinedTriangle(draw_list, right_position, radius, outline_thickness, angle);
			DrawOutlinedTriangle(draw_list, left_position, radius, outline_thickness, angle);
		}
	}
}

void Frontend::Update()
{
	if (emulator_on && (!emulator->IsPaused() || emulator_frame_advance) && !file_utilities.IsDialogOpen() && (!emulator->rewinding || !emulator->IsRewindExhausted()))
	{
		emulator->Update();
		++frame_counter;
	}

	window->StartDearImGuiFrame();

	// Handle drag-and-drop event.
	if (!file_utilities.IsDialogOpen() && !drag_and_drop_filename.empty())
	{
		if (CDReader::IsDefinitelyACD(drag_and_drop_filename))
		{
			LoadCDFile(drag_and_drop_filename, SDL::IOStream(drag_and_drop_filename, "rb"));
			emulator->SetPaused(false);
		}
		else if (emulator->ValidateSaveStateFile(drag_and_drop_filename))
		{
			if (emulator_on)
				LoadSaveState(drag_and_drop_filename);
		}
		else if (LoadCartridgeFile(drag_and_drop_filename))
		{
			emulator->SetPaused(false);
		}

		drag_and_drop_filename.clear();
	}

#ifndef NDEBUG
	if (dear_imgui_demo_window)
		ImGui::ShowDemoWindow(&dear_imgui_demo_window);
#endif

	const ImGuiViewport *viewport = ImGui::GetMainViewport();

	// Maximise the window if needed.
	if (!screen_subwindow)
	{
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
	}

	// Prevent the window from getting too small or we'll get division by zero errors later on.
	const auto window_minimum_dimension = 100.0f * window->GetDPIScale();
	ImGui::SetNextWindowSizeConstraints(ImVec2(window_minimum_dimension, window_minimum_dimension), ImVec2(FLT_MAX, FLT_MAX)); // Width > 100, Height > 100

	const auto AnyPopupsOpen = []()
	{
		return std::apply(
			[]<typename... Ts>(const Ts&... windows)
			{
				return (windows->has_value() || ...);
			}, popup_windows
		);
	};

	auto &io = ImGui::GetIO();
	io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
	if (!emulator_on || emulator->IsPaused())
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	const bool show_menu_bar = !ShouldBeInFullscreenMode()
	                         || screen_subwindow
	                         || AnyPopupsOpen()
	                         || (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0;

	// Hide mouse when the user just wants a fullscreen display window
	if (!show_menu_bar)
		ImGui::SetMouseCursor(ImGuiMouseCursor_None);

	// We don't want the emulator window overlapping the others while it's maximised.
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBringToFrontOnFocus;

	// Hide as much of the window as possible when maximised.
	if (!screen_subwindow)
		window_flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

	// Hide the menu bar when maximised in fullscreen.
	if (show_menu_bar)
		window_flags |= ImGuiWindowFlags_MenuBar;

	// Show black borders around emulator display.
	if (emulator_on)
		window_flags |= ImGuiWindowFlags_NoBackground;

	// Tweak the style so that the screen fills the window.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	const bool not_collapsed = ImGui::Begin("Screen", nullptr, window_flags);
	ImGui::PopStyleVar();

	if (not_collapsed)
	{
		if (show_menu_bar && ImGui::BeginMenuBar())
		{
			const auto PopupButton = [this]<typename T, typename... Ts>(const char* const label, std::optional<T> &window, const char* const title = nullptr, Ts &&...args)
			{
				if (ImGui::MenuItem(label, nullptr, window.has_value()))
				{
					if (window.has_value())
						window.reset();
					else
						window.emplace(title == nullptr ? label : title, *this->window, NativeWindowsActive() ? nullptr : &*this->window, std::forward<Ts>(args)...);
				}
			};

			if (ImGui::BeginMenu("Software"))
			{
				if (ImGui::MenuItem("Load Cartridge File..."))
				{
					file_utilities.LoadFile(*window, "Load Cartridge File", nullptr, {{{"Mega Drive Cartridge Software", "bin;md;gen;zip"}}}, [this](const std::filesystem::path &path, SDL::IOStream &&file)
					{
						const bool success = LoadCartridgeFile(path, file);

						if (success)
							emulator->SetPaused(false);

						return success;
					});
				}

				if (ImGui::MenuItem("Unload Cartridge File", nullptr, false, emulator->IsCartridgeInserted()))
				{
					emulator->UnloadCartridgeFile();

					emulator->SetPaused(false);
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Load CD File..."))
				{
					file_utilities.LoadFile(*window, "Load CD File", nullptr, {{{"Mega CD Disc Software", "bin;cue;iso;chd"}}}, [this](const std::filesystem::path &path, SDL::IOStream &&file)
					{
						if (!LoadCDFile(path, std::move(file)))
							return false;

						emulator->SetPaused(false);
						return true;
					});
				}

				if (ImGui::MenuItem("Unload CD File", nullptr, false, emulator->IsCDInserted()))
				{
					emulator->UnloadCDFile();

					emulator->SetPaused(false);
				}

				ImGui::Separator();

				PopupButton("Cheats", cheats_window, nullptr, std::make_pair(360, 214));

				ImGui::Separator();

				bool paused = emulator->IsPaused();
				if (ImGui::MenuItem("Pause", nullptr, &paused, emulator_on))
					emulator->SetPaused(paused);

				if (ImGui::MenuItem("Reset", nullptr, false, emulator_on))
				{
					emulator->SoftReset();
					emulator->SetPaused(false);
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
						ImGui::PushID(&recent_software);

						// Display only the filename.
						FileUtilities::PathToCString(recent_software.path.filename(),
							[&](const char* const string)
							{
								if (ImGui::MenuItem(string))
									selected_software = &recent_software;
							}
						);

						ImGui::PopID();

						// Show the full path as a tooltip.
						DoToolTip(recent_software.path);
					}

					if (selected_software != nullptr && LoadSoftwareFile(selected_software->is_cd_file, selected_software->path))
						emulator->SetPaused(false);
				}
			#endif

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Save States"))
			{
				if (ImGui::MenuItem("Quick Save", nullptr, false, emulator_on))
				{
					quick_save_state.emplace(*emulator);
				}

				if (ImGui::MenuItem("Quick Load", nullptr, false, emulator_on && quick_save_state))
				{
					quick_save_state->Apply(*emulator);

					emulator->SetPaused(false);
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Save to File...", nullptr, false, emulator_on))
				{
				#ifdef FILE_PATH_SUPPORT
					file_utilities.CreateSaveFileDialog(*window, "Create Save State", "state.bin", {}, [this](const std::filesystem::path &path)
					{
							return SaveState(path);
					});
				#else
					file_utilities.SaveFile(*window, "Create Save State", "state.bin", {}, [](const FileUtilities::SaveFileInnerCallback &callback)
					{
						// Inefficient, but it's the only way...
						try
						{
							std::vector<unsigned char> save_state_buffer;
							save_state_buffer.resize(emulator->GetSaveStateFileSize());

							SDL::IOStream file(save_state_buffer.data(), save_state_buffer.size());

							if (file)
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
					file_utilities.LoadFile(*window, "Load Save State", nullptr, {}, [this]([[maybe_unused]] const std::filesystem::path &path, SDL::IOStream &&file)
					{
						LoadSaveState(file);
						return true;
					});

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Debugging"))
			{
				const auto &clownmdemu = emulator->GetState();
				const auto &vdp = emulator->GetVDPState();

				PopupButton("Log", debug_log_window, nullptr, std::make_pair(800, 600));

				PopupButton("Toggles", debugging_toggles_window);

				PopupButton("Disassembler", disassembler_window, nullptr, std::make_pair(380, 410));

				ImGui::Separator();

				PopupButton("Frontend", debug_frontend_window, nullptr, std::make_pair(400, 210));

				if (ImGui::BeginMenu("Main-68000"))
				{
					PopupButton("Registers", m68k_status_window, "Main-68000 Registers");
					PopupButton("WORK-RAM", m68k_ram_viewer_window, nullptr, clownmdemu.m68k.ram, window->monospace_font);
					PopupButton("External RAM", external_ram_viewer_window, nullptr, clownmdemu.external_ram.buffer, window->monospace_font);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Sub-68000"))
				{
					PopupButton("Registers", mcd_m68k_status_window, "Sub-68000 Registers");
					PopupButton("PRG-RAM", prg_ram_viewer_window, nullptr, clownmdemu.mega_cd.prg_ram.buffer, window->monospace_font);
					PopupButton("WORD-RAM", word_ram_viewer_window, nullptr, clownmdemu.mega_cd.word_ram.buffer, window->monospace_font);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Z80"))
				{
					PopupButton("Registers", z80_status_window, "Z80 Registers");
					PopupButton("SOUND-RAM", z80_ram_viewer_window, nullptr, clownmdemu.z80.ram, window->monospace_font);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("VDP"))
				{
					PopupButton("Registers", vdp_registers_window, "VDP Registers");
					PopupButton("Sprites", sprite_list_window, nullptr, std::make_pair(1000, 344));
					PopupButton("VRAM", vram_viewer_window, nullptr, vdp.vram, window->monospace_font);
					PopupButton("CRAM", cram_viewer_window, nullptr, vdp.cram, window->monospace_font);
					PopupButton("VSRAM", vsram_viewer_window, nullptr, vdp.vsram, window->monospace_font);
					ImGui::SeparatorText("Visualisers");
					ImGui::PushID("Visualisers");
					PopupButton("Sprite Plane", sprite_plane_visualiser_window, nullptr, std::make_pair(730, 700));
					PopupButton("Window Plane", window_plane_visualiser_window, nullptr, std::make_pair(1100, 600));
					PopupButton("Plane A", plane_a_visualiser_window, nullptr, std::make_pair(1100, 600));
					PopupButton("Plane B", plane_b_visualiser_window, nullptr, std::make_pair(1100, 600));
					PopupButton("Tiles", tile_visualiser_window, nullptr, 8);
					PopupButton("Colours", colour_visualiser_window);
					PopupButton("Stamps", stamp_visualiser_window, nullptr, 16);
					PopupButton("Stamp Map", stamp_map_visualiser_window, nullptr, std::make_pair(1100, 600));
					ImGui::PopID();
					ImGui::EndMenu();
				}

				PopupButton("FM", fm_status_window, "FM Registers");

				PopupButton("PSG", psg_status_window, "PSG Registers");

				if (ImGui::BeginMenu("PCM"))
				{
					PopupButton("Registers", pcm_status_window, "PCM Registers");
					PopupButton("WAVE-RAM", wave_ram_viewer_window, nullptr, emulator->GetPCMState().wave_ram, window->monospace_font);
					ImGui::EndMenu();
				}

				PopupButton("CDDA", cdda_status_window);

				PopupButton("CDC", cdc_status_window);

				PopupButton("Other", other_status_window);

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Miscellaneous"))
			{
				if (ImGui::MenuItem("Fullscreen", nullptr, window->GetFullscreen()))
					window->ToggleFullscreen();

				if (!NativeWindowsActive() || screen_subwindow)
				{
					ImGui::MenuItem("Sub-Window", nullptr, &screen_subwindow);
					ImGui::Separator();
				}

				PopupButton("Options", options_window, nullptr, std::make_pair(360, 360));

				PopupButton("About", about_window, nullptr, std::make_pair(626, 430));

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
		const ImVec2 display_position = ImGui::GetCursorScreenPos();
		const ImVec2 cursor = ImGui::GetCursorPos();
		ImGui::InvisibleButton("Magical emulator focus detector", size_of_display_region);
		ImGui::SetItemDefaultFocus();

		emulator_has_focus = ImGui::IsItemFocused();

		if (!emulator_on)
		{
			constexpr std::string_view label = "Load software using the menu bar above.";
			const ImVec2 label_size = ImGui::CalcTextSize(label);

			ImGui::SetCursorPos(cursor + (size_of_display_region - label_size) / 2);
			ImGui::TextUnformatted(label);
		}
		else
		{
			ImGui::SetCursorPos(cursor);

			const ImVec2 current_screen_size = {static_cast<float>(emulator->GetCurrentScreenWidth()), static_cast<float>(emulator->GetCurrentScreenHeight())};

			ImVec2 destination_size = current_screen_size;

			// Correct the aspect ratio of the rendered frame.
			// (256x224 and 320x240 should be the same width, but 320x224 and 320x240 should be different heights - this matches the behaviour of a real Mega Drive).
			if (!emulator->GetVDPState().h40_enabled && screen_scaling != ScreenScaling::PIXEL_PERFECT)
				destination_size.x = destination_size.x * VDP_H40_SCREEN_WIDTH_IN_TILE_PAIRS / VDP_H32_SCREEN_WIDTH_IN_TILE_PAIRS;

			// Squish the aspect ratio vertically when in Interlace Mode 2.
			if (emulator->GetVDPState().double_resolution_enabled && !tall_double_resolution_mode)
				destination_size.x *= 2;

			ImVec2 uv0 = {0, 0};
			ImVec2 uv1 = current_screen_size;

			switch (screen_scaling)
			{
				case ScreenScaling::PIXEL_PERFECT:
				{
					// Round down to the nearest multiple of 'destination_width' and 'destination_height'.
					const unsigned int framebuffer_upscale_factor = std::min(size_of_display_region.x / destination_size.x, size_of_display_region.y / destination_size.y);

					if (framebuffer_upscale_factor != 0)
					{
						destination_size *= framebuffer_upscale_factor;
						break;
					}
				}
					// Fall-back on fit scaling when the window is smaller than the screen.
					[[fallthrough]];
				case ScreenScaling::FIT:
				case ScreenScaling::FILL:
				{
					if ((size_of_display_region.x > size_of_display_region.y * destination_size.x / destination_size.y) != (screen_scaling == ScreenScaling::FILL))
					{
						destination_size.x = size_of_display_region.y * destination_size.x / destination_size.y;
						destination_size.y = size_of_display_region.y;
					}
					else
					{
						destination_size.y = size_of_display_region.x * destination_size.y / destination_size.x;
						destination_size.x = size_of_display_region.x;
					}
					
					const auto &ClampDimension = [&](const unsigned int dimension_index)
					{
						if (destination_size[dimension_index] > size_of_display_region[dimension_index])
						{
							const auto new_size = uv1[dimension_index] * size_of_display_region[dimension_index] / destination_size[dimension_index];
							uv0[dimension_index] = (uv1[dimension_index] - new_size) / 2;
							uv1[dimension_index] = uv0[dimension_index] + new_size;

							destination_size[dimension_index] = size_of_display_region[dimension_index];
						}
					};
					
					ClampDimension(0);
					ClampDimension(1);

					break;
				}
			}

			// Centre the framebuffer in the available region.
			auto offset = (size_of_display_region - destination_size) / 2;

			if (screen_scaling == ScreenScaling::PIXEL_PERFECT)
			{
				// Round to the nearest integer to prevent blurry rendering.
				for (unsigned int i = 0; i < 2; ++i)
					offset[i] = std::round(offset[i]);
			}

			ImGui::SetCursorPos(ImGui::GetCursorPos() + offset);

			// Draw the upscaled framebuffer in the window.
			const ImVec2 uv_div = {static_cast<float>(FRAMEBUFFER_WIDTH), static_cast<float>(FRAMEBUFFER_HEIGHT)};
			ImGui::ImageCopyable(*window, ImTextureRef(window->framebuffer_texture), destination_size, uv0 / uv_div, uv1 / uv_div);

			DrawStatusIndicator(display_position, size_of_display_region);
		}
	}

	ImGui::End();

	const auto &clownmdemu = emulator->GetState();
	const auto &vdp = emulator->GetVDPState();

	const auto DisplayWindow = []<typename T, typename... Ts>(std::optional<T> &window, Ts&&... arguments)
	{
		if (window.has_value())
			if (!window->Display(std::forward<Ts>(arguments)...))
				window.reset();
	};

	DisplayWindow(cheats_window, *emulator);
	DisplayWindow(debug_log_window);
	DisplayWindow(debugging_toggles_window);
	DisplayWindow(disassembler_window);
	DisplayWindow(debug_frontend_window);
	DisplayWindow(m68k_status_window, emulator->GetM68kState());
	DisplayWindow(mcd_m68k_status_window, emulator->GetSubM68kState());
	DisplayWindow(z80_status_window);
	DisplayWindow(m68k_ram_viewer_window, clownmdemu.m68k.ram);
	DisplayWindow(external_ram_viewer_window, clownmdemu.external_ram.buffer);
	DisplayWindow(z80_ram_viewer_window, clownmdemu.z80.ram);
	DisplayWindow(prg_ram_viewer_window, clownmdemu.mega_cd.prg_ram.buffer);
	DisplayWindow(word_ram_viewer_window, clownmdemu.mega_cd.word_ram.buffer);
	DisplayWindow(wave_ram_viewer_window, emulator->GetPCMState().wave_ram);
	DisplayWindow(vdp_registers_window);
	DisplayWindow(sprite_list_window);
	DisplayWindow(vram_viewer_window, vdp.vram);
	DisplayWindow(cram_viewer_window, vdp.cram);
	DisplayWindow(vsram_viewer_window, vdp.vsram);
	DisplayWindow(sprite_plane_visualiser_window);
	DisplayWindow(window_plane_visualiser_window, DebugVDP::Plane::WINDOW);
	DisplayWindow(plane_a_visualiser_window, DebugVDP::Plane::A);
	DisplayWindow(plane_b_visualiser_window, DebugVDP::Plane::B);
	DisplayWindow(tile_visualiser_window);
	DisplayWindow(colour_visualiser_window);
	DisplayWindow(stamp_visualiser_window);
	DisplayWindow(stamp_map_visualiser_window);
	DisplayWindow(fm_status_window);
	DisplayWindow(psg_status_window);
	DisplayWindow(pcm_status_window);
	DisplayWindow(cdda_status_window);
	DisplayWindow(cdc_status_window);
	DisplayWindow(other_status_window);
	DisplayWindow(options_window);
	DisplayWindow(about_window);

	file_utilities.DisplayFileDialog(drag_and_drop_filename);

	window->FinishDearImGuiFrame();

	PreEventStuff();
}

bool Frontend::WantsToQuit()
{
	return quit;
}

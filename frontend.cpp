#include "frontend.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <climits> // For INT_MAX.
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <forward_list>
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

#include "common/core/clowncommon/clowncommon.h"
#include "common/core/clownmdemu.h"

#include "libraries/imgui/imgui.h"
#include "libraries/imgui/backends/imgui_impl_sdl3.h"
#include "libraries/imgui/backends/imgui_impl_sdlrenderer3.h"
#include "libraries/inih/ini.h"

#include "cd-reader.h"
#include "debug-log.h"
#include "emulator-instance.h"
#include "file-utilities.h"
#include "windows/debug-fm.h"
#include "windows/debug-frontend.h"
#include "windows/debug-log-viewer.h"
#include "windows/debug-m68k.h"
#include "windows/debug-memory.h"
#include "windows/debug-pcm.h"
#include "windows/debug-psg.h"
#include "windows/debug-vdp.h"
#include "windows/debug-z80.h"
#include "windows/disassembler.h"
#include "windows/common/window-with-framebuffer.h"

#ifndef __EMSCRIPTEN__
#define FILE_PATH_SUPPORT
#endif

#define VERSION "v1.2.1"

using namespace Frontend;

static constexpr unsigned int INITIAL_WINDOW_WIDTH = 320 * 2;
static constexpr unsigned int INITIAL_WINDOW_HEIGHT = 224 * 2;

static constexpr unsigned int FRAMEBUFFER_WIDTH = 320;
static constexpr unsigned int FRAMEBUFFER_HEIGHT = 240 * 2; // *2 because of double-resolution mode.

static constexpr char DEFAULT_TITLE[] = "ClownMDEmu";

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

class AboutWindow : public WindowPopup<AboutWindow>
{
private:
	using Base = WindowPopup<AboutWindow>;

	static constexpr Uint32 window_flags = ImGuiWindowFlags_HorizontalScrollbar;

	void DisplayInternal()
	{
		static const auto licence_clownmdemu = std::to_array<const char>({
			#include "licences/clownmdemu.h"
		});
		static const auto licence_dear_imgui = std::to_array<const char>({
			#include "licences/dear-imgui.h"
		});
	#ifdef __EMSCRIPTEN__
		static const auto licence_emscripten_browser_file = std::to_array<const char>({
			#include "licences/emscripten-browser-file.h"
		});
	#endif
	#ifdef IMGUI_ENABLE_FREETYPE
		static const auto licence_freetype = std::to_array<const char>({
			#include "licences/freetype.h"
		});
		static const auto licence_freetype_bdf = std::to_array<const char>({
			#include "licences/freetype-bdf.h"
		});
		static const auto licence_freetype_pcf = std::to_array<const char>({
			#include "licences/freetype-pcf.h"
		});
		static const auto licence_freetype_fthash = std::to_array<const char>({
			#include "licences/freetype-fthash.h"
		});
		static const auto licence_freetype_ft_hb = std::to_array<const char>({
			#include "licences/freetype-ft-hb.h"
		});
	#endif
		static const auto licence_inih = std::to_array<const char>({
			#include "licences/inih.h"
		});
		static const auto licence_noto_sans = std::to_array<const char>({
			#include "licences/noto-sans.h"
		});
		static const auto licence_inconsolata = std::to_array<const char>({
			#include "licences/inconsolata.h"
		});

		const auto monospace_font = GetMonospaceFont();

		ImGui::SeparatorText("ClownMDEmu " VERSION);

		ImGui::TextUnformatted("This is a Sega Mega Drive (AKA Sega Genesis) emulator. Created by Clownacy.");
		constexpr auto DoURL = [](const char* const url)
		{
			if (ImGui::TextLink(url))
				SDL_OpenURL(url);
		};
		DoURL("https://github.com/Clownacy/clownmdemu-frontend");
		DoURL("https://github.com/Clownacy/clownmdemu-core");

		ImGui::SeparatorText("Licences");

		const auto DoLicence = [&monospace_font]<size_t S>(const std::array<char, S> &text)
		{
			ImGui::PushFont(monospace_font);
			ImGui::TextUnformatted(&text.front(), &text.back());
			ImGui::PopFont();
		};

		if (ImGui::CollapsingHeader("ClownMDEmu"))
			DoLicence(licence_clownmdemu);

		if (ImGui::CollapsingHeader("Dear ImGui"))
			DoLicence(licence_dear_imgui);

	#ifdef __EMSCRIPTEN__
		if (ImGui::CollapsingHeader("Emscripten Browser File Library"))
			DoLicence(licence_emscripten_browser_file);
	#endif

	#ifdef IMGUI_ENABLE_FREETYPE
		if (ImGui::CollapsingHeader("FreeType"))
		{
			if (ImGui::TreeNode("General"))
			{
				DoLicence(licence_freetype);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("BDF Driver"))
			{
				DoLicence(licence_freetype_bdf);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("PCF Driver"))
			{
				DoLicence(licence_freetype_pcf);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("fthash.c & fthash.h"))
			{
				DoLicence(licence_freetype_fthash);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("ft-hb.c & ft-hb.h"))
			{
				DoLicence(licence_freetype_ft_hb);
				ImGui::TreePop();
			}
		}
	#endif

		if (ImGui::CollapsingHeader("inih"))
			DoLicence(licence_inih);

		if (ImGui::CollapsingHeader("Noto Sans"))
			DoLicence(licence_noto_sans);

		if (ImGui::CollapsingHeader("Inconsolata"))
			DoLicence(licence_inconsolata);
	}

public:
	using Base::WindowPopup;

	friend Base;
};

static void Popup_DoTable(const char* const name, const std::function<void()> &callback)
{
	ImGui::SeparatorText(name);

	if (ImGui::BeginTable(name, 2, ImGuiTableFlags_Borders))
	{
		ImGui::TableSetupColumn("Property");
		ImGui::TableSetupColumn("Value");
		ImGui::TableHeadersRow();

		callback();

		ImGui::EndTable();
	}
};

template<typename... T>
static void Popup_DoProperty(ImFont* const font, const char* const label, fmt::format_string<T...> format, T &&...args)
{
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(label);
	ImGui::TableNextColumn();
	if (font != nullptr)
		ImGui::PushFont(font);
	ImGui::TextFormatted(format, std::forward<T>(args)...);
	if (font != nullptr)
		ImGui::PopFont();
};

class DebugCDC : public WindowPopup<DebugCDC>
{
private:
	using Base = WindowPopup<DebugCDC>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal()
	{
		const auto &cdc = Frontend::emulator->CurrentState().clownmdemu.mega_cd.cdc;

		Popup_DoTable("Sector Buffer", [&]()
			{
				Popup_DoProperty(nullptr, "Total Buffered Sectors", "{}", cdc.buffered_sectors_total);
				Popup_DoProperty(nullptr, "Read Index", "{}", cdc.buffered_sectors_read_index);
				Popup_DoProperty(nullptr, "Write Index", "{}", cdc.buffered_sectors_write_index);
			});

		Popup_DoTable("Host Data", [&]()
			{
				Popup_DoProperty(nullptr, "Bound", "{}", cdc.host_data_bound ? "Yes" : "No");
				Popup_DoProperty(nullptr, "Target SUB-CPU", "{}", cdc.host_data_target_sub_cpu ? "Yes" : "No");
				Popup_DoProperty(nullptr, "Buffered Sector Index", "{}", cdc.host_data_buffered_sector_index);
				Popup_DoProperty(nullptr, "Word Index", "{}", cdc.host_data_word_index);
			});

		Popup_DoTable("Other", [&]()
			{
				Popup_DoProperty(nullptr, "Current Sector", "{}", cdc.current_sector);
				Popup_DoProperty(nullptr, "Sectors Remaining", "{}", cdc.sectors_remaining);
				Popup_DoProperty(nullptr, "Device Destination", "{}", cdc.device_destination);
			});
	}

public:
	using Base::WindowPopup;

	friend Base;
};

class DebugCDDA : public WindowPopup<DebugCDDA>
{
private:
	using Base = WindowPopup<DebugCDDA>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal()
	{
		const auto &cdda = Frontend::emulator->CurrentState().clownmdemu.mega_cd.cdda;

		Popup_DoTable("Volume", [&]()
			{
				Popup_DoProperty(nullptr, "Volume", "0x{:03X}", cdda.volume);
				Popup_DoProperty(nullptr, "Master Volume", "0x{:03X}", cdda.master_volume);
			});

		Popup_DoTable("Fade", [&]()
			{
				Popup_DoProperty(nullptr, "Target Volume", "0x{:03X}", cdda.target_volume);
				Popup_DoProperty(nullptr, "Fade Step", "0x{:03X}", cdda.fade_step);
				Popup_DoProperty(nullptr, "Fade Remaining", "0x{:03X}", cdda.fade_remaining);
				Popup_DoProperty(nullptr, "Fade Direction", "{}", cdda.subtract_fade_step ? "Negative" : "Positive");
			});

		Popup_DoTable("Other", [&]()
			{
				Popup_DoProperty(nullptr, "Playing", "{}", cdda.playing ? "True" : "False");
				Popup_DoProperty(nullptr, "Paused", "{}", cdda.paused ? "True" : "False");
			});
	}

public:
	using Base::WindowPopup;

	friend Base;
};

class DebugOther : public WindowPopup<DebugOther>
{
private:
	using Base = WindowPopup<DebugOther>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal()
	{
		const auto monospace_font = GetMonospaceFont();
		const ClownMDEmu_State &clownmdemu_state = Frontend::emulator->CurrentState().clownmdemu;

		if (ImGui::BeginTable("Other", 2, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Property");
			ImGui::TableSetupColumn("Value");
			ImGui::TableHeadersRow();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Z80 Bank");
			ImGui::TableNextColumn();
			ImGui::PushFont(monospace_font);
			ImGui::TextFormatted("0x{:06X}-0x{:06X}", clownmdemu_state.z80.bank * 0x8000, (clownmdemu_state.z80.bank + 1) * 0x8000);
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
			ImGui::TextFormatted("0x{:05X}-0x{:05X}", clownmdemu_state.mega_cd.prg_ram.bank * 0x20000, (clownmdemu_state.mega_cd.prg_ram.bank + 1) * 0x20000);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("PRG-RAM Write-Protect");
			ImGui::TableNextColumn();
			ImGui::PushFont(monospace_font);
			ImGui::TextFormatted("0x00000-0x{:05X}", clownmdemu_state.mega_cd.prg_ram.write_protect * 0x200);
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
			ImGui::TextFormatted("0x{:04X}", clownmdemu_state.mega_cd.communication.flag);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("68000 Communication Command");
			ImGui::TableNextColumn();
			ImGui::PushFont(monospace_font);
			for (std::size_t i = 0; i < std::size(clownmdemu_state.mega_cd.communication.command); i += 2)
				ImGui::TextFormatted("0x{:04X} 0x{:04X}", clownmdemu_state.mega_cd.communication.command[i + 0], clownmdemu_state.mega_cd.communication.command[i + 1]);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("68000 Communication Status");
			ImGui::TableNextColumn();
			ImGui::PushFont(monospace_font);
			for (std::size_t i = 0; i < std::size(clownmdemu_state.mega_cd.communication.status); i += 2)
				ImGui::TextFormatted("0x{:04X} 0x{:04X}", clownmdemu_state.mega_cd.communication.status[i + 0], clownmdemu_state.mega_cd.communication.status[i + 1]);
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

		Popup_DoTable("Graphics Transformation",
			[&]()
			{
				const auto &graphics = clownmdemu_state.mega_cd.rotation;
				Popup_DoProperty(nullptr, "Stamp Size", "{} pixels", graphics.large_stamp ? "32x32" : "16x16");
				Popup_DoProperty(nullptr, "Stamp Map Size", "{} pixels", graphics.large_stamp_map ? "4096x4096" : "256x256");
				Popup_DoProperty(nullptr, "Repeating Stamp Map", "{}", graphics.repeating_stamp_map ? "Yes" : "No");
				Popup_DoProperty(nullptr, "Stamp Map Address", "0x{:X}", graphics.stamp_map_address * 4);
				Popup_DoProperty(nullptr, "Image Buffer Height in Tiles", "{}", graphics.image_buffer_height_in_tiles + 1);
				Popup_DoProperty(nullptr, "Image Buffer Address", "0x{:X}", graphics.image_buffer_address * 4);
				Popup_DoProperty(nullptr, "Image Buffer X Offset", "{}", graphics.image_buffer_x_offset);
				Popup_DoProperty(nullptr, "Image Buffer Y Offset", "{}", graphics.image_buffer_y_offset);
				Popup_DoProperty(nullptr, "Image Buffer Width", "{}", graphics.image_buffer_width);
				Popup_DoProperty(nullptr, "Image Buffer Height", "{}", graphics.image_buffer_height);
			}
		);
	}

public:
	using Base::WindowPopup;

	friend Base;
};

class DebugToggles : public WindowPopup<DebugToggles>
{
private:
	using Base = WindowPopup<DebugToggles>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal()
	{
		const auto DoButton = [](cc_bool &disabled, const char* const label)
		{
			ImGui::TableNextColumn();
			bool temp = !disabled;
			if (ImGui::Checkbox(label, &temp))
				disabled = !disabled;
		};

		ImGui::SeparatorText("VDP");

		if (ImGui::BeginTable("VDP Options", 2, ImGuiTableFlags_SizingStretchSame))
		{
			VDP_Configuration &vdp = Frontend::emulator->GetConfigurationVDP();

			DoButton(vdp.sprites_disabled, "Sprite Plane");
			DoButton(vdp.window_disabled, "Window Plane");
			DoButton(vdp.planes_disabled[0], "Plane A");
			DoButton(vdp.planes_disabled[1], "Plane B");

			ImGui::EndTable();
		}

		ImGui::SeparatorText("FM");

		if (ImGui::BeginTable("FM Options", 2, ImGuiTableFlags_SizingStretchSame))
		{
			FM_Configuration &fm = Frontend::emulator->GetConfigurationFM();

			char buffer[] = "FM1";

			for (auto disabled = std::begin(fm.fm_channels_disabled); disabled != std::end(fm.fm_channels_disabled); ++disabled)
			{
				buffer[2] = '1' + std::distance(std::begin(fm.fm_channels_disabled), disabled);
				DoButton(*disabled, buffer);
			}

			DoButton(fm.dac_channel_disabled, "DAC");

			ImGui::EndTable();
		}

		ImGui::SeparatorText("PSG");

		if (ImGui::BeginTable("PSG Options", 2, ImGuiTableFlags_SizingStretchSame))
		{
			PSG_Configuration &psg = Frontend::emulator->GetConfigurationPSG();

			char buffer[] = "PSG1";

			for (auto disabled = std::begin(psg.tone_disabled); disabled != std::end(psg.tone_disabled); ++disabled)
			{
				buffer[3] = '1' + std::distance(std::begin(psg.tone_disabled), disabled);
				DoButton(*disabled, buffer);
			}

			DoButton(psg.noise_disabled, "PSG Noise");

			ImGui::EndTable();
		}

		ImGui::SeparatorText("PCM");

		if (ImGui::BeginTable("PCM Options", 2, ImGuiTableFlags_SizingStretchSame))
		{
			PCM_Configuration &pcm = Frontend::emulator->GetConfigurationPCM();

			char buffer[] = "PCM1";

			for (auto disabled = std::begin(pcm.channels_disabled); disabled != std::end(pcm.channels_disabled); ++disabled)
			{
				buffer[3] = '1' + std::distance(std::begin(pcm.channels_disabled), disabled);
				DoButton(*disabled, buffer);
			}

			ImGui::EndTable();
		}
	}

public:
	using Base::WindowPopup;

	friend Base;
};

class OptionsWindow : public WindowPopup<OptionsWindow>
{
private:
	using Base = WindowPopup<OptionsWindow>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal()
	{
		ImGui::SeparatorText("Console");

		if (ImGui::BeginTable("Console Options", 3))
		{
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("TV Standard:");
			DoToolTip(u8"Some games only work with a certain TV standard.");
			ImGui::TableNextColumn();
			if (ImGui::RadioButton("NTSC", !Frontend::emulator->GetPALMode()))
				if (Frontend::emulator->GetPALMode())
					Frontend::SetAudioPALMode(false);
			DoToolTip(u8"60 FPS");
			ImGui::TableNextColumn();
			if (ImGui::RadioButton("PAL", Frontend::emulator->GetPALMode()))
				if (!Frontend::emulator->GetPALMode())
					Frontend::SetAudioPALMode(true);
			DoToolTip(u8"50 FPS");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Region:");
			DoToolTip(u8"Some games only work with a certain region.");
			ImGui::TableNextColumn();
			if (ImGui::RadioButton("Japan", Frontend::emulator->GetDomestic()))
				Frontend::emulator->SetDomestic(true);
			DoToolTip(u8"Games may show Japanese text.");
			ImGui::TableNextColumn();
			if (ImGui::RadioButton("Elsewhere", !Frontend::emulator->GetDomestic()))
				Frontend::emulator->SetDomestic(false);
			DoToolTip(u8"Games may show English text.");

			ImGui::EndTable();
		}

		ImGui::SeparatorText("Video");

		if (ImGui::BeginTable("Video Options", 2))
		{
			ImGui::TableNextColumn();
			if (ImGui::Checkbox("V-Sync", &Frontend::use_vsync))
				Frontend::window->SetVSync(Frontend::use_vsync);
			DoToolTip(u8"Prevents screen tearing.");

			ImGui::TableNextColumn();
			if (ImGui::Checkbox("Integer Screen Scaling", &Frontend::integer_screen_scaling) && Frontend::integer_screen_scaling)
			{
				// Reclaim memory used by the upscaled framebuffer, since we won't be needing it anymore.
				SDL_DestroyTexture(Frontend::framebuffer_texture_upscaled);
				Frontend::framebuffer_texture_upscaled = nullptr;
			}
			DoToolTip(u8"Preserves pixel aspect ratio,\navoiding non-square pixels.");

			ImGui::TableNextColumn();
			ImGui::Checkbox("Tall Interlace Mode 2", &Frontend::tall_double_resolution_mode);
			DoToolTip(u8"Makes games that use Interlace Mode 2\nfor split-screen not appear squashed.");

			ImGui::EndTable();
		}

		ImGui::SeparatorText("Audio");

		if (ImGui::BeginTable("Audio Options", 2))
		{
			ImGui::TableNextColumn();
			bool low_pass_filter = Frontend::emulator->GetLowPassFilter();
			if (ImGui::Checkbox("Low-Pass Filter", &low_pass_filter))
				Frontend::emulator->SetLowPassFilter(low_pass_filter);
			DoToolTip(u8"Makes the audio sound 'softer',\njust like on a real Mega Drive.");

			ImGui::TableNextColumn();
			bool ladder_effect = !Frontend::emulator->GetConfigurationFM().ladder_effect_disabled;
			if (ImGui::Checkbox("Low-Volume Distortion", &ladder_effect))
				Frontend::emulator->GetConfigurationFM().ladder_effect_disabled = !ladder_effect;
			DoToolTip(u8"Approximates the so-called 'ladder effect' that\nis present in early Mega Drives. Without this,\ncertain sounds in some games will be too quiet.");

			ImGui::EndTable();
		}

		ImGui::SeparatorText("Miscellaneous");

		if (ImGui::BeginTable("Miscellaneous Options", 2))
		{
	#ifndef __EMSCRIPTEN__
			ImGui::TableNextColumn();
			ImGui::Checkbox("Native Windows", &native_windows);
			DoToolTip(u8"Use real windows instead of 'fake' windows\nthat are stuck inside the main window.");
	#endif

			ImGui::TableNextColumn();
			bool rewinding_enabled = Frontend::emulator->RewindingEnabled();
			if (ImGui::Checkbox("Rewinding", &rewinding_enabled))
				Frontend::emulator->EnableRewinding(rewinding_enabled);
			DoToolTip(
				u8"Allows the emulated console to be played in"
				"\nreverse for up to 10 seconds. This uses a lot"
				"\nof RAM and increases CPU usage, so disable"
				"\nthis if there is lag.");

			ImGui::EndTable();
		}

		ImGui::SeparatorText("Keyboard Input");

		static bool sorted_scancodes_done;
		static std::array<SDL_Scancode, SDL_SCANCODE_COUNT> sorted_scancodes; // TODO: `SDL_SCANCODE_COUNT` is an internal macro, so use something standard!

		if (!sorted_scancodes_done)
		{
			sorted_scancodes_done = true;

			for (auto scancode = std::begin(sorted_scancodes); scancode != std::end(sorted_scancodes); ++scancode)
				*scancode = static_cast<SDL_Scancode>(std::distance(std::begin(sorted_scancodes), scancode));

			SDL_qsort(&sorted_scancodes, sorted_scancodes.size(), sizeof(sorted_scancodes[0]),
				[](const void* const a, const void* const b)
			{
				const SDL_Scancode* const binding_1 = static_cast<const SDL_Scancode*>(a);
				const SDL_Scancode* const binding_2 = static_cast<const SDL_Scancode*>(b);

				return Frontend::keyboard_bindings[*binding_1] - Frontend::keyboard_bindings[*binding_2];
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
			"Rewind",
			"Quick Save State",
			"Quick Load State",
			"Toggle Fullscreen",
			"Toggle Control Pad"
		};

		if (ImGui::BeginTable("Keyboard Input Options", 2))
		{
			ImGui::TableNextColumn();
			if (ImGui::RadioButton("Control Pad #1", Frontend::keyboard_input.bound_joypad == 0))
				Frontend::keyboard_input.bound_joypad = 0;
			DoToolTip(u8"Binds the keyboard to Control Pad #1.");


			ImGui::TableNextColumn();
			if (ImGui::RadioButton("Control Pad #2", Frontend::keyboard_input.bound_joypad == 1))
				Frontend::keyboard_input.bound_joypad = 1;
			DoToolTip(u8"Binds the keyboard to Control Pad #2.");

			ImGui::EndTable();
		}

		if (ImGui::BeginTable("control pad bindings", 3, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Key");
			ImGui::TableSetupColumn("Action");
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableHeadersRow();
			for (const auto &scancode : sorted_scancodes)
			{
				if (Frontend::keyboard_bindings[scancode] != INPUT_BINDING_NONE)
				{
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode)));

					ImGui::TableNextColumn();
					ImGui::TextUnformatted(binding_names[Frontend::keyboard_bindings[scancode]]);

					ImGui::TableNextColumn();
					ImGui::PushID(&scancode);
					if (ImGui::Button("X"))
						Frontend::keyboard_bindings[scancode] = INPUT_BINDING_NONE;
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
			const bool* const keys_pressed = SDL_GetKeyboardState(&total_keys);

			for (int i = 0; i < total_keys; ++i)
			{
				if (keys_pressed[i] && i != SDL_GetScancodeFromKey(SDLK_LALT, nullptr))
				{
					ImGui::CloseCurrentPopup();

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

			ImGui::EndPopup();

			if (next_menu)
				ImGui::OpenPopup("Select Action");
		}

		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Select Action", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			bool previous_menu = false;

			ImGui::TextFormatted("Selected Key: {}", SDL_GetScancodeName(selected_scancode));

			if (ImGui::BeginListBox("##Actions"))
			{
				for (unsigned int i = INPUT_BINDING_NONE + 1; i < INPUT_BINDING__TOTAL; i = i + 1)
				{
					if (ImGui::Selectable(binding_names[i]))
					{
						ImGui::CloseCurrentPopup();
						Frontend::keyboard_bindings[selected_scancode] = static_cast<InputBinding>(i);
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

public:
	using Base::WindowPopup;

	friend Base;
};

std::optional<EmulatorInstance> Frontend::emulator;
std::optional<WindowWithFramebuffer> Frontend::window;
FileUtilities Frontend::file_utilities;
unsigned int Frontend::frame_counter;
SDL::Texture Frontend::framebuffer_texture_upscaled;

unsigned int Frontend::output_width, Frontend::output_height;
unsigned int Frontend::upscale_width, Frontend::upscale_height;

bool Frontend::use_vsync;
bool Frontend::integer_screen_scaling;
bool Frontend::tall_double_resolution_mode;
bool Frontend::fast_forward_in_progress;
bool Frontend::native_windows;

Input Frontend::keyboard_input;
std::array<InputBinding, SDL_SCANCODE_COUNT > Frontend::keyboard_bindings; // TODO: `SDL_SCANCODE_COUNT` is an internal macro, so use something standard!

static std::array<InputBinding, SDL_SCANCODE_COUNT > keyboard_bindings_cached; // TODO: `SDL_SCANCODE_COUNT` is an internal macro, so use something standard!
static std::array<bool, SDL_SCANCODE_COUNT > key_pressed; // TODO: `SDL_SCANCODE_COUNT` is an internal macro, so use something standard!

static Frontend::FrameRateCallback frame_rate_callback;

static std::forward_list<ControllerInput> controller_input_list;

#ifdef FILE_PATH_SUPPORT
static std::list<RecentSoftware> recent_software_list;
#endif
static std::filesystem::path drag_and_drop_filename;

static bool emulator_has_focus; // Used for deciding when to pass inputs to the emulator.
static bool emulator_paused;
static bool emulator_frame_advance;

static bool quick_save_exists;
static EmulatorInstance::State quick_save_state;

static std::optional<DebugLogViewer> debug_log_window;
static std::optional<DebugToggles> debugging_toggles_window;
static std::optional<Disassembler> m68k_disassembler_window;
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
static std::optional<DebugCDC> cdc_status_window;
static std::optional<DebugCDDA> cdda_status_window;
static std::optional<DebugOther> other_status_window;
static std::optional<OptionsWindow> options_window;
static std::optional<AboutWindow> about_window;

static constexpr auto popup_windows = std::make_tuple(
	&debug_log_window,
	&debugging_toggles_window,
	&m68k_disassembler_window,
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
	&cdc_status_window,
	&cdda_status_window,
	&other_status_window,
	&options_window,
	&about_window
);

// Manages whether the program exits or not.
static bool quit;

// Used for tracking when to pop the emulation display out into its own little window.
static bool pop_out;

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

static void UpdateFastForwardStatus()
{
	fast_forward_in_progress = keyboard_input.fast_forward;

	for (const auto &controller_input : controller_input_list)
		fast_forward_in_progress |= controller_input.input.fast_forward != 0;
}

static void UpdateRewindStatus()
{
	bool will_rewind = keyboard_input.rewind;

	for (const auto &controller_input : controller_input_list)
		will_rewind |= controller_input.input.rewind != 0;

	emulator->Rewind(will_rewind);
}


//////////////////////////
// Upscaled Framebuffer //
//////////////////////////

static unsigned int framebuffer_texture_upscaled_width;
static unsigned int framebuffer_texture_upscaled_height;

static void RecreateUpscaledFramebuffer(const unsigned int display_width, const unsigned int display_height)
{
	static unsigned int previous_framebuffer_size_factor = 0;

	const unsigned int framebuffer_size_factor = std::max(1u, std::min(CC_DIVIDE_CEILING(display_width, 640), CC_DIVIDE_CEILING(display_height, 480)));

	if (!framebuffer_texture_upscaled || framebuffer_size_factor != previous_framebuffer_size_factor)
	{
		previous_framebuffer_size_factor = framebuffer_size_factor;

		framebuffer_texture_upscaled_width = 640 * framebuffer_size_factor;
		framebuffer_texture_upscaled_height = 480 * framebuffer_size_factor;

		framebuffer_texture_upscaled = SDL::CreateTexture(window->GetRenderer(), SDL_TEXTUREACCESS_TARGET, framebuffer_texture_upscaled_width, framebuffer_texture_upscaled_height, SDL_SCALEMODE_LINEAR);
	}
}

bool Frontend::GetUpscaledFramebufferSize(unsigned int &width, unsigned int &height)
{
	if (!framebuffer_texture_upscaled)
		return false;

	float width_float, height_float;
	if (!SDL_GetTextureSize(framebuffer_texture_upscaled, &width_float, &height_float))
	{
		debug_log.Log("SDL_GetTextureSize failed with the following message - '{}'", SDL_GetError());
		return false;
	}

	width = static_cast<unsigned int>(width_float);
	height = static_cast<unsigned int>(height_float);
	return true;
}


///////////
// Misc. //
///////////

std::filesystem::path Frontend::GetConfigurationDirectoryPath()
{
	const auto path_cstr = SDL::Pointer<char8_t>(reinterpret_cast<char8_t*>(SDL_GetPrefPath("clownacy", "clownmdemu-frontend")));

	if (path_cstr == nullptr)
		return {};

	const std::filesystem::path path(path_cstr.get());

	return path;
}

std::filesystem::path Frontend::GetSaveDataDirectoryPath()
{
	return GetConfigurationDirectoryPath() / "Save Data";
}

static std::filesystem::path GetConfigurationFilePath()
{
	return GetConfigurationDirectoryPath() / "configuration.ini";
}

static std::filesystem::path GetDearImGuiSettingsFilePath()
{
	return GetConfigurationDirectoryPath() / "dear-imgui-settings.ini";
}

static std::filesystem::path GetSaveDataFilePath(const std::filesystem::path &rom_path)
{
	return (GetSaveDataDirectoryPath() / rom_path.stem()).replace_extension(".srm");
}

static void SetWindowTitleToSoftwareName()
{
	const std::string name = emulator->GetSoftwareName();

	// Use the default title if the ROM does not provide a name.
	// Micro Machines is one game that lacks a name.
	SDL_SetWindowTitle(window->GetSDLWindow(), name.empty() ? DEFAULT_TITLE : name.c_str());
}

static void LoadCartridgeFile(const std::filesystem::path &path, std::vector<unsigned char> &&file_buffer)
{
	const auto save_file_path = GetSaveDataFilePath(path);

	std::filesystem::create_directories(save_file_path.parent_path());

#ifdef FILE_PATH_SUPPORT
	AddToRecentSoftware(path, false, false);
#endif

	quick_save_exists = false;
	emulator->LoadCartridgeFile(std::move(file_buffer), save_file_path);

	SetWindowTitleToSoftwareName();
}

static bool LoadCartridgeFile(const std::filesystem::path &path, SDL::IOStream &file)
{
	auto file_buffer = file_utilities.LoadFileToBuffer(file);

	if (!file_buffer.has_value())
	{
		debug_log.Log("Could not load the cartridge file");
		window->ShowErrorMessageBox("Failed to load the cartridge file.");
		return false;
	}

	LoadCartridgeFile(path, std::move(*file_buffer));

	return true;
}

static bool LoadCartridgeFile(const std::filesystem::path &path)
{
	SDL::IOStream file = SDL::IOFromFile(path, "rb");

	if (!file)
	{
		debug_log.Log("Could not load the cartridge file");
		window->ShowErrorMessageBox("Failed to load the cartridge file.");
		return false;
	}

	return LoadCartridgeFile(path, file);
}

static bool LoadCDFile(const std::filesystem::path &path, SDL::IOStream &&file)
{
#ifdef FILE_PATH_SUPPORT
	AddToRecentSoftware(path, true, false);
#endif

	// Load the CD.
	if (!emulator->LoadCDFile(std::move(file), path))
		return false;

	SetWindowTitleToSoftwareName();

	return true;
}

#ifdef FILE_PATH_SUPPORT
static bool LoadCDFile(const std::filesystem::path &path)
{
	SDL::IOStream file = SDL::IOFromFile(path, "rb");

	if (!file)
	{
		debug_log.Log("Could not load the CD file");
		window->ShowErrorMessageBox("Failed to load the CD file.");
		return false;
	}

	return LoadCDFile(path, std::move(file));
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

static bool LoadSaveState(SDL::IOStream &file)
{
	const auto file_buffer = file_utilities.LoadFileToBuffer(file);

	if (!file_buffer.has_value())
	{
		debug_log.Log("Could not load save state file");
		window->ShowErrorMessageBox("Could not load save state file.");
		return false;
	}

	return LoadSaveState(*file_buffer);
}

#ifdef FILE_PATH_SUPPORT
static bool CreateSaveState(const std::filesystem::path &path)
{
	bool success = true;

	SDL::IOStream file = SDL::IOFromFile(path, "wb");

	if (!file || !emulator->WriteSaveStateFile(file))
	{
		debug_log.Log("Could not create save state file");
		window->ShowErrorMessageBox("Could not create save state file.");
		success = false;
	}

	return success;
}
#endif

void Frontend::SetAudioPALMode(const bool enabled)
{
	frame_rate_callback(enabled);
	emulator->SetPALMode(enabled);
}


/////////////
// Tooltip //
/////////////

void DoToolTip(const std::u8string &text)
{
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		const char* const characters = reinterpret_cast<const char*>(text.c_str());
		ImGui::TextUnformatted(characters, characters + text.size());
		ImGui::EndTooltip();
	}
}


/////////
// INI //
/////////

static char* INIReadCallback(char* const buffer, const int length, void* const user)
{
	SDL::IOStream &file = *static_cast<SDL::IOStream*>(user);

	int i = 0;

	while (i < length - 1)
	{
		char character;
		if (SDL_ReadIO(file, &character, 1) == 0)
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
	const std::u8string_view section(reinterpret_cast<const char8_t*>(section_cstr));
	const std::u8string_view name(reinterpret_cast<const char8_t*>(name_cstr));
	const std::u8string_view value(reinterpret_cast<const char8_t*>(value_cstr));

	if (section == u8"Miscellaneous")
	{
		const bool state = value == u8"on";

		if (name == u8"vsync")
			use_vsync = state;
		else if (name == u8"integer-screen-scaling")
			integer_screen_scaling = state;
		else if (name == u8"tall-interlace-mode-2")
			tall_double_resolution_mode = state;
	#ifndef __EMSCRIPTEN__
		else if (name == u8"native-windows")
			native_windows = state;
	#endif
		else if (name == u8"rewinding")
			emulator->EnableRewinding(state);
		else if (name == u8"low-pass-filter")
			emulator->SetLowPassFilter(state);
		else if (name == u8"low-volume-distortion")
			emulator->GetConfigurationFM().ladder_effect_disabled = !state;
		else if (name == u8"pal")
			SetAudioPALMode(state);
		else if (name == u8"japanese")
			emulator->SetDomestic(state);
	}
	else if (section == u8"Keyboard Bindings")
	{
		unsigned int scancode_integer;
		const auto scancode_integer_result = std::from_chars(reinterpret_cast<const char*>(&name.front()), reinterpret_cast<const char*>(&name.back()) + 1, scancode_integer, 10);

		if (scancode_integer_result.ec == std::errc{} && scancode_integer_result.ptr == reinterpret_cast<const char*>(&name.back()) + 1 && scancode_integer < SDL_SCANCODE_COUNT)
		{
			const SDL_Scancode scancode = static_cast<SDL_Scancode>(scancode_integer);

			unsigned int binding_index;
			const auto binding_index_result = std::from_chars(reinterpret_cast<const char*>(&value.front()), reinterpret_cast<const char*>(&value.back()) + 1, binding_index, 10);

			InputBinding input_binding = INPUT_BINDING_NONE;

			if (binding_index_result.ec == std::errc() && binding_index_result.ptr == reinterpret_cast<const char*>(&value.back()) + 1)
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
					INPUT_BINDING_REWIND,
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
				if (value == u8"INPUT_BINDING_CONTROLLER_UP")
					input_binding = INPUT_BINDING_CONTROLLER_UP;
				else if (value == u8"INPUT_BINDING_CONTROLLER_DOWN")
					input_binding = INPUT_BINDING_CONTROLLER_DOWN;
				else if (value == u8"INPUT_BINDING_CONTROLLER_LEFT")
					input_binding = INPUT_BINDING_CONTROLLER_LEFT;
				else if (value == u8"INPUT_BINDING_CONTROLLER_RIGHT")
					input_binding = INPUT_BINDING_CONTROLLER_RIGHT;
				else if (value == u8"INPUT_BINDING_CONTROLLER_A")
					input_binding = INPUT_BINDING_CONTROLLER_A;
				else if (value == u8"INPUT_BINDING_CONTROLLER_B")
					input_binding = INPUT_BINDING_CONTROLLER_B;
				else if (value == u8"INPUT_BINDING_CONTROLLER_C")
					input_binding = INPUT_BINDING_CONTROLLER_C;
				else if (value == u8"INPUT_BINDING_CONTROLLER_X")
					input_binding = INPUT_BINDING_CONTROLLER_X;
				else if (value == u8"INPUT_BINDING_CONTROLLER_Y")
					input_binding = INPUT_BINDING_CONTROLLER_Y;
				else if (value == u8"INPUT_BINDING_CONTROLLER_Z")
					input_binding = INPUT_BINDING_CONTROLLER_Z;
				else if (value == u8"INPUT_BINDING_CONTROLLER_START")
					input_binding = INPUT_BINDING_CONTROLLER_START;
				else if (value == u8"INPUT_BINDING_CONTROLLER_MODE")
					input_binding = INPUT_BINDING_CONTROLLER_MODE;
				else if (value == u8"INPUT_BINDING_PAUSE")
					input_binding = INPUT_BINDING_PAUSE;
				else if (value == u8"INPUT_BINDING_RESET")
					input_binding = INPUT_BINDING_RESET;
				else if (value == u8"INPUT_BINDING_FAST_FORWARD")
					input_binding = INPUT_BINDING_FAST_FORWARD;
				else if (value == u8"INPUT_BINDING_REWIND")
					input_binding = INPUT_BINDING_REWIND;
				else if (value == u8"INPUT_BINDING_QUICK_SAVE_STATE")
					input_binding = INPUT_BINDING_QUICK_SAVE_STATE;
				else if (value == u8"INPUT_BINDING_QUICK_LOAD_STATE")
					input_binding = INPUT_BINDING_QUICK_LOAD_STATE;
				else if (value == u8"INPUT_BINDING_TOGGLE_FULLSCREEN")
					input_binding = INPUT_BINDING_TOGGLE_FULLSCREEN;
				else if (value == u8"INPUT_BINDING_TOGGLE_CONTROL_PAD")
					input_binding = INPUT_BINDING_TOGGLE_CONTROL_PAD;
			}

			keyboard_bindings[scancode] = input_binding;
		}

	}
#ifdef FILE_PATH_SUPPORT
	else if (section == u8"Recent Software")
	{
		static bool is_cd_file = false;

		if (name == u8"cd")
		{
			is_cd_file = value == u8"true";
		}
		else if (name == u8"path")
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
	const SDL_DisplayID display_index = SDL_GetDisplayForWindow(window->GetSDLWindow());

	if (display_index == 0)
	{
		debug_log.Log("SDL_GetDisplayForWindow failed with the following message - '{}'", SDL_GetError());
	}
	else
	{
		const SDL_DisplayMode* const display_mode = SDL_GetCurrentDisplayMode(display_index);

		if (display_mode == nullptr)
		{
			debug_log.Log("SDL_GetCurrentDisplayMode failed with the following message - '{}'", SDL_GetError());
		}
		else
		{
			// Enable V-sync on displays with an FPS of a multiple of 60.
			// TODO: ...But what about PAL50?
			use_vsync = std::lround(display_mode->refresh_rate) % 60 == 0;
		}
	}

	// Default other settings.
	emulator->SetLowPassFilter(true);
	integer_screen_scaling = false;
	tall_double_resolution_mode = false;
#ifdef __EMSCRIPTEN__
	native_windows = false;
#else
	native_windows = true;
#endif
	emulator->EnableRewinding(true);

	emulator->SetDomestic(false);
	SetAudioPALMode(false);

	const SDL::IOStream file = SDL::IOFromFile(GetConfigurationFilePath(), "r");

	// Load the configuration file, overwriting the above settings.
	if (!file || ini_parse_stream(INIReadCallback, const_cast<SDL::IOStream*>(&file), INIParseCallback, nullptr) != 0)
	{
		// Failed to read configuration file: set defaults key bindings.
		for (auto &keyboard_binding : keyboard_bindings)
			keyboard_binding = INPUT_BINDING_NONE;

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
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_PAUSE, nullptr)] = INPUT_BINDING_PAUSE;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F11, nullptr)] = INPUT_BINDING_TOGGLE_FULLSCREEN;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_TAB, nullptr)] = INPUT_BINDING_RESET;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F1, nullptr)] = INPUT_BINDING_TOGGLE_CONTROL_PAD;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F5, nullptr)] = INPUT_BINDING_QUICK_SAVE_STATE;
		keyboard_bindings[SDL_GetScancodeFromKey(SDLK_F9, nullptr)] = INPUT_BINDING_QUICK_LOAD_STATE;
		keyboard_bindings[SDL_SCANCODE_SPACE] = INPUT_BINDING_FAST_FORWARD;
		keyboard_bindings[SDL_SCANCODE_R] = INPUT_BINDING_REWIND;
	}

	// Apply the V-sync setting, now that it's been decided.
	window->SetVSync(use_vsync);
}

static void SaveConfiguration()
{
#ifdef SDL_PLATFORM_WIN32
#define ENDL "\r\n"
#else
#define ENDL "\n"
#endif
	// Save configuration file:
	SDL::IOStream file = SDL::IOFromFile(GetConfigurationFilePath(), "w");

	if (!file)
	{
		debug_log.Log("Could not open configuration file for writing.");
	}
	else
	{
	#define PRINT_STRING(FILE, STRING) SDL_WriteIO(FILE, STRING, sizeof(STRING) - 1)
		// Save keyboard bindings.
		PRINT_STRING(file, "[Miscellaneous]" ENDL);

	#define PRINT_BOOLEAN_OPTION(FILE, NAME, VARIABLE) \
		PRINT_STRING(FILE, NAME " = "); \
		if (VARIABLE) \
			PRINT_STRING(FILE, "on" ENDL); \
		else \
			PRINT_STRING(FILE, "off" ENDL);

		PRINT_BOOLEAN_OPTION(file, "vsync", use_vsync);
		PRINT_BOOLEAN_OPTION(file, "integer-screen-scaling", integer_screen_scaling);
		PRINT_BOOLEAN_OPTION(file, "tall-interlace-mode-2", tall_double_resolution_mode);
	#ifndef __EMSCRIPTEN__
		PRINT_BOOLEAN_OPTION(file, "native-windows", native_windows);
	#endif
		PRINT_BOOLEAN_OPTION(file, "rewinding", emulator->RewindingEnabled());
		PRINT_BOOLEAN_OPTION(file, "low-pass-filter", emulator->GetLowPassFilter());
		PRINT_BOOLEAN_OPTION(file, "low-volume-distortion", !emulator->GetConfigurationFM().ladder_effect_disabled);
		PRINT_BOOLEAN_OPTION(file, "pal", emulator->GetPALMode());
		PRINT_BOOLEAN_OPTION(file, "japanese", emulator->GetDomestic());

		// Save keyboard bindings.
		PRINT_STRING(file, ENDL "[Keyboard Bindings]" ENDL);

		for (auto keyboard_binding = std::cbegin(keyboard_bindings); keyboard_binding != std::cend(keyboard_bindings); ++keyboard_binding)
		{
			if (*keyboard_binding != INPUT_BINDING_NONE)
			{
				const char *binding_string;

				// Default, to shut up potential warnings.
				binding_string = "INPUT_BINDING_NONE";

				switch (*keyboard_binding)
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

					case INPUT_BINDING_REWIND:
						binding_string = "INPUT_BINDING_REWIND";
						break;

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

				const std::string buffer = fmt::format("{} = {}" ENDL, std::distance(std::cbegin(keyboard_bindings), keyboard_binding), binding_string);
				SDL_WriteIO(file, buffer.data(), buffer.size());
			}
		}

	#ifdef FILE_PATH_SUPPORT
		// Save recent software paths.
		PRINT_STRING(file, ENDL "[Recent Software]" ENDL);

		for (const auto &recent_software : recent_software_list)
		{
			PRINT_STRING(file, "cd = ");
			if (recent_software.is_cd_file)
				PRINT_STRING(file, "true" ENDL);
			else
				PRINT_STRING(file, "false" ENDL);

			PRINT_STRING(file, "path = ");
			const auto path_string = recent_software.path.u8string();
			SDL_WriteIO(file, reinterpret_cast<const char*>(path_string.c_str()), path_string.length());
			PRINT_STRING(file, ENDL);
		}
	#endif
	}
#undef ENDL
}


///////////////////
// Main function //
///////////////////

static void PreEventStuff()
{
	emulator_on = emulator->IsCartridgeFileLoaded() || emulator->IsCDFileLoaded();
	emulator_running = emulator_on && (!emulator_paused || emulator_frame_advance) && !file_utilities.IsDialogOpen() && !emulator->RewindingExhausted();

	emulator_frame_advance = false;
}

bool Frontend::Initialise(const int argc, char** const argv, const FrameRateCallback &frame_rate_callback_param)
{
	frame_rate_callback = frame_rate_callback_param;

	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, "ClownMDEmu");
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, VERSION);
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, "com.clownacy.clownmdemu");
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, "Clownacy");
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "Copyright (c) 2025 Clownacy");
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, "https://github.com/Clownacy/clownmdemu-frontend");
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game");

	// Initialise SDL
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD))
	{
		debug_log.Log("SDL_Init failed with the following message - '{}'", SDL_GetError());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to initialise SDL. The program will now close.", nullptr);
	}
	else
	{
		IMGUI_CHECKVERSION();

		window.emplace(DEFAULT_TITLE, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, true);
		emulator.emplace(window->framebuffer_texture, ReadInputCallback);

		LoadConfiguration();

		if (!native_windows)
			ImGui::LoadIniSettingsFromDisk(reinterpret_cast<const char*>(GetDearImGuiSettingsFilePath().u8string().c_str()));

		// We shouldn't resize the window if something is overriding its size.
		// This is needed for the Emscripen build to work correctly in a full-window HTML canvas.
		int window_width, window_height;
		SDL_GetWindowSize(window->GetSDLWindow(), &window_width, &window_height);
		const float scale = window->GetSizeScale();

		if (window_width == static_cast<int>(INITIAL_WINDOW_WIDTH * scale) && window_height == static_cast<int>(INITIAL_WINDOW_HEIGHT * scale))
		{
			// Resize the window so that there's room for the menu bar.
			SDL_SetWindowSize(window->GetSDLWindow(), static_cast<int>(INITIAL_WINDOW_WIDTH * scale), static_cast<int>((INITIAL_WINDOW_HEIGHT + window->GetMenuBarSize()) * scale));
		}
#ifdef FILE_PATH_SUPPORT
		// If the user passed the path to the software on the command line, then load it here, automatically.
		if (argc > 1)
		{
			const auto path = reinterpret_cast<const char8_t*>(argv[1]);
			LoadSoftwareFile(CDReader::IsMegaCDGame(path), path);
		}
#endif
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

	// Destroy windows BEFORE we shut-down SDL.
	std::apply(
		[]<typename... Ts>(const Ts&... windows)
		{
			(windows->reset(), ...);
		}, popup_windows
	);

	framebuffer_texture_upscaled.release();

	if (!native_windows)
		ImGui::SaveIniSettingsToDisk(reinterpret_cast<const char*>(GetDearImGuiSettingsFilePath().u8string().c_str()));

	SaveConfiguration();

#ifdef FILE_PATH_SUPPORT
	// Free recent software list.
	// TODO: Once the frontend is a class, this won't be necessary.
	recent_software_list.clear();
#endif

	// TODO: Once the frontend is a class, this won't be necessary.
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
				switch (keyboard_bindings[event.key.scancode])
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

					case INPUT_BINDING_REWIND:
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
				debug_log.Log("SDL_OpenGamepad failed with the following message - '{}'", SDL_GetError());
			}
			else
			{
				try
				{
					controller_input_list.emplace_front(event.gdevice.which);
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
				debug_log.Log("SDL_GetGamepadFromID failed with the following message - '{}'", SDL_GetError());
			else
				SDL_CloseGamepad(controller);

			controller_input_list.remove_if([&event](const ControllerInput &controller_input){ return controller_input.joystick_instance_id == event.gdevice.which;});

			break;
		}

		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			if (event.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK)
			{
				// Toggle Dear ImGui gamepad controls.
				io.ConfigFlags ^= ImGuiConfigFlags_NavEnableGamepad;
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
						#define DO_BUTTON(state, code) case code: controller_input.input.buttons[state] = pressed; break

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
							unsigned int button;

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
							controller_input.input.buttons[button] = controller_input.left_stick[direction] || controller_input.dpad[direction];

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
			drag_and_drop_filename = reinterpret_cast<const char8_t*>(event.drop.data);
			break;

		default:
			break;
	}

	if (give_event_to_imgui)
		ImGui_ImplSDL3_ProcessEvent(&event);
}

void Frontend::HandleEvent(const SDL_Event &event)
{
	SDL_Window* const event_window = SDL_GetWindowFromEvent(&event);

	if (event_window == nullptr)
	{
		HandleMainWindowEvent(event);
	}
	else if (event_window == window->GetSDLWindow())
	{
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

		std::apply(
			[&]<typename... Ts>(Ts const&... windows)
			{
				(DoWindow(*windows), ...);
			}, popup_windows
		);
	}
}

void Frontend::Update()
{
	if (emulator_running)
	{
		emulator->Update(fast_forward_in_progress);
		++frame_counter;
	}

	window->StartDearImGuiFrame();

	// Handle drag-and-drop event.
	if (!file_utilities.IsDialogOpen() && !drag_and_drop_filename.empty())
	{
		if (CDReader::IsMegaCDGame(drag_and_drop_filename))
		{
			LoadCDFile(drag_and_drop_filename, SDL::IOFromFile(drag_and_drop_filename, "rb"));
			emulator_paused = false;
		}
		else
		{
			auto file_buffer = file_utilities.LoadFileToBuffer(drag_and_drop_filename);

			if (file_buffer.has_value())
			{
				if (emulator->ValidateSaveStateFile(*file_buffer))
				{
					if (emulator_on)
						LoadSaveState(*file_buffer);
				}
				else
				{
					LoadCartridgeFile(drag_and_drop_filename, std::move(*file_buffer));
					emulator_paused = false;
				}
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

	const bool show_menu_bar = !window->GetFullscreen()
							|| pop_out
							|| AnyPopupsOpen()
							|| (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0;

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
					file_utilities.LoadFile(*window, "Load Cartridge File", [](const std::filesystem::path &path, SDL::IOStream &&file)
					{
						const bool success = LoadCartridgeFile(path, file);

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
					file_utilities.LoadFile(*window, "Load CD File", [](const std::filesystem::path &path, SDL::IOStream &&file)
					{
						if (!LoadCDFile(path, std::move(file)))
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
						ImGui::PushID(&recent_software);

						// Display only the filename.
						if (ImGui::MenuItem(reinterpret_cast<const char*>(recent_software.path.filename().u8string().c_str())))
							selected_software = &recent_software;

						ImGui::PopID();

						// Show the full path as a tooltip.
						DoToolTip(recent_software.path.u8string());
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

							SDL::IOStream file = SDL::IOStream(SDL_IOFromMem(save_state_buffer.data(), save_state_buffer.size()));

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
					file_utilities.LoadFile(*window, "Load Save State", []([[maybe_unused]] const std::filesystem::path &path, SDL::IOStream &&file)
					{
						LoadSaveState(file);
						return true;
					});

				ImGui::EndMenu();
			}

			const auto PopupButton = []<typename T>(const char* const label, std::optional<T> &window, const int width, const int height, const bool resizeable, const std::optional<float> forced_scale = std::nullopt, const char* const title = nullptr)
			{
				if (ImGui::MenuItem(label, nullptr, window.has_value()))
				{
					if (window.has_value())
						window.reset();
					else
						window.emplace(title == nullptr ? label : title, width, height, resizeable, *::window, forced_scale, native_windows ? nullptr : &*::window);
				}
			};

			if (ImGui::BeginMenu("Debugging"))
			{
				PopupButton("Log", debug_log_window, 800, 600, true);

				PopupButton("Toggles", debugging_toggles_window, 234, 444, false);

				PopupButton("68000 Disassembler", m68k_disassembler_window, 380, 410, true);

				ImGui::Separator();

				PopupButton("Frontend", debug_frontend_window, 164, 324, false);

				if (ImGui::BeginMenu("Main-68000"))
				{
					PopupButton("Registers", m68k_status_window, 376, 120, false, std::nullopt, "Main-68000 Registers");
					PopupButton("WORK-RAM", m68k_ram_viewer_window, 400, 400, true);
					PopupButton("External RAM", external_ram_viewer_window, 460, 460, true);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Sub-68000"))
				{
					PopupButton("Registers", mcd_m68k_status_window, 376, 120, false, std::nullopt, "Sub-68000 Registers");
					PopupButton("PRG-RAM", prg_ram_viewer_window, 410, 410, true);
					PopupButton("WORD-RAM", word_ram_viewer_window, 410, 410, true);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Z80"))
				{
					PopupButton("Registers", z80_status_window, 390, 80, false, std::nullopt, "Z80 Registers");
					PopupButton("SOUND-RAM", z80_ram_viewer_window, 460, 460, true);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("VDP"))
				{
					PopupButton("Registers", vdp_registers_window, 360, 924, false, std::nullopt, "VDP Registers");
					PopupButton("Sprites", sprite_list_window, 540, 344, true);
					PopupButton("VRAM", vram_viewer_window, 460, 460, true);
					PopupButton("CRAM", cram_viewer_window, 384, 192, false);
					PopupButton("VSRAM", vsram_viewer_window, 384, 192, false);
					ImGui::SeparatorText("Visualisers");
					ImGui::PushID("Visualisers");
					PopupButton("Sprite Plane", sprite_plane_visualiser_window, 540, 610, true, 1.0f);
					PopupButton("Window Plane", window_plane_visualiser_window, 540, 610, true, 1.0f);
					PopupButton("Plane A", plane_a_visualiser_window, 1050, 610, true, 1.0f);
					PopupButton("Plane B", plane_b_visualiser_window, 1050, 610, true, 1.0f);
					PopupButton("Tiles", tile_visualiser_window, 530, 530, true);
					PopupButton("Colours", colour_visualiser_window, 456, 186, false);
					PopupButton("Stamps", stamp_visualiser_window, 530, 530, true);
					PopupButton("Stamp Map", stamp_map_visualiser_window, 1050, 610, true, 1.0f);
					ImGui::PopID();
					ImGui::EndMenu();
				}

				PopupButton("FM", fm_status_window, 540, 920, false, std::nullopt, "FM Registers");

				PopupButton("PSG", psg_status_window, 290, 264, false, std::nullopt, "PSG Registers");

				if (ImGui::BeginMenu("PCM"))
				{
					PopupButton("Registers", pcm_status_window, 630, 200, false, std::nullopt, "PCM Registers");
					PopupButton("WAVE-RAM", wave_ram_viewer_window, 460, 460, true);
					ImGui::EndMenu();
				}

				PopupButton("CDC", cdc_status_window, 200, 364, false);

				PopupButton("CDDA", cdda_status_window, 180, 324, false);

				PopupButton("Other", other_status_window, 350, 810, false);

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Misc."))
			{
				if (ImGui::MenuItem("Fullscreen", nullptr, window->GetFullscreen()))
					window->ToggleFullscreen();

				if (!native_windows)
				{
					ImGui::MenuItem("Display Window", nullptr, &pop_out);
					ImGui::Separator();
				}

				PopupButton("Options", options_window, 360, 360, true);

				PopupButton("About", about_window, 626, 430, true);

			#ifndef __EMSCRIPTEN__
				ImGui::Separator();

				if (ImGui::MenuItem("Exit"))
					quit = true;
			#endif

				ImGui::EndMenu();
			}

		#ifndef NDEBUG
			if (ImGui::BeginMenu("Development"))
			{
				ImGui::MenuItem("Dear ImGui Demo Window", nullptr, &dear_imgui_demo_window);
				ImGui::MenuItem("Native File Dialogs", nullptr, &file_utilities.use_native_file_dialogs);
				ImGui::EndMenu();
			}
		#endif

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
		ImGui::SetItemDefaultFocus();

		emulator_has_focus = ImGui::IsItemFocused();

		if (emulator_on)
		{
			ImGui::SetCursorPos(cursor);

			SDL_Texture *selected_framebuffer_texture = window->framebuffer_texture;

			const unsigned int work_width = static_cast<unsigned int>(size_of_display_region.x);
			const unsigned int work_height = static_cast<unsigned int>(size_of_display_region.y);

			unsigned int destination_width;
			unsigned int destination_height;

			switch (emulator->GetCurrentScreenHeight())
			{
				default:
					assert(false);
					[[fallthrough]];
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
				if (framebuffer_texture_upscaled && (destination_width_scaled % destination_width != 0 || destination_height_scaled % destination_height != 0))
				{
					// Render the upscaled framebuffer to the screen.
					selected_framebuffer_texture = framebuffer_texture_upscaled;

					// Before we can do that though, we have to actually render the upscaled framebuffer.
					SDL_FRect framebuffer_rect;
					framebuffer_rect.x = 0;
					framebuffer_rect.y = 0;
					framebuffer_rect.w = emulator->GetCurrentScreenWidth();
					framebuffer_rect.h = emulator->GetCurrentScreenHeight();

					SDL_FRect upscaled_framebuffer_rect;
					upscaled_framebuffer_rect.x = 0;
					upscaled_framebuffer_rect.y = 0;
					upscaled_framebuffer_rect.w = destination_width * framebuffer_upscale_factor;
					upscaled_framebuffer_rect.h = destination_height * framebuffer_upscale_factor;

					// Render to the upscaled framebuffer.
					SDL_SetRenderTarget(window->GetRenderer(), framebuffer_texture_upscaled);

					// Render.
					SDL_RenderTexture(window->GetRenderer(), window->framebuffer_texture, &framebuffer_rect, &upscaled_framebuffer_rect);

					// Switch back to actually rendering to the screen.
					SDL_SetRenderTarget(window->GetRenderer(), nullptr);

					// Update the texture UV to suit the upscaled framebuffer.
					uv1.x = static_cast<float>(upscaled_framebuffer_rect.w) / static_cast<float>(framebuffer_texture_upscaled_width);
					uv1.y = static_cast<float>(upscaled_framebuffer_rect.h) / static_cast<float>(framebuffer_texture_upscaled_height);

					upscale_width = upscaled_framebuffer_rect.w;
					upscale_height = upscaled_framebuffer_rect.h;
				}
			}

			// Center the framebuffer in the available region.
			ImGui::SetCursorPosX(static_cast<float>(static_cast<int>(ImGui::GetCursorPosX()) + (static_cast<int>(size_of_display_region.x) - destination_width_scaled) / 2));
			ImGui::SetCursorPosY(static_cast<float>(static_cast<int>(ImGui::GetCursorPosY()) + (static_cast<int>(size_of_display_region.y) - destination_height_scaled) / 2));

			// Draw the upscaled framebuffer in the window.
			ImGui::Image(selected_framebuffer_texture, ImVec2(static_cast<float>(destination_width_scaled), static_cast<float>(destination_height_scaled)), ImVec2(0, 0), uv1);

			output_width = destination_width_scaled;
			output_height = destination_height_scaled;
		}
	}

	ImGui::End();

	const auto &clownmdemu = emulator->CurrentState().clownmdemu;

	const auto DisplayWindow = []<typename T, typename... Ts>(std::optional<T> &window, Ts&&... arguments)
	{
		if (window.has_value())
			if (!window->Display(std::forward<Ts>(arguments)...))
				window.reset();
	};

	DisplayWindow(debug_log_window);
	DisplayWindow(debugging_toggles_window);
	DisplayWindow(m68k_disassembler_window);
	DisplayWindow(debug_frontend_window);
	DisplayWindow(m68k_status_window, clownmdemu.m68k.state);
	DisplayWindow(mcd_m68k_status_window, clownmdemu.mega_cd.m68k.state);
	DisplayWindow(z80_status_window);
	DisplayWindow(m68k_ram_viewer_window, clownmdemu.m68k.ram);
	DisplayWindow(external_ram_viewer_window, clownmdemu.external_ram.buffer);
	DisplayWindow(z80_ram_viewer_window, clownmdemu.z80.ram);
	DisplayWindow(prg_ram_viewer_window, clownmdemu.mega_cd.prg_ram.buffer);
	DisplayWindow(word_ram_viewer_window, clownmdemu.mega_cd.word_ram.buffer);
	DisplayWindow(wave_ram_viewer_window, clownmdemu.mega_cd.pcm.wave_ram);
	DisplayWindow(vdp_registers_window);
	DisplayWindow(sprite_list_window);
	DisplayWindow(vram_viewer_window, clownmdemu.vdp.vram);
	DisplayWindow(cram_viewer_window, clownmdemu.vdp.cram);
	DisplayWindow(vsram_viewer_window, clownmdemu.vdp.vsram);
	DisplayWindow(sprite_plane_visualiser_window);
	{
		const auto window_plane_width = clownmdemu.vdp.h40_enabled ? 64 : 32;
		const auto window_plane_height = 32;
		DisplayWindow(window_plane_visualiser_window, clownmdemu.vdp.window_address, window_plane_width, window_plane_height);
	}
	{
		const auto scrolling_plane_width = clownmdemu.vdp.plane_width_bitmask + 1;
		const auto scrolling_plane_height = clownmdemu.vdp.plane_height_bitmask + 1;
		DisplayWindow(plane_a_visualiser_window, clownmdemu.vdp.plane_a_address, scrolling_plane_width, scrolling_plane_height);
		DisplayWindow(plane_b_visualiser_window, clownmdemu.vdp.plane_b_address, scrolling_plane_width, scrolling_plane_height);
	}
	DisplayWindow(tile_visualiser_window);
	DisplayWindow(colour_visualiser_window);
	DisplayWindow(stamp_visualiser_window);
	DisplayWindow(stamp_map_visualiser_window);
	DisplayWindow(fm_status_window);
	DisplayWindow(psg_status_window);
	DisplayWindow(pcm_status_window);
	DisplayWindow(cdc_status_window);
	DisplayWindow(cdda_status_window);
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

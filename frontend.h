#ifndef FRONTEND_H
#define FRONTEND_H

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "SDL.h"

#include "common/core/clownmdemu.h"

#include "debug-log.h"
#include "emulator-instance.h"
#include "file-utilities.h"
#include "windows/common/window-with-framebuffer.h"

void DoToolTip(const std::u8string &text);

namespace Frontend
{
	struct Input
	{
		unsigned int bound_joypad = 0;
		std::array<unsigned char, CLOWNMDEMU_BUTTON_MAX> buttons = {0};
		unsigned char fast_forward = 0;
		unsigned char rewind = 0;
	};

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

	using FrameRateCallback = std::function<void(bool pal_mode)>;

	extern float dpi_scale;
	extern std::optional<EmulatorInstance> emulator;
	extern std::optional<WindowWithFramebuffer> window;
	extern FileUtilities file_utilities;
	extern unsigned int frame_counter;
	extern SDL::Texture framebuffer_texture_upscaled;

	extern Input keyboard_input;
	extern std::array<InputBinding, SDL_NUM_SCANCODES> keyboard_bindings; // TODO: `SDL_NUM_SCANCODES` is an internal macro, so use something standard!

	extern unsigned int output_width, output_height;
	extern unsigned int upscale_width, upscale_height;

	extern bool use_vsync;
	extern bool integer_screen_scaling;
	extern bool tall_double_resolution_mode;
	extern bool fast_forward_in_progress;
	extern bool dear_imgui_windows;

	std::filesystem::path GetConfigurationDirectoryPath();
	bool GetUpscaledFramebufferSize(unsigned int &width, unsigned int &height);
	void SetAudioPALMode(bool enabled);
	bool Initialise(const int argc, char** const argv, const FrameRateCallback &frame_rate_callback);
	void HandleEvent(const SDL_Event &event);
	void Update();
	void Deinitialise();
	bool WantsToQuit();
	template<typename T>
	T DivideByPALFramerate(T value) { return CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(value); }
	template<typename T>
	T DivideByNTSCFramerate(T value) { return CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(value); };
}

#endif /* FRONTEND_H */

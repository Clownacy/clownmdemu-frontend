#ifndef FRONTEND_H
#define FRONTEND_H

#include <functional>
#include <optional>

#include "SDL.h"

#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#include "debug-log.h"
#include "emulator-instance.h"
#include "file-utilities.h"

namespace Frontend
{
	using FrameRateCallback = std::function<void(bool pal_mode)>;

	extern DebugLog debug_log;
	extern float dpi_scale;
	extern std::optional<EmulatorInstance> emulator;
	extern FileUtilities file_utilities;
	extern unsigned int frame_counter;

	extern unsigned int output_width, output_height;
	extern unsigned int upscale_width, upscale_height;

	bool GetUpscaledFramebufferSize(unsigned int &width, unsigned int &height);
	bool Initialise(const int argc, char** const argv, const FrameRateCallback &frame_rate_callback);
	void HandleEvent(const SDL_Event &event);
	void Update();
	void Deinitialise();
	bool WantsToQuit();
	bool IsFastForwarding();
	template<typename T>
	T DivideByPALFramerate(T value) { return CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(value); }
	template<typename T>
	T DivideByNTSCFramerate(T value) { return CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(value); };
}

#endif /* FRONTEND_H */

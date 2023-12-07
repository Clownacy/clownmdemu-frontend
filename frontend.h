#ifndef FRONTEND_H
#define FRONTEND_H

#include <functional>

#include "SDL.h"

#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

namespace Frontend
{
	using FrameRateCallback = std::function<void(bool pal_mode)>;

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

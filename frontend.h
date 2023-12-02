#ifndef FRONTEND_H
#define FRONTEND_H

#include <functional>

#include "SDL.h"

namespace Frontend
{
	typedef std::function<void(bool pal_mode)> FrameRateCallback;

	bool Initialise(const int argc, char** const argv, const FrameRateCallback &frame_rate_callback);
	void HandleEvent(const SDL_Event &event);
	void Update();
	void Deinitialise();
	bool WantsToQuit();
	bool IsFastForwarding();
	Uint32 DivideByPALFramerate(Uint32 value);
	Uint32 DivideByNTSCFramerate(Uint32 value);
}

#endif /* FRONTEND_H */

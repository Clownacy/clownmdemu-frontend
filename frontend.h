#ifndef FRONTEND_H
#define FRONTEND_H

#include "SDL.h"

namespace Frontend
{
	bool Initialise(const int argc, char** const argv);
	void HandleEvent(const SDL_Event &event);
	void Update();
	void Deinitialise();
	bool WantsToQuit();
	bool PALMode();
	bool IsFastForwarding();
	Uint32 DivideByPALFramerate(Uint32 value);
	Uint32 DivideByNTSCFramerate(Uint32 value);
}

#endif /* FRONTEND_H */

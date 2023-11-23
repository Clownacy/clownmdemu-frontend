#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "frontend.h"

int main(const int argc, char** const argv)
{
#ifdef __EMSCRIPTEN__
	if (Frontend::Initialise(argc, argv))
	{
		const auto callback = []()
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
				Frontend::HandleEvent(event);

			Frontend::Update();

			if (Frontend::WantsToQuit())
			{
				Frontend::Deinitialise();
				emscripten_cancel_main_loop();
			}
		};

		// TODO: 50FPS (PAL)
		emscripten_set_main_loop(callback, 60, 0);
	}
#else
	if (Frontend::Initialise(argc, argv))
	{
		while (!Frontend::WantsToQuit())
		{
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

					if (Frontend::PALMode())
					{
						// Run at 50FPS
						delta = Frontend::DivideByPALFramerate(1000ul * MULTIPLIER);
					}
					else
					{
						// Run at roughly 59.94FPS (60 divided by 1.001)
						delta = Frontend::DivideByNTSCFramerate(1000ul * MULTIPLIER);
					}

					next_time += delta >> (Frontend::IsFastForwarding() ? 2 : 0);

					break;
				}

				#undef MULTIPLIER

				Frontend::HandleEvent(event);
			}

			Frontend::Update();
		}

		Frontend::Deinitialise();
	}
#endif

	return EXIT_SUCCESS;
}

#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "frontend.h"

#ifdef __EMSCRIPTEN__
static void Terminate()
{
	Frontend::Deinitialise();
	emscripten_cancel_main_loop();

	// Deinitialise persistent storage.
	EM_ASM({
		FS.syncfs(false, function (err) {
			Module._StorageLoaded();
		});
	}, 0);
}

static int EventFilter(void* /*const userdata*/, SDL_Event* const event)
{
	if (event->type == SDL_APP_TERMINATING)
		Terminate();

	return 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE void StorageLoaded()
{
	if (Frontend::Initialise(0, nullptr))
	{
		SDL_AddEventWatch(EventFilter, nullptr);

		const auto callback = []()
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
				Frontend::HandleEvent(event);

			Frontend::Update();

			if (Frontend::WantsToQuit())
				Terminate();
		};

		// TODO: 50FPS (PAL)
		emscripten_set_main_loop(callback, 60, 0);
	}
}
#endif

int main(const int argc, char** const argv)
{
#ifdef __EMSCRIPTEN__
	static_cast<void>(argc);
	static_cast<void>(argv);

	// Initialise persistent storage.
	EM_ASM({
		FS.mkdir('/clownmdemu-frontend');
		FS.mount(IDBFS, {}, '/clownmdemu-frontend');
		FS.chdir('/clownmdemu-frontend');

		FS.syncfs(true, function (err) {
			Module._StorageLoaded();
		});
	}, 0);
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

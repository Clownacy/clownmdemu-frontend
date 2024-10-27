#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "frontend.h"

#ifdef __EMSCRIPTEN__
static double next_time;
static double time_delta;

static void Terminate()
{
	Frontend::Deinitialise();
	emscripten_cancel_main_loop();

	// Deinitialise persistent storage.
	EM_ASM({
		FS.syncfs(false, function (err) {});
	}, 0);
}

static void FrameRateCallback(const bool pal_mode)
{
	time_delta = pal_mode ? Frontend::DivideByPALFramerate(1000.0) : Frontend::DivideByNTSCFramerate(1000.0);
}

static int EventFilter(void* /*const userdata*/, SDL_Event* const event)
{
	// The event loop will never have time to catch this, so we
	// must use this callback to intercept it as soon as possible.
	if (event->type == SDL_EVENT_TERMINATING)
		Terminate();

	return 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE void StorageLoaded()
{
	const auto callback = []()
	{
		const double current_time = emscripten_get_now();

		if (current_time >= next_time)
		{
			// If massively delayed, resynchronise to avoid fast-forwarding.
			if (current_time >= next_time + 100.0)
				next_time = current_time;

			next_time += time_delta / (Frontend::IsFastForwarding() ? 4 : 1);

			SDL_Event event;
			while (SDL_PollEvent(&event))
				Frontend::HandleEvent(event);

			Frontend::Update();

			if (Frontend::WantsToQuit())
				Terminate();
		}
	};

	next_time = emscripten_get_now();

	emscripten_set_main_loop(callback, 0, 0);

	if (Frontend::Initialise(0, nullptr, FrameRateCallback))
		SDL_AddEventWatch(EventFilter, nullptr);
}
#else
// 300 is the magic number that prevents the frame delta calculations from ever dipping into
// numbers smaller than 1 (technically it's only required by the NTSC framerate: PAL doesn't need it).
#define FRAME_DELTA_MULTIPLIER 300

static Uint32 frame_rate_delta;

static void FrameRateCallback(const bool pal_mode)
{
	if (pal_mode)
	{
		// Run at 50FPS
		frame_rate_delta = Frontend::DivideByPALFramerate(1000ul * FRAME_DELTA_MULTIPLIER);
	}
	else
	{
		// Run at roughly 59.94FPS (60 divided by 1.001)
		frame_rate_delta = Frontend::DivideByNTSCFramerate(1000ul * FRAME_DELTA_MULTIPLIER);
	}
}
#endif

int main([[maybe_unused]] const int argc, [[maybe_unused]] char** const argv)
{
#ifdef __EMSCRIPTEN__
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
	if (Frontend::Initialise(argc, argv, FrameRateCallback))
	{
		while (!Frontend::WantsToQuit())
		{
			// This loop processes events and manages the framerate.
			for (;;)
			{
				static Uint64 next_time;
				const Uint64 current_time = SDL_GetTicks() * FRAME_DELTA_MULTIPLIER;

				int timeout = 0;

				if (current_time < next_time)
					timeout = (next_time - current_time) / FRAME_DELTA_MULTIPLIER;
				else if (current_time > next_time + 100 * FRAME_DELTA_MULTIPLIER) // If we're way past our deadline, then resync to the current tick instead of fast-forwarding
					next_time = current_time;

				// Obtain an event
				SDL_Event event;
				if (!SDL_WaitEventTimeout(&event, timeout)) // If the timeout has expired and there are no more events, exit this loop
				{
					// Calculate when the next frame will be
					next_time += frame_rate_delta >> (Frontend::IsFastForwarding() ? 2 : 0);
					break;
				}

				Frontend::HandleEvent(event);
			}

			Frontend::Update();
		}

		Frontend::Deinitialise();
	}
#endif

	return EXIT_SUCCESS;
}

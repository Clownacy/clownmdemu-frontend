#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#ifndef __EMSCRIPTEN__
#define SDL_MAIN_USE_CALLBACKS
#endif
#include <SDL3/SDL_main.h>

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

int main([[maybe_unused]] const int argc, [[maybe_unused]] char** const argv)
{
	// Initialise persistent storage.
	EM_ASM({
		FS.mkdir('/clownmdemu-frontend');
		FS.mount(IDBFS, {}, '/clownmdemu-frontend');
		FS.chdir('/clownmdemu-frontend');

		FS.syncfs(true, function(err) {
			Module._StorageLoaded();
		});
		}, 0);
	return EXIT_SUCCESS;
}

#else
static Uint64 time_delta;

static void FrameRateCallback(const bool pal_mode)
{
	if (pal_mode)
	{
		// Run at 50FPS
		time_delta = Frontend::DivideByPALFramerate(1000ul);
	}
	else
	{
		// Run at roughly 59.94FPS (60 divided by 1.001)
		time_delta = Frontend::DivideByNTSCFramerate(1000ul);
	}
}

SDL_AppResult SDL_AppInit([[maybe_unused]] void** const appstate, const int argc, char** const argv)
{
	return Frontend::Initialise(argc, argv, FrameRateCallback) ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppIterate([[maybe_unused]] void* const appstate)
{
	static Uint64 next_time;

	const Uint64 current_time = SDL_GetTicks();

	if (current_time < next_time)
		return SDL_APP_CONTINUE;

	// If massively delayed, resynchronise to avoid fast-forwarding.
	if (current_time >= next_time + 100)
		next_time = current_time;

	next_time += time_delta / (Frontend::IsFastForwarding() ? 4 : 1);

	Frontend::Update();
	return Frontend::WantsToQuit() ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent([[maybe_unused]] void* const appstate, SDL_Event* const event)
{
	Frontend::HandleEvent(*event);
	return Frontend::WantsToQuit() ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void SDL_AppQuit([[maybe_unused]] void* const appstate, [[maybe_unused]] const SDL_AppResult result)
{
	Frontend::Deinitialise();
}
#endif

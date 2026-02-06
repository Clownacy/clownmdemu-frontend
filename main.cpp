#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#else
#include <sstream>
#include <lyra/lyra.hpp>
#endif

#ifndef __EMSCRIPTEN__
#define SDL_MAIN_USE_CALLBACKS
#endif
#include <SDL3/SDL_main.h>

#include "file-utilities.h"
#include "frontend.h"

#ifdef __EMSCRIPTEN__
static std::filesystem::path cartridge_file_path, disc_file_path;

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

static bool EventFilter(void* /*const userdata*/, SDL_Event* const event)
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

			next_time += time_delta;

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

	if (Frontend::Initialise(FrameRateCallback, false, "", cartridge_file_path, disc_file_path))
		SDL_AddEventWatch(EventFilter, nullptr);
}

int main(const int argc_p, char** const argv_p)
{
	static int argc = argc_p;
	static char **argv = argv_p;

	const auto &CartridgeFileLoaded = [](const char* const cartridge_file_path_p)
	{
		cartridge_file_path = cartridge_file_path_p;

		const auto &DiscFileLoaded = [](const char* const disc_file_path_p)
		{
			disc_file_path = disc_file_path_p;

			// Initialise persistent storage.
			EM_ASM({
				FS.mount(IDBFS, {}, UTF8ToString($0));

				FS.syncfs(true, function(err) {
					Module._StorageLoaded();
				});
				}, Frontend::GetDefaultConfigurationDirectoryPath().u8string().c_str());
		};

		if (argc > 2 && argv[2][0] != '\0')
			emscripten_async_wget(argv[2], "disc", DiscFileLoaded, DiscFileLoaded);
		else
			DiscFileLoaded("");
	};

	if (argc > 1 && argv[1][0] != '\0')
		emscripten_async_wget(argv[1], "cartridge", CartridgeFileLoaded, CartridgeFileLoaded);
	else
		CartridgeFileLoaded("");

	return EXIT_SUCCESS;
}

#else
static bool frontend_initialised = false;
static Uint64 time_delta;

static void FrameRateCallback(const bool pal_mode)
{
	if (pal_mode)
	{
		// Run at 50FPS
		time_delta = Frontend::DivideByPALFramerate(SDL_NS_PER_SECOND);
	}
	else
	{
		// Run at roughly 59.94FPS (60 divided by 1.001)
		time_delta = Frontend::DivideByNTSCFramerate(SDL_NS_PER_SECOND);
	}
}

SDL_AppResult SDL_AppInit([[maybe_unused]] void** const appstate, const int argc, char** const argv)
{
	std::string user_data_path_raw, cartridge_path_raw, cd_path_raw, cartridge_or_cd_path_raw;
	bool fullscreen = false;
	bool help = false;

	const auto cli = lyra::help(help).description("ClownMDEmu " VERSION " - A Sega Mega Drive emulator.")
		| lyra::opt(fullscreen)
			["-f"]["--fullscreen"]
			("Start the emulator in fullscreen.")
		| lyra::opt(user_data_path_raw, "path")
			["-u"]["--user"]
			("Directory to store user data, such as settings and save data.")
		| lyra::opt(cartridge_path_raw, "path")
			["-c"]["--cartridge"]
			("Cartridge software to load.")
		| lyra::opt(cd_path_raw, "path")
			["-d"]["--disc"]
			("Disc software to load.")
		| lyra::arg(cartridge_or_cd_path_raw, "path")
			("Cartridge or disc software to load.");

	const auto result = cli.parse({argc, argv});

	if (!result)
	{
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error in command line: %s\n", result.message().c_str());
		return SDL_APP_FAILURE;
	}

	if (help)
	{
		std::stringstream stream;
		stream << cli;
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s\n", stream.str().c_str());
		return SDL_APP_SUCCESS;
	}

	auto user_data_path = FileUtilities::U8Path(user_data_path_raw);
	auto cartridge_path = FileUtilities::U8Path(cartridge_path_raw);
	auto cd_path = FileUtilities::U8Path(cd_path_raw);
	const auto &cartridge_or_cd_path = FileUtilities::U8Path(cartridge_or_cd_path_raw);

	if (!cartridge_or_cd_path.empty())
	{
		if (Frontend::IsFileCD(cartridge_or_cd_path))
			cd_path = cartridge_or_cd_path;
		else
			cartridge_path = cartridge_or_cd_path;
	}

	frontend_initialised = Frontend::Initialise(FrameRateCallback, fullscreen, user_data_path, cartridge_path, cd_path);

	return frontend_initialised ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppIterate([[maybe_unused]] void* const appstate)
{
	static Uint64 next_time;

	const Uint64 current_time = SDL_GetTicksNS();

	if (current_time < next_time)
		return SDL_APP_CONTINUE;

	// If massively delayed, resynchronise to avoid fast-forwarding.
	if (current_time >= next_time + SDL_NS_PER_SECOND / 10)
		next_time = current_time;

	next_time += time_delta;

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
	if (frontend_initialised)
		Frontend::Deinitialise();
}
#endif

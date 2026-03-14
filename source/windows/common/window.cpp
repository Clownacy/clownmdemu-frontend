#include "window.h"

#include <algorithm>
#include <stdexcept>

#include "../../common/core/libraries/clowncommon/clowncommon.h"

#include "../../frontend.h"
#include "../../tar.h"
#include "winapi.h"

#ifndef SDL_PLATFORM_WIN32
namespace WindowIcon
{
	static
	#include "../../assets/icon/archive.tar.lzma.h"
}

static SDL::Surface LoadWindowIcon()
{
	TarBall archive(WindowIcon::buffer, WindowIcon::uncompressed_size, TarBall::Compression::LZMA);

	const auto &LoadPNG = [&](const std::filesystem::path &path) -> SDL::Surface
	{
		const auto &file = archive.OpenFile(path);

		if (!file.has_value())
			return nullptr;

		return SDL::Surface(SDL_LoadPNG_IO(SDL::IOStream(std::data(*file), std::size(*file)), false));
	};

	auto surface = LoadPNG("icon-16.png");
	SDL_AddSurfaceAlternateImage(surface, LoadPNG("icon-20.png"));
	SDL_AddSurfaceAlternateImage(surface, LoadPNG("icon-24.png"));
	SDL_AddSurfaceAlternateImage(surface, LoadPNG("icon-32.png"));
	SDL_AddSurfaceAlternateImage(surface, LoadPNG("icon-512.png"));

	return surface;
}
#endif

static float HandleDPIError(const float dpi_scale)
{
	// Prevent any insanity if we somehow get bad values.
	if (dpi_scale == 0.0f)
		return 1.0f;

	return dpi_scale;
}

float Window::GetDisplayDPIScale()
{
	const SDL_DisplayID display_index = SDL_GetPrimaryDisplay();

	if (display_index == 0)
	{
		Frontend::debug_log.SDLError("SDL_GetPrimaryDisplay");
		return 1.0f;
	}

	return HandleDPIError(SDL_GetDisplayContentScale(display_index));
}

float Window::GetSizeScale()
{
	const SDL_DisplayID display_index = SDL_GetDisplayForWindow(GetSDLWindow());

	if (display_index == 0)
	{
		Frontend::debug_log.SDLError("SDL_GetDisplayForWindow");
		return 1.0f;
	}

	return HandleDPIError(SDL_GetDisplayContentScale(display_index));
}

float Window::GetDPIScale()
{
	return HandleDPIError(SDL_GetWindowDisplayScale(GetSDLWindow()));
}

void Window::SetFullscreen(const bool enabled)
{
	if (!SDL_SetWindowFullscreen(GetSDLWindow(), enabled))
		Frontend::debug_log.SDLError("SDL_SetWindowFullscreen");

	if (!enabled)
		DisableRounding();
}

Window::Window(const char* const window_title, const float window_width, const float window_height, const bool resizeable, const std::optional<float> &forced_scale, Uint32 window_flags)
{
	const float scale = forced_scale.value_or(GetDisplayDPIScale());

	window_flags |= SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

	if (resizeable)
		window_flags |= SDL_WINDOW_RESIZABLE;

	// SDL3's documentation currently advises using this instead of 'SDL_CreateWindow'
	// and 'SDL_CreateRenderer' separately to avoid "window flicker".
	// https://wiki.libsdl.org/SDL3/SDL_CreateWindow
	SDL_Window *window;
	SDL_Renderer *renderer;
	if (!SDL_CreateWindowAndRenderer(window_title, static_cast<int>(window_width * scale), static_cast<int>(window_height * scale), window_flags, &window, &renderer))
		throw std::runtime_error(DebugLog::GetSDLErrorMessage("SDL_CreateWindowAndRenderer"));

	sdl_window = SDL::Window(window);
	this->renderer = SDL::Renderer(renderer);

	DisableRounding();

#ifndef SDL_PLATFORM_WIN32
	static SDL::Surface window_icon = LoadWindowIcon();
	SDL_SetWindowIcon(window, window_icon);
#endif
}

void Window::SetTitleBarColour(const unsigned char red, const unsigned char green, const unsigned char blue)
{
	SetWindowTitleBarColour(sdl_window, red, green, blue);
}

void Window::DisableRounding()
{
	DisableWindowRounding(sdl_window);
}

void Window::ShowWarningMessageBox(const char* const message)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning", message, GetSDLWindow());
}

void Window::ShowErrorMessageBox(const char* const message)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message, GetSDLWindow());
}

void Window::ShowFatalMessageBox(const char* const message)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", message, GetSDLWindow());
}

#include "winapi.h"

#include <stddef.h>

#ifdef SDL_PLATFORM_WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#endif

#ifdef SDL_PLATFORM_WIN32
using DwmSetWindowAttribute_Type = HRESULT(WINAPI*)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);

static DwmSetWindowAttribute_Type GetDwmSetWindowAttribute()
{
	static DwmSetWindowAttribute_Type DwmSetWindowAttribute_Function;

	if (DwmSetWindowAttribute_Function == nullptr)
	{
		static SDL::SharedObject dwmapi_dll;

		dwmapi_dll = SDL::SharedObject(SDL_LoadObject("dwmapi.dll"));

		if (dwmapi_dll != nullptr)
			DwmSetWindowAttribute_Function = reinterpret_cast<DwmSetWindowAttribute_Type>(SDL_LoadFunction(dwmapi_dll, "DwmSetWindowAttribute"));
	}

	return DwmSetWindowAttribute_Function;
}
#endif

void SetWindowTitleBarColour([[maybe_unused]] SDL_Window* const window, [[maybe_unused]] const unsigned char red, [[maybe_unused]] const unsigned char green, [[maybe_unused]] const unsigned char blue)
{
#ifdef SDL_PLATFORM_WIN32
	const HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

	if (hwnd == 0)
		return;

	const auto DwmSetWindowAttribute_Function = GetDwmSetWindowAttribute();

	if (DwmSetWindowAttribute_Function != nullptr)
	{
		// Colour the title bar.
		const COLORREF winapi_colour = RGB(red, green, blue);
		DwmSetWindowAttribute_Function(hwnd, 35/*DWMWA_CAPTION_COLOR*/, &winapi_colour, sizeof(winapi_colour));
	}
#endif
}

void DisableWindowRounding([[maybe_unused]] SDL_Window* const window)
{
#ifdef SDL_PLATFORM_WIN32
	const HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

	if (hwnd == 0)
		return;

	const auto DwmSetWindowAttribute_Function = GetDwmSetWindowAttribute();

	if (DwmSetWindowAttribute_Function == nullptr)
		return;

	// Disable the dumbass window rounding.
	// TODO: This function should probably be renamed.
	const int rounding_mode = 1; // DWMWCP_DONOTROUND
	DwmSetWindowAttribute_Function(hwnd, 33/*DWMWA_WINDOW_CORNER_PREFERENCE*/, &rounding_mode, sizeof(rounding_mode));
#endif
}

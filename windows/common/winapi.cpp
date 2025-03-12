#include "winapi.h"

#include <stddef.h>

#ifdef SDL_PLATFORM_WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#endif

void SetWindowTitleBarColour([[maybe_unused]] SDL_Window* const window, [[maybe_unused]] const unsigned char red, [[maybe_unused]] const unsigned char green, [[maybe_unused]] const unsigned char blue)
{
#ifdef SDL_PLATFORM_WIN32
	const HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

	if (hwnd == 0)
		return;

	SDL_SharedObject* const handle = SDL_LoadObject("dwmapi.dll");

	if (handle == nullptr)
		return;

	using DwmSetWindowAttribute_Type = HRESULT(WINAPI*)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);

	const auto DwmSetWindowAttribute_Function = reinterpret_cast<DwmSetWindowAttribute_Type>(SDL_LoadFunction(handle, "DwmSetWindowAttribute"));

	if (DwmSetWindowAttribute_Function != nullptr)
	{
		// Colour the title bar.
		const COLORREF winapi_colour = RGB(red, green, blue);
		DwmSetWindowAttribute_Function(hwnd, 35/*DWMWA_CAPTION_COLOR*/, &winapi_colour, sizeof(winapi_colour));

		// Disable the dumbass window rounding.
		// TODO: This function should probably be renamed.
		const int rounding_mode = 1; // DWMWCP_DONOTROUND
		DwmSetWindowAttribute_Function(hwnd, 33/*DWMWA_WINDOW_CORNER_PREFERENCE*/, &rounding_mode, sizeof(rounding_mode));
	}

	SDL_UnloadObject(handle);
#endif
}

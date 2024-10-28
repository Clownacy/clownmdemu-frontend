#include "winapi.h"

#include <stddef.h>

#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>

 #include <SDL3/SDL.h>
#endif

void SetWindowTitleBarColour(SDL_Window* const window, const unsigned char red, const unsigned char green, const unsigned char blue)
{
// TODO: Replace all '_WIN32' checks with 'SDL_PLATFORM_WIN32'.
#ifdef _WIN32
	const HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

	if (hwnd != 0)
	{
		void* const handle = SDL_LoadObject("dwmapi.dll");

		if (handle != NULL)
		{
			typedef HRESULT(WINAPI *DwmSetWindowAttribute_Type)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);

			const DwmSetWindowAttribute_Type DwmSetWindowAttribute_Function = (DwmSetWindowAttribute_Type)SDL_LoadFunction(handle, "DwmSetWindowAttribute");

			if (DwmSetWindowAttribute_Function != NULL)
			{
				{
					/* Colour the title bar. */
					const COLORREF winapi_colour = RGB(red, green, blue);
					DwmSetWindowAttribute_Function(hwnd, 35/*DWMWA_CAPTION_COLOR*/, &winapi_colour, sizeof(winapi_colour));
				}

				{
					/* Disable the dumbass window rounding. */
					const int rounding_mode = 1; /* DWMWCP_DONOTROUND */
					DwmSetWindowAttribute_Function(hwnd, 33/*DWMWA_WINDOW_CORNER_PREFERENCE*/, &rounding_mode, sizeof(rounding_mode));
				}
			}

			SDL_UnloadObject(handle);
		}
	}
#else
	(void)window;
	(void)red;
	(void)green;
	(void)blue;
#endif
}

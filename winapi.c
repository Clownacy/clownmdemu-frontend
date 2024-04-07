#include "winapi.h"

#include <stddef.h>

#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>

 #include "SDL_syswm.h"
#endif

void SetWindowTitleBarColour(SDL_Window* const window, const unsigned char red, const unsigned char green, const unsigned char blue)
{
#ifdef _WIN32
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (SDL_GetWindowWMInfo(window, &info))
	{
		void* const handle = SDL_LoadObject("dwmapi.dll");

		if (handle != NULL)
		{
			typedef HRESULT(WINAPI *DwmSetWindowAttribute_Type)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);

			const DwmSetWindowAttribute_Type DwmSetWindowAttribute_Function = (DwmSetWindowAttribute_Type)SDL_LoadFunction(handle, "DwmSetWindowAttribute");

			if (DwmSetWindowAttribute_Function != NULL)
			{
				const COLORREF winapi_colour = RGB(red, green, blue);
				DwmSetWindowAttribute_Function(info.info.win.window, 35/*DWMWA_CAPTION_COLOR*/, &winapi_colour, sizeof(winapi_colour));
			}

			SDL_UnloadObject(handle);
		}
	}
#else
	(void)window;
	(void)colour;
#endif
}

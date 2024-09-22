#ifndef WINDOW_POPUP_H
#define WINDOW_POPUP_H

#include <optional>
#include <string>

#include "SDL.h"

#include "window-with-dear-imgui.h"

class WindowPopup
{
private:
	std::optional<WindowWithDearImGui> window;
	std::string title;
	int dear_imgui_window_width, dear_imgui_window_height;

public:
	WindowPopup(DebugLog &debug_log, const char *window_title, int window_width, int window_height, Window *parent_window = nullptr);

	bool Begin(ImGuiWindowFlags window_flags = 0);
	void End();

	bool IsWindowID(const Uint32 window_id) const
	{
		if (!window.has_value())
			return false;

		return window_id == SDL_GetWindowID(window->GetSDLWindow());
	}

	void ProcessEvent(const SDL_Event &event)
	{
		const auto &previous_context = ImGui::GetCurrentContext();
		window->MakeDearImGuiContextCurrent();

		ImGui_ImplSDL2_ProcessEvent(&event);

		ImGui::SetCurrentContext(previous_context);
	}
};

#endif /* WINDOW_POPUP_H */

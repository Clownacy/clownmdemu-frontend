#ifndef WINDOW_POPUP_H
#define WINDOW_POPUP_H

#include <functional>
#include <optional>
#include <string>

#include "SDL.h"

#include "window-with-dear-imgui.h"

template<typename Derived>
class WindowPopup
{
private:
	std::optional<WindowWithDearImGui> window;
	std::string title;
	bool resizeable;
	WindowWithDearImGui *parent_window;
	int dear_imgui_window_width, dear_imgui_window_height;

	bool Begin(bool* const open = nullptr, ImGuiWindowFlags window_flags = 0)
	{
		if (window.has_value())
		{
			window->StartDearImGuiFrame();

			const ImGuiViewport *viewport = ImGui::GetMainViewport();

			// Maximise the Dear ImGui window.
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);

			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
		}
		else
		{
			ImGui::SetNextWindowSize(ImVec2(dear_imgui_window_width, dear_imgui_window_height), ImGuiCond_FirstUseEver);
		}

		if (!resizeable)
			window_flags |= ImGuiWindowFlags_AlwaysAutoResize;

		//ImGui::PushID(this);
		return ImGui::Begin(title.c_str(), open, window_flags);
	}

	void End()
	{
		ImGui::End();
		//ImGui::PopID();

		if (window.has_value())
			window->FinishDearImGuiFrame();
	}

public:
	WindowPopup(const char* const window_title, const int window_width, const int window_height, const bool resizeable, WindowWithDearImGui* const parent_window = nullptr)
		: title(window_title)
		, resizeable(resizeable)
		, parent_window(parent_window)
	{
		if (parent_window != nullptr)
		{
			dear_imgui_window_width = window_width * parent_window->GetDPIScale();
			dear_imgui_window_height = window_height * parent_window->GetDPIScale();
		}
		else
		{
			window.emplace(window_title, window_width, window_height, resizeable);
			SDL_ShowWindow(window->GetSDLWindow());
		}
	}

	template<typename... Ts>
	bool Display(Ts&&... arguments)
	{
		const auto derived = static_cast<Derived*>(this);

		bool alive = true;

		if (Begin(&alive, derived->window_flags))
			derived->DisplayInternal(arguments...);

		End();

		return alive;
	}

	bool IsWindowID(const Uint32 window_id)
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

	WindowWithDearImGui& GetWindow()
	{
		if (window.has_value())
			return *window;
		else
			return *parent_window;
	}

	ImFont* GetMonospaceFont()
	{
		return GetWindow().monospace_font;
	}

	bool IsResizeable() const { return resizeable; }
};

#endif /* WINDOW_POPUP_H */

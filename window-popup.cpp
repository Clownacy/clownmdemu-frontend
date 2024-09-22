#include "window-popup.h"

WindowPopup::WindowPopup(DebugLog &debug_log, const char* const window_title, const int window_width, const int window_height, WindowWithDearImGui* const parent_window)
	: title(window_title)
	, parent_window(parent_window)
{
	if (parent_window != nullptr)
	{
		dear_imgui_window_width = window_width * parent_window->GetDPIScale();
		dear_imgui_window_height = window_height * parent_window->GetDPIScale();
	}
	else
	{
		window.emplace(debug_log, window_title, window_width, window_height);
		SDL_ShowWindow(window->GetSDLWindow());
	}
}

bool WindowPopup::Begin(ImGuiWindowFlags window_flags)
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

	//ImGui::PushID(this);
	return ImGui::Begin(title.c_str(), nullptr, window_flags);
}

void WindowPopup::End()
{
	ImGui::End();
	//ImGui::PopID();

	if (window.has_value())
		window->FinishDearImGuiFrame();
}

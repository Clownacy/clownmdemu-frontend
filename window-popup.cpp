#include "window-popup.h"

WindowPopup::WindowPopup(DebugLog &debug_log, const char* const window_title, const int window_width, const int window_height, Window* const parent_window)
	: title(window_title)
	, width(window_width)
	, height(window_height)
	, parent_window(parent_window)
{
	if (parent_window == nullptr)
	{
		window.emplace(debug_log, window_title, window_width, window_height);
		SDL_ShowWindow(window->GetSDLWindow());
	}
}

bool WindowPopup::Begin()
{
	ImGuiWindowFlags window_flags = 0;

	if (parent_window == nullptr)
	{
		window->StartDearImGuiFrame();

		const ImGuiViewport *viewport = ImGui::GetMainViewport();

		// Maximise the Dear ImGui window.
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);

		window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
	}
	else
	{
		ImGui::SetNextWindowSize(ImVec2(width * parent_window->GetDPIScale(), height * parent_window->GetDPIScale()), ImGuiCond_FirstUseEver);
	}

	//ImGui::PushID(this);
	return ImGui::Begin(title.c_str(), nullptr, window_flags);
}

void WindowPopup::End()
{
	ImGui::End();
	//ImGui::PopID();

	if (parent_window == nullptr)
		window->FinishDearImGuiFrame();
}

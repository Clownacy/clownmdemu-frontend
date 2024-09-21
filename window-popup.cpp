#include "window-popup.h"

bool WindowPopup::Begin()
{
	StartDearImGuiFrame();

	const ImGuiViewport *viewport = ImGui::GetMainViewport();

	// Maximise the Dear ImGui window.
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	ImGui::PushID(this);
	return ImGui::Begin("Popup Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
}

void WindowPopup::End()
{
	ImGui::End();
	ImGui::PopID();

	FinishDearImGuiFrame();
}

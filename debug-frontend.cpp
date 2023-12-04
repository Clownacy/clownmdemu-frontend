#include "debug-frontend.h"

void DebugFrontend::Display(bool &open)
{
	if (ImGui::Begin("Frontend", &open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::SeparatorText("SDL2 Drivers");

		if (ImGui::BeginTable("SDL2 Drivers", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
		{
			// Render
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Render");

			ImGui::TableNextColumn();

			SDL_RendererInfo info;
			if (SDL_GetRendererInfo(window.renderer, &info) == 0)
				ImGui::TextUnformatted(info.name);
			else
				ImGui::TextUnformatted("Unknown");

			// Video
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Video");

			ImGui::TableNextColumn();
			const char* const audio_driver_name = SDL_GetCurrentVideoDriver();
			ImGui::TextUnformatted(audio_driver_name != nullptr ? audio_driver_name : "None");

			// Audio
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Audio");

			ImGui::TableNextColumn();
			const char* const video_driver_name = SDL_GetCurrentAudioDriver();
			ImGui::TextUnformatted(video_driver_name != nullptr ? video_driver_name : "None");

			ImGui::EndTable();
		}

		ImGui::SeparatorText("Audio");

		if (ImGui::BeginTable("Audio", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Sample Rate");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIuFAST32, audio_output.GetSampleRate());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Buffer Size");
			ImGui::TableNextColumn();
			ImGui::Text("0x%" CC_PRIXFAST32, audio_output.GetBufferSize());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Target Frames");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIuFAST32, audio_output.GetTargetFrames());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Average Frames");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIuFAST32, audio_output.GetAverageFrames());

			ImGui::EndTable();
		}
	}

	ImGui::End();
}

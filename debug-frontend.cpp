#include "debug-frontend.h"

static void DoToolTip(const char* const text)
{
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(text);
		ImGui::EndTooltip();
	}
}

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
			DoToolTip("The number of audio frames played per second.");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIuFAST32, audio_output.GetSampleRate());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Buffer Frames");
			DoToolTip("The number of audio frames that are pulled from the buffer in a single batch.");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIdFAST32, audio_output.GetTotalBufferFrames());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Target Frames");
			DoToolTip("The number of buffered audio frames that the audio system tries to maintain.");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIuFAST32, audio_output.GetTargetFrames());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Average Frames");
			DoToolTip("The current average number of buffered audio frames.");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIuFAST32, audio_output.GetAverageFrames());

			ImGui::EndTable();
		}
	}

	ImGui::End();
}

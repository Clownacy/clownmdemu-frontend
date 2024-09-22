#include "debug-frontend.h"

#include "window-popup.h"

static void DoToolTip(const char* const text)
{
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(text);
		ImGui::EndTooltip();
	}
}

void DebugFrontend::Display(WindowPopup &window)
{
	if (window.Begin(ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::SeparatorText("SDL2 Drivers");

		if (ImGui::BeginTable("SDL2 Drivers", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
		{
			// Render
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Render");

			ImGui::TableNextColumn();

			SDL_RendererInfo info;
			if (SDL_GetRendererInfo(window.GetWindow().GetRenderer(), &info) == 0)
				ImGui::TextUnformatted(info.name);
			else
				ImGui::TextUnformatted("Unknown");

			// Video
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Video");

			ImGui::TableNextColumn();
			const char* const video_driver_name = SDL_GetCurrentVideoDriver();
			ImGui::TextUnformatted(video_driver_name != nullptr ? video_driver_name : "None");

			// Audio
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Audio");

			ImGui::TableNextColumn();
			const char* const audio_driver_name = SDL_GetCurrentAudioDriver();
			ImGui::TextUnformatted(audio_driver_name != nullptr ? audio_driver_name : "None");

			ImGui::EndTable();
		}

		ImGui::SeparatorText("Video");

		if (ImGui::BeginTable("Video", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Output");
			DoToolTip("The size that the frame is drawn at.");
			ImGui::TableNextColumn();

			if (output_width != 0 || output_height != 0)
			{
				ImGui::Text("%ux%u", output_width, output_height);

				output_width = output_height = 0;
			}
			else
			{
				ImGui::TextUnformatted("N/A");
			}

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Upscale");
			DoToolTip("The size that the frame is upscaled to for fractional scaling.");
			ImGui::TableNextColumn();

			if (upscale_width != 0 || upscale_height != 0)
			{
				ImGui::Text("%ux%u", upscale_width, upscale_height);

				upscale_width = upscale_height = 0;
			}
			else
			{
				ImGui::TextUnformatted("N/A");
			}

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Texture");
			DoToolTip("The size of the texture that is used for fractional upscaling.");
			ImGui::TableNextColumn();

			unsigned int texture_width, texture_height;
			if (get_upscaled_framebuffer_size(texture_width, texture_height))
				ImGui::Text("%ux%u", texture_width, texture_height);
			else
				ImGui::TextUnformatted("N/A");

			ImGui::EndTable();
		}

		ImGui::SeparatorText("Audio");

		if (ImGui::BeginTable("Audio", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Sample Rate");
			DoToolTip("The number of audio frames played per second.");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIuFAST32, emulator.GetAudioSampleRate());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Buffer Frames");
			DoToolTip("The number of audio frames that are pulled from the buffer in a single batch.");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIuFAST32, emulator.GetAudioTotalBufferFrames());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Target Frames");
			DoToolTip("The number of buffered audio frames that the audio system tries to maintain.");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIuFAST32, emulator.GetAudioTargetFrames());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Average Frames");
			DoToolTip("The current average number of buffered audio frames.");
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIuFAST32, emulator.GetAudioAverageFrames());

			ImGui::EndTable();
		}
	}

	window.End();
}

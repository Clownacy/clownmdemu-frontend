#include "debug-frontend.h"

#include "frontend.h"

bool DebugFrontend::Display()
{
	return BeginAndEnd(0,
		[&]()
		{
			ImGui::SeparatorText("SDL2 Drivers");

			if (ImGui::BeginTable("SDL2 Drivers", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
			{
				// Render
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Render");

				ImGui::TableNextColumn();

				SDL_RendererInfo info;
				if (SDL_GetRendererInfo(GetWindow().GetRenderer(), &info) == 0)
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

				if (Frontend::output_width != 0 || Frontend::output_height != 0)
				{
					ImGui::Text("%ux%u", Frontend::output_width, Frontend::output_height);

					Frontend::output_width = Frontend::output_height = 0;
				}
				else
				{
					ImGui::TextUnformatted("N/A");
				}

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Upscale");
				DoToolTip("The size that the frame is upscaled to for fractional scaling.");
				ImGui::TableNextColumn();

				if (Frontend::upscale_width != 0 || Frontend::upscale_height != 0)
				{
					ImGui::Text("%ux%u", Frontend::upscale_width, Frontend::upscale_height);

					Frontend::upscale_width = Frontend::upscale_height = 0;
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
				if (Frontend::GetUpscaledFramebufferSize(texture_width, texture_height))
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
				ImGui::Text("%" CC_PRIuFAST32, Frontend::emulator->GetAudioSampleRate());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Buffer Frames");
				DoToolTip("The number of audio frames that are pulled from the buffer in a single batch.");
				ImGui::TableNextColumn();
				ImGui::Text("%" CC_PRIuFAST32, Frontend::emulator->GetAudioTotalBufferFrames());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Target Frames");
				DoToolTip("The number of buffered audio frames that the audio system tries to maintain.");
				ImGui::TableNextColumn();
				ImGui::Text("%" CC_PRIuFAST32, Frontend::emulator->GetAudioTargetFrames());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Average Frames");
				DoToolTip("The current average number of buffered audio frames.");
				ImGui::TableNextColumn();
				ImGui::Text("%" CC_PRIuFAST32, Frontend::emulator->GetAudioAverageFrames());

				ImGui::EndTable();
			}
		}
	);
}

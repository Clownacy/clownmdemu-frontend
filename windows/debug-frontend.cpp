#include "debug-frontend.h"

#include "../frontend.h"

void DebugFrontend::DisplayInternal()
{
	ImGui::SeparatorText("SDL Drivers");

	if (ImGui::BeginTable("SDL Drivers", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
	{
		// Render
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Render");

		ImGui::TableNextColumn();

		const char* const renderer_name = SDL_GetRendererName(GetWindow().GetRenderer());
		ImGui::TextUnformatted(renderer_name == nullptr ? "Unknown" : renderer_name);

		// GPU
		const char* const gpu_name = [&]() -> const char*
		{
			const auto property_id = SDL_GetRendererProperties(GetWindow().GetRenderer());

			if (property_id == 0)
				return "Unknown";

			const auto device_pointer = static_cast<SDL_GPUDevice*>(SDL_GetPointerProperty(property_id, SDL_PROP_RENDERER_GPU_DEVICE_POINTER, nullptr));

			if (device_pointer == nullptr)
				return "None";

			const auto name = SDL_GetGPUDeviceDriver(device_pointer);

			if (name == nullptr)
				return "Unknown";

			return name;
		}();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("GPU");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(gpu_name);

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
		DoToolTip(u8"The size that the frame is drawn at.");
		ImGui::TableNextColumn();

		if (Frontend::output_width != 0 || Frontend::output_height != 0)
		{
			ImGui::TextFormatted("{}x{}", Frontend::output_width, Frontend::output_height);

			Frontend::output_width = Frontend::output_height = 0;
		}
		else
		{
			ImGui::TextUnformatted("N/A");
		}

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Upscale");
		DoToolTip(u8"The size that the frame is upscaled to for fractional scaling.");
		ImGui::TableNextColumn();

		if (Frontend::upscale_width != 0 || Frontend::upscale_height != 0)
		{
			ImGui::TextFormatted("{}x{}", Frontend::upscale_width, Frontend::upscale_height);

			Frontend::upscale_width = Frontend::upscale_height = 0;
		}
		else
		{
			ImGui::TextUnformatted("N/A");
		}

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Texture");
		DoToolTip(u8"The size of the texture that is used for fractional upscaling.");
		ImGui::TableNextColumn();

		unsigned int texture_width, texture_height;
		if (Frontend::GetUpscaledFramebufferSize(texture_width, texture_height))
			ImGui::TextFormatted("{}x{}", texture_width, texture_height);
		else
			ImGui::TextUnformatted("N/A");

		ImGui::EndTable();
	}

	ImGui::SeparatorText("Audio");

	if (ImGui::BeginTable("Audio", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Sample Rate");
		DoToolTip(u8"The number of audio frames played per second.");
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{}", Frontend::emulator->GetAudioSampleRate());

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Buffer Frames");
		DoToolTip(u8"The number of audio frames that are pulled from the buffer in a single batch.");
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{}", Frontend::emulator->GetAudioTotalBufferFrames());

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Target Frames");
		DoToolTip(u8"The number of buffered audio frames that the audio system tries to maintain.");
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{}", Frontend::emulator->GetAudioTargetFrames());

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Average Frames");
		DoToolTip(u8"The current average number of buffered audio frames.");
		ImGui::TableNextColumn();
		ImGui::TextFormatted("{}", Frontend::emulator->GetAudioAverageFrames());

		ImGui::EndTable();
	}
}

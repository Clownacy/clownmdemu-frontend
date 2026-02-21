#include "debug-frontend.h"

#include "../frontend.h"

void DebugFrontend::DisplayInternal()
{
	if (ImGui::BeginTable("Tables", 2, ImGuiTableFlags_SizingStretchSame))
	{
		ImGui::TableNextColumn();
		ImGui::SeparatorText("SDL Drivers");

		if (ImGui::BeginTable("SDL Drivers", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
		{
			// Render
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Render");

			ImGui::TableNextColumn();

			const char* const renderer_name = SDL_GetRendererName(GetWindow().GetRenderer());
			ImGui::TextUnformatted(renderer_name == nullptr ? "unknown" : renderer_name);

			// GPU
			const char* const gpu_name = [&]() -> const char*
			{
				const auto property_id = SDL_GetRendererProperties(GetWindow().GetRenderer());

				if (property_id == 0)
					return "unknown";

				const auto device_pointer = static_cast<SDL_GPUDevice*>(SDL_GetPointerProperty(property_id, SDL_PROP_RENDERER_GPU_DEVICE_POINTER, nullptr));

				if (device_pointer == nullptr)
					return "none";

				const auto name = SDL_GetGPUDeviceDriver(device_pointer);

				if (name == nullptr)
					return "unknown";

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
			ImGui::TextUnformatted(video_driver_name != nullptr ? video_driver_name : "none");

			// Audio
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Audio");

			ImGui::TableNextColumn();
			const char* const audio_driver_name = SDL_GetCurrentAudioDriver();
			ImGui::TextUnformatted(audio_driver_name != nullptr ? audio_driver_name : "none");

			ImGui::EndTable();
		}

		ImGui::TableNextColumn();
		ImGui::SeparatorText("Audio");

		if (ImGui::BeginTable("Audio", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Sample Rate");
			DoToolTip("The number of audio frames played per second.");
			ImGui::TableNextColumn();
			ImGui::TextFormatted("{}", Frontend::emulator->GetAudioSampleRate());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Buffer Frames");
			DoToolTip("The number of audio frames that are pulled from the buffer in a single batch.");
			ImGui::TableNextColumn();
			ImGui::TextFormatted("{}", Frontend::emulator->GetAudioTotalBufferFrames());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Target Frames");
			DoToolTip("The number of buffered audio frames that the audio system tries to maintain.");
			ImGui::TableNextColumn();
			ImGui::TextFormatted("{}", Frontend::emulator->GetAudioTargetFrames());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Average Frames");
			DoToolTip("The current average number of buffered audio frames.");
			ImGui::TableNextColumn();
			ImGui::TextFormatted("{}", Frontend::emulator->GetAudioAverageFrames());

			ImGui::EndTable();
		}

		ImGui::EndTable();
	}

	ImGui::SeparatorText("Paths");

	if (ImGui::BeginTable("Paths", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
	{
		const auto &DoPath = [](const char* const label, const std::filesystem::path &path)
		{
			auto path_string = std::filesystem::absolute(path).u8string();
			const std::string_view path_string_view(reinterpret_cast<char*>(std::data(path_string)), std::size(path_string));

			ImGui::PushID(label);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(label);
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputTextReadOnly("", std::data(path_string_view), std::size(path_string_view) + 1, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ElideLeft);
			ImGui::PopID();
		};

		DoPath("Configuration", Frontend::GetConfigurationDirectoryPath());
		DoPath("Save Data", Frontend::GetSaveDataDirectoryPath());

		ImGui::EndTable();
	}
}

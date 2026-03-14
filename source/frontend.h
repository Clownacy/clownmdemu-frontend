#ifndef FRONTEND_H
#define FRONTEND_H

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include <SDL3/SDL.h>

#include "../common/core/source/clownmdemu.h"

#include "debug-log.h"
#include "emulator-instance.h"
#include "file-utilities.h"
#include "windows/common/window-with-framebuffer.h"

#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif

#define VERSION "v1.6.9" GIT_VERSION

template<typename T>
inline void DoToolTip(const T& text)
{
	if (ImGui::BeginItemTooltip())
	{
		ImGui::TextUnformatted(text);
		ImGui::EndTooltip();
	}
}

namespace Frontend
{
	using FrameRateCallback = std::function<void(bool pal_mode)>;

	extern std::optional<EmulatorInstance> emulator;
	extern std::optional<WindowWithFramebuffer> window;
	extern FileUtilities file_utilities;
	extern unsigned int frame_counter;

	extern bool tall_double_resolution_mode;
	extern bool native_windows;

	bool IsFileCD(const std::filesystem::path &path);
	const std::filesystem::path& GetConfigurationDirectoryPath();
	std::filesystem::path GetSaveDataDirectoryPath();
	void SetTVStandard(ClownMDEmu_TVStandard tv_standard);
	bool Initialise(const FrameRateCallback &frame_rate_callback, bool fullscreen = false, const std::filesystem::path &user_data_path = "", const std::filesystem::path &cartridge_path = "", const std::filesystem::path &cd_path = "");
	void HandleEvent(const SDL_Event &event);
	void Update();
	void Deinitialise();
	void WriteSaveData();
	bool WantsToQuit();
	template<typename T>
	T DivideByPALFramerate(T value) { return CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(value); }
	template<typename T>
	T DivideByNTSCFramerate(T value) { return CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(value); };
}

#endif /* FRONTEND_H */

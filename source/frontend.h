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

#define VERSION "v1.6.10" GIT_VERSION

template<typename T>
inline void DoToolTip(const T& text)
{
	if (ImGui::BeginItemTooltip())
	{
		ImGui::TextUnformatted(text);
		ImGui::EndTooltip();
	}
}

class Frontend
{
private:
	void UpdateFastForwardStatus();
	void UpdateRewindStatus();
	static std::filesystem::path GetConfigurationFilePath();
	static std::filesystem::path GetDearImGuiSettingsFilePath();
	void LoadCartridgeFile(const std::filesystem::path &path, std::vector<cc_u16l> &&file_buffer);
	bool LoadCartridgeFile(const std::filesystem::path &path, SDL::IOStream &file);
	bool LoadCartridgeFile(const std::filesystem::path &path);
	bool LoadCDFile(const std::filesystem::path &path, SDL::IOStream &&file);
	bool LoadCDFile(const std::filesystem::path &path);
	bool LoadSoftwareFile(const bool is_cd_file, const std::filesystem::path &path);
	bool LoadSaveState(SDL::IOStream &file);
	bool LoadSaveState(const std::filesystem::path &path);
	bool SaveState(const std::filesystem::path &path);
	bool ShouldBeInFullscreenMode();
	bool NativeWindowsActive();
	void LoadConfiguration();
	void SaveConfiguration();
	void PreEventStuff();
	void HandleMainWindowEvent(const SDL_Event &event);
	void DrawStatusIndicator(const ImVec2 &display_position, const ImVec2 &display_size);

public:
	std::optional<EmulatorInstance> emulator;
	std::optional<WindowWithFramebuffer> window;
	unsigned int frame_counter;

	bool tall_double_resolution_mode;
	bool native_windows;
	
	static bool IsFileCD(const std::filesystem::path &path);
	static const std::filesystem::path& GetConfigurationDirectoryPath();
	static std::filesystem::path GetSaveDataDirectoryPath();
	Frontend(const EmulatorInstance::FramerateCallback &framerate_callback, bool fullscreen = false, const std::filesystem::path &user_data_path = "", const std::filesystem::path &cartridge_path = "", const std::filesystem::path &cd_path = "");
	void HandleEvent(const SDL_Event &event);
	void Update();
	~Frontend();
	void WriteSaveData();
	bool WantsToQuit();
	template<typename T>
	static T DivideByPALFramerate(T value) { return CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(value); }
	template<typename T>
	static T DivideByNTSCFramerate(T value) { return CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(value); };
};

inline std::optional<Frontend> frontend;

#endif /* FRONTEND_H */

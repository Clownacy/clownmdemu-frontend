#ifndef FILE_UTILITIES_H
#define FILE_UTILITIES_H

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "sdl-wrapper.h"

#include "debug-log.h"
#include "windows/common/window.h"

class FileUtilities
{
public:
	using SaveFileInnerCallback = std::function<bool(const void *data, std::size_t data_size)>;

private:
	using PopupCallback = std::function<bool(const std::filesystem::path &path)>;
	using LoadFileCallback = std::function<bool(const std::filesystem::path &path, SDL::IOStream &&file)>;
	using SaveFileCallback = std::function<bool(const SaveFileInnerCallback &save_file)>;

	std::string text_buffer;

	DebugLog &debug_log;
	std::optional<std::string> active_file_picker_popup;
	PopupCallback popup_callback;
	bool is_save_dialog = false;

	void CreateFileDialog(const Window &window, const std::string &title, const PopupCallback &callback, bool save);

public:
	bool use_native_file_dialogs = true;

	FileUtilities(DebugLog &debug_log) : debug_log(debug_log) {}
	void CreateOpenFileDialog(const Window &window, const std::string &title, const PopupCallback &callback);
	void CreateSaveFileDialog(const Window &window, const std::string &title, const PopupCallback &callback);
	void DisplayFileDialog(std::filesystem::path &drag_and_drop_filename);
	bool IsDialogOpen() const { return active_file_picker_popup.has_value(); }

	bool FileExists(const std::filesystem::path &path);
	bool LoadFileToBuffer(std::vector<unsigned char> &file_buffer, const std::filesystem::path &path);
	bool LoadFileToBuffer(std::vector<unsigned char> &file_buffer, const SDL::IOStream &file);

	void LoadFile(const Window &window, const std::string &title, const LoadFileCallback &callback);
	void SaveFile(const Window &window, const std::string &title, const SaveFileCallback &callback);
};

#endif /* FILE_UTILITIES_H */

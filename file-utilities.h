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

	std::optional<std::string> active_file_picker_popup;
	PopupCallback popup_callback;
	bool is_save_dialog = false;

	void CreateFileDialog(Window &window, const std::string &title, const PopupCallback &callback, bool save);

public:
	bool use_native_file_dialogs = true;

	void CreateOpenFileDialog(Window &window, const std::string &title, const PopupCallback &callback);
	void CreateSaveFileDialog(Window &window, const std::string &title, const PopupCallback &callback);
	void DisplayFileDialog(std::filesystem::path &drag_and_drop_filename);
	bool IsDialogOpen() const { return active_file_picker_popup.has_value(); }

	bool FileExists(const std::filesystem::path &path);
	std::optional<std::vector<unsigned char>> LoadFileToBuffer(const std::filesystem::path &path);
	std::optional<std::vector<unsigned char>> LoadFileToBuffer(SDL::IOStream &file);

	void LoadFile(Window &window, const std::string &title, const LoadFileCallback &callback);
	void SaveFile(Window &window, const std::string &title, const SaveFileCallback &callback);
};

#endif /* FILE_UTILITIES_H */

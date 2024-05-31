#ifndef FILE_UTILITIES_H
#define FILE_UTILITIES_H

#if defined(__unix__) && !defined(__EMSCRIPTEN__)
#include <unistd.h>

#if defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L
#define FILE_PICKER_POSIX
#endif
#endif

#if defined(_WIN32) || defined(FILE_PICKER_POSIX)
#define FILE_PICKER_HAS_NATIVE_FILE_DIALOGS
#endif

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "sdl-wrapper.h"

#include "debug-log.h"
#include "window.h"

class FileUtilities
{
public:
	using SaveFileInnerCallback = std::function<bool(const void *data, std::size_t data_size)>;

private:
	using PopupCallback = std::function<bool(const std::filesystem::path &path)>;
	using LoadFileCallback = std::function<bool(const std::filesystem::path &path, SDL::RWops &&file)>;
	using SaveFileCallback = std::function<bool(const SaveFileInnerCallback &save_file)>;

	std::string text_buffer;

	DebugLog &debug_log;
	std::optional<std::string> active_file_picker_popup;
	PopupCallback popup_callback;
	bool is_save_dialog = false;

	void CreateFileDialog(const Window &window, const std::string &title, const PopupCallback &callback, bool save);

public:
#ifdef FILE_PICKER_POSIX
	std::filesystem::path last_file_dialog_directory;
	bool prefer_kdialog = false;
#endif

#ifdef FILE_PICKER_HAS_NATIVE_FILE_DIALOGS
	bool use_native_file_dialogs = true;
#endif

	FileUtilities(DebugLog &debug_log) : debug_log(debug_log) {}
	void CreateOpenFileDialog(const Window &window, const std::string &title, const PopupCallback &callback);
	void CreateSaveFileDialog(const Window &window, const std::string &title, const PopupCallback &callback);
	void DisplayFileDialog(std::filesystem::path &drag_and_drop_filename);
	bool IsDialogOpen() const { return active_file_picker_popup.has_value(); }

	bool FileExists(const std::filesystem::path &path);
	bool LoadFileToBuffer(std::vector<unsigned char> &file_buffer, const std::filesystem::path &path);
	bool LoadFileToBuffer(std::vector<unsigned char> &file_buffer, const SDL::RWops &file);

	void LoadFile(const Window &window, const std::string &title, const LoadFileCallback &callback);
	void SaveFile(const Window &window, const std::string &title, const SaveFileCallback &callback);
};

#endif /* FILE_UTILITIES_H */

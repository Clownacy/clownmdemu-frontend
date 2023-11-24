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

#include <functional>

#include "SDL.h"

#include "debug-log.h"
#include "window.h"

class FileUtilities
{
private:
	DebugLog &debug_log;
	Window &window;
	const char *active_file_picker_popup;
	std::function<bool(const char *path)> popup_callback;
	bool is_save_dialog;

	void CreateFileDialog(const char *title, std::function<bool(const char *path)> callback, bool save);

public:
#ifdef FILE_PICKER_POSIX
	char *last_file_dialog_directory;
	bool prefer_kdialog;
#endif

#ifdef FILE_PICKER_HAS_NATIVE_FILE_DIALOGS
	bool use_native_file_dialogs = true;
#endif

	FileUtilities(DebugLog &debug_log, Window &window) : debug_log(debug_log), window(window) {}
#ifdef FILE_PICKER_POSIX
	~FileUtilities() {SDL_free(last_file_dialog_directory);}
#endif
	void CreateOpenFileDialog(const char *title, std::function<bool(const char *path)> callback);
	void CreateSaveFileDialog(const char *title, std::function<bool(const char *path)> callback);
	void DisplayFileDialog(char *&drag_and_drop_filename);
	bool IsDialogOpen() const { return active_file_picker_popup != nullptr; }

	bool FileExists(const char *filename);
	void LoadFileToBuffer(const char *filename, unsigned char *&file_buffer, std::size_t &file_size);
	void LoadFileToBuffer(SDL_RWops *file, unsigned char *&file_buffer, std::size_t &file_size);

	void LoadFile(const char *title, std::function<void(const char* const path, SDL_RWops *file)> callback);
	void SaveFile(const char *title, const std::function<bool(const std::function<bool(const void *data, std::size_t data_size)> &save_file)> &callback);
};

#endif /* FILE_UTILITIES_H */

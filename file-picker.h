#ifndef FILE_PICKER_H
#define FILE_PICKER_H

#ifdef __unix__
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

class FilePicker
{
private:
	SDL_Window *window;
	const char *active_file_picker_popup;
	std::function<bool(const char *path)> popup_callback;
	bool is_save_dialog;
#ifdef FILE_PICKER_POSIX
	char *last_file_dialog_directory;
	bool prefer_kdialog;
#endif

	void CreateFileDialog(char const* const title, const std::function<bool(const char *path)> callback, bool save);

public:
#ifdef FILE_PICKER_HAS_NATIVE_FILE_DIALOGS
	bool use_native_file_dialogs = true;
#endif

	FilePicker(SDL_Window* const window) : window(window) {}
	void CreateOpenFileDialog(char const* const title, const std::function<bool(const char *path)> callback);
	void CreateSaveFileDialog(char const* const title, const std::function<bool(const char *path)> callback);
	void Update(char *&drag_and_drop_filename);
	bool IsDialogOpen() const { return active_file_picker_popup != nullptr; };
};

#endif /* FILE_PICKER_H */
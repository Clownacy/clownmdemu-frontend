#include "file-utilities.h"

#include <climits>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include "SDL_syswm.h"
#elif defined(FILE_PICKER_POSIX)
#include <cstdio>
#include <sys/wait.h>
#endif

#include "SDL.h"

#ifdef __EMSCRIPTEN__
#include "libraries/emscripten-browser-file/emscripten_browser_file.h"
#endif
#include "libraries/imgui/imgui.h"

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

void FileUtilities::CreateFileDialog(const char* const title, const std::function<bool(const char *path)> &callback, const bool save)
{
#ifndef _WIN32
	static_cast<void>(window); // Unused
#endif

#ifdef _WIN32
	if (use_native_file_dialogs)
	{
		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);

		char path_buffer[MAX_PATH];
		path_buffer[0] = '\0';

		OPENFILENAME ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = SDL_GetWindowWMInfo(window.sdl, &info) ? info.info.win.window : nullptr;
		ofn.lpstrFile = path_buffer;
		ofn.nMaxFile = CC_COUNT_OF(path_buffer);
		ofn.lpstrTitle = title;

		// Common File Dialog changes the current directory, so back it up here first.
		char *working_directory_buffer = nullptr;
		const DWORD working_directory_buffer_size = GetCurrentDirectory(0, nullptr);

		if (working_directory_buffer_size != 0)
		{
			working_directory_buffer = static_cast<char*>(SDL_malloc(working_directory_buffer_size));

			if (working_directory_buffer != nullptr)
			{
				if (GetCurrentDirectory(working_directory_buffer_size, working_directory_buffer) == 0)
				{
					SDL_free(working_directory_buffer);
					working_directory_buffer = nullptr;
				}
			}
		}

		// Invoke the file dialog.
		if (save)
		{
			ofn.Flags = OFN_OVERWRITEPROMPT;
			if (GetSaveFileName(&ofn))
				callback(path_buffer);
		}
		else
		{
			ofn.Flags = OFN_FILEMUSTEXIST;
			if (GetOpenFileName(&ofn))
				callback(path_buffer);
		}

		// Restore the current directory.
		if (working_directory_buffer != nullptr)
		{
			SetCurrentDirectory(working_directory_buffer);
			SDL_free(working_directory_buffer);
		}
	}
	else
#elif defined(FILE_PICKER_POSIX)
	bool done = false;

	if (use_native_file_dialogs)
	{
		for (unsigned int i = 0; i < 2; ++i)
		{
			if (!done)
			{
				char *command;

				// Construct the command to invoke Zenity/kdialog.
				const int bytes_printed = (i == 0) != prefer_kdialog ?
					SDL_asprintf(&command, "zenity --file-selection %s --title=\"%s\" --filename=\"%s\"",
						save ? "--save" : "",
						title,
						last_file_dialog_directory == nullptr ? "" : last_file_dialog_directory)
					:
					SDL_asprintf(&command, "kdialog --get%sfilename --title \"%s\" \"%s\"",
						save ? "save" : "open",
						title,
						last_file_dialog_directory == nullptr ? "" : last_file_dialog_directory)
					;

				if (bytes_printed >= 0)
				{
					// Invoke Zenity/kdialog.
					FILE *path_stream = popen(command, "r");

					SDL_free(command);

					if (path_stream != nullptr)
					{
					#define GROW_SIZE 0x100
						// Read the whole path returned by Zenity/kdialog.
						// This is very complicated due to handling arbitrarily long paths.
						std::size_t path_buffer_length = 0;
						char *path_buffer = static_cast<char*>(SDL_malloc(GROW_SIZE + 1)); // '+1' for the null character.

						if (path_buffer != nullptr)
						{
							for (;;)
							{
								const std::size_t path_length = std::fread(&path_buffer[path_buffer_length], 1, GROW_SIZE, path_stream);
								path_buffer_length += path_length;

								if (path_length != GROW_SIZE)
									break;

								char* const new_path_buffer = static_cast<char*>(SDL_realloc(path_buffer, path_buffer_length + GROW_SIZE + 1));

								if (new_path_buffer == nullptr)
								{
									SDL_free(path_buffer);
									path_buffer = nullptr;
									break;
								}

								path_buffer = new_path_buffer;
							}
						#undef GROW_SIZE

							if (path_buffer != nullptr)
							{
								// Handle Zenity's/kdialog's return value.
								const int exit_status = pclose(path_stream);
								path_stream = nullptr;

								if (exit_status != -1 && WIFEXITED(exit_status))
								{
									switch (WEXITSTATUS(exit_status))
									{
										case 0: // Success.
										{
											done = true;

											path_buffer[path_buffer_length - (path_buffer[path_buffer_length - 1] == '\n')] = '\0';
											callback(path_buffer);

											char* const directory_separator = SDL_strrchr(path_buffer, '/');

											if (directory_separator == nullptr)
												path_buffer[0] = '\0';
											else
												directory_separator[1] = '\0';

											if (last_file_dialog_directory != nullptr)
												SDL_free(last_file_dialog_directory);

											last_file_dialog_directory = path_buffer;
											path_buffer = nullptr;

											break;
										}

										case 1: // No file selected.
											done = true;
											break;
									}
								}
							}

							SDL_free(path_buffer);
						}

						if (path_stream != nullptr)
							pclose(path_stream);
					}
				}
			}
		}
	}

	if (!done)
	#endif
	{
		active_file_picker_popup = title;
		popup_callback = callback;
		is_save_dialog = save;
	}
}

void FileUtilities::CreateOpenFileDialog(const char* const title, const std::function<bool(const char *path)> &callback)
{
	CreateFileDialog(title, callback, false);
}

void FileUtilities::CreateSaveFileDialog(const char* const title, const std::function<bool(const char *path)> &callback)
{
	CreateFileDialog(title, callback, true);
}

void FileUtilities::DisplayFileDialog(char *&drag_and_drop_filename)
{
	if (active_file_picker_popup != nullptr)
	{
		if (!ImGui::IsPopupOpen(active_file_picker_popup))
			ImGui::OpenPopup(active_file_picker_popup);

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(active_file_picker_popup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			static int text_buffer_size;
			static char *text_buffer = nullptr;

			if (text_buffer == nullptr)
			{
				text_buffer_size = 0x40;
				text_buffer = static_cast<char*>(SDL_malloc(text_buffer_size));
				text_buffer[0] = '\0';
			}

			const ImGuiInputTextCallback callback = [](ImGuiInputTextCallbackData* const data)
			{
				if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
				{
					if (data->BufSize > text_buffer_size)
					{
						int new_text_buffer_size = (INT_MAX >> 1) + 1; // Largest power of 2.

						// Find the first power of two that is larger than the requested buffer size.
						while (data->BufSize < new_text_buffer_size >> 1)
							new_text_buffer_size >>= 1;

						char* const new_text_buffer = static_cast<char*>(SDL_realloc(text_buffer, new_text_buffer_size));

						if (new_text_buffer != nullptr)
						{
							data->BufSize = text_buffer_size = new_text_buffer_size;
							data->Buf = text_buffer = new_text_buffer;
						}
					}
				}

				return 0;
			};

			/* Make it so that the text box is selected by default,
			   so that the user doesn't have to click on it first.
			   If a file is dropped onto the dialog, focus on the
			   'open' button instead or else the text box won't show
			   the dropped file's path. */
			if (drag_and_drop_filename != nullptr)
				ImGui::SetKeyboardFocusHere(1);
			else if (ImGui::IsWindowAppearing())
				ImGui::SetKeyboardFocusHere();

			ImGui::TextUnformatted("Filename:");
			const bool enter_pressed = ImGui::InputText("##filename", text_buffer, text_buffer_size, ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_EnterReturnsTrue, callback);

			// Set the text box's contents to the dropped file's path.
			if (drag_and_drop_filename != nullptr)
			{
				SDL_free(text_buffer);
				text_buffer = drag_and_drop_filename;
				drag_and_drop_filename = nullptr;

				text_buffer_size = SDL_strlen(text_buffer) + 1;
			}

			const auto centre_buttons = [](const std::vector<const char*> &labels)
			{
				float width = 0;

				width += ImGui::GetStyle().ItemSpacing.x * (labels.size() - 1);
				width += ImGui::GetStyle().FramePadding.x * 2 * labels.size();

				for (const auto label : labels)
					width += ImGui::CalcTextSize(label).x;

				ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x + (ImGui::GetContentRegionAvail().x - width) / 2);
			};

			centre_buttons({is_save_dialog ? "Save" : "Open", "Cancel"});
			const bool ok_pressed = ImGui::Button(is_save_dialog ? "Save" : "Open");
			ImGui::SameLine();
			bool exit = ImGui::Button("Cancel");

			bool submit = false;

			if (enter_pressed || ok_pressed)
			{
				if (!is_save_dialog || !FileExists(text_buffer))
					submit = true;
				else
					ImGui::OpenPopup("File Already Exists");
			}

			ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

			if (ImGui::BeginPopupModal("File Already Exists", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::TextUnformatted("A file with that name already exists. Overwrite it?");

				centre_buttons({"Yes", "No"});
				if (ImGui::Button("Yes"))
				{
					submit = true;
					ImGui::CloseCurrentPopup();
				}

				ImGui::SameLine();

				if (ImGui::Button("No"))
					ImGui::CloseCurrentPopup();

				ImGui::EndPopup();
			}

			if (submit)
			{
				if (popup_callback(text_buffer))
					exit = true;
				else
					ImGui::OpenPopup(is_save_dialog ? "Could Not Save File" : "Could Not Open File");
			}

			if (ImGui::BeginPopupModal("Could Not Open File", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::TextUnformatted("File could not be opened.");

				centre_buttons({"OK"});
				if (ImGui::Button("OK"))
					ImGui::CloseCurrentPopup();

				ImGui::EndPopup();
			}

			if (ImGui::BeginPopupModal("Could Not Save File", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::TextUnformatted("File could not be saved.");

				centre_buttons({"OK"});
				if (ImGui::Button("OK"))
					ImGui::CloseCurrentPopup();

				ImGui::EndPopup();
			}

			if (exit)
			{
				ImGui::CloseCurrentPopup();
				SDL_free(text_buffer);
				text_buffer = nullptr;
				active_file_picker_popup = nullptr;
			}

			ImGui::EndPopup();
		}
	}
}

bool FileUtilities::FileExists(const char* const filename)
{
	SDL_RWops* const file = SDL_RWFromFile(filename, "rb");

	if (file != nullptr)
	{
		SDL_RWclose(file);
		return true;
	}

	return false;
}

void FileUtilities::LoadFileToBuffer(const char *filename, unsigned char *&file_buffer, std::size_t &file_size)
{
	file_buffer = nullptr;
	file_size = 0;

	SDL_RWops *file = SDL_RWFromFile(filename, "rb");

	if (file == nullptr)
	{
		debug_log.Log("SDL_RWFromFile failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		LoadFileToBuffer(file, file_buffer, file_size);

		if (SDL_RWclose(file) < 0)
			debug_log.Log("SDL_RWclose failed with the following message - '%s'", SDL_GetError());
	}
}

void FileUtilities::LoadFileToBuffer(SDL_RWops *file, unsigned char *&file_buffer, std::size_t &file_size)
{
	const Sint64 size_s64 = SDL_RWsize(file);

	if (size_s64 < 0)
	{
		debug_log.Log("SDL_RWsize failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		const std::size_t size = static_cast<std::size_t>(size_s64);

		file_buffer = static_cast<unsigned char*>(SDL_malloc(size));

		if (file_buffer == nullptr)
		{
			debug_log.Log("Could not allocate memory for file");
		}
		else
		{
			file_size = size;

			SDL_RWread(file, file_buffer, 1, size);
		}
	}
}

void FileUtilities::LoadFile(const char* const title, const std::function<void(const char* const path, SDL_RWops *file)> &callback)
{
#ifdef __EMSCRIPTEN__
	static_cast<void>(title);

	struct CallbackHolder
	{
		std::function<void(const char* const path, SDL_RWops *file)> callback;
	};

	CallbackHolder* const holder = (CallbackHolder*)SDL_malloc(sizeof(CallbackHolder));

	if (holder != nullptr)
	{
		new(holder) CallbackHolder{callback};

		const auto call_callback = [](const std::string &/*filename*/, const std::string &/*mime_type*/, std::string_view buffer, void* const user_data)
		{
			CallbackHolder* const holder = static_cast<CallbackHolder*>(user_data);
			SDL_RWops* const file = SDL_RWFromConstMem(buffer.data(), buffer.size());

			if (file != nullptr)
			{
				holder->callback(nullptr, file);
				SDL_free(holder);
			}
		};

		emscripten_browser_file::upload("", call_callback, holder);
	}
#else
	CreateOpenFileDialog(title, [callback](const char* const path)
	{
		SDL_RWops* const file = SDL_RWFromFile(path, "rb");

		if (file == nullptr)
			return false;

		callback(path, file);

		return true;
	});
#endif
}

void FileUtilities::SaveFile(const char* const title, const std::function<bool(const std::function<bool(const void *data, std::size_t data_size)> &save_file)> &callback)
{
#ifdef __EMSCRIPTEN__
	static_cast<void>(title);

	callback([](const void* const data, const std::size_t data_size)
	{
		emscripten_browser_file::download("", "application/octet-stream", std::string_view(static_cast<const char*>(data), data_size));
		return true;
	});
#else
	CreateSaveFileDialog(title, [callback](const char* const path)
	{
		const auto save_file = [path](const void* const data, const std::size_t data_size)
		{
			SDL_RWops* const file = SDL_RWFromFile(path, "wb");

			if (file == nullptr)
				return false;

			const bool success = SDL_RWwrite(file, data, 1, data_size) == data_size;

			SDL_RWclose(file);

			return success;
		};

		return callback(save_file);
	});
#endif
}

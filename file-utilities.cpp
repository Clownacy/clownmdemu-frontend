#include "file-utilities.h"

#include <array>
#include <climits>
#include <vector>

#ifdef _WIN32
#include <memory>
#include <windows.h>
#include "SDL_syswm.h"
#elif defined(FILE_PICKER_POSIX)
#include <cstdio>
#include <format>
#include <sys/wait.h>
#endif

#ifdef __EMSCRIPTEN__
#include "libraries/emscripten-browser-file/emscripten_browser_file.h"
#endif
#include "libraries/imgui/imgui.h"

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#ifdef _WIN32
// Adapted from SDL2.
#define StringToUTF8W(S) SDL::Pointer<char>(SDL_iconv_string("UTF-8", "UTF-16LE", reinterpret_cast<const char*>(S), (SDL_wcslen(S) + 1) * sizeof(WCHAR)))
#define UTF8ToStringW(S) SDL::Pointer<WCHAR>(reinterpret_cast<WCHAR*>(SDL_iconv_string("UTF-16LE", "UTF-8", reinterpret_cast<const char*>(S), SDL_strlen(S) + 1)))
#define StringToUTF8A(S) SDL::Pointer<char>(SDL_iconv_string("UTF-8", "ASCII", reinterpret_cast<const char*>(S), (SDL_strlen(S) + 1)))
#define UTF8ToStringA(S) SDL::Pointer<char>(SDL_iconv_string("ASCII", "UTF-8", reinterpret_cast<const char*>(S), SDL_strlen(S) + 1))
#ifdef UNICODE
#define StringToUTF8 StringToUTF8W
#define UTF8ToString UTF8ToStringW
#else
#define StringToUTF8 StringToUTF8A
#define UTF8ToString UTF8ToStringA
#endif
#endif

void FileUtilities::CreateFileDialog([[maybe_unused]] const Window &window, const std::string &title, const PopupCallback &callback, const bool save)
{
	bool success = true;

#ifdef _WIN32
	if (use_native_file_dialogs)
	{
		const auto title_utf16 = UTF8ToString(title.c_str());

		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);

		std::array<TCHAR, MAX_PATH> path_buffer;
		path_buffer[0] = '\0';

		OPENFILENAME ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = SDL_GetWindowWMInfo(window.GetSDLWindow(), &info) ? info.info.win.window : nullptr;
		ofn.lpstrFile = &path_buffer[0];
		ofn.nMaxFile = path_buffer.size();
		ofn.lpstrTitle = title_utf16.get(); // It's okay for this to be nullptr.
		ofn.Flags = save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST;

		// Common File Dialog changes the current directory, so back it up here first.
		std::vector<TCHAR> working_directory(GetCurrentDirectory(0, nullptr));
		GetCurrentDirectory(working_directory.size(), working_directory.data());

		// Invoke the file dialog.
		const bool file_selected = save ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn);

		if (file_selected)
		{
			if (!callback(path_buffer.data()))
				success = false;
		}

		// Restore the current directory.
		if (!working_directory.empty())
			SetCurrentDirectory(working_directory.data());
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
				// Construct the command to invoke Zenity/kdialog.
				const std::string command = (i == 0) != prefer_kdialog ?
					std::format("zenity --file-selection {} --title=\"{}\" --filename=\"{}\"",
						save ? "--save" : "",
						title,
						last_file_dialog_directory.string())
					:
					std::format("kdialog --get{}filename --title \"{}\" \"{}\"",
						save ? "save" : "open",
						title,
						last_file_dialog_directory.string())
					;

				// Invoke Zenity/kdialog.
				FILE *path_stream = popen(command.c_str(), "r");

				if (path_stream != nullptr)
				{
				#define GROW_SIZE 0x100
					// Read the whole path returned by Zenity/kdialog.
					// This is very complicated due to handling arbitrarily long paths.
					char *path_buffer = static_cast<char*>(SDL_malloc(GROW_SIZE + 1)); // '+1' for the null character.

					if (path_buffer != nullptr)
					{
						std::size_t path_buffer_length = 0;

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
										if (!callback(path_buffer))
											success = false;

										char* const directory_separator = SDL_strrchr(path_buffer, '/');

										if (directory_separator == nullptr)
											path_buffer[0] = '\0';
										else
											directory_separator[1] = '\0';

										last_file_dialog_directory = path_buffer;
										SDL_free(path_buffer);
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

	if (!done)
	#endif
	{
		active_file_picker_popup = title;
		popup_callback = callback;
		is_save_dialog = save;
	}

	if (!success)
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", is_save_dialog ? "Could not save file." : "Could not open file.", nullptr);
}

void FileUtilities::CreateOpenFileDialog(const Window &window, const std::string &title, const PopupCallback &callback)
{
	CreateFileDialog(window, title, callback, false);
}

void FileUtilities::CreateSaveFileDialog(const Window &window, const std::string &title, const PopupCallback &callback)
{
	CreateFileDialog(window, title, callback, true);
}

void FileUtilities::DisplayFileDialog(std::filesystem::path &drag_and_drop_filename)
{
	if (active_file_picker_popup.has_value())
	{
		if (!ImGui::IsPopupOpen(active_file_picker_popup->c_str()))
			ImGui::OpenPopup(active_file_picker_popup->c_str());

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(active_file_picker_popup->c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			const ImGuiInputTextCallback callback = [](ImGuiInputTextCallbackData* const data)
			{
				FileUtilities* const file_utilities = static_cast<FileUtilities*>(data->UserData);

				if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
				{
					if (static_cast<std::size_t>(data->BufSize) > file_utilities->text_buffer.size())
					{
						file_utilities->text_buffer.resize(data->BufSize);
						data->Buf = file_utilities->text_buffer.data();
					}
				}

				return 0;
			};

			/* Make it so that the text box is selected by default,
			   so that the user doesn't have to click on it first.
			   If a file is dropped onto the dialog, focus on the
			   'open' button instead or else the text box won't show
			   the dropped file's path. */
			if (!drag_and_drop_filename.empty())
				ImGui::SetKeyboardFocusHere(1);
			else if (ImGui::IsWindowAppearing())
				ImGui::SetKeyboardFocusHere();

			ImGui::TextUnformatted("Filename:");
			const bool enter_pressed = ImGui::InputText("##filename", text_buffer.data(), text_buffer.capacity() + 1, ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_EnterReturnsTrue, callback, this);

			// Set the text box's contents to the dropped file's path.
			if (!drag_and_drop_filename.empty())
			{
				text_buffer = drag_and_drop_filename.string();
				drag_and_drop_filename.clear();
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
				text_buffer.clear();
				text_buffer.shrink_to_fit();
				active_file_picker_popup.reset();
			}

			ImGui::EndPopup();
		}
	}
}

bool FileUtilities::FileExists(const std::filesystem::path &path)
{
	return SDL::RWFromFile(path, "rb") != nullptr;
}

bool FileUtilities::LoadFileToBuffer(std::vector<unsigned char> &file_buffer, const std::filesystem::path &path)
{
	SDL::RWops file = SDL::RWFromFile(path, "rb");

	if (file == nullptr)
	{
		debug_log.Log("SDL_RWFromFile failed with the following message - '%s'", SDL_GetError());
		return false;
	}

	return LoadFileToBuffer(file_buffer, file);
}

bool FileUtilities::LoadFileToBuffer(std::vector<unsigned char> &file_buffer, const SDL::RWops &file)
{
	const Sint64 size_s64 = SDL_RWsize(file.get());

	if (size_s64 < 0)
	{
		debug_log.Log("SDL_RWsize failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		const std::size_t size = static_cast<std::size_t>(size_s64);

		try
		{
			file_buffer.resize(size);
			SDL_RWread(file.get(), file_buffer.data(), 1, size);
			return true;
		}
		catch (const std::bad_alloc&)
		{
			debug_log.Log("Could not allocate memory for file");
		}
	}

	return false;
}

void FileUtilities::LoadFile([[maybe_unused]] const Window &window, [[maybe_unused]] const std::string &title, const LoadFileCallback &callback)
{
#ifdef __EMSCRIPTEN__
	try
	{
		LoadFileCallback* const callback_detatched = new LoadFileCallback(callback);

		const auto call_callback = [](const std::string &/*filename*/, const std::string &/*mime_type*/, std::string_view buffer, void* const user_data)
		{
			const std::unique_ptr<LoadFileCallback> callback(static_cast<LoadFileCallback*>(user_data));
			SDL::RWops file = SDL::RWops(SDL_RWFromConstMem(buffer.data(), buffer.size()));

			if (file != nullptr)
				(*callback.get())(nullptr, file);
		};

		emscripten_browser_file::upload("", call_callback, callback_detatched);
	}
	catch (const std::bad_alloc&)
	{
		debug_log.Log("FileUtilities::LoadFile: Failed to allocate memory.");
	}
#else
	CreateOpenFileDialog(window, title, [callback](const std::filesystem::path &path)
	{
		SDL::RWops file = SDL::RWFromFile(path, "rb");

		if (file == nullptr)
			return false;

		return callback(path, std::move(file));
	});
#endif
}

void FileUtilities::SaveFile([[maybe_unused]] const Window &window, [[maybe_unused]] const std::string &title, const SaveFileCallback &callback)
{
#ifdef __EMSCRIPTEN__
	callback([](const void* const data, const std::size_t data_size)
	{
		emscripten_browser_file::download("", "application/octet-stream", std::string_view(static_cast<const char*>(data), data_size));
		return true;
	});
#else
	CreateSaveFileDialog(window, title, [callback](const std::filesystem::path &path)
	{
		const auto save_file = [path](const void* const data, const std::size_t data_size)
		{
			const SDL::RWops file = SDL::RWFromFile(path, "wb");

			if (file == nullptr)
				return false;

			return SDL_RWwrite(file.get(), data, 1, data_size) == data_size;
		};

		return callback(save_file);
	});
#endif
}

#include "file-utilities.h"

#include <climits>
#include <vector>

#ifdef __EMSCRIPTEN__
#include "libraries/emscripten-browser-file/emscripten_browser_file.h"
#endif
#include "libraries/imgui/imgui.h"

#include "common/core/clowncommon/clowncommon.h"

void FileUtilities::CreateFileDialog([[maybe_unused]] Window &window, const std::string &title, const PopupCallback &callback, const bool save)
{
	bool done = false;

	if (use_native_file_dialogs)
	{
		// TODO: According to documentation, the callback may be ran on a different thread, so find some way to ensure that there are no race conditions!
		try
		{
			PopupCallback* const callback_detatched = new PopupCallback(callback);

			const auto call_callback = [](void* const user_data, const char* const* const file_list, [[maybe_unused]] const int filter)
			{
				const std::unique_ptr<PopupCallback> callback(static_cast<PopupCallback*>(user_data));

				if (file_list == nullptr || *file_list == nullptr)
					return;

				(*callback.get())(reinterpret_cast<const char8_t*>(*file_list));
			};

			const auto properties = SDL_CreateProperties();
			SDL_SetPointerProperty(properties, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window.GetSDLWindow());
			SDL_SetStringProperty(properties, SDL_PROP_FILE_DIALOG_TITLE_STRING, title.c_str());
			SDL_ShowFileDialogWithProperties(save ? SDL_FILEDIALOG_SAVEFILE : SDL_FILEDIALOG_OPENFILE, call_callback, callback_detatched, properties);
			SDL_DestroyProperties(properties);

			done = true;
		}
		catch (const std::bad_alloc&)
		{
			Frontend::debug_log.Log("FileUtilities::CreateFileDialog: Failed to allocate memory.");
		}
	}

	if (!done)
	{
		active_file_picker_popup = title;
		popup_callback = callback;
		is_save_dialog = save;
	}
}

void FileUtilities::CreateOpenFileDialog(Window &window, const std::string &title, const PopupCallback &callback)
{
	CreateFileDialog(window, title, callback, false);
}

void FileUtilities::CreateSaveFileDialog(Window &window, const std::string &title, const PopupCallback &callback)
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
	return SDL::GetPathInfo(path, nullptr);
}

void FileUtilities::LoadFile([[maybe_unused]] Window &window, [[maybe_unused]] const std::string &title, const LoadFileCallback &callback)
{
#ifdef __EMSCRIPTEN__
	try
	{
		LoadFileCallback* const callback_detatched = new LoadFileCallback(callback);

		const auto call_callback = [](const std::string &filename, const std::string &/*mime_type*/, std::string_view buffer, void* const user_data)
		{
			const std::unique_ptr<LoadFileCallback> callback(static_cast<LoadFileCallback*>(user_data));
			SDL::IOStream file(buffer.data(), buffer.size());

			if (file)
				(*callback.get())(filename, std::move(file));
		};

		emscripten_browser_file::upload("", call_callback, callback_detatched);
	}
	catch (const std::bad_alloc&)
	{
		Frontend::debug_log.Log("FileUtilities::LoadFile: Failed to allocate memory.");
	}
#else
	CreateOpenFileDialog(window, title, [callback](const std::filesystem::path &path)
	{
		SDL::IOStream file(path, "rb");

		if (!file)
			return false;

		return callback(path, std::move(file));
	});
#endif
}

void FileUtilities::SaveFile([[maybe_unused]] Window &window, [[maybe_unused]] const std::string &title, const SaveFileCallback &callback)
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
			SDL::IOStream file(path, "wb");

			if (!file)
				return false;

			return SDL_WriteIO(file, data, data_size) == data_size;
		};

		return callback(save_file);
	});
#endif
}

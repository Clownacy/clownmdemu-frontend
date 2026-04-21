#include "file-utilities.h"

#include <climits>
#include <cstdlib>
#include <vector>

#ifdef __EMSCRIPTEN__
#include "../libraries/emscripten-browser-file/emscripten_browser_file.h"
#endif
#include "../libraries/imgui/misc/cpp/imgui_stdlib.h"

#include "../common/clowncd/libraries/chd/libchdr/deps/lzma-25.01/include/LzmaDec.h"
#include "../common/clowncd/libraries/chd/libchdr/deps/miniz-3.1.1/miniz.h"
#include "../common/core/libraries/clowncommon/clowncommon.h"

void FileUtilities::CreateFileDialog([[maybe_unused]] Window &window, const std::string &title, PopupCallback callback, const bool save)
{
	const auto CreateFallbackFileDialog = [=, this](PopupCallback callback)
	{
		active_file_picker_popup = title;
		popup_callback = std::move(callback);
		is_save_dialog = save;
	};

	if (use_native_file_dialogs)
	{
		const auto properties = SDL_CreateProperties();
		SDL_SetPointerProperty(properties, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window.GetSDLWindow());
		SDL_SetStringProperty(properties, SDL_PROP_FILE_DIALOG_TITLE_STRING, title.c_str());
		SDL::ShowFileDialogWithProperties(
			save ? SDL_FILEDIALOG_SAVEFILE : SDL_FILEDIALOG_OPENFILE,
			[=, callback = std::move(callback)](const char* const* const file_list, [[maybe_unused]] const int filter) mutable
			{
				// The callback may be ran on a different thread, so proxy to the main thread to avoid race conditions!
				SDL::RunOnMainThread(
					[=, callback = std::move(callback)]() mutable
					{
						// SDL error; use fallback dialog.
						if (file_list == nullptr)
						{
							CreateFallbackFileDialog(std::move(callback));
							return;
						}

						// User cancelled.
						if (*file_list == nullptr)
							return;

						// Success.
						callback(U8Path(*file_list));
					}
				);
			},
			properties
		);
		SDL_DestroyProperties(properties);
	}
	else
	{
		CreateFallbackFileDialog(std::move(callback));
	}
}

void FileUtilities::CreateOpenFileDialog(Window &window, const std::string &title, PopupCallback callback)
{
	CreateFileDialog(window, title, std::move(callback), false);
}

void FileUtilities::CreateSaveFileDialog(Window &window, const std::string &title, PopupCallback callback)
{
	CreateFileDialog(window, title, std::move(callback), true);
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
			const bool enter_pressed = ImGui::InputText("##filename", &text_buffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_ElideLeft);

			// Set the text box's contents to the dropped file's path.
			if (!drag_and_drop_filename.empty())
			{
				FileUtilities::PathToStringView(drag_and_drop_filename,
					[&](const std::string_view &string_view)
					{
						text_buffer = string_view;
					}
				);

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
				if (!is_save_dialog || !FileExists(U8Path(text_buffer)))
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
				if (popup_callback(U8Path(text_buffer)))
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

void FileUtilities::LoadFile([[maybe_unused]] Window &window, [[maybe_unused]] const std::string &title, LoadFileCallback callback)
{
#ifdef __EMSCRIPTEN__
	try
	{
		LoadFileCallback* const callback_detatched = new LoadFileCallback(callback);

		const auto call_callback = [](const std::string &filename, const std::string &/*mime_type*/, emscripten_browser_file::buffer_unique_ptr &&buffer, size_t buffer_size, void* const user_data)
		{
			const std::unique_ptr<LoadFileCallback> callback(static_cast<LoadFileCallback*>(user_data));
			SDL::IOStream file(buffer.release(), buffer_size);

			if (file)
			{
				SDL_SetPointerProperty(SDL_GetIOProperties(file), SDL_PROP_IOSTREAM_MEMORY_FREE_FUNC_POINTER, reinterpret_cast<void*>(std::free));

				(*callback.get())(filename, std::move(file));
			}
		};

		emscripten_browser_file::upload("", call_callback, callback_detatched);
	}
	catch (const std::bad_alloc&)
	{
		Frontend::debug_log.Log("FileUtilities::LoadFile: Failed to allocate memory.");
	}
#else
	CreateOpenFileDialog(window, title, [callback = std::move(callback)](const std::filesystem::path &path) mutable
	{
		SDL::IOStream file(path, "rb");

		if (!file)
			return false;

		return callback(path, std::move(file));
	});
#endif
}

void FileUtilities::SaveFile([[maybe_unused]] Window &window, [[maybe_unused]] const std::string &title, SaveFileCallback callback)
{
#ifdef __EMSCRIPTEN__
	callback([](const void* const data, const std::size_t data_size)
	{
		emscripten_browser_file::download("", "application/octet-stream", std::string_view(static_cast<const char*>(data), data_size));
		return true;
	});
#else
	CreateSaveFileDialog(window, title, [callback = std::move(callback)](const std::filesystem::path &path) mutable
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

std::optional<std::vector<cc_u16l>> FileUtilities::LoadZIPFileToBuffer(SDL::IOStream &file, const unsigned int file_index)
{
	const auto starting_position = SDL_TellIO(file);

	std::optional<std::vector<cc_u16l>> file_buffer;

	mz_zip_archive miniz;
	mz_zip_zero_struct(&miniz);
	miniz.m_pRead = [](void* const pOpaque, const mz_uint64 file_ofs, void* const pBuf, const std::size_t n) -> std::size_t
	{
		SDL::IOStream &file = *static_cast<SDL::IOStream*>(pOpaque);

		SDL_SeekIO(file, file_ofs, SDL_IO_SEEK_SET);
		return SDL_ReadIO(file, pBuf, n);
	};
	miniz.m_pIO_opaque = &file;

	if (mz_zip_reader_init(&miniz, SDL_GetIOSize(file), 0))
	{
		if (mz_zip_validate_file(&miniz, file_index, MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY))
		{
			file_buffer.emplace();

			if (!mz_zip_reader_extract_to_callback(
				&miniz, file_index,
				[](void* const pOpaque, const mz_uint64 file_ofs, const void* const pBuf, const std::size_t n) -> std::size_t
				{
					auto &file_buffer = *static_cast<std::vector<cc_u16l>*>(pOpaque);
					auto bytes = static_cast<const unsigned char*>(pBuf);

					const auto end = (file_ofs + n + 1) / 2;
					if (file_buffer.size() < end)
						file_buffer.resize(end);

					for (std::size_t i = 0; i < n; ++i)
					{
						const auto byte_position = file_ofs + i;
						const bool upper_byte = byte_position % 2 == 0;
						const unsigned int shift = upper_byte ? 8 : 0;
						auto &value = file_buffer[byte_position / 2];

						value &= ~(0xFFu << shift);
						value |= bytes[i] << shift;
					}

					return n;
				},
				&*file_buffer, 0
			))
				file_buffer = std::nullopt;
		}

		mz_zip_reader_end(&miniz);
	}

	SDL_SeekIO(file, starting_position, SDL_IO_SEEK_SET);

	return file_buffer;
}

std::optional<std::vector<unsigned char>> FileUtilities::DecompressLZMABuffer(const unsigned char* const input_buffer, const std::size_t input_buffer_size, const std::size_t uncompressed_size)
{
	std::optional<std::vector<unsigned char>> decompressed_buffer(uncompressed_size);

	SizeT output_written = std::size(*decompressed_buffer), input_read = input_buffer_size;
	constexpr unsigned int header_size = 13;
	ELzmaStatus status;
	static constexpr ISzAlloc allocation = {
		[]([[maybe_unused]] const ISzAllocPtr p, const std::size_t size)
		{
			return std::malloc(size);
		},
		[]([[maybe_unused]] const ISzAllocPtr p, void* const address)
		{
			std::free(address);
		}
	};
	const auto result = LzmaDecode(std::data(*decompressed_buffer), &output_written, input_buffer + header_size, &input_read, input_buffer, header_size, LZMA_FINISH_END, &status, &allocation);

	if (result != SZ_OK)
		decompressed_buffer = std::nullopt;

	return decompressed_buffer;
}

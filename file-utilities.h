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

	template<typename T, std::size_t S>
	T ReadFromIOStream(SDL::IOStream &file);

	template<typename T, std::size_t S>
	void ReadFromIOStream(SDL::IOStream &file, std::vector<T> &buffer)
	{
		for (auto &value : buffer)
			value = ReadFromIOStream<T, S>(file);
	}

	template<typename T, std::size_t S>
	std::optional<std::vector<T>> LoadFileToBuffer(SDL::IOStream &file)
	{
		const Sint64 size_s64 = SDL_GetIOSize(file) / S;

		if (size_s64 < 0)
		{
			Frontend::debug_log.Log("SDL_GetIOSize failed with the following message - '{}'", SDL_GetError());
		}
		else
		{
			const std::size_t size = static_cast<std::size_t>(size_s64);

			try
			{
				std::vector<T> buffer(size);
				ReadFromIOStream<T, S>(file, buffer);
				return std::move(buffer);
			}
			catch (const std::bad_alloc&)
			{
				Frontend::debug_log.Log("Could not allocate memory for file");
			}
		}

		return std::nullopt;
	}

	template<typename T, std::size_t S>
	std::optional<std::vector<T>> LoadFileToBuffer(const std::filesystem::path &path)
	{
		SDL::IOStream file(path, "rb");

		if (!file)
		{
			Frontend::debug_log.Log("SDL_IOFromFile failed with the following message - '{}'", SDL_GetError());
			return std::nullopt;
		}

		return LoadFileToBuffer<T, S>(file);
	}

	void LoadFile(Window &window, const std::string &title, const LoadFileCallback &callback);
	void SaveFile(Window &window, const std::string &title, const SaveFileCallback &callback);
};

template<>
inline cc_u16l FileUtilities::ReadFromIOStream<cc_u16l, 2>(SDL::IOStream &file)
{
	Uint16 integer;
	SDL_ReadU16BE(file, &integer);
	return integer;
}

template<>
inline void FileUtilities::ReadFromIOStream<unsigned char, 1>(SDL::IOStream &file, std::vector<unsigned char> &buffer)
{
	SDL_ReadIO(file, std::data(buffer), std::size(buffer));
}

#endif /* FILE_UTILITIES_H */

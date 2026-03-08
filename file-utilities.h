#ifndef FILE_UTILITIES_H
#define FILE_UTILITIES_H

#include <array>
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

	static bool FileExists(const std::filesystem::path &path);

	template<typename T, std::size_t S>
	static bool ReadFromIOStream(SDL::IOStream &file, T &integer)
	{
		// Read bytes.
		std::array<unsigned char, S> bytes;
		if (SDL_ReadIO(file, std::data(bytes), std::size(bytes)) != std::size(bytes))
			return false;

		// Combine bytes into an integer.
		integer = 0;
		for (auto &byte : bytes)
		{
			integer <<= 8;
			integer |= byte;
		}

		return true;
	}

	template<typename T, std::size_t S>
	static bool ReadFromIOStream(SDL::IOStream &file, std::vector<T> &buffer)
	{
		for (auto &value : buffer)
			 if (!ReadFromIOStream<T, S>(file, value))
				 return false;

		return true;
	}

	template<typename T, std::size_t S>
	static std::optional<std::vector<T>> LoadFileToBuffer(SDL::IOStream &file)
	{
		const Sint64 size_s64 = SDL_GetIOSize(file) / S;

		if (size_s64 < 0)
		{
			Frontend::debug_log.SDLError("SDL_GetIOSize");
		}
		else
		{
			const std::size_t size = static_cast<std::size_t>(size_s64);

			try
			{
				std::vector<T> buffer(size);

				if (!ReadFromIOStream<T, S>(file, buffer))
					return std::nullopt;

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
	static std::optional<std::vector<T>> LoadFileToBuffer(const std::filesystem::path &path)
	{
		SDL::IOStream file(path, "rb");

		if (!file)
		{
			Frontend::debug_log.SDLError("SDL_IOFromFile");
			return std::nullopt;
		}

		return LoadFileToBuffer<T, S>(file);
	}

	static std::optional<std::vector<cc_u16l>> LoadZIPFileToBuffer(SDL::IOStream &file, unsigned int file_index);
	static std::optional<std::vector<unsigned char>> DecompressLZMABuffer(const unsigned char *buffer, std::size_t buffer_size, std::size_t uncompressed_size);

	void LoadFile(Window &window, const std::string &title, const LoadFileCallback &callback);
	void SaveFile(Window &window, const std::string &title, const SaveFileCallback &callback);

	template<typename... Ts>
	static auto U8Path(Ts &&...args)
	{
		return SDL::U8Path(std::forward<Ts>(args)...);
	}
};

template<>
inline bool FileUtilities::ReadFromIOStream<unsigned char, 1>(SDL::IOStream &file, std::vector<unsigned char> &buffer)
{
	return SDL_ReadIO(file, std::data(buffer), std::size(buffer)) == std::size(buffer);
}

#endif /* FILE_UTILITIES_H */

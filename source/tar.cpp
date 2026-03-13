#include "tar.h"

#include <algorithm>

#include "file-utilities.h"

TarBall::TarBall(const DataSpan &input_buffer, const std::size_t uncompressed_size, const Compression compression)
{
	switch (compression)
	{
		case Compression::None:
			buffer = input_buffer;
			break;

		case Compression::LZMA:
			auto decompressed_input_buffer = FileUtilities::DecompressLZMABuffer(std::data(input_buffer), std::size(input_buffer), uncompressed_size);

			if (!decompressed_input_buffer.has_value())
				return;

			decompressed_buffer = std::move(*decompressed_input_buffer);
			buffer = decompressed_buffer;
			break;
	}
}

std::optional<TarBall::DataSpan> TarBall::OpenFile(const std::filesystem::path &path)
{
	const auto &ReadOctal = [&](const std::size_t offset, const std::size_t length)
	{
		const auto &span = buffer.subspan(offset, length);
		const auto &start = reinterpret_cast<const char*>(span.begin());
		const auto &end = reinterpret_cast<const char*>(span.end());
		return FileUtilities::StringToInteger<std::size_t>(std::string_view(start, end), 8);
	};

	for (std::size_t offset = 0; offset < std::size(buffer); offset += block_size)
	{
		const auto file_size = ReadOctal(offset + 124, 11);

		if (!file_size.has_value())
			return std::nullopt;

		const std::u8string_view file_path_string(reinterpret_cast<const char8_t*>(buffer.begin() + offset), 100);
		const std::filesystem::path file_path(file_path_string.substr(0, file_path_string.find_first_of('\0'))); // Remove trailing null characters.

		if (path == file_path)
			return buffer.subspan(offset + block_size, *file_size);

		offset += CC_DIVIDE_CEILING(*file_size, block_size) * block_size;
	}

	return std::nullopt;
}

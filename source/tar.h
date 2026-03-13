#ifndef TAR_H
#define TAR_H

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

#include <tcb/span.hpp>

class TarBall
{
private:
	static constexpr std::size_t block_size = 0x200;

	std::vector<unsigned char> decompressed_buffer;
	tcb::span<const unsigned char> buffer;

public:
	using DataSpan = tcb::span<const unsigned char>;

	enum class Compression
	{
		None,
		LZMA,
	};

	TarBall(const DataSpan &input_buffer, std::size_t uncompressed_size = 0, Compression compression = Compression::None);

	std::optional<DataSpan> OpenFile(const std::filesystem::path &path);
};

#endif // TAR_H

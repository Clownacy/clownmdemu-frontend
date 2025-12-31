#include "cd-reader.h"

#include <climits>

CDReader::ErrorCallback CDReader::error_callback;

void* CDReader::FileOpenCallback(const char* const filename, const ClownCD_FileMode mode)
{
	const char *mode_string;

	switch (mode)
	{
		case CLOWNCD_RB:
			mode_string = "rb";
			break;

		case CLOWNCD_WB:
			mode_string = "wb";
			break;

		default:
			return nullptr;
	}

	return SDL_IOFromFile(filename, mode_string);
}

int CDReader::FileCloseCallback(void* const stream)
{
	return SDL_CloseIO(static_cast<SDL_IOStream*>(stream));
}

std::size_t CDReader::FileReadCallback(void* const buffer, const std::size_t size, const std::size_t count, void* const stream)
{
	if (size == 0 || count == 0)
		return 0;

	return SDL_ReadIO(static_cast<SDL_IOStream*>(stream), buffer, size * count) / size;
}

std::size_t CDReader::FileWriteCallback(const void* const buffer, const std::size_t size, const std::size_t count, void* const stream)
{
	if (size == 0 || count == 0)
		return 0;

	return SDL_WriteIO(static_cast<SDL_IOStream*>(stream), buffer, size * count) / size;
}

long CDReader::FileTellCallback(void* const stream)
{
	const auto position = SDL_TellIO(static_cast<SDL_IOStream*>(stream));

	if (position < LONG_MIN || position > LONG_MAX)
		return -1L;

	return position;
}

int CDReader::FileSeekCallback(void* const stream, const long position, const ClownCD_FileOrigin origin)
{
	SDL_IOWhence whence;

	switch (origin)
	{
		case CLOWNCD_SEEK_SET:
			whence = SDL_IO_SEEK_SET;
			break;

		case CLOWNCD_SEEK_CUR:
			whence = SDL_IO_SEEK_CUR;
			break;

		case CLOWNCD_SEEK_END:
			whence = SDL_IO_SEEK_END;
			break;

		default:
			return -1;
	}

	return SDL_SeekIO(static_cast<SDL_IOStream*>(stream), position, whence) == -1 ? -1 : 0;
}

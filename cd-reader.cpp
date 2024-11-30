#include "cd-reader.h"

#include <climits>

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

	return SDL_RWFromFile(filename, mode_string);
}

int CDReader::FileCloseCallback(void* const stream)
{
	return SDL_RWclose(static_cast<SDL_RWops*>(stream));
}

std::size_t CDReader::FileReadCallback(void* const buffer, const std::size_t size, const std::size_t count, void* const stream)
{
	return SDL_RWread(static_cast<SDL_RWops*>(stream), buffer, size, count);
}

std::size_t CDReader::FileWriteCallback(const void* const buffer, const std::size_t size, const std::size_t count, void* const stream)
{
	return SDL_RWwrite(static_cast<SDL_RWops*>(stream), buffer, size, count);
}

long CDReader::FileTellCallback(void* const stream)
{
	const auto position = SDL_RWtell(static_cast<SDL_RWops*>(stream));

	if (position < LONG_MIN || position > LONG_MAX)
		return -1L;

	return position;
}

int CDReader::FileSeekCallback(void* const stream, const long position, const ClownCD_FileOrigin origin)
{
	int whence;

	switch (origin)
	{
		case CLOWNCD_SEEK_SET:
			whence = RW_SEEK_SET;
			break;

		case CLOWNCD_SEEK_CUR:
			whence = RW_SEEK_CUR;
			break;

		case CLOWNCD_SEEK_END:
			whence = RW_SEEK_END;
			break;

		default:
			return -1;
	}

	return SDL_RWseek(static_cast<SDL_RWops*>(stream), position, whence) == -1 ? -1 : 0;
}

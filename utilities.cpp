#include "utilities.h"

#include "SDL.h"

#include "common.h"

bool Utilities::FileExists(const char* const filename)
{
	SDL_RWops* const file = SDL_RWFromFile(filename, "rb");

	if (file != nullptr)
	{
		SDL_RWclose(file);
		return true;
	}

	return false;
}

void Utilities::LoadFileToBuffer(const char *filename, unsigned char *&file_buffer, std::size_t &file_size)
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

		if (SDL_RWclose(file) < 0)
			debug_log.Log("SDL_RWclose failed with the following message - '%s'", SDL_GetError());
	}
}

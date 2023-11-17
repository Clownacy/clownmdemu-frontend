#ifndef UTILITIES_H
#define UTILITIES_H

#include <cstdarg>
#include <cstddef>

namespace Utilities
{
	bool FileExists(const char *filename);
	void LoadFileToBuffer(const char *filename, unsigned char *&file_buffer, std::size_t &file_size);
}

#endif /* UTILITIES_H */

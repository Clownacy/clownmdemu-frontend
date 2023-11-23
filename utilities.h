#ifndef UTILITIES_H
#define UTILITIES_H

#include <cstdarg>
#include <cstddef>

#include "debug-log.h"

class Utilities
{
private:
	DebugLog &debug_log;

public:
	Utilities(DebugLog &debug_log) : debug_log(debug_log) {}
	bool FileExists(const char *filename);
	void LoadFileToBuffer(const char *filename, unsigned char *&file_buffer, std::size_t &file_size);
};

#endif /* UTILITIES_H */

#include "error.h"


// TODO: This whole file is just a shim now: get rid of it.

void InitError()
{
	
}

void PrintErrorInternal(const char *format, std::va_list args)
{
	debug_log.Log(format, args);
}

void PrintError(const char *format, ...)
{
	std::va_list args;
	va_start(args, format);
	debug_log.Log(format, args);
	va_end(args);
}

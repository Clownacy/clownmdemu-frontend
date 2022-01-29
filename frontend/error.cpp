#include "error.h"

#include "SDL.h"

void PrintErrorInternal(const char *format, va_list args)
{
	SDL_LogMessageV(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR, format, args);
}

void PrintError(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	PrintErrorInternal(format, args);
	va_end(args);
}

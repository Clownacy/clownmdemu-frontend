#ifndef ERROR_H
#define ERROR_H

#include <cstdarg>

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#include "debug_log.h"

extern DebugLog debug_log;

void InitError(void);
void PrintErrorInternal(const char *format, std::va_list args);
CC_ATTRIBUTE_PRINTF(1, 2) void PrintError(const char *format, ...);

#endif /* ERROR_H */

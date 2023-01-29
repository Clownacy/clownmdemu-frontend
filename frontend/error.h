#ifndef ERROR_H
#define ERROR_H

#include <stdarg.h>

#include "../clowncommon/clowncommon.h"

void InitError(void);
void PrintErrorInternal(const char *format, va_list args);
CC_ATTRIBUTE_PRINTF(1, 2) void PrintError(const char *format, ...);

#endif /* ERROR_H */

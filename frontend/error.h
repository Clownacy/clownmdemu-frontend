#ifndef ERROR_H
#define ERROR_H

#include <stdarg.h>

void PrintErrorInternal(const char *format, va_list args);
void PrintError(const char *format, ...);

#endif /* ERROR_H */

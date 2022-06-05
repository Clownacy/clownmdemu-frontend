#ifndef DEBUG_MEMORY_H
#define DEBUG_MEMORY_H

#include "libraries/imgui/imgui.h"

void Debug_Memory(bool *open, ImFont *monospace_font, const char *window_name, const unsigned char *buffer, size_t buffer_length);

#endif /* DEBUG_MEMORY_H */

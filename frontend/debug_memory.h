#ifndef DEBUG_MEMORY_H
#define DEBUG_MEMORY_H

#include "libraries/imgui/imgui.h"
#include "../clowncommon.h"

void Debug_Memory(bool *open, ImFont *monospace_font, const char *window_name, const cc_u8l *buffer, size_t buffer_length);

#endif /* DEBUG_MEMORY_H */

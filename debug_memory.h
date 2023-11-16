#ifndef DEBUG_MEMORY_H
#define DEBUG_MEMORY_H

#include <cstddef>

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

void Debug_Memory(bool *open, ImFont *monospace_font, const char *window_name, const cc_u8l *buffer, std::size_t buffer_length);
void Debug_Memory(bool *open, ImFont *monospace_font, const char *window_name, const cc_u16l *buffer, std::size_t buffer_length);

#endif /* DEBUG_MEMORY_H */

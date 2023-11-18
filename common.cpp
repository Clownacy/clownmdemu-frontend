#include "common.h"

Window window(debug_log);
float dpi_scale;
unsigned int frame_counter;
DebugLog debug_log;
ImFont *monospace_font;
FilePicker file_picker(window.sdl);

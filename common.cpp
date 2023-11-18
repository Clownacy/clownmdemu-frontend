#include "common.h"

Window window(debug_log);
DebugLog debug_log;
FilePicker file_picker(window.sdl);
float dpi_scale;
unsigned int frame_counter;
ImFont *monospace_font;

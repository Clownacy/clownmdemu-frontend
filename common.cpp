#include "common.h"

EmulationState *emulation_state;
Window window(debug_log);
float dpi_scale;
unsigned int frame_counter;
DebugLog debug_log;
ClownMDEmu clownmdemu;
ImFont *monospace_font;
FilePicker file_picker(window.sdl);

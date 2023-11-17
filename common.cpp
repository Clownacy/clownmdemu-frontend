#include "common.h"

EmulationState *emulation_state;
SDL_Renderer *renderer;
SDL_Window *window;
float dpi_scale;
unsigned int frame_counter;
DebugLog debug_log;
ClownMDEmu clownmdemu;
ImFont *monospace_font;
FilePicker file_picker(window);

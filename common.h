#ifndef COMMON_H
#define COMMON_H

#include "SDL.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#include "debug_log.h"
#include "file_picker.h"
#include "window.h"

typedef struct EmulationState
{
	ClownMDEmu_State clownmdemu;
	Uint32 colours[3 * 4 * 16];
} EmulationState;

extern EmulationState *emulation_state;
extern Window window;
extern float dpi_scale;
extern unsigned int frame_counter;
extern DebugLog debug_log;
extern ClownMDEmu clownmdemu;
extern ImFont *monospace_font;
extern FilePicker file_picker;

#endif /* COMMON_H */

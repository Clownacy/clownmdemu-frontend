#ifndef COMMON_H
#define COMMON_H

#include "SDL.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#include "debug-log.h"
#include "file-picker.h"
#include "window.h"

extern Window window;
extern float dpi_scale;
extern unsigned int frame_counter;
extern DebugLog debug_log;
extern ImFont *monospace_font;
extern FilePicker file_picker;

#endif /* COMMON_H */

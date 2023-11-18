#ifndef COMMON_H
#define COMMON_H

#include "debug-log.h"
#include "file-picker.h"
#include "window.h"

extern Window window;
extern DebugLog debug_log;
extern FilePicker file_picker;
extern float dpi_scale;
extern unsigned int frame_counter;
extern ImFont *monospace_font;

#endif /* COMMON_H */

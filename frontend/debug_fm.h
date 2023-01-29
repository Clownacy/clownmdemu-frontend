#ifndef DEBUG_FM_H
#define DEBUG_FM_H

#include "libraries/imgui/imgui.h"
#include "../clowncommon/clowncommon.h"
#include "../clownmdemu.h"

void Debug_DAC_Channel(bool *open, const ClownMDEmu *clownmdemu, ImFont *monospace_font);
void Debug_FM_Channel(bool *open, const ClownMDEmu *clownmdemu, ImFont *monospace_font, cc_u16f channel_index);

#endif /* DEBUG_FM_H */

#ifndef DEBUG_FM_H
#define DEBUG_FM_H

#include "libraries/imgui/imgui.h"
#include "../clownmdemu.h"

void Debug_DAC_Channel(bool *open, const ClownMDEmu *clownmdemu, ImFont *monospace_font);
void Debug_FM_Channel(bool *open, const ClownMDEmu *clownmdemu, ImFont *monospace_font, unsigned int channel_index);

#endif /* DEBUG_FM_H */

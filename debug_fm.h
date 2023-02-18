#ifndef DEBUG_FM_H
#define DEBUG_FM_H

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

void Debug_FM(bool *open, const ClownMDEmu *clownmdemu, ImFont *monospace_font);

#endif /* DEBUG_FM_H */

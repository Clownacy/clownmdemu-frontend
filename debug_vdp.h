#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include "SDL.h"

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

struct Debug_VDP_Data
{
	const Uint32 *colours;
	SDL_Renderer *renderer;
	float dpi_scale;
};

void Debug_WindowPlane(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data);
void Debug_PlaneA(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data);
void Debug_PlaneB(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data);
void Debug_VRAM(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data);
void Debug_CRAM(bool *open, const ClownMDEmu *clownmdemu, const Debug_VDP_Data *data, ImFont *monospace_font);
void Debug_VDP(bool *open, const ClownMDEmu *clownmdemu, ImFont *monospace_font);

#endif /* DEBUG_VDP_H */

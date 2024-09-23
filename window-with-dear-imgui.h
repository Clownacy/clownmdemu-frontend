#ifndef WINDOW_WITH_DEAR_IMGUI_H
#define WINDOW_WITH_DEAR_IMGUI_H

#include "sdl-wrapper.h"

#include "libraries/imgui/imgui.h"
#include "libraries/imgui/backends/imgui_impl_sdl2.h"
#include "libraries/imgui/backends/imgui_impl_sdlrenderer2.h"

#include "raii-wrapper.h"
#include "window.h"

class WindowWithDearImGui : public Window
{
private:
	MAKE_RAII_POINTER(DearImGuiContext, ImGuiContext, ImGui::DestroyContext);

	DearImGuiContext dear_imgui_context;
	ImGuiContext *previous_dear_imgui_context;
	ImGuiStyle style_backup;
	float dpi_scale;

	float GetFontScale();
	unsigned int CalculateFontSize();
	void ReloadFonts(unsigned int font_size);

public:
	// TODO: Make this private.
	ImFont *monospace_font;

	WindowWithDearImGui(const char *window_title, int window_width, int window_height, bool resizeable);
	~WindowWithDearImGui();
	void MakeDearImGuiContextCurrent() { ImGui::SetCurrentContext(dear_imgui_context.get()); }
	void StartDearImGuiFrame();
	void FinishDearImGuiFrame();
	float GetMenuBarSize();
};

#endif /* WINDOW_WITH_DEAR_IMGUI_H */

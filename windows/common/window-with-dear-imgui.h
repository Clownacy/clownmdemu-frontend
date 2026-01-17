#ifndef WINDOW_WITH_DEAR_IMGUI_H
#define WINDOW_WITH_DEAR_IMGUI_H

#include <format>
#include <utility>

#include "../../sdl-wrapper.h"

#include "../../libraries/imgui/imgui.h"
#include "../../libraries/imgui/backends/imgui_impl_sdl3.h"
#include "../../libraries/imgui/backends/imgui_impl_sdlrenderer3.h"

#include "../../raii-wrapper.h"
#include "window.h"

namespace ImGui
{
	template <typename T>
	void TextUnformatted(const T &string)
	{
		ImGui::TextUnformatted(&string.front(), &string.back() + 1);
	}

	template <>
	inline void TextUnformatted(const std::filesystem::path &path)
	{
		const auto &string = path.u8string();
		ImGui::TextUnformatted(reinterpret_cast<const char*>(&string.front()), reinterpret_cast<const char*>(&string.back()) + 1);
	}

	// TODO: Go back to `std::format_string` when Visual Studio stops sucking balls (lacking it).
	template <typename... T>
	void TextFormatted(const std::string_view &format, T &&...args)
	{
		const auto string = std::vformat(format, std::make_format_args(args...));
		ImGui::TextUnformatted(string);
	}

	inline void PushFont(ImFont* const font)
	{
		ImGui::PushFont(font, font->LegacySize);
	}
}

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

	WindowWithDearImGui(const char *window_title, int window_width, int window_height, bool resizeable, Uint32 window_flags = 0);
	~WindowWithDearImGui();
	void MakeDearImGuiContextCurrent() { ImGui::SetCurrentContext(dear_imgui_context); }
	void StartDearImGuiFrame();
	void FinishDearImGuiFrame();
	float GetMenuBarSize();
};

#endif /* WINDOW_WITH_DEAR_IMGUI_H */

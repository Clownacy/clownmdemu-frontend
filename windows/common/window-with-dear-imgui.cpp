#include "window-with-dear-imgui.h"

#include <stdexcept>

#include "inconsolata-regular.h"
#include "noto-sans-regular.h"


///////////
// Fonts //
///////////

static constexpr float UNSCALED_FONT_SIZE = 16.0f;

float WindowWithDearImGui::GetFontScale()
{
	return CalculateFontSize() / UNSCALED_FONT_SIZE;
}

unsigned int WindowWithDearImGui::CalculateFontSize()
{
	// Note that we are purposefully flooring, as Dear ImGui's docs recommend.
	return static_cast<unsigned int>(UNSCALED_FONT_SIZE * dpi_scale);
}

void WindowWithDearImGui::ReloadFonts(const unsigned int font_size)
{
	ImGuiIO &io = ImGui::GetIO();

	io.Fonts->Clear();

	ImFontConfig font_cfg;
	*fmt::format_to_n(font_cfg.Name, std::size(font_cfg.Name) - 1, "Noto Sans Regular, {}px", font_size).out = '\0';
	io.Fonts->AddFontFromMemoryCompressedTTF(noto_sans_regular_compressed_data, noto_sans_regular_compressed_size, static_cast<float>(font_size), &font_cfg);
	*fmt::format_to_n(font_cfg.Name, std::size(font_cfg.Name) - 1, "Inconsolata Regular, {}px", font_size).out = '\0';
	monospace_font = io.Fonts->AddFontFromMemoryCompressedTTF(inconsolata_regular_compressed_data, inconsolata_regular_compressed_size, static_cast<float>(font_size), &font_cfg);
}


///////////////
// Not Fonts //
///////////////

WindowWithDearImGui::WindowWithDearImGui(const char* const window_title, const int window_width, const int window_height, const bool resizeable, const Uint32 window_flags)
	: Window(window_title, window_width, window_height, resizeable, window_flags)
	, dear_imgui_context(ImGui::CreateContext())
	, dpi_scale(GetDPIScale())
{
	const auto &previous_context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(dear_imgui_context);

	ImGuiIO &io = ImGui::GetIO();
	ImGuiStyle &style = ImGui::GetStyle();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.IniFilename = nullptr; // Disable automatic loading/saving so we can do it ourselves.

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();
	//ImGui::StyleColorsClassic();

	style.WindowBorderSize = 0.0f;
	style.PopupBorderSize = 0.0f;
	style.ChildBorderSize = 0.0f;
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.TabRounding = 0.0f;
	style.ScrollbarRounding = 0.0f;

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Text] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.13f, 0.13f, 0.94f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.35f, 0.35f, 0.35f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.69f, 0.69f, 0.69f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.69f, 0.69f, 0.69f, 0.67f);
	colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.43f, 0.50f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.63f, 0.63f, 0.63f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.41f, 0.41f, 0.41f, 0.63f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.38f, 0.38f, 0.38f, 0.39f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.38f, 0.38f, 0.38f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.43f, 0.50f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.56f, 0.56f, 0.56f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.63f, 0.63f, 0.63f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.51f, 0.51f, 0.51f, 0.43f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.51f, 0.51f, 0.51f, 0.79f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.51f, 0.51f, 0.51f, 0.95f);
	colors[ImGuiCol_Tab] = ImVec4(0.31f, 0.31f, 0.31f, 0.86f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.51f, 0.51f, 0.51f, 0.80f);
	colors[ImGuiCol_TabSelected] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);

	style_backup = style;

	// Apply DPI scale.
	style.ScaleAllSizes(dpi_scale);

	const auto &menu_bar_bg_colour = colors[ImGuiCol_MenuBarBg];
	SetTitleBarColour(menu_bar_bg_colour.x * 0xFF, menu_bar_bg_colour.y * 0xFF, menu_bar_bg_colour.z * 0xFF);

	// Setup Platform/Renderer backends
	ImGui_ImplSDL3_InitForSDLRenderer(GetSDLWindow(), GetRenderer());
	ImGui_ImplSDLRenderer3_Init(GetRenderer());

	// Load fonts
	ReloadFonts(CalculateFontSize());

	ImGui::SetCurrentContext(previous_context);
}

WindowWithDearImGui::~WindowWithDearImGui()
{
	const auto &previous_context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(dear_imgui_context);

	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();

	ImGui::SetCurrentContext(previous_context);
}

void WindowWithDearImGui::StartDearImGuiFrame()
{
	previous_dear_imgui_context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(dear_imgui_context);

	// Handle dynamic DPI support
	const float new_dpi = GetDPIScale();

	if (dpi_scale != new_dpi)
	{
		dpi_scale = new_dpi;

		auto& style = ImGui::GetStyle();
		style = style_backup;
		style.ScaleAllSizes(dpi_scale);
		ReloadFonts(CalculateFontSize());
	}

	// Start the Dear ImGui frame
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void WindowWithDearImGui::FinishDearImGuiFrame()
{
	SDL_RenderClear(GetRenderer());

	// Render Dear ImGui.
	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), GetRenderer());

	// Finally display the rendered frame to the user.
	SDL_RenderPresent(GetRenderer());

	ImGui::SetCurrentContext(previous_dear_imgui_context);
}

float WindowWithDearImGui::GetMenuBarSize()
{
	const auto &previous_context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(dear_imgui_context);

	const float menu_bar_size = UNSCALED_FONT_SIZE + ImGui::GetStyle().FramePadding.y * 2.0f; // An inlined ImGui::GetFrameHeight that actually works

	ImGui::SetCurrentContext(previous_context);

	return menu_bar_size;
}

#ifndef WINDOW_POPUP_H
#define WINDOW_POPUP_H

#include <functional>
#include <optional>
#include <string>

#include <SDL3/SDL.h>

#include "window-with-dear-imgui.h"

template<typename Derived>
class WindowPopup
{
private:
	std::optional<WindowWithDearImGui> window;
	std::string title;
	bool resizeable;
	WindowWithDearImGui *host_window;
	int dear_imgui_window_width, dear_imgui_window_height;

	bool Begin(bool* const open = nullptr, ImGuiWindowFlags window_flags = 0)
	{
		if (window.has_value())
		{
			window->StartDearImGuiFrame();

			const ImGuiViewport *viewport = ImGui::GetMainViewport();

			// Maximise the Dear ImGui window.
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);

			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
		}
		else
		{
			ImGui::SetNextWindowSize(ImVec2(dear_imgui_window_width, dear_imgui_window_height), ImGuiCond_FirstUseEver);
		}

		if (!resizeable)
			window_flags |= ImGuiWindowFlags_AlwaysAutoResize;

		//ImGui::PushID(this);
		return ImGui::Begin(title.c_str(), open, window_flags);
	}

	void End()
	{
		ImGui::End();
		//ImGui::PopID();

		if (window.has_value())
			window->FinishDearImGuiFrame();
	}

protected:
	void DoTable(const char* const name, const std::function<void()> &callback)
	{
		// Don't bother with IDs that have no visible part.
		if (name[0] != '#' || name[1] != '#')
			ImGui::SeparatorText(name);

		if (ImGui::BeginTable(name, 2, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Property");
			ImGui::TableSetupColumn("Value");
			ImGui::TableHeadersRow();

			callback();

			ImGui::EndTable();
		}
	};

	void DoProperty(ImFont* const font, const char* const label, const std::function<void()> &callback)
	{
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(label);
		ImGui::TableNextColumn();
		if (font != nullptr)
			ImGui::PushFont(font);
		callback();
		if (font != nullptr)
			ImGui::PopFont();
	};

	template<typename... T>
	void DoProperty(ImFont* const font, const char* const label, fmt::format_string<T...> format, T &&...args)
	{
		DoProperty(font, label,
			[&]()
			{
				ImGui::TextFormatted(format, std::forward<T>(args)...);
			}
		);
	};

public:
	WindowPopup(const char* const window_title, const int window_width, const int window_height, const bool resizeable, Window &parent_window, const std::optional<float> forced_scale, WindowWithDearImGui* const host_window = nullptr)
		: title(window_title)
		, resizeable(resizeable)
		, host_window(host_window)
	{
		const auto dpi_scale = host_window != nullptr ? host_window->GetDPIScale() : parent_window.GetDPIScale();
		const auto scaled_window_width = window_width * forced_scale.value_or(dpi_scale);
		const auto scaled_window_height = window_height * forced_scale.value_or(dpi_scale);

		if (host_window != nullptr)
		{
			dear_imgui_window_width = scaled_window_width;
			dear_imgui_window_height = scaled_window_height;
		}
		else
		{
			window.emplace(window_title, static_cast<int>(scaled_window_width / dpi_scale), static_cast<int>(scaled_window_height / dpi_scale), resizeable);
			SDL_SetWindowParent(window->GetSDLWindow(), parent_window.GetSDLWindow());
			SDL_ShowWindow(window->GetSDLWindow());
		}
	}

	template<typename... Ts>
	bool Display(Ts&&... arguments)
	{
		const auto derived = static_cast<Derived*>(this);

		bool alive = true;

		if (Begin(&alive, derived->window_flags))
			derived->DisplayInternal(std::forward<Ts>(arguments)...);

		End();

		return alive;
	}

	bool IsWindow(const SDL_Window* const other_window) const
	{
		if (!window.has_value())
			return false;

		return window->GetSDLWindow() == other_window;
	}

	void ProcessEvent(const SDL_Event &event)
	{
		const auto &previous_context = ImGui::GetCurrentContext();
		window->MakeDearImGuiContextCurrent();

		ImGui_ImplSDL3_ProcessEvent(&event);

		ImGui::SetCurrentContext(previous_context);
	}

	WindowWithDearImGui& GetWindow()
	{
		if (window.has_value())
			return *window;
		else
			return *host_window;
	}

	ImFont* GetMonospaceFont()
	{
		return GetWindow().monospace_font;
	}

	bool IsResizeable() const { return resizeable; }
};

#endif /* WINDOW_POPUP_H */

#ifndef DEBUG_MEMORY_H
#define DEBUG_MEMORY_H

#include <cmath>
#include <cstddef>
#include <functional>
#include <vector>

#include "../common/core/clowncommon/clowncommon.h"

#include "../frontend.h"
#include "common/window-popup.h"

class DebugMemory : public WindowPopup<DebugMemory>
{
private:
	using Base = WindowPopup<DebugMemory>;

	static constexpr Uint32 window_flags = 0;

	template<typename T>
	static constexpr std::size_t IntegerSize()
	{
		using Type = std::remove_cvref_t<T>;
		if constexpr (std::is_same_v<Type, cc_u8l>)
		{
			return 1;
		}
		else if constexpr (std::is_same_v<Type, cc_u16l>)
		{
			return 2;
		}
		else
		{
			static_assert(false, "Unsupported type passed to IntegerSize");
			return 0;
		}
	}

	template<typename T>
	void PrintLine(const T *buffer, std::size_t offset_digits, int index);

	template<typename T>
	void DisplayInternal(const T &buffer)
	{
	#if 0
		ImGui::PushFont(window.GetMonospaceFont());

		// Fit window's width to text, and disable horizontal resizing.
		const ImGuiStyle &style = ImGui::GetStyle();
		const float width = style.ScrollbarSize + style.WindowPadding.x * 2 + ImGui::CalcTextSize("0000: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F").x;
		ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0), ImVec2(width, FLT_MAX));

		ImGui::SetNextWindowSize(ImVec2(width, width), ImGuiCond_FirstUseEver);

		ImGui::PopFont();
	#endif

		const auto bytes_per_value = IntegerSize<decltype(*std::cbegin(buffer))>();

		if (ImGui::Button("Save to File"))
		{
			std::vector<unsigned char> save_buffer;
			save_buffer.reserve(std::size(buffer) * bytes_per_value);

			for (const auto &value : buffer)
			{
				auto working_value = +value; // The '+' is to promote this to 'int' if needed, so that the below shift by 8 is valid.
				for (std::size_t i = 0; i < bytes_per_value; ++i)
				{
					save_buffer.push_back(working_value >> ((bytes_per_value - 1) * 8) & 0xFF);
					working_value <<= 8;
				}
			}

			Frontend::file_utilities.SaveFile(GetWindow(), "Save RAM Dump",
			[save_buffer](const FileUtilities::SaveFileInnerCallback &callback)
			{
				return callback(std::data(save_buffer), std::size(save_buffer));
			});
		}

		if (IsResizeable())
			ImGui::BeginChild("Memory contents");

		ImGui::PushFont(GetMonospaceFont());

		const auto bytes_per_line = 0x10;
		const auto values_per_line = bytes_per_line / bytes_per_value;
		const auto total_lines = std::size(buffer) / values_per_line;
		const auto maximum_index = (total_lines - 1) * bytes_per_line;
		const auto maximum_index_bits = 1 + static_cast<std::size_t>(std::log2(maximum_index));
		const auto maximum_index_digits = CC_DIVIDE_CEILING(maximum_index_bits, 4);

		ImGuiListClipper clipper;
		clipper.Begin(total_lines);
		while (clipper.Step())
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
				PrintLine(std::data(buffer), maximum_index_digits, i);

		ImGui::PopFont();

		if (IsResizeable())
			ImGui::EndChild();
	}

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DEBUG_MEMORY_H */

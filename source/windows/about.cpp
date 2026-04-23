#include "about.h"

#include "../version.h"

void AboutWindow::DisplayInternal()
{
	const auto monospace_font = GetMonospaceFont();

	ImGui::SeparatorText("ClownMDEmu " VERSION);

	ImGui::TextUnformatted("This is a Sega Mega Drive (AKA Sega Genesis) emulator. Created by Clownacy.");
	constexpr auto DoURL = [](const char* const url)
	{
		if (ImGui::TextLink(url))
			SDL_OpenURL(url);
	};
	DoURL("https://github.com/Clownacy/clownmdemu-frontend");
	DoURL("https://github.com/Clownacy/clownmdemu-core");

	ImGui::SeparatorText("Licences");

	const auto DoLicence = [&monospace_font]<std::size_t S>(const std::array<char, S> &text)
	{
		ImGui::PushFont(monospace_font);
		ImGui::TextUnformatted(&text.front(), &text.back());
		ImGui::PopFont();
	};

	if (ImGui::CollapsingHeader("ClownMDEmu"))
	{
		static constexpr auto text = std::to_array<char>({
			#include "../../licences/clownmdemu.h"
		});

		DoLicence(text);
	}

	if (ImGui::CollapsingHeader("Dear ImGui"))
	{
		if (ImGui::TreeNode("General"))
		{
			static constexpr auto text = std::to_array<char>({
			#include "../../licences/dear-imgui.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("ImGuiTextSelect"))
		{
			static constexpr auto text = std::to_array<char>({
				#include "../../licences/dear-imgui-text-select.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}
	}

#ifdef __EMSCRIPTEN__
	if (ImGui::CollapsingHeader("Emscripten Browser File Library"))
	{
		static constexpr auto text = std::to_array<char>({
			#include "../../licences/emscripten-browser-file.h"
		});

		DoLicence(text);
	}
#endif

#ifdef IMGUI_ENABLE_FREETYPE
	if (ImGui::CollapsingHeader("FreeType"))
	{
		if (ImGui::TreeNode("General"))
		{
			static constexpr auto text = std::to_array<char>({
				#include "../../licences/freetype.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("BDF Driver"))
		{
			static constexpr auto text = std::to_array<char>({
				#include "../../licences/freetype-bdf.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("PCF Driver"))
		{
			static constexpr auto text = std::to_array<char>({
				#include "../../licences/freetype-pcf.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("fthash.c & fthash.h"))
		{
			static constexpr auto text = std::to_array<char>({
				#include "../../licences/freetype-fthash.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("ft-hb.c & ft-hb.h"))
		{
			static constexpr auto text = std::to_array<char>({
				#include "../../licences/freetype-ft-hb.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}
	}
#endif

	if (ImGui::CollapsingHeader("Fonts"))
	{
		if (ImGui::TreeNode("Noto Sans"))
		{
			static constexpr auto text = std::to_array<char>({
				#include "../../licences/noto-sans.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Inconsolata"))
		{
			static constexpr auto text = std::to_array<char>({
				#include "../../licences/inconsolata.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}
	}

	if (ImGui::CollapsingHeader("CHD Support"))
	{
		if (ImGui::TreeNode("libretro-common"))
		{
			static constexpr auto text = std::to_array<char>({
				#include "../../licences/libretro-common.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("libchdr"))
		{
			static constexpr auto text = std::to_array<char>({
				#include "../../licences/libchdr.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("miniz"))
		{
			static constexpr auto text = std::to_array<char>({
				#include "../../licences/miniz.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("zstd"))
		{
			static constexpr auto text = std::to_array<char>({
				#include "../../licences/zstd.h"
			});

			DoLicence(text);

			ImGui::TreePop();
		}
	}
}
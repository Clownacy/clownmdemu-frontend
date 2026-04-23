#include "debug-toggles.h"

#include "../frontend.h"

void DebugToggles::DisplayInternal()
{
	#define DO_BUTTON(IDENTIFIER, LABEL) \
	do { \
		ImGui::TableNextColumn(); \
		bool temp = frontend->emulator->Get##IDENTIFIER(); \
		if (ImGui::Checkbox(LABEL, &temp)) \
			frontend->emulator->Set##IDENTIFIER(temp); \
	} while (false)

	ImGui::SeparatorText("VDP");

	if (ImGui::BeginTable("VDP Options", 2, ImGuiTableFlags_SizingStretchSame))
	{
		DO_BUTTON(SpritePlaneEnabled, "Sprite Plane");
		DO_BUTTON(WindowPlaneEnabled, "Window Plane");
		DO_BUTTON(ScrollPlaneAEnabled, "Plane A");
		DO_BUTTON(ScrollPlaneBEnabled, "Plane B");
		ImGui::EndTable();
	}

	ImGui::SeparatorText("FM");

	if (ImGui::BeginTable("FM Options", 2, ImGuiTableFlags_SizingStretchSame))
	{
		DO_BUTTON(FM1Enabled, "FM1");
		DO_BUTTON(FM2Enabled, "FM2");
		DO_BUTTON(FM3Enabled, "FM3");
		DO_BUTTON(FM4Enabled, "FM4");
		DO_BUTTON(FM5Enabled, "FM5");
		DO_BUTTON(FM6Enabled, "FM6");
		DO_BUTTON(DACEnabled, "DAC");
		ImGui::EndTable();
	}

	ImGui::SeparatorText("PSG");

	if (ImGui::BeginTable("PSG Options", 2, ImGuiTableFlags_SizingStretchSame))
	{
		DO_BUTTON(PSG1Enabled, "PSG1");
		DO_BUTTON(PSG2Enabled, "PSG2");
		DO_BUTTON(PSG3Enabled, "PSG3");
		DO_BUTTON(PSGNoiseEnabled, "PSG Noise");
		ImGui::EndTable();
	}

	ImGui::SeparatorText("PCM");

	if (ImGui::BeginTable("PCM Options", 2, ImGuiTableFlags_SizingStretchSame))
	{
		DO_BUTTON(PCM1Enabled, "PCM1");
		DO_BUTTON(PCM2Enabled, "PCM2");
		DO_BUTTON(PCM3Enabled, "PCM3");
		DO_BUTTON(PCM4Enabled, "PCM4");
		DO_BUTTON(PCM5Enabled, "PCM5");
		DO_BUTTON(PCM6Enabled, "PCM6");
		DO_BUTTON(PCM7Enabled, "PCM7");
		DO_BUTTON(PCM8Enabled, "PCM8");
		ImGui::EndTable();
	}

	ImGui::SeparatorText("CDDA");

	DO_BUTTON(CDDAEnabled, "CDDA");

	#undef DO_BUTTON
}

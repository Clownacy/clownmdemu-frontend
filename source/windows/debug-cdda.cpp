#include "debug-cdda.h"

#include "../frontend.h"

void DebugCDDA::DisplayInternal()
{
	const auto &cdda = frontend->emulator->GetCDDAState();

	DoTable("Volume", [&]()
		{
			DoProperty(nullptr, "Volume", "0x{:03X}", cdda.volume);
			DoProperty(nullptr, "Master Volume", "0x{:03X}", cdda.master_volume);
		});

	DoTable("Fade", [&]()
		{
			DoProperty(nullptr, "Target Volume", "0x{:03X}", cdda.target_volume);
			DoProperty(nullptr, "Fade Step", "0x{:03X}", cdda.fade_step);
			DoProperty(nullptr, "Fade Remaining", "0x{:03X}", cdda.fade_remaining);
			DoProperty(nullptr, "Fade Direction", "{}", cdda.subtract_fade_step ? "Negative" : "Positive");
		});

	DoTable("Other", [&]()
		{
			DoProperty(nullptr, "Playing", "{}", cdda.playing ? "True" : "False");
			DoProperty(nullptr, "Paused", "{}", cdda.paused ? "True" : "False");
		});
}

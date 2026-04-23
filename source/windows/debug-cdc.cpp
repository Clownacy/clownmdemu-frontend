#include "debug-cdc.h"

#include "../frontend.h"

void DebugCDC::DisplayInternal()
{
	const auto &cdc = frontend->emulator->GetCDCState();

	DoTable("Sector Buffer", [&]()
		{
			DoProperty(nullptr, "Total Buffered Sectors", "{}", cdc.buffered_sectors_total);
			DoProperty(nullptr, "Read Index", "{}", cdc.buffered_sectors_read_index);
			DoProperty(nullptr, "Write Index", "{}", cdc.buffered_sectors_write_index);
		});

	DoTable("Host Data", [&]()
		{
			DoProperty(nullptr, "Bound", "{}", cdc.host_data_bound ? "Yes" : "No");
			DoProperty(nullptr, "Target SUB-CPU", "{}", cdc.host_data_target_sub_cpu ? "Yes" : "No");
			DoProperty(nullptr, "Buffered Sector Index", "{}", cdc.host_data_buffered_sector_index);
			DoProperty(nullptr, "Word Index", "{}", cdc.host_data_word_index);
		});

	DoTable("Other", [&]()
		{
			DoProperty(nullptr, "Current Sector", "{}", cdc.current_sector);
			DoProperty(nullptr, "Sectors Remaining", "{}", cdc.sectors_remaining);
			DoProperty(nullptr, "Device Destination", "{}", cdc.device_destination);
		});
}

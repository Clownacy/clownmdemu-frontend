#ifndef VGM_LOGGER_H
#define VGM_LOGGER_H

#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "../common/core/source/clownmdemu.h"

class VGMLogger
{
private:
	// All VGM timing is measured in samples at this rate, regardless of chip clocks.
	static constexpr cc_u32f sample_rate = 44100;

	bool recording = false;
	cc_u32f master_clock;
	cc_u32f cycles_per_frame;
	cc_u32f ym2612_clock;
	cc_u32f sn76489_clock;
	cc_u32f frame_rate;
	Uint64 frame_cycle_base; // Master-clock cycles elapsed before the current frame.
	Uint64 start_sample;     // Sample of the first chip write; the leading silence is trimmed.
	Uint64 total_samples;    // Samples emitted so far as wait commands.
	std::vector<unsigned char> commands;
	std::string software_name;

	void WriteWait(Uint64 samples);

public:
	[[nodiscard]] bool IsRecording() const { return recording; }

	void Start(bool pal_mode, std::string software_name);
	void LogWrite(ClownMDEmu_SoundChip chip, cc_u8f address, cc_u8f data, cc_u32f cycle);
	void AdvanceFrame();
	[[nodiscard]] std::vector<unsigned char> Finish();
};

#endif // VGM_LOGGER_H

#include "vgm-logger.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "text-encoding.h"

static void WriteU16LE(std::vector<unsigned char> &buffer, const cc_u16f value)
{
	buffer.push_back((value >> 0) & 0xFF);
	buffer.push_back((value >> 8) & 0xFF);
}

static void WriteU32LE(std::vector<unsigned char> &buffer, const cc_u32f value)
{
	buffer.push_back((value >> 0) & 0xFF);
	buffer.push_back((value >> 8) & 0xFF);
	buffer.push_back((value >> 16) & 0xFF);
	buffer.push_back((value >> 24) & 0xFF);
}

static void WriteU32LE(std::vector<unsigned char> &buffer, const std::size_t offset, const cc_u32f value)
{
	buffer[offset + 0] = (value >> 0) & 0xFF;
	buffer[offset + 1] = (value >> 8) & 0xFF;
	buffer[offset + 2] = (value >> 16) & 0xFF;
	buffer[offset + 3] = (value >> 24) & 0xFF;
}

// Writes 'text' converted from UTF-8 to null-terminated UTF-16LE.
static void WriteGD3String(std::vector<unsigned char> &buffer, const std::string &text)
{
	cc_u8f bytes_read;

	for (std::size_t i = 0; i < std::size(text); i += bytes_read)
	{
		cc_u32f codepoint = UTF8ToUTF32(reinterpret_cast<const unsigned char*>(&text[i]), &bytes_read);

		if (codepoint <= 0xFFFF)
		{
			WriteU16LE(buffer, codepoint);
		}
		else
		{
			// Encode as a UTF-16 surrogate pair.
			codepoint -= 0x10000;
			WriteU16LE(buffer, 0xD800 + (codepoint >> 10));
			WriteU16LE(buffer, 0xDC00 + (codepoint & 0x3FF));
		}
	}

	WriteU16LE(buffer, 0);
}

void VGMLogger::WriteWait(Uint64 samples)
{
	// The wait command's sample count is only 16-bit, so long waits must be split-up.
	while (samples != 0)
	{
		const cc_u16f chunk = std::min<Uint64>(samples, 0xFFFF);

		commands.push_back(0x61); // Wait.
		WriteU16LE(commands, chunk);

		samples -= chunk;
		total_samples += chunk;
	}
}

void VGMLogger::Start(const bool pal_mode, std::string software_name)
{
	master_clock = pal_mode ? CLOWNMDEMU_MASTER_CLOCK_PAL : CLOWNMDEMU_MASTER_CLOCK_NTSC;
	cycles_per_frame = pal_mode ? CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(CLOWNMDEMU_MASTER_CLOCK_PAL) : CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(CLOWNMDEMU_MASTER_CLOCK_NTSC);
	ym2612_clock = pal_mode ? CLOWNMDEMU_M68K_CLOCK_PAL : CLOWNMDEMU_M68K_CLOCK_NTSC;
	sn76489_clock = pal_mode ? CLOWNMDEMU_Z80_CLOCK_PAL : CLOWNMDEMU_Z80_CLOCK_NTSC;
	frame_rate = pal_mode ? 50 : 60;
	frame_cycle_base = 0;
	start_sample = 0;
	total_samples = 0;
	commands.clear();
	this->software_name = std::move(software_name);

	recording = true;
}

void VGMLogger::LogWrite(const ClownMDEmu_SoundChip chip, const cc_u8f address, const cc_u8f data, const cc_u32f cycle)
{
	if (!recording)
		return;

	// Convert the master-clock cycle at which the write occurred to a VGM sample.
	const Uint64 target_sample = (frame_cycle_base + cycle) * sample_rate / master_clock;

	// Begin the recording at the first write, trimming the leading silence.
	if (commands.empty())
		start_sample = target_sample;

	if (target_sample > start_sample + total_samples)
		WriteWait(target_sample - start_sample - total_samples);

	switch (chip)
	{
		case CLOWNMDEMU_SOUND_CHIP_FM_PORT0:
			commands.push_back(0x52); // YM2612 port 0 write.
			commands.push_back(address);
			commands.push_back(data);
			break;

		case CLOWNMDEMU_SOUND_CHIP_FM_PORT1:
			commands.push_back(0x53); // YM2612 port 1 write.
			commands.push_back(address);
			commands.push_back(data);
			break;

		case CLOWNMDEMU_SOUND_CHIP_PSG:
			commands.push_back(0x50); // SN76489 write.
			commands.push_back(data);
			break;
	}
}

void VGMLogger::AdvanceFrame()
{
	if (recording)
		frame_cycle_base += cycles_per_frame;
}

std::vector<unsigned char> VGMLogger::Finish()
{
	recording = false;

	commands.push_back(0x66); // End of sound data.

	// Produce the GD3 tag.
	std::vector<unsigned char> gd3;
	gd3.push_back('G');
	gd3.push_back('d');
	gd3.push_back('3');
	gd3.push_back(' ');
	WriteU32LE(gd3, 0x100); // Version 1.00.

	const auto gd3_length_offset = std::size(gd3);
	WriteU32LE(gd3, 0); // Placeholder for the length of the string data.

	const auto gd3_strings_offset = std::size(gd3);
	WriteGD3String(gd3, "");                          // Track name (English)
	WriteGD3String(gd3, "");                          // Track name (Japanese)
	WriteGD3String(gd3, software_name);               // Game name (English)
	WriteGD3String(gd3, "");                          // Game name (Japanese)
	WriteGD3String(gd3, "Sega Mega Drive / Genesis"); // System name (English)
	WriteGD3String(gd3, "");                          // System name (Japanese)
	WriteGD3String(gd3, "");                          // Track author (English)
	WriteGD3String(gd3, "");                          // Track author (Japanese)
	WriteGD3String(gd3, "");                          // Release date
	WriteGD3String(gd3, "ClownMDEmu");                // VGM creator
	WriteGD3String(gd3, "");                          // Notes
	WriteU32LE(gd3, gd3_length_offset, std::size(gd3) - gd3_strings_offset);

	// Lay out the file: the header, then the command stream, then the GD3 tag.
	constexpr std::size_t header_size = 0x100;
	const auto gd3_offset = header_size + std::size(commands);
	const auto total_size = gd3_offset + std::size(gd3);

	std::vector<unsigned char> file_buffer(header_size);
	file_buffer[0x00] = 'V';
	file_buffer[0x01] = 'g';
	file_buffer[0x02] = 'm';
	file_buffer[0x03] = ' ';
	WriteU32LE(file_buffer, 0x04, total_size - 0x04);  // End-of-file offset
	WriteU32LE(file_buffer, 0x08, 0x151);              // Version 1.51
	WriteU32LE(file_buffer, 0x0C, sn76489_clock);
	WriteU32LE(file_buffer, 0x14, gd3_offset - 0x14);  // GD3 tag offset
	WriteU32LE(file_buffer, 0x18, total_samples);
	WriteU32LE(file_buffer, 0x24, frame_rate);
	file_buffer[0x28] = 0x09;                          // SN76489 feedback pattern
	file_buffer[0x2A] = 16;                            // SN76489 shift register width
	WriteU32LE(file_buffer, 0x2C, ym2612_clock);
	WriteU32LE(file_buffer, 0x34, header_size - 0x34); // Command stream offset

	file_buffer.insert(file_buffer.end(), commands.begin(), commands.end());
	file_buffer.insert(file_buffer.end(), gd3.begin(), gd3.end());

	return file_buffer;
}

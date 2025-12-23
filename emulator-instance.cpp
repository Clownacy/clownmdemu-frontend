#include "emulator-instance.h"

#include <algorithm>
#include <bit>
#include <iterator>
#include <utility>

#include "frontend.h"
#include "text-encoding.h"

void EmulatorInstance::Cartridge::Insert(const std::vector<cc_u16l> &in_rom_file_buffer, const std::filesystem::path &in_save_data_path)
{
	if (IsInserted())
		Eject();

	rom_file_buffer = in_rom_file_buffer;
	save_data_path = in_save_data_path;

	// Load save data from disk.
	if (Frontend::file_utilities.FileExists(save_data_path))
	{
		const auto save_data_buffer = Frontend::file_utilities.LoadFileToBuffer<cc_u8l, 1>(save_data_path);

		if (save_data_buffer.has_value())
		{
			auto &external_ram_buffer = emulator.GetExternalRAM();
			if (std::size(*save_data_buffer) > std::size(external_ram_buffer))
			{
				Frontend::debug_log.Log("Save data file size (0x{:X} bytes) is larger than the internal save data buffer size (0x{:X} bytes)", std::size(*save_data_buffer), std::size(emulator.GetState().external_ram.buffer));
			}
			else
			{
				std::copy(std::begin(*save_data_buffer), std::end(*save_data_buffer), external_ram_buffer);
				std::fill(std::begin(external_ram_buffer) + std::size(*save_data_buffer), std::end(external_ram_buffer), 0xFF);
			}
		}
	}
}

void EmulatorInstance::Cartridge::Eject()
{
	rom_file_buffer.clear();

	// Write save data to disk.
	if (emulator.GetState().external_ram.non_volatile && emulator.GetState().external_ram.size != 0)
	{
		SDL::IOStream file = SDL::IOFromFile(save_data_path, "wb");

		if (!file || SDL_WriteIO(file, emulator.GetState().external_ram.buffer, emulator.GetState().external_ram.size) != emulator.GetState().external_ram.size)
			Frontend::debug_log.Log("Could not write save data file");
	}

	save_data_path.clear();
}

void EmulatorInstance::ColourUpdated(const cc_u16f index, const cc_u16f colour)
{
	// Decompose XBGR4444 into individual colour channels
	const cc_u32f red = (colour >> 4 * 0) & 0xF;
	const cc_u32f green = (colour >> 4 * 1) & 0xF;
	const cc_u32f blue = (colour >> 4 * 2) & 0xF;

	// Reassemble into ARGB8888
	frontend_state.colours[index] = static_cast<Uint32>(0xFF000000 | (blue << 4 * 0) | (blue << 4 * 1) | (green << 4 * 2) | (green << 4 * 3) | (red << 4 * 4) | (red << 4 * 5));
}

void EmulatorInstance::ScanlineRendered(const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f left_boundary, const cc_u16f right_boundary, const cc_u16f screen_width, const cc_u16f screen_height)
{
	current_screen_width = screen_width;
	current_screen_height = screen_height;

	if (framebuffer_texture_pixels == nullptr)
		return;

	auto input_pointer = pixels + left_boundary;
	auto output_pointer = &framebuffer_texture_pixels[scanline * framebuffer_texture_pitch + left_boundary];

	for (cc_u16f i = left_boundary; i < right_boundary; ++i)
		*output_pointer++ = frontend_state.colours[*input_pointer++];
}

cc_bool EmulatorInstance::InputRequested(const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
	return input_callback(player_id, button_id);
}

void EmulatorInstance::FMAudioToBeGenerated(const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	generate_fm_audio(clownmdemu, audio_output.MixerAllocateFMSamples(total_frames), total_frames);
}

void EmulatorInstance::PSGAudioToBeGenerated(const ClownMDEmu* const clownmdemu, const std::size_t total_samples, void (* const generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_samples))
{
	generate_psg_audio(clownmdemu, audio_output.MixerAllocatePSGSamples(total_samples), total_samples);
}

void EmulatorInstance::PCMAudioToBeGenerated(const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	generate_pcm_audio(clownmdemu, audio_output.MixerAllocatePCMSamples(total_frames), total_frames);
}

void EmulatorInstance::CDDAAudioToBeGenerated(const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	generate_cdda_audio(clownmdemu, audio_output.MixerAllocateCDDASamples(total_frames), total_frames);
}

cc_bool EmulatorInstance::SaveFileOpenedForReading(const char* const filename)
{
	save_data_stream = SDL::IOFromFile(Frontend::GetSaveDataDirectoryPath() / filename, "rb");
	return save_data_stream != nullptr;
}

cc_s16f EmulatorInstance::SaveFileRead()
{
	Uint8 byte;

	if (!SDL_ReadU8(save_data_stream, &byte))
		return -1;

	return byte;
}

cc_bool EmulatorInstance::SaveFileOpenedForWriting(const char* const filename)
{
	save_data_stream = SDL::IOFromFile(Frontend::GetSaveDataDirectoryPath() / filename, "wb");
	return save_data_stream != nullptr;
}

void EmulatorInstance::SaveFileWritten(const cc_u8f byte)
{
	SDL_WriteU8(save_data_stream, byte);
}

void EmulatorInstance::SaveFileClosed()
{
	save_data_stream.reset();
}

cc_bool EmulatorInstance::SaveFileRemoved(const char* const filename)
{
	return SDL::RemovePath(Frontend::GetSaveDataDirectoryPath() / filename);
}

cc_bool EmulatorInstance::SaveFileSizeObtained(const char* const filename, std::size_t* const size)
{
	SDL_PathInfo info;
	if (!SDL::GetPathInfo(Frontend::GetSaveDataDirectoryPath() / filename, &info))
		return cc_false;

	*size = info.size;
	return cc_true;
}

EmulatorInstance::EmulatorInstance(
	SDL::Texture &texture,
	const InputCallback &input_callback
)
	: EmulatorWithCDReader(emulator_configuration)
	, audio_output()
	, texture(texture)
	, input_callback(input_callback)
	, rewind(false)
{
	ClownCD_SetErrorCallback([]([[maybe_unused]] void* const user_data, const char* const message) { Frontend::debug_log.Log("ClownCD: {}", message); }, nullptr);

	// This should be called before any other clownmdemu functions are called!
	SetLogCallback([](const char* const format, va_list args) { Frontend::debug_log.Log(format, args); });
}

void EmulatorInstance::Update(const cc_bool fast_forward)
{
	// Lock the texture so that we can write to its pixels later
	if (!SDL_LockTexture(texture, nullptr, reinterpret_cast<void**>(&framebuffer_texture_pixels), &framebuffer_texture_pitch))
		framebuffer_texture_pixels = nullptr;

	framebuffer_texture_pitch /= sizeof(SDL::Pixel);

	// Run the emulator for a frame
	for (cc_u8f i = 0; i < (fast_forward ? 3 : 1) && !RewindingExhausted(); ++i)
	{
		if (rewind.Enabled())
		{
			// Handle rewinding.
			if (IsRewinding())
				ApplySaveState(rewind.GetBackward());
			else
				rewind.GetForward() = CreateSaveState();
		}

		// Reset the audio buffers so that they can be mixed into.
		audio_output.MixerBegin();

		Iterate();

		// Resample, mix, and output the audio for this frame.
		audio_output.MixerEnd();
	}

	// Unlock the texture so that we can draw it
	SDL_UnlockTexture(texture);
}

void EmulatorInstance::SoftResetConsole()
{
	SoftReset();
}

void EmulatorInstance::HardResetConsole()
{
	rewind.Clear();

	HardReset();
}

void EmulatorInstance::LoadCartridgeFile(std::vector<cc_u16l> &&file_buffer, const std::filesystem::path &path)
{
	cartridge.Insert(std::move(file_buffer), path);

	const auto &buffer = cartridge.GetROMBuffer();
	InsertCartridge(std::data(buffer), std::size(buffer));
	HardResetConsole();
}

void EmulatorInstance::UnloadCartridgeFile()
{
	cartridge.Eject();

	EjectCartridge();
}

bool EmulatorInstance::LoadCDFile(SDL::IOStream &&stream, const std::filesystem::path &path)
{
	if (!InsertCD(std::move(stream), path))
		return false;

	HardResetConsole();
	return true;
}

void EmulatorInstance::UnloadCDFile()
{
	EjectCD();
}

static const std::array<char, 8> save_state_magic = {"CMDEFSS"}; // Clownacy Mega Drive Emulator Frontend Save State

bool EmulatorInstance::ValidateSaveStateFile(const std::vector<unsigned char> &file_buffer)
{
	return file_buffer.size() == save_state_magic.size() + sizeof(State) && std::equal(save_state_magic.cbegin(), save_state_magic.cend(), file_buffer.cbegin());
}

bool EmulatorInstance::LoadSaveStateFile(const std::vector<unsigned char> &file_buffer)
{
	bool success = false;

	if (ValidateSaveStateFile(file_buffer))
	{
		ApplySaveState(*reinterpret_cast<const State*>(&file_buffer[save_state_magic.size()]));

		success = true;
	}

	return success;
}

std::size_t EmulatorInstance::GetSaveStateFileSize()
{
	return save_state_magic.size() + sizeof(State);
}

bool EmulatorInstance::WriteSaveStateFile(SDL::IOStream &file)
{
	bool success = false;

	const auto &save_state = CreateSaveState();

	if (SDL_WriteIO(file, &save_state_magic, sizeof(save_state_magic)) == sizeof(save_state_magic) && SDL_WriteIO(file, &save_state, sizeof(save_state)) == sizeof(save_state))
		success = true;

	return success;
}

std::string EmulatorInstance::GetSoftwareName()
{
	std::string name_buffer;

	if (IsCartridgeInserted() || IsCDInserted())
	{
		constexpr cc_u8f name_buffer_size = 0x30;
		// '*4' for the maximum UTF-8 length.
		name_buffer.reserve(name_buffer_size * 4);

		std::array<unsigned char, name_buffer_size> in_buffer;
		// Choose the proper name based on the current region.
		const auto header_offset = GetDomestic() ? 0x120 : 0x150;

		if (IsCartridgeInserted())
		{
			// TODO: This seems unsafe - add some bounds checks?
			const auto words = &cartridge.GetROMBuffer()[header_offset / 2];

			for (cc_u8f i = 0; i < name_buffer_size / 2; ++i)
			{
				const auto word = words[i];

				in_buffer[i * 2 + 0] = (word >> 8) & 0xFF;
				in_buffer[i * 2 + 1] = (word >> 0) & 0xFF;
			}
		}
		else //if (IsCDInserted)
		{
			std::array<unsigned char, CDReader::SECTOR_SIZE> sector;
			ReadMegaCDHeaderSector(sector.data());
			const auto bytes = &sector[header_offset];
			std::copy(bytes, bytes + name_buffer_size, std::begin(in_buffer));
		}

		cc_u32f previous_codepoint = '\0';

		// In Columns, both regions' names are encoded in SHIFT-JIS, so both names are decoded as SHIFT-JIS here.
		for (cc_u8f in_index = 0, in_bytes_read; in_index < name_buffer_size; in_index += in_bytes_read)
		{
			const cc_u32f codepoint = ShiftJISToUTF32(&in_buffer[in_index], &in_bytes_read);

			// Eliminate padding (the Sonic games tend to use padding to make the name look good in a hex editor).
			if (codepoint != ' ' || previous_codepoint != ' ')
			{
				const auto utf8_codepoint = UTF32ToUTF8(codepoint);

				if (utf8_codepoint.has_value())
					name_buffer += *utf8_codepoint;
			}

			previous_codepoint = codepoint;
		}

		// Eliminate trailing space.
		if (!name_buffer.empty() && name_buffer.back() == ' ')
			name_buffer.pop_back();
	}

	return name_buffer;
}

#include "emulator-instance.h"

#include <algorithm>
#include <bit>
#include <iterator>
#include <utility>

#include "frontend.h"
#include "text-encoding.h"

void EmulatorInstance::ScanlineRendered(const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f left_boundary, const cc_u16f right_boundary, const cc_u16f screen_width, const cc_u16f screen_height)
{
	current_screen_width = screen_width;
	current_screen_height = screen_height;

	if (framebuffer_texture_pixels == nullptr)
		return;

	auto input_pointer = pixels + left_boundary;
	auto output_pointer = &framebuffer_texture_pixels[scanline * framebuffer_texture_pitch + left_boundary];

	for (cc_u16f i = left_boundary; i < right_boundary; ++i)
		*output_pointer++ = GetColour(*input_pointer++);
}

cc_bool EmulatorInstance::InputRequested(const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
	return input_callback(player_id, button_id);
}

EmulatorInstance::EmulatorInstance(
	SDL::Texture &texture,
	const InputCallback &input_callback
)
	: EmulatorExtended(emulator_configuration, false, Frontend::GetSaveDataDirectoryPath())
	, texture(texture)
	, input_callback(input_callback)
{
	ClownCD_SetErrorCallback([]([[maybe_unused]] void* const user_data, const char* const message) { Frontend::debug_log.Log("ClownCD: {}", message); }, nullptr);

	// This should be called before any other clownmdemu functions are called!
	SetLogCallback([](const char* const format, va_list args) { Frontend::debug_log.Log(format, args); });
}

void EmulatorInstance::Update()
{
	// Lock the texture so that we can write to its pixels later
	if (!SDL_LockTexture(texture, nullptr, reinterpret_cast<void**>(&framebuffer_texture_pixels), &framebuffer_texture_pitch))
		framebuffer_texture_pixels = nullptr;

	framebuffer_texture_pitch /= sizeof(SDL::Pixel);

	// Run the emulator for a frame
	Iterate();

	// Unlock the texture so that we can draw it
	SDL_UnlockTexture(texture);
}

void EmulatorInstance::LoadCartridgeFile(std::vector<cc_u16l> &&file_buffer, const std::filesystem::path &path)
{
	rom_file_buffer = std::move(file_buffer);
	InsertCartridge(path, std::data(rom_file_buffer), std::size(rom_file_buffer));
	Reset();
}

void EmulatorInstance::UnloadCartridgeFile()
{
	rom_file_buffer.clear();
	rom_file_buffer.shrink_to_fit();

	EjectCartridge();
}

bool EmulatorInstance::LoadCDFile(SDL::IOStream &&stream, const std::filesystem::path &path)
{
	if (!InsertCD(std::move(stream), path))
		return false;

	Reset();
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
		LoadState(*reinterpret_cast<const State*>(&file_buffer[save_state_magic.size()]));

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

	const auto &save_state = SaveState();

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
			const auto words = &rom_file_buffer[header_offset / 2];

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

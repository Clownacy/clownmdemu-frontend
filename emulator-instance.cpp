#include "emulator-instance.h"

#include <algorithm>
#include <bit>
#include <iterator>
#include <utility>

#include "frontend.h"

void EmulatorInstance::ScanlineRendered(const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f left_boundary, const cc_u16f right_boundary, const cc_u16f screen_width, const cc_u16f screen_height)
{
	current_screen_width = screen_width;
	current_screen_height = screen_height;
	current_widescreen_tiles = GetWidescreenTiles();

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
	const InputCallback &input_callback,
	const TitleCallback &title_callback
)
	: EmulatorExtended({}, false, Frontend::GetSaveDataDirectoryPath())
	, texture(texture)
	, input_callback(input_callback)
	, title_callback(title_callback)
{}

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
}

void EmulatorInstance::UnloadCartridgeFile()
{
	rom_file_buffer.clear();
	rom_file_buffer.shrink_to_fit();

	EjectCartridge();
}

bool EmulatorInstance::LoadCDFile(SDL::IOStream &&stream, const std::filesystem::path &path)
{
	cd_stream = std::move(stream);

	return InsertCD(cd_stream, path);
}

void EmulatorInstance::UnloadCDFile()
{
	EjectCD();

	cd_stream.reset();
}

static const std::array<char, 8> save_state_magic = {"CMDEFSS"}; // Clownacy Mega Drive Emulator Frontend Save State

bool EmulatorInstance::ValidateSaveStateFile(const std::vector<unsigned char> &file_buffer) const
{
	return file_buffer.size() == save_state_magic.size() + sizeof(StateBackup) && std::equal(save_state_magic.cbegin(), save_state_magic.cend(), file_buffer.cbegin());
}

bool EmulatorInstance::LoadSaveStateFile(const std::vector<unsigned char> &file_buffer)
{
	bool success = false;

	if (ValidateSaveStateFile(file_buffer))
	{
		reinterpret_cast<const StateBackup*>(&file_buffer[save_state_magic.size()])->Apply(*this);

		success = true;
	}

	return success;
}

std::size_t EmulatorInstance::GetSaveStateFileSize() const
{
	return save_state_magic.size() + sizeof(StateBackup);
}

bool EmulatorInstance::WriteSaveStateFile(SDL::IOStream &file)
{
	bool success = false;

	// Allocate on the heap to prevent stack exhaustion.
	const auto &save_state = std::make_unique<StateBackup>(*this);

	if (SDL_WriteIO(file, &save_state_magic, sizeof(save_state_magic)) == sizeof(save_state_magic) && SDL_WriteIO(file, &*save_state, sizeof(*save_state)) == sizeof(*save_state))
		success = true;

	return success;
}

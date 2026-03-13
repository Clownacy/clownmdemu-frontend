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
	return InsertCD(std::move(stream), path);
}

void EmulatorInstance::UnloadCDFile()
{
	EjectCD();
}

using SaveStateMagic = std::array<char, 8>;
static const SaveStateMagic save_state_magic = {"CMDEFSS"}; // Clownacy Mega Drive Emulator Frontend Save State

bool EmulatorInstance::ValidateSaveStateFile(SDL::IOStream &file) const
{
	if (SDL_GetIOSize(file) != static_cast<Sint64>(GetSaveStateFileSize()))
		return false;

	const auto starting_position = SDL_TellIO(file);

	SaveStateMagic magic_buffer;
	SDL_ReadIO(file, std::data(magic_buffer), std::size(magic_buffer));

	SDL_SeekIO(file, starting_position, SDL_IO_SEEK_SET);

	return magic_buffer == save_state_magic;
}

bool EmulatorInstance::ValidateSaveStateFile(const std::filesystem::path &path) const
{
	SDL::IOStream file(path, "rb");
	return ValidateSaveStateFile(file);
}

bool EmulatorInstance::LoadSaveStateFile(SDL::IOStream &file)
{
	if (!ValidateSaveStateFile(file))
		return false;

	const auto &file_buffer = FileUtilities::LoadFileToBuffer<unsigned char, 1>(file);

	if (!file_buffer.has_value())
		return false;

	reinterpret_cast<const StateBackup*>(&(*file_buffer)[sizeof(SaveStateMagic)])->Apply(*this);
	return true;
}

std::size_t EmulatorInstance::GetSaveStateFileSize() const
{
	return sizeof(SaveStateMagic) + sizeof(StateBackup);
}

bool EmulatorInstance::WriteSaveStateFile(SDL::IOStream &file)
{
	// Allocate on the heap to prevent stack exhaustion.
	const auto &save_state = std::make_unique<StateBackup>(*this);

	return SDL_WriteIO(file, &save_state_magic, sizeof(save_state_magic)) == sizeof(save_state_magic) && SDL_WriteIO(file, &*save_state, sizeof(*save_state)) == sizeof(*save_state);
}

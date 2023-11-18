#include "emulator-instance.h"

#include "SDL.h"

#include "utilities.h"

bool EmulatorInstance::clownmdemu_initialised;
ClownMDEmu_Constant EmulatorInstance::clownmdemu_constant;

cc_u8f EmulatorInstance::CartridgeReadCallback(const cc_u32f address)
{
	if (address >= rom_buffer_size)
		return 0;

	return rom_buffer[address];
}

cc_u8f EmulatorInstance::CartridgeReadCallback(const void* const user_data, const cc_u32f address)
{
	EmulatorInstance* const emulator_instance = static_cast<EmulatorInstance*>(const_cast<void*>(user_data));

	return emulator_instance->CartridgeReadCallback(address);
}

void EmulatorInstance::CartridgeWrittenCallback(const cc_u32f /*address*/, const cc_u8f /*value*/)
{
	// For now, let's pretend that the cartridge is read-only, like retail cartridges are.

	/*
	if (address >= rom_buffer_size)
		return;

	rom_buffer[address] = value;
	*/
}

void EmulatorInstance::CartridgeWrittenCallback(const void* const user_data, const cc_u32f address, const cc_u8f value)
{
	EmulatorInstance* const emulator_instance = static_cast<EmulatorInstance*>(const_cast<void*>(user_data));

	emulator_instance->CartridgeWrittenCallback(address, value);
}

void EmulatorInstance::ColourUpdatedCallback(const cc_u16f index, const cc_u16f colour)
{
	// Decompose XBGR4444 into individual colour channels
	const cc_u32f red = (colour >> 4 * 0) & 0xF;
	const cc_u32f green = (colour >> 4 * 1) & 0xF;
	const cc_u32f blue = (colour >> 4 * 2) & 0xF;

	// Reassemble into ARGB8888
	state->colours[index] = static_cast<Uint32>((blue << 4 * 0) | (blue << 4 * 1) | (green << 4 * 2) | (green << 4 * 3) | (red << 4 * 4) | (red << 4 * 5));
}

void EmulatorInstance::ColourUpdatedCallback(const void* const user_data, const cc_u16f index, const cc_u16f colour)
{
	EmulatorInstance* const emulator_instance = static_cast<EmulatorInstance*>(const_cast<void*>(user_data));

	emulator_instance->ColourUpdatedCallback(index, colour);
}

void EmulatorInstance::ScanlineRenderedCallback(const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f screen_width, const cc_u16f screen_height)
{
	current_screen_width = screen_width;
	current_screen_height = screen_height;

	if (framebuffer_texture_pixels != nullptr)
		for (cc_u16f i = 0; i < screen_width; ++i)
			framebuffer_texture_pixels[scanline * framebuffer_texture_pitch + i] = state->colours[pixels[i]];
}

void EmulatorInstance::ScanlineRenderedCallback(const void* const user_data, const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f screen_width, const cc_u16f screen_height)
{
	EmulatorInstance* const emulator_instance = static_cast<EmulatorInstance*>(const_cast<void*>(user_data));

	emulator_instance->ScanlineRenderedCallback(scanline, pixels, screen_width, screen_height);
}

cc_bool EmulatorInstance::ReadInputCallback(const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
	// TODO: Merge me with the below function and make it a static method.
	return input_callback(player_id, button_id);
}

cc_bool EmulatorInstance::ReadInputCallback(const void* const user_data, const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
	EmulatorInstance* const emulator_instance = static_cast<EmulatorInstance*>(const_cast<void*>(user_data));

	return emulator_instance->ReadInputCallback(player_id, button_id);
}

void EmulatorInstance::FMAudioCallback(const std::size_t total_frames, void (* const generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	generate_fm_audio(&clownmdemu, audio_output.MixerAllocateFMSamples(total_frames), total_frames);
}

void EmulatorInstance::FMAudioCallback(const void* const user_data, const std::size_t total_frames, void (* const generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	EmulatorInstance* const emulator_instance = static_cast<EmulatorInstance*>(const_cast<void*>(user_data));

	emulator_instance->FMAudioCallback(total_frames, generate_fm_audio);
}

void EmulatorInstance::PSGAudioCallback(const std::size_t total_samples, void (* const generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_samples))
{
	generate_psg_audio(&clownmdemu, audio_output.MixerAllocatePSGSamples(total_samples), total_samples);
}

void EmulatorInstance::PSGAudioCallback(const void* const user_data, const std::size_t total_samples, void (* const generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_samples))
{
	EmulatorInstance* const emulator_instance = static_cast<EmulatorInstance*>(const_cast<void*>(user_data));

	emulator_instance->PSGAudioCallback(total_samples, generate_psg_audio);
}

void EmulatorInstance::CDSeekCallback(const cc_u32f sector_index)
{
	if (cd_file != nullptr)
		SDL_RWseek(cd_file, sector_size_2352 ? sector_index * 2352 + 0x10 : sector_index * 2048, RW_SEEK_SET);
}

void EmulatorInstance::CDSeekCallback(const void* const user_data, const cc_u32f sector_index)
{
	EmulatorInstance* const emulator_instance = static_cast<EmulatorInstance*>(const_cast<void*>(user_data));

	emulator_instance->CDSeekCallback(sector_index);
}

const cc_u8l* EmulatorInstance::CDSectorReadCallback()
{
	static cc_u8l sector_buffer[2048];

	if (cd_file != nullptr)
		SDL_RWread(cd_file, sector_buffer, 2048, 1);

	if (sector_size_2352)
		SDL_RWseek(cd_file, 2352 - 2048, SEEK_CUR);

	return sector_buffer;
}

const cc_u8l* EmulatorInstance::CDSectorReadCallback(const void* const user_data)
{
	EmulatorInstance* const emulator_instance = static_cast<EmulatorInstance*>(const_cast<void*>(user_data));

	return emulator_instance->CDSectorReadCallback();
}

EmulatorInstance::EmulatorInstance(
	AudioOutput &audio_output,
	DebugLog &debug_log,
	const std::function<bool(cc_u8f player_id, ClownMDEmu_Button button_id)> input_callback
) :
	audio_output(audio_output),
	debug_log(debug_log),
	input_callback(input_callback),
	callbacks({this, CartridgeReadCallback, CartridgeWrittenCallback, ColourUpdatedCallback, ScanlineRenderedCallback, ReadInputCallback, FMAudioCallback, PSGAudioCallback, CDSeekCallback, CDSectorReadCallback})
{
	// This should be called before any other clownmdemu functions are called!
	// TODO: ClownMDEmu_SetErrorCallback([](const char* const format, va_list args) {debug_log.Log(format, args); });

	// Initialise the clownmdemu configuration struct.
	clownmdemu_configuration.vdp.sprites_disabled = cc_false;
	clownmdemu_configuration.vdp.window_disabled = cc_false;
	clownmdemu_configuration.vdp.planes_disabled[0] = cc_false;
	clownmdemu_configuration.vdp.planes_disabled[1] = cc_false;
	clownmdemu_configuration.fm.fm_channels_disabled[0] = cc_false;
	clownmdemu_configuration.fm.fm_channels_disabled[1] = cc_false;
	clownmdemu_configuration.fm.fm_channels_disabled[2] = cc_false;
	clownmdemu_configuration.fm.fm_channels_disabled[3] = cc_false;
	clownmdemu_configuration.fm.fm_channels_disabled[4] = cc_false;
	clownmdemu_configuration.fm.fm_channels_disabled[5] = cc_false;
	clownmdemu_configuration.fm.dac_channel_disabled = cc_false;
	clownmdemu_configuration.psg.tone_disabled[0] = cc_false;
	clownmdemu_configuration.psg.tone_disabled[1] = cc_false;
	clownmdemu_configuration.psg.tone_disabled[2] = cc_false;
	clownmdemu_configuration.psg.noise_disabled = cc_false;

	// Initialise persistent data such as lookup tables.
	if (!clownmdemu_initialised)
	{
		clownmdemu_initialised = true;
		ClownMDEmu_Constant_Initialise(&clownmdemu_constant);
	}
}

void EmulatorInstance::Update(Window &window)
{
	// Reset the audio buffers so that they can be mixed into.
	audio_output.MixerBegin();

	// Lock the texture so that we can write to its pixels later
	if (SDL_LockTexture(window.framebuffer_texture, nullptr, reinterpret_cast<void**>(&framebuffer_texture_pixels), &framebuffer_texture_pitch) < 0)
		framebuffer_texture_pixels = nullptr;

	framebuffer_texture_pitch /= sizeof(Uint32);

	// Run the emulator for a frame
	ClownMDEmu_Iterate(&clownmdemu, &callbacks);

	// Unlock the texture so that we can draw it
	SDL_UnlockTexture(window.framebuffer_texture);

	// Resample, mix, and output the audio for this frame.
	audio_output.MixerEnd();

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	// Handle rewinding.

	// We maintain a ring buffer of emulator states:
	// when rewinding, we go backwards through this buffer,
	// and when not rewinding, we go forwards through it.
	std::size_t from_index, to_index;

	if (rewind_in_progress)
	{
		--state_rewind_remaining;

		from_index = to_index = state_rewind_index;

		if (from_index == 0)
			from_index = CC_COUNT_OF(state_rewind_buffer) - 1;
		else
			--from_index;

		state_rewind_index = from_index;
	}
	else
	{
		if (state_rewind_remaining < CC_COUNT_OF(state_rewind_buffer) - 1)
			++state_rewind_remaining;

		from_index = to_index = state_rewind_index;

		if (to_index == CC_COUNT_OF(state_rewind_buffer) - 1)
			to_index = 0;
		else
			++to_index;

		state_rewind_index = to_index;
	}

	SDL_memcpy(&state_rewind_buffer[to_index], &state_rewind_buffer[from_index], sizeof(state_rewind_buffer[0]));

	state = &state_rewind_buffer[to_index];
	ClownMDEmu_Parameters_Initialise(&clownmdemu, &clownmdemu_configuration, &clownmdemu_constant, &state->clownmdemu);
#endif
}

void EmulatorInstance::SoftResetConsole()
{
	ClownMDEmu_Reset(&clownmdemu, &callbacks, !IsCartridgeFileLoaded());
}

void EmulatorInstance::HardResetConsole()
{
	ClownMDEmu_State_Initialise(&state->clownmdemu);
	ClownMDEmu_Parameters_Initialise(&clownmdemu, &clownmdemu_configuration, &clownmdemu_constant, &state->clownmdemu);
	SoftResetConsole();
}

void EmulatorInstance::LoadCartridgeFileFromMemory(unsigned char *rom_buffer_parameter, std::size_t rom_buffer_size_parameter)
{
	// Unload the previous ROM in memory.
	SDL_free(rom_buffer);

	rom_buffer = rom_buffer_parameter;
	rom_buffer_size = rom_buffer_size_parameter;

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	state_rewind_remaining = 0;
	state_rewind_index = 0;
#endif
	state = &state_rewind_buffer[0];

	HardResetConsole();
}

bool EmulatorInstance::LoadCartridgeFileFromFile(const char *path)
{
	unsigned char *temp_rom_buffer;
	std::size_t temp_rom_buffer_size;

	// Load ROM to memory.
	Utilities::LoadFileToBuffer(path, temp_rom_buffer, temp_rom_buffer_size);

	if (temp_rom_buffer == nullptr)
		return false;

	LoadCartridgeFileFromMemory(temp_rom_buffer, temp_rom_buffer_size);
	return true;
}

void EmulatorInstance::UnloadCartridgeFile()
{
	SDL_free(rom_buffer);
	rom_buffer = nullptr;
	rom_buffer_size = 0;
}

bool EmulatorInstance::LoadCDFile(const char* const path)
{
	cd_file = SDL_RWFromFile(path, "rb");

	if (cd_file == nullptr)
		return false;

	// Detect if the sector size is 2048 bytes or 2352 bytes.
	sector_size_2352 = false;

	unsigned char buffer[0x10];
	if (SDL_RWread(cd_file, buffer, sizeof(buffer), 1) == 1)
	{
		static const unsigned char sector_header[0x10] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x02, 0x00, 0x01};

		if (SDL_memcmp(buffer, sector_header, sizeof(sector_header)) == 0)
			sector_size_2352 = true;
	}

	HardResetConsole();

	return true;
}

void EmulatorInstance::UnloadCDFile()
{
	SDL_RWclose(cd_file);
	cd_file = nullptr;
}

static const char save_state_magic[8] = "CMDEFSS"; // Clownacy Mega Drive Emulator Frontend Save State

bool EmulatorInstance::ValidateSaveStateFromMemory(const unsigned char* const file_buffer, const std::size_t file_size)
{
	return file_size == sizeof(save_state_magic) + sizeof(State) && SDL_memcmp(file_buffer, save_state_magic, sizeof(save_state_magic)) == 0;
}

bool EmulatorInstance::ValidateSaveStateFromFile(const char* const save_state_path)
{
	unsigned char *file_buffer;
	std::size_t file_size;
	Utilities::LoadFileToBuffer(save_state_path, file_buffer, file_size);

	const bool valid = ValidateSaveStateFromMemory(file_buffer, file_size);

	SDL_free(file_buffer);
	return valid;
}

bool EmulatorInstance::LoadSaveStateFromMemory(const unsigned char* const file_buffer, const std::size_t file_size)
{
	bool success = false;

	if (file_buffer != nullptr) // TODO: This can't be necessary, right?
	{
		if (ValidateSaveStateFromMemory(file_buffer, file_size))
		{
			SDL_memcpy(state, &file_buffer[sizeof(save_state_magic)], sizeof(State));
			success = true;
		}
	}

	return success;
}

bool EmulatorInstance::LoadSaveStateFromFile(const char* const save_state_path)
{
	unsigned char *file_buffer;
	std::size_t file_size;
	Utilities::LoadFileToBuffer(save_state_path, file_buffer, file_size);

	bool success = false;

	if (file_buffer != nullptr)
	{
		success = LoadSaveStateFromMemory(file_buffer, file_size);
		SDL_free(file_buffer);
	}

	return success;
}

bool EmulatorInstance::CreateSaveState(const char* const save_state_path)
{
	bool success = false;

	// Save the current state to the specified file.
	SDL_RWops *file = SDL_RWFromFile(save_state_path, "wb");

	if (file != nullptr)
	{
		if (SDL_RWwrite(file, save_state_magic, sizeof(save_state_magic), 1) != 1 || SDL_RWwrite(file, state, sizeof(*state), 1) == 1)
			success = true;

		SDL_RWclose(file);
	}

	return success;
}

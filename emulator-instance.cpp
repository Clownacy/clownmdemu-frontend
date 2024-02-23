#include "emulator-instance.h"

#include <utility>

bool EmulatorInstance::clownmdemu_initialised;
ClownMDEmu_Constant EmulatorInstance::clownmdemu_constant;

cc_u8f EmulatorInstance::CartridgeReadCallback(void* const user_data, const cc_u32f address)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	if (address >= emulator->rom_buffer.size())
		return 0;

	return emulator->rom_buffer[address];
}

void EmulatorInstance::CartridgeWrittenCallback(void* const user_data, const cc_u32f address, const cc_u8f value)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	// For now, let's pretend that the cartridge is read-only, like retail cartridges are.
	static_cast<void>(emulator);
	static_cast<void>(address);
	static_cast<void>(value);

	/*
	if (address >= emulator->rom_buffer_size)
		return;

	emulator->rom_buffer[address] = value;
	*/
}

void EmulatorInstance::ColourUpdatedCallback(void* const user_data, const cc_u16f index, const cc_u16f colour)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	// Decompose XBGR4444 into individual colour channels
	const cc_u32f red = (colour >> 4 * 0) & 0xF;
	const cc_u32f green = (colour >> 4 * 1) & 0xF;
	const cc_u32f blue = (colour >> 4 * 2) & 0xF;

	// Reassemble into ARGB8888
	emulator->state->colours[index] = static_cast<Uint32>(0xFF000000 | (blue << 4 * 0) | (blue << 4 * 1) | (green << 4 * 2) | (green << 4 * 3) | (red << 4 * 4) | (red << 4 * 5));
}

void EmulatorInstance::ScanlineRenderedCallback(void* const user_data, const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f screen_width, const cc_u16f screen_height)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	emulator->current_screen_width = screen_width;
	emulator->current_screen_height = screen_height;

	if (emulator->framebuffer_texture_pixels != nullptr)
		for (cc_u16f i = 0; i < screen_width; ++i)
			emulator->framebuffer_texture_pixels[scanline * emulator->framebuffer_texture_pitch + i] = emulator->state->colours[pixels[i]];
}

cc_bool EmulatorInstance::ReadInputCallback(void* const user_data, const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	return emulator->input_callback(player_id, button_id);
}

void EmulatorInstance::FMAudioCallback(void* const user_data, const std::size_t total_frames, void (* const generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	generate_fm_audio(&emulator->clownmdemu, emulator->audio_output.MixerAllocateFMSamples(total_frames), total_frames);
}

void EmulatorInstance::PSGAudioCallback(void* const user_data, const std::size_t total_samples, void (* const generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_samples))
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	generate_psg_audio(&emulator->clownmdemu, emulator->audio_output.MixerAllocatePSGSamples(total_samples), total_samples);
}

void EmulatorInstance::CDSeekCallback(void* const user_data, const cc_u32f sector_index)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	if (emulator->cd_file != nullptr)
		SDL_RWseek(emulator->cd_file.get(), emulator->sector_size_2352 ? sector_index * 2352 + 0x10 : sector_index * 2048, RW_SEEK_SET);
}

const cc_u8l* EmulatorInstance::CDSectorReadCallback(void* const user_data)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	static std::array<cc_u8l, 2048> sector_buffer;

	if (emulator->cd_file != nullptr)
	{
		SDL_RWread(emulator->cd_file.get(), &sector_buffer, 2048, 1);

		if (emulator->sector_size_2352)
			SDL_RWseek(emulator->cd_file.get(), 2352 - 2048, SEEK_CUR);
	}

	return &sector_buffer[0];
}

EmulatorInstance::EmulatorInstance(
	DebugLog &debug_log,
	Window &window,
	const InputCallback &input_callback
)
	: audio_output()
	, window(window)
	, input_callback(input_callback)
	, callbacks({this, CartridgeReadCallback, CartridgeWrittenCallback, ColourUpdatedCallback, ScanlineRenderedCallback, ReadInputCallback, FMAudioCallback, PSGAudioCallback, CDSeekCallback, CDSectorReadCallback})
{
	// This should be called before any other clownmdemu functions are called!
	ClownMDEmu_SetErrorCallback([](void* const user_data, const char* const format, va_list args) { static_cast<DebugLog*>(user_data)->Log(format, args); }, &debug_log);

	// Initialise persistent data such as lookup tables.
	if (!clownmdemu_initialised)
	{
		clownmdemu_initialised = true;
		ClownMDEmu_Constant_Initialise(&clownmdemu_constant);
	}

	HardResetConsole();
}

void EmulatorInstance::Update()
{
	// Reset the audio buffers so that they can be mixed into.
	audio_output.MixerBegin();

	// Lock the texture so that we can write to its pixels later
	if (SDL_LockTexture(window.GetFramebufferTexture(), nullptr, reinterpret_cast<void**>(&framebuffer_texture_pixels), &framebuffer_texture_pitch) < 0)
		framebuffer_texture_pixels = nullptr;

	framebuffer_texture_pitch /= sizeof(Uint32);

	// Run the emulator for a frame
	ClownMDEmu_Iterate(&clownmdemu, &callbacks);

	// Unlock the texture so that we can draw it
	SDL_UnlockTexture(window.GetFramebufferTexture());

	// Resample, mix, and output the audio for this frame.
	audio_output.MixerEnd();

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	// Handle rewinding.

	// We maintain a ring buffer of emulator states:
	// when rewinding, we go backwards through this buffer,
	// and when not rewinding, we go forwards through it.
	std::size_t from_index, to_index;

	if (IsRewinding())
	{
		--state_rewind_remaining;

		from_index = to_index = state_rewind_index;

		if (from_index == 0)
			from_index = state_rewind_buffer.size() - 1;
		else
			--from_index;

		state_rewind_index = from_index;
	}
	else
	{
		if (state_rewind_remaining < state_rewind_buffer.size() - 1)
			++state_rewind_remaining;

		from_index = to_index = state_rewind_index;

		if (to_index == state_rewind_buffer.size() - 1)
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

void EmulatorInstance::LoadCartridgeFile(const std::vector<unsigned char> &file_buffer)
{
	rom_buffer = std::move(file_buffer);

#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	state_rewind_remaining = 0;
	state_rewind_index = 0;
#endif
	state = &state_rewind_buffer[0];

	HardResetConsole();
}

void EmulatorInstance::UnloadCartridgeFile()
{
	rom_buffer.clear();
}

void EmulatorInstance::LoadCDFile(SDL::RWops &file)
{
	cd_file = std::move(file);

	// Detect if the sector size is 2048 bytes or 2352 bytes.
	sector_size_2352 = false;

	std::array<unsigned char, 0x10> buffer;
	if (SDL_RWread(cd_file.get(), &buffer, sizeof(buffer), 1) == 1)
	{
		static const std::array<unsigned char, 0x10> sector_header = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x02, 0x00, 0x01};

		if (SDL_memcmp(&buffer, &sector_header, sizeof(sector_header)) == 0)
			sector_size_2352 = true;
	}

	HardResetConsole();
}

void EmulatorInstance::UnloadCDFile()
{
	cd_file = nullptr;
}

static const std::array<char, 8> save_state_magic = {"CMDEFSS"}; // Clownacy Mega Drive Emulator Frontend Save State

bool EmulatorInstance::ValidateSaveState(const std::vector<unsigned char> &file_buffer)
{
	return file_buffer.size() == sizeof(save_state_magic) + sizeof(State) && SDL_memcmp(file_buffer.data(), &save_state_magic, sizeof(save_state_magic)) == 0;
}

bool EmulatorInstance::LoadSaveState(const std::vector<unsigned char> &file_buffer)
{
	bool success = false;

	if (ValidateSaveState(file_buffer))
	{
		SDL_memcpy(state, &file_buffer[sizeof(save_state_magic)], sizeof(State));
		success = true;
	}

	return success;
}

std::size_t EmulatorInstance::GetSaveStateSize()
{
	return sizeof(save_state_magic) + sizeof(*state);
}

bool EmulatorInstance::CreateSaveState(const SDL::RWops &file)
{
	bool success = false;

	if (SDL_RWwrite(file.get(), &save_state_magic, sizeof(save_state_magic), 1) != 1 || SDL_RWwrite(file.get(), state, sizeof(*state), 1) == 1)
		success = true;

	return success;
}

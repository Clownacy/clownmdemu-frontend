#include "emulator-instance.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "text-encoding.h"

bool EmulatorInstance::clownmdemu_initialised;
ClownMDEmu_Constant EmulatorInstance::clownmdemu_constant;

cc_u8f EmulatorInstance::CartridgeReadCallback(void* const user_data, const cc_u32f address)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	if (address >= emulator->rom_buffer.size())
		return 0;

	return emulator->rom_buffer[address];
}

void EmulatorInstance::CartridgeWrittenCallback([[maybe_unused]] void* const user_data, [[maybe_unused]] const cc_u32f address, [[maybe_unused]] const cc_u8f value)
{
	//EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	// For now, let's pretend that the cartridge is read-only, like retail cartridges are.

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
	emulator->state->Colours(index) = static_cast<Uint32>(0xFF000000 | (blue << 4 * 0) | (blue << 4 * 1) | (green << 4 * 2) | (green << 4 * 3) | (red << 4 * 4) | (red << 4 * 5));
}

void EmulatorInstance::ScanlineRenderedCallback(void* const user_data, const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f screen_width, const cc_u16f screen_height)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	emulator->current_screen_width = screen_width;
	emulator->current_screen_height = screen_height;

	if (emulator->framebuffer_texture_pixels != nullptr)
		for (cc_u16f i = 0; i < screen_width; ++i)
			emulator->framebuffer_texture_pixels[scanline * emulator->framebuffer_texture_pitch + i] = emulator->state->Colours(pixels[i]);
}

cc_bool EmulatorInstance::ReadInputCallback(void* const user_data, const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	return emulator->input_callback(player_id, button_id);
}

void EmulatorInstance::FMAudioCallback(void* const user_data, const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	generate_fm_audio(clownmdemu, emulator->audio_output.MixerAllocateFMSamples(total_frames), total_frames);
}

void EmulatorInstance::PSGAudioCallback(void* const user_data, const ClownMDEmu* const clownmdemu, const std::size_t total_samples, void (* const generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_samples))
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	generate_psg_audio(clownmdemu, emulator->audio_output.MixerAllocatePSGSamples(total_samples), total_samples);
}

void EmulatorInstance::PCMAudioCallback(void* const user_data, const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	generate_pcm_audio(clownmdemu, emulator->audio_output.MixerAllocatePCMSamples(total_frames), total_frames);
}

void EmulatorInstance::CDDAAudioCallback(void* const user_data, const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	generate_cdda_audio(clownmdemu, emulator->audio_output.MixerAllocateCDDASamples(total_frames), total_frames);
}

void EmulatorInstance::CDSeekCallback(void* const user_data, const cc_u32f sector_index)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	emulator->cd_file.SeekToSector(sector_index);
}

const cc_u8l* EmulatorInstance::CDSectorReadCallback(void* const user_data)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	emulator->sector = emulator->cd_file.ReadSector();

	return emulator->sector.data();
}

cc_bool EmulatorInstance::CDSeekTrackCallback(void* const user_data, const cc_u16f track_index, ClownMDEmu_CDDAMode mode)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	CDReader::PlaybackSetting playback_setting;

	switch (mode)
	{
		default:
			SDL_assert(false);
			return cc_false;

		case CLOWNMDEMU_CDDA_PLAY_ALL:
			playback_setting = CDReader::PlaybackSetting::ALL;
			break;

		case CLOWNMDEMU_CDDA_PLAY_ONCE:
			playback_setting = CDReader::PlaybackSetting::ONCE;
			break;

		case CLOWNMDEMU_CDDA_PLAY_REPEAT:
			playback_setting = CDReader::PlaybackSetting::REPEAT;
			break;
	}

	return emulator->cd_file.PlayAudio(track_index, playback_setting);
}

std::size_t EmulatorInstance::CDAudioReadCallback(void* const user_data, cc_s16l* const sample_buffer, const std::size_t total_frames)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	return emulator->cd_file.ReadAudio(sample_buffer, total_frames);
}

EmulatorInstance::EmulatorInstance(
	DebugLog &debug_log,
	Window &window,
	const InputCallback &input_callback
)
	: audio_output(debug_log)
	, window(window)
	, input_callback(input_callback)
	, callbacks({this, CartridgeReadCallback, CartridgeWrittenCallback, ColourUpdatedCallback, ScanlineRenderedCallback, ReadInputCallback, FMAudioCallback, PSGAudioCallback, PCMAudioCallback, CDDAAudioCallback, CDSeekCallback, CDSectorReadCallback, CDSeekTrackCallback, CDAudioReadCallback})
{
	// This should be called before any other clownmdemu functions are called!
	ClownMDEmu_SetLogCallback([](void* const user_data, const char* const format, va_list args) { static_cast<DebugLog*>(user_data)->Log(format, args); }, &debug_log);

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
#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	// Handle rewinding.

	// We maintain a ring buffer of emulator states:
	// when rewinding, we go backwards through this buffer,
	// and when not rewinding, we go forwards through it.
	const auto rewind_buffer_size = state_rewind_buffer.size();

	if (IsRewinding())
	{
		--state_rewind_remaining;

		if (state_rewind_index == 0)
			state_rewind_index = rewind_buffer_size - 1;
		else
			--state_rewind_index;
	}
	else
	{
		if (state_rewind_remaining < rewind_buffer_size - 2) // -2 because we use a slot to hold the current state, so -1 does not suffice.
			++state_rewind_remaining;

		if (state_rewind_index == rewind_buffer_size - 1)
			state_rewind_index = 0;
		else
			++state_rewind_index;
	}

	auto &previous_state = state_rewind_buffer[state_rewind_index];
	auto &next_state = state_rewind_buffer[(state_rewind_index + 1) % rewind_buffer_size];

	if (IsRewinding())
	{
		state = &next_state;
		LoadState(&previous_state);
	}
	else
	{
		SaveState(&next_state);
		state = &next_state;
	}

	ClownMDEmu_Parameters_Initialise(&clownmdemu, &clownmdemu_configuration, &clownmdemu_constant, &state->clownmdemu, &callbacks);
#endif

	// Reset the audio buffers so that they can be mixed into.
	audio_output.MixerBegin();

	// Lock the texture so that we can write to its pixels later
	if (SDL_LockTexture(window.GetFramebufferTexture(), nullptr, reinterpret_cast<void**>(&framebuffer_texture_pixels), &framebuffer_texture_pitch) < 0)
		framebuffer_texture_pixels = nullptr;

	framebuffer_texture_pitch /= sizeof(Uint32);

	// Run the emulator for a frame
	ClownMDEmu_Iterate(&clownmdemu);

	// Unlock the texture so that we can draw it
	SDL_UnlockTexture(window.GetFramebufferTexture());

	// Resample, mix, and output the audio for this frame.
	audio_output.MixerEnd();

	// Log CD state for this frame.
	state->cd = cd_file.GetState();
}

void EmulatorInstance::SoftResetConsole()
{
	ClownMDEmu_Reset(&clownmdemu, !IsCartridgeFileLoaded());
}

void EmulatorInstance::HardResetConsole()
{
#ifdef CLOWNMDEMU_FRONTEND_REWINDING
	state_rewind_remaining = 0;
	state_rewind_index = 0;
#endif
	state = &state_rewind_buffer[0];
	// A real console does not retain its RAM contents between games, as RAM
	// is cleared when the console is powered-off.
	// Failing to clear RAM causes issues with Sonic games and ROM-hacks,
	// which skip initialisation when a certain magic number is found in RAM.
	auto &m68k_ram = state_rewind_buffer[0].clownmdemu.m68k.ram;
	std::fill(std::begin(m68k_ram), std::end(m68k_ram), 0);

	ClownMDEmu_State_Initialise(&state->clownmdemu);
	ClownMDEmu_Parameters_Initialise(&clownmdemu, &clownmdemu_configuration, &clownmdemu_constant, &state->clownmdemu, &callbacks);
	SoftResetConsole();
}

void EmulatorInstance::LoadCartridgeFile(const std::vector<unsigned char> &&file_buffer)
{
	rom_buffer = std::move(file_buffer);

	HardResetConsole();
}

void EmulatorInstance::UnloadCartridgeFile()
{
	rom_buffer.clear();
}

bool EmulatorInstance::LoadCDFile(SDL::RWops &&stream, const std::filesystem::path &path)
{
	cd_file.Open(std::move(stream), path);
	if (!cd_file.SeekToSector(0))
	{
		cd_file.Close();
		return false;
	}

	HardResetConsole();
	return true;
}

void EmulatorInstance::UnloadCDFile()
{
	cd_file.Close();
}

void EmulatorInstance::LoadState(const void* const buffer)
{
	SDL_memcpy(state, buffer, sizeof(*state));
	cd_file.SetState(state->cd);
}

void EmulatorInstance::SaveState(void* const buffer)
{
	SDL_memcpy(buffer, state, sizeof(*state));
}

static const std::array<char, 8> save_state_magic = {"CMDEFSS"}; // Clownacy Mega Drive Emulator Frontend Save State

bool EmulatorInstance::ValidateSaveStateFile(const std::vector<unsigned char> &file_buffer)
{
	return file_buffer.size() == save_state_magic.size() + sizeof(*state) && std::equal(save_state_magic.cbegin(), save_state_magic.cend(), file_buffer.cbegin());
}

bool EmulatorInstance::LoadSaveStateFile(const std::vector<unsigned char> &file_buffer)
{
	bool success = false;

	if (ValidateSaveStateFile(file_buffer))
	{
		LoadState(&file_buffer[save_state_magic.size()]);

		success = true;
	}

	return success;
}

std::size_t EmulatorInstance::GetSaveStateFileSize()
{
	return save_state_magic.size() + sizeof(*state);
}

bool EmulatorInstance::WriteSaveStateFile(const SDL::RWops &file)
{
	bool success = false;

	if (SDL_RWwrite(file.get(), &save_state_magic, sizeof(save_state_magic), 1) == 1 && SDL_RWwrite(file.get(), state, sizeof(*state), 1) == 1)
		success = true;

	return success;
}

std::string EmulatorInstance::GetSoftwareName()
{
	std::string name_buffer;

	if (IsCartridgeFileLoaded() || cd_file.IsOpen())
	{
		constexpr cc_u8f name_buffer_size = 0x30;
		// '*4' for the maximum UTF-8 length.
		name_buffer.reserve(name_buffer_size * 4);

		const unsigned char* header_bytes;
		CDReader::Sector sector;

		if (IsCartridgeFileLoaded())
		{
			header_bytes = &rom_buffer[0x100];
		}
		else //if (cd_file.IsOpen())
		{
			sector = cd_file.ReadSector(0);
			header_bytes = &sector[0x100];
		}

		// Choose the proper name based on the current region.
		const unsigned char* const in_buffer = &header_bytes[GetDomestic() ? 0x20 : 0x50];

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

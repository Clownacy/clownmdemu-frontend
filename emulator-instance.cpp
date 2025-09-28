#include "emulator-instance.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "frontend.h"
#include "text-encoding.h"

const ClownMDEmu_Constant EmulatorInstance::clownmdemu_constant = []()
{
	ClownMDEmu_Constant constant;
	ClownMDEmu_Constant_Initialise(&constant);
	return constant;
}();

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
			if (std::size(*save_data_buffer) > std::size(emulator.state->clownmdemu.external_ram.buffer))
			{
				Frontend::debug_log.Log("Save data file size (0x{:X} bytes) is larger than the internal save data buffer size (0x{:X} bytes)", std::size(*save_data_buffer), std::size(emulator.state->clownmdemu.external_ram.buffer));
			}
			else
			{
				std::copy(std::begin(*save_data_buffer), std::end(*save_data_buffer), emulator.state->clownmdemu.external_ram.buffer);
				std::fill(std::begin(emulator.state->clownmdemu.external_ram.buffer) + std::size(*save_data_buffer), std::end(emulator.state->clownmdemu.external_ram.buffer), 0xFF);
			}
		}
	}
}

void EmulatorInstance::Cartridge::Eject()
{
	rom_file_buffer.clear();

	// Write save data to disk.
	if (emulator.state->clownmdemu.external_ram.non_volatile && emulator.state->clownmdemu.external_ram.size != 0)
	{
		SDL::IOStream file = SDL::IOFromFile(save_data_path, "wb");

		if (!file || SDL_WriteIO(file, emulator.state->clownmdemu.external_ram.buffer, emulator.state->clownmdemu.external_ram.size) != emulator.state->clownmdemu.external_ram.size)
			Frontend::debug_log.Log("Could not write save data file");
	}

	save_data_path.clear();
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

void EmulatorInstance::ScanlineRenderedCallback(void* const user_data, const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f left_boundary, const cc_u16f right_boundary, const cc_u16f screen_width, const cc_u16f screen_height)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	emulator->current_screen_width = screen_width;
	emulator->current_screen_height = screen_height;

	if (emulator->framebuffer_texture_pixels == nullptr)
		return;

	auto input_pointer = pixels + left_boundary;
	auto output_pointer = &emulator->framebuffer_texture_pixels[scanline * emulator->framebuffer_texture_pitch + left_boundary];

	for (cc_u16f i = left_boundary; i < right_boundary; ++i)
		*output_pointer++ = emulator->state->colours[*input_pointer++];
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

void EmulatorInstance::CDSectorReadCallback(void* const user_data, cc_u16l* const buffer)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	emulator->cd_file.ReadSector(buffer);
}

cc_bool EmulatorInstance::CDSeekTrackCallback(void* const user_data, const cc_u16f track_index, const ClownMDEmu_CDDAMode mode)
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

cc_bool EmulatorInstance::SaveFileOpenedForReadingCallback(void* const user_data, const char* const filename)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	emulator->save_data_stream = SDL::IOFromFile(Frontend::GetSaveDataDirectoryPath() / filename, "rb");
	return emulator->save_data_stream != nullptr;
}

cc_s16f EmulatorInstance::SaveFileReadCallback(void* const user_data)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);
	Uint8 byte;

	if (!SDL_ReadU8(emulator->save_data_stream, &byte))
		return -1;

	return byte;
}

cc_bool EmulatorInstance::SaveFileOpenedForWriting(void* const user_data, const char* const filename)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	emulator->save_data_stream = SDL::IOFromFile(Frontend::GetSaveDataDirectoryPath() / filename, "wb");
	return emulator->save_data_stream != nullptr;
}

void EmulatorInstance::SaveFileWritten(void* const user_data, const cc_u8f byte)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	SDL_WriteU8(emulator->save_data_stream, byte);
}

void EmulatorInstance::SaveFileClosed(void* const user_data)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	emulator->save_data_stream.reset();
}

cc_bool EmulatorInstance::SaveFileRemoved([[maybe_unused]] void* const user_data, const char* const filename)
{
	return SDL::RemovePath(Frontend::GetSaveDataDirectoryPath() / filename);
}

cc_bool EmulatorInstance::SaveFileSizeObtained([[maybe_unused]] void* const user_data, const char* const filename, std::size_t* const size)
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
	: audio_output()
	, texture(texture)
	, input_callback(input_callback)
	, callbacks({this,
		ColourUpdatedCallback,
		ScanlineRenderedCallback,
		ReadInputCallback,
		FMAudioCallback,
		PSGAudioCallback,
		PCMAudioCallback,
		CDDAAudioCallback,
		CDSeekCallback,
		CDSectorReadCallback,
		CDSeekTrackCallback,
		CDAudioReadCallback,
		SaveFileOpenedForReadingCallback,
		SaveFileReadCallback,
		SaveFileOpenedForWriting,
		SaveFileWritten,
		SaveFileClosed,
		SaveFileRemoved,
		SaveFileSizeObtained
	})
{
	ClownCD_SetErrorCallback([]([[maybe_unused]] void* const user_data, const char* const message) { Frontend::debug_log.Log("ClownCD: {}", message); }, nullptr);

	// This should be called before any other clownmdemu functions are called!
	ClownMDEmu_SetLogCallback([](void* const user_data, const char* const format, va_list args) { static_cast<DebugLog*>(user_data)->Log(format, args); }, &Frontend::debug_log);

	ClownMDEmu_State_Initialise(&state->clownmdemu);
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

			// We maintain a ring buffer of emulator states:
			// when rewinding, we go backwards through this buffer,
			// and when not rewinding, we go forwards through it.
			const auto rewind_buffer_size = rewind.buffer.size();

			if (IsRewinding())
			{
				--rewind.remaining;

				if (rewind.index == 0)
					rewind.index = rewind_buffer_size - 1;
				else
					--rewind.index;
			}
			else
			{
				if (rewind.remaining < rewind_buffer_size - 2) // -2 because we use a slot to hold the current state, so -1 does not suffice.
					++rewind.remaining;

				if (rewind.index == rewind_buffer_size - 1)
					rewind.index = 0;
				else
					++rewind.index;
			}

			auto &previous_state = rewind.buffer[rewind.index];
			auto &next_state = rewind.buffer[(rewind.index + 1) % rewind_buffer_size];

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
		}

		// Reset the audio buffers so that they can be mixed into.
		audio_output.MixerBegin();

		ClownMDEmu_Iterate(&clownmdemu);

		// Resample, mix, and output the audio for this frame.
		audio_output.MixerEnd();

		// Log CD state for this frame.
		state->cd = cd_file.GetState();
	}

	// Unlock the texture so that we can draw it
	SDL_UnlockTexture(texture);
}

void EmulatorInstance::SoftResetConsole()
{
	ClownMDEmu_Reset(&clownmdemu, !IsCartridgeFileLoaded());
}

void EmulatorInstance::HardResetConsole()
{
	rewind.remaining = 0;

	ClownMDEmu_State_Initialise(&state->clownmdemu);
	ClownMDEmu_Parameters_Initialise(&clownmdemu, &clownmdemu_configuration, &clownmdemu_constant, &state->clownmdemu, &callbacks);
	const auto &buffer = cartridge.GetROMBuffer();
	ClownMDEmu_SetCartridge(&clownmdemu, std::data(buffer), std::size(buffer));
	SoftResetConsole();
}

void EmulatorInstance::LoadCartridgeFile(std::vector<cc_u16l> &&file_buffer, const std::filesystem::path &path)
{
	cartridge.Insert(std::move(file_buffer), path);

	HardResetConsole();
}

void EmulatorInstance::UnloadCartridgeFile()
{
	cartridge.Eject();

	ClownMDEmu_SetCartridge(&clownmdemu, nullptr, 0);
}

bool EmulatorInstance::LoadCDFile(SDL::IOStream &&stream, const std::filesystem::path &path)
{
	cd_file.Open(std::move(stream), path);
	if (!cd_file.IsOpen())
		return false;

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

bool EmulatorInstance::WriteSaveStateFile(SDL::IOStream &file)
{
	bool success = false;

	if (SDL_WriteIO(file, &save_state_magic, sizeof(save_state_magic)) == sizeof(save_state_magic) && SDL_WriteIO(file, state, sizeof(*state)) == sizeof(*state))
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

		std::array<unsigned char, name_buffer_size> in_buffer;
		// Choose the proper name based on the current region.
		const auto header_offset = GetDomestic() ? 0x120 : 0x150;

		if (IsCartridgeFileLoaded())
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
		else //if (cd_file.IsOpen())
		{
			std::array<unsigned char, CDReader::SECTOR_SIZE> sector;
			cd_file.ReadMegaCDHeaderSector(sector.data());
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

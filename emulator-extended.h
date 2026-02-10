#ifndef EMULATOR_EXTENDED_H
#define EMULATOR_EXTENDED_H

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <type_traits>

#include "common/core/clownmdemu.h"

#include "audio-output.h"
#include "cd-reader.h"
#include "debug-log.h"
#include "sdl-wrapper.h"
#include "text-encoding.h"

template<typename Derived, typename Colour>
class EmulatorExtended : public ClownMDEmuCXX::Emulator<Derived>
{
protected:
	using Emulator = ClownMDEmuCXX::Emulator<Derived>;
	friend Emulator;

	struct Palette
	{
		static constexpr unsigned int total_brightnesses = VDP_TOTAL_BRIGHTNESSES;
		static constexpr unsigned int total_lines = VDP_TOTAL_PALETTE_LINES;
		static constexpr unsigned int total_colours_in_line = VDP_PALETTE_LINE_LENGTH;

		std::array<Colour, total_colours_in_line * total_lines * total_brightnesses> colours;
	};

public:
	class StateBackup
	{
	private:
		ClownMDEmuCXX::StateBackup emulator;
		CDReader::StateBackup cd_reader;
		Palette palette;

	public:
		StateBackup(const EmulatorExtended &emulator)
			: emulator(emulator)
			, cd_reader(emulator.cd_reader)
			, palette(emulator.palette)
		{}

		void Apply(EmulatorExtended &emulator) const
		{
			this->emulator.Apply(emulator);
			cd_reader.Apply(emulator.cd_reader);
			emulator.palette = palette;
		}
	};

	// Ensure that this is safe to save to (and read from) a file.
	static_assert(std::is_trivially_copyable_v<StateBackup>);

	// TODO: Make this private and use getters and setters instead, for consistency?
	bool rewinding = false;

private:
	class StateRingBuffer
	{
	protected:
		std::vector<std::array<std::byte, sizeof(StateBackup)>> state_buffer;
		std::size_t state_buffer_index = 0;
		std::size_t state_buffer_remaining = 0;

		std::size_t NextIndex(std::size_t index) const
		{
			++index;

			if (index == std::size(state_buffer))
				index = 0;

			return index;
		}

		std::size_t PreviousIndex(std::size_t index) const
		{
			if (index == 0)
				index = std::size(state_buffer);

			--index;

			return index;
		}

	public:
		StateRingBuffer(const bool enabled)
			: state_buffer(enabled ? 10 * 60 : 0) // 10 seconds
		{}

		void Clear()
		{
			state_buffer_index = state_buffer_remaining = 0;
		}

		[[nodiscard]] bool Exhausted() const
		{
			// We need at least two frames, because rewinding pops one frame and then samples the frame below the head.
			return state_buffer_remaining < 2;
		}

		[[nodiscard]] constexpr std::size_t Capacity() const
		{
			return std::size(state_buffer) - 2;
		}

		void Push(EmulatorExtended &emulator)
		{
			assert(Exists());

			state_buffer_remaining = std::min(state_buffer_remaining + 1, Capacity());

			const auto old_index = state_buffer_index;
			state_buffer_index = NextIndex(state_buffer_index);
			new(&state_buffer[old_index]) StateBackup(emulator);
		}

		void Pop(EmulatorExtended &emulator)
		{
			assert(Exists());
			assert(!Exhausted());
			--state_buffer_remaining;

			state_buffer_index = PreviousIndex(state_buffer_index);
			auto &state = *reinterpret_cast<StateBackup*>(&state_buffer[PreviousIndex(state_buffer_index)]);
			state.Apply(emulator);
			state.~StateBackup();
		}

		[[nodiscard]] bool Exists() const
		{
			return !state_buffer.empty();
		}

		[[nodiscard]] std::size_t Size() const
		{
			return state_buffer_remaining;
		}
	};

	bool paused = false;
	CDReader cd_reader;
	AudioOutput audio_output;
	Palette palette;
	StateRingBuffer state_rewind_buffer;
	std::fstream save_data_stream;
	std::filesystem::path save_file_directory;
	std::filesystem::path cartridge_save_file_path;
	unsigned int speed = 1;

	////////////////////////
	// Emulator Callbacks //
	////////////////////////

	void ColourUpdated(const cc_u16f index, const cc_u16f colour)
	{
		palette.colours[index] = colour;
	}

	void FMAudioToBeGenerated(ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_fm_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
	{
		generate_fm_audio(clownmdemu, audio_output.MixerAllocateFMSamples(total_frames), total_frames);
	}
	void PSGAudioToBeGenerated(ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_psg_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
	{
		generate_psg_audio(clownmdemu, audio_output.MixerAllocatePSGSamples(total_frames), total_frames);
	}
	void PCMAudioToBeGenerated(ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_pcm_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
	{
		generate_pcm_audio(clownmdemu, audio_output.MixerAllocatePCMSamples(total_frames), total_frames);
	}
	void CDDAAudioToBeGenerated(ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_cdda_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
	{
		generate_cdda_audio(clownmdemu, audio_output.MixerAllocateCDDASamples(total_frames), total_frames);
	}

	void CDSeeked(cc_u32f sector_index)
	{
		cd_reader.SeekToSector(sector_index);
	}
	void CDSectorRead(cc_u16l *buffer)
	{
		cd_reader.ReadSector(buffer);
	}
	cc_bool CDTrackSeeked(cc_u16f track_index, ClownMDEmu_CDDAMode mode)
	{
		CDReader::PlaybackSetting playback_setting;

		switch (mode)
		{
			default:
				assert(false);
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

		return cd_reader.PlayAudio(track_index, playback_setting);
	}
	std::size_t CDAudioRead(cc_s16l *sample_buffer, std::size_t total_frames)
	{
		return cd_reader.ReadAudio(sample_buffer, total_frames);
	}

	cc_bool SaveFileOpenedForReading(const char *filename)
	{
		save_data_stream.open(save_file_directory / filename, std::ios::binary | std::ios::in);
		return save_data_stream.is_open();
	}
	cc_s16f SaveFileRead()
	{
		const auto byte = save_data_stream.get();

		if (save_data_stream.eof())
			return -1;

		return byte;
	}
	cc_bool SaveFileOpenedForWriting(const char *filename)
	{
		save_data_stream.open(save_file_directory / filename, std::ios::binary | std::ios::out);
		return save_data_stream.is_open();
	}
	void SaveFileWritten(cc_u8f byte)
	{
		save_data_stream.put(byte);
	}
	void SaveFileClosed()
	{
		save_data_stream.close();
	}
	cc_bool SaveFileRemoved(const char *filename)
	{
		return std::filesystem::remove(save_file_directory / filename);
	}
	cc_bool SaveFileSizeObtained(const char *filename, std::size_t *size)
	{
		std::error_code ec;
		*size = std::filesystem::file_size(save_file_directory / filename, ec);
		return !ec;
	}

	/////////////////////////
	// Cartridge Save Data //
	/////////////////////////

	void LoadCartridgeSaveData(const std::filesystem::path &cartridge_file_path)
	{
		cartridge_save_file_path = save_file_directory / cartridge_file_path.stem().replace_extension(".srm");

		// Reset external RAM to its default 'empty' state.
		// This way, even if a file is not loaded, the buffer is at least initialised to something.
		auto &external_ram_buffer = this->GetExternalRAM();
		external_ram_buffer.fill(0xFF);

		// Load save data from disk.
		if (std::filesystem::exists(cartridge_save_file_path))
		{
			const auto save_data_size = std::filesystem::file_size(cartridge_save_file_path);

			if (save_data_size > std::size(external_ram_buffer))
				Frontend::debug_log.Log("Save data file size (0x{:X} bytes) is larger than the internal save data buffer size (0x{:X} bytes)", save_data_size, std::size(external_ram_buffer));
			else
				std::ifstream(cartridge_save_file_path, std::ios::binary).read(reinterpret_cast<char*>(std::data(external_ram_buffer)), save_data_size);
		}
	}

	void SaveCartridgeSaveData()
	{
		if (cartridge_save_file_path.empty())
			return;

		// Write save data to disk.
		const auto &external_ram = this->GetState().external_ram;

		if (external_ram.non_volatile && external_ram.size != 0)
		{
			std::ofstream save_data_stream(cartridge_save_file_path, std::ios::binary);

			if (!save_data_stream.is_open())
				Frontend::debug_log.Log("Could not open save data for writing");
			else
				save_data_stream.write(reinterpret_cast<const char*>(external_ram.buffer), external_ram.size);
		}

		cartridge_save_file_path.clear();
	}

	///////////////////
	// Miscellaneous //
	///////////////////

	std::string GetSoftwareName()
	{
		std::string name_buffer;

		if (this->IsCartridgeInserted() || IsCDInserted())
		{
			constexpr cc_u8f name_buffer_size = 0x30;
			// '*4' for the maximum UTF-8 length.
			name_buffer.reserve(name_buffer_size * 4);

			std::array<unsigned char, name_buffer_size> in_buffer;
			// Choose the proper name based on the current region.
			const auto header_offset = this->GetRegion() == CLOWNMDEMU_REGION_DOMESTIC ? 0x120 : 0x150;

			if (this->IsCartridgeInserted())
			{
				// TODO: This seems unsafe - add some bounds checks?
				const auto words = &this->cartridge_buffer[header_offset / 2];

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

	void UpdateTitle()
	{
		static_cast<Derived*>(this)->TitleChanged(GetSoftwareName());
	}

public:
	EmulatorExtended(const ClownMDEmuCXX::InitialConfiguration &configuration, const bool rewinding_enabling, const std::filesystem::path &save_file_directory)
		: Emulator(configuration)
		, audio_output(this->GetTVStandard() == CLOWNMDEMU_TV_STANDARD_PAL, paused)
		, state_rewind_buffer(rewinding_enabling)
		, save_file_directory(save_file_directory)
	{
		std::filesystem::create_directories(save_file_directory);

		// This should be called before any other ClownMDEmu functions are called!
		this->SetLogCallback([](const char* const format, std::va_list args) { Frontend::debug_log.Log(format, args); });
		cd_reader.SetErrorCallback([](const std::string_view &message) { Frontend::debug_log.Log("ClownCD: {}", message); });
	}

	~EmulatorExtended()
	{
		SaveCartridgeSaveData();
	}

	///////////////
	// Cartridge //
	///////////////

	template<typename... Ts>
	void InsertCartridge(const std::filesystem::path &cartridge_file_path, Ts &&...args)
	{
		// Reset state buffer, since it cannot undo the cartridge or CD being changed.
		state_rewind_buffer.Clear();

		Emulator::InsertCartridge(std::forward<Ts>(args)...);
		LoadCartridgeSaveData(cartridge_file_path);

		HardReset();

		UpdateTitle();
	}

	template<typename... Ts>
	void EjectCartridge(Ts &&...args)
	{
		state_rewind_buffer.Clear();

		SaveCartridgeSaveData();
		Emulator::EjectCartridge(std::forward<Ts>(args)...);

		if (IsCDInserted())
			HardReset();

		UpdateTitle();
	}

	////////
	// CD //
	////////

	[[nodiscard]] bool InsertCD(SDL::IOStream &&stream, const std::filesystem::path &path)
	{
		state_rewind_buffer.Clear();

		cd_reader.Open(path, std::move(stream));
		if (!cd_reader.IsOpen())
			return false;

		HardReset();

		UpdateTitle();

		return true;
	}

	void EjectCD()
	{
		state_rewind_buffer.Clear();

		cd_reader.Close();

		if (this->IsCartridgeInserted())
			HardReset();

		UpdateTitle();
	}

	[[nodiscard]] bool IsCDInserted() const
	{
		return cd_reader.IsOpen();
	}

	///////////////////
	// Miscellaneous //
	///////////////////

	bool ReadMegaCDHeaderSector(unsigned char* const buffer)
	{
		// TODO: Make this return an array instead!
		return cd_reader.ReadMegaCDHeaderSector(buffer);
	}

	void SoftReset(const cc_bool cd_inserted) = delete;
	void SoftReset()
	{
		Emulator::SoftReset(IsCDInserted());
	}

	void HardReset(const cc_bool cd_inserted) = delete;
	void HardReset()
	{
		Emulator::HardReset(IsCDInserted());
	}

	bool Iterate()
	{
		for (unsigned int i = 0; i < speed; ++i)
		{
			if (state_rewind_buffer.Exists())
			{
				if (rewinding)
				{
					if (state_rewind_buffer.Exhausted())
						return i != 0;

					state_rewind_buffer.Pop(*this);
				}
				else
				{
					state_rewind_buffer.Push(*this);
				}
			}

			// Reset the audio buffers so that they can be mixed into.
			audio_output.MixerBegin();

			Emulator::Iterate();

			// Resample, mix, and output the audio for this frame.
			audio_output.MixerEnd();
		}

		return true;
	}

	void SetFastForwarding(const unsigned int speed)
	{
		this->speed = speed == 0 ? 1 : std::pow(3, speed);
	}

	bool IsFastForwarding() const
	{
		return speed != 1;
	}

	void SetPaused(const bool paused)
	{
		this->paused = paused;
		audio_output.SetPaused(paused);
	}

	bool IsPaused() const
	{
		return paused;
	}

	///////////
	// Audio //
	///////////

	[[nodiscard]] cc_u32f GetAudioAverageFrames() const { return audio_output.GetAverageFrames(); }
	[[nodiscard]] cc_u32f GetAudioTargetFrames() const { return audio_output.GetTargetFrames(); }
	[[nodiscard]] cc_u32f GetAudioTotalBufferFrames() const { return audio_output.GetTotalBufferFrames(); }
	[[nodiscard]] cc_u32f GetAudioSampleRate() const { return audio_output.GetSampleRate(); }

	////////////////////
	// Colour Palette //
	////////////////////

	[[nodiscard]] const Colour* GetPaletteLine(const cc_u8f brightness, const cc_u8f palette_line) const
	{
		return &palette.colours[brightness * Palette::total_lines * Palette::total_colours_in_line + palette_line * Palette::total_colours_in_line];
	}
	[[nodiscard]] const Colour& GetColour(const cc_u8f brightness, const cc_u8f palette_line, const cc_u8f colour_index) const
	{
		return GetPaletteLine(brightness, palette_line)[colour_index];
	}
	[[nodiscard]] const Colour& GetColour(const cc_u8f index) const
	{
		return palette.colours[index];
	}

	////////////
	// Rewind //
	////////////

	[[nodiscard]] bool GetRewindEnabled() const
	{
		return state_rewind_buffer.Exists();
	}

	void SetRewindEnabled(const bool enabled)
	{
		state_rewind_buffer = {enabled};
	}

	[[nodiscard]] bool IsRewindExhausted() const
	{
		return state_rewind_buffer.Exhausted();
	}

	[[nodiscard]] float GetRewindAmount() const
	{
		return static_cast<float>(state_rewind_buffer.Size()) / state_rewind_buffer.Capacity();
	}

	/////////////////////////////
	// Option Setter Overrides //
	/////////////////////////////

	void SetTVStandard(const ClownMDEmu_TVStandard tv_standard)
	{
		Emulator::SetTVStandard(tv_standard);
		audio_output = AudioOutput(tv_standard == CLOWNMDEMU_TV_STANDARD_PAL, audio_output.GetPaused());
	}

	void SetRegion(const ClownMDEmu_Region region)
	{
		Emulator::SetRegion(region);
		UpdateTitle();
	}
};

#endif // EMULATOR_EXTENDED_H

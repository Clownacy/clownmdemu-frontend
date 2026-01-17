#ifndef EMULATOR_EXTENDED_H
#define EMULATOR_EXTENDED_H

#include <array>
#include <filesystem>
#include <fstream>
#include <type_traits>

#include "common/core/clownmdemu.h"

#include "audio-output.h"
#include "cd-reader.h"
#include "debug-log.h"
#include "sdl-wrapper.h"

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
	bool rewinding = false, fastforwarding = false;

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

	CDReader cd_reader;
	AudioOutput audio_output;
	Palette palette;
	StateRingBuffer state_rewind_buffer;
	std::fstream save_data_stream;
	std::filesystem::path save_file_directory;
	std::filesystem::path cartridge_save_file_path;

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

		// Load save data from disk.
		if (std::filesystem::exists(cartridge_save_file_path))
		{
			const auto save_data_size = std::filesystem::file_size(cartridge_save_file_path);

			auto &external_ram_buffer = Emulator::GetExternalRAM();

			if (save_data_size > std::size(external_ram_buffer))
			{
				Frontend::debug_log.Log("Save data file size (0x{:X} bytes) is larger than the internal save data buffer size (0x{:X} bytes)", save_data_size, std::size(external_ram_buffer));
			}
			else
			{
				std::ifstream save_data_stream(cartridge_save_file_path, std::ios::binary);
				save_data_stream.read(reinterpret_cast<char*>(external_ram_buffer), save_data_size);
				std::fill(std::begin(external_ram_buffer) + save_data_size, std::end(external_ram_buffer), 0xFF);
			}
		}
	}

	void SaveCartridgeSaveData()
	{
		if (cartridge_save_file_path.empty())
			return;

		// Write save data to disk.
		const auto &external_ram = Emulator::GetState().external_ram;

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

public:
	EmulatorExtended(const ClownMDEmuCXX::InitialConfiguration &configuration, const bool rewinding_enabling, const std::filesystem::path &save_file_directory)
		: Emulator(configuration)
		, audio_output(Emulator::GetTVStandard() == CLOWNMDEMU_TV_STANDARD_PAL)
		, state_rewind_buffer(rewinding_enabling)
		, save_file_directory(save_file_directory)
	{
		std::filesystem::create_directories(save_file_directory);

		// This should be called before any other ClownMDEmu functions are called!
		Emulator::SetLogCallback([](const char* const format, std::va_list args) { Frontend::debug_log.Log(format, args); });
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
	}

	template<typename... Ts>
	void EjectCartridge(Ts &&...args)
	{
		state_rewind_buffer.Clear();

		SaveCartridgeSaveData();
		Emulator::EjectCartridge(std::forward<Ts>(args)...);
	}

	////////
	// CD //
	////////

	[[nodiscard]] bool InsertCD(SDL::IOStream &stream, const std::filesystem::path &path)
	{
		state_rewind_buffer.Clear();

		cd_reader.Open(path, stream);
		return cd_reader.IsOpen();
	}

	void EjectCD()
	{
		state_rewind_buffer.Clear();

		cd_reader.Close();
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

	void Reset(const cc_bool cd_inserted) = delete;
	void Reset()
	{
		Emulator::Reset(IsCDInserted());
	}

	bool Iterate()
	{
		for (unsigned int i = 0; i < (fastforwarding ? 3 : 1); ++i)
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
		audio_output = AudioOutput(tv_standard == CLOWNMDEMU_TV_STANDARD_PAL);
	}
};

#endif // EMULATOR_EXTENDED_H

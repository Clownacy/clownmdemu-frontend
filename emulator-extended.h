#ifndef EMULATOR_EXTENDED_H
#define EMULATOR_EXTENDED_H

#include <array>
#include <filesystem>
#include <type_traits>

#include "audio-output.h"
#include "cd-reader.h"
#include "emulator.h"
#include "sdl-wrapper-inner.h"
#include "state-ring-buffer.h"

template<typename Colour>
class EmulatorExtended : public Emulator
{
protected:
	struct Palette
	{
		static constexpr unsigned int total_brightnesses = VDP_TOTAL_BRIGHTNESSES;
		static constexpr unsigned int total_lines = VDP_TOTAL_PALETTE_LINES;
		static constexpr unsigned int total_colours_in_line = VDP_PALETTE_LINE_LENGTH;

		std::array<Colour, total_colours_in_line * total_lines * total_brightnesses> colours;
	};

public:
	struct State
	{
		Emulator::State emulator;
		CDReader::State cd_reader;
		Palette palette;
	};

	// Ensure that this is safe to save to (and read from) a file.
	static_assert(std::is_trivially_copyable_v<State>);

	// TODO: Make this private and use getters and setters instead, for consistency?
	bool rewinding = false;

private:
	CDReader cd_reader;
	AudioOutput audio_output;
	Palette palette;
	StateRingBuffer<State> state_rewind_buffer;

	void ColourUpdated(const cc_u16f index, const cc_u16f colour) override final
	{
		palette.colours[index] = colour;
	}

	void FMAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) override final
	{
		generate_fm_audio(clownmdemu, audio_output.MixerAllocateFMSamples(total_frames), total_frames);
	}
	void PSGAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) override final
	{
		generate_psg_audio(clownmdemu, audio_output.MixerAllocatePSGSamples(total_frames), total_frames);
	}
	void PCMAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) override final
	{
		generate_pcm_audio(clownmdemu, audio_output.MixerAllocatePCMSamples(total_frames), total_frames);
	}
	void CDDAAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) override final
	{
		generate_cdda_audio(clownmdemu, audio_output.MixerAllocateCDDASamples(total_frames), total_frames);
	}

	void CDSeeked(cc_u32f sector_index) override final
	{
		cd_reader.SeekToSector(sector_index);
	}
	void CDSectorRead(cc_u16l *buffer) override final
	{
		cd_reader.ReadSector(buffer);
	}
	cc_bool CDTrackSeeked(cc_u16f track_index, ClownMDEmu_CDDAMode mode) override final
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
	std::size_t CDAudioRead(cc_s16l *sample_buffer, std::size_t total_frames) override final
	{
		return cd_reader.ReadAudio(sample_buffer, total_frames);
	}

public:
	EmulatorExtended(const Emulator::Configuration &configuration, const bool rewinding_enabling)
		: Emulator(configuration)
		, state_rewind_buffer(rewinding_enabling)
	{}

	[[nodiscard]] bool InsertCD(SDL::IOStream &&stream, const std::filesystem::path &path)
	{
		cd_reader.Open(std::move(stream), path);
		return cd_reader.IsOpen();
	}

	void EjectCD()
	{
		cd_reader.Close();
	}

	[[nodiscard]] bool IsCDInserted() const
	{
		return cd_reader.IsOpen();
	}

	[[nodiscard]] State SaveState() const
	{
		return {Emulator::GetState(), cd_reader.SaveState(), palette};
	}

	void LoadState(const State &state)
	{
		Emulator::SetState(state.emulator);
		cd_reader.LoadState(state.cd_reader);
		palette = state.palette;
	}

	// Hide this, so that the frontend cannot accidentally set only part of the state.
	void SetState(const Emulator::State &state) = delete;

	void ReadMegaCDHeaderSector(unsigned char* const buffer)
	{
		// TODO: Make this return an array instead!
		cd_reader.ReadMegaCDHeaderSector(buffer);
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
		if (state_rewind_buffer.Exists())
		{
			if (rewinding)
			{
				if (state_rewind_buffer.Exhausted())
					return false;

				LoadState(state_rewind_buffer.GetBackward());
			}
			else
			{
				state_rewind_buffer.GetForward() = SaveState();
			}
		}

		// Reset the audio buffers so that they can be mixed into.
		audio_output.MixerBegin();

		Emulator::Iterate();

		// Resample, mix, and output the audio for this frame.
		audio_output.MixerEnd();

		return true;
	}

	// TODO: Make Qt frontend use this!!!
	void SetAudioPALMode(const bool enabled)
	{
		audio_output.SetPALMode(enabled);
	}

	[[nodiscard]] cc_u32f GetAudioAverageFrames() const { return audio_output.GetAverageFrames(); }
	[[nodiscard]] cc_u32f GetAudioTargetFrames() const { return audio_output.GetTargetFrames(); }
	[[nodiscard]] cc_u32f GetAudioTotalBufferFrames() const { return audio_output.GetTotalBufferFrames(); }
	[[nodiscard]] cc_u32f GetAudioSampleRate() const { return audio_output.GetSampleRate(); }

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

	[[nodiscard]] bool GetRewindEnabled() const
	{
		return state_rewind_buffer.Exists();
	}

	void SetRewindEnabled(const bool enabled)
	{
		state_rewind_buffer = {enabled};
	}

	void ClearRewindBuffer()
	{
		state_rewind_buffer.Clear();
	}

	[[nodiscard]] bool IsRewindExhausted() const
	{
		return state_rewind_buffer.Exhausted();
	}
};

#endif // EMULATOR_EXTENDED_H

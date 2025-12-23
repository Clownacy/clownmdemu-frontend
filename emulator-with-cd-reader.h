#ifndef EMULATOR_WITH_CD_READER_H
#define EMULATOR_WITH_CD_READER_H

#include <filesystem>

#include "cd-reader.h"
#include "emulator.h"
#include "sdl-wrapper-inner.h"

class EmulatorWithCDReader : public Emulator
{
protected:
	CDReader cd_reader;

	void CDSeeked(cc_u32f sector_index) override final;
	void CDSectorRead(cc_u16l *buffer) override final;
	cc_bool CDTrackSeeked(cc_u16f track_index, ClownMDEmu_CDDAMode mode) override final;
	std::size_t CDAudioRead(cc_s16l *sample_buffer, std::size_t total_frames) override final;

public:
	struct State
	{
		Emulator::State emulator;
		CDReader::State cd_reader;
	};

	using Emulator::Emulator;

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

	State SaveState() const
	{
		return {Emulator::GetState(), cd_reader.SaveState()};
	}

	void LoadState(const State &state)
	{
		Emulator::SetState(state.emulator);
		cd_reader.LoadState(state.cd_reader);
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
};

#endif // EMULATOR_WITH_CD_READER_H

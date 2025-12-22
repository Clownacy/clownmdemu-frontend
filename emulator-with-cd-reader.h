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

	CDReader::State GetCDState() const
	{
		return cd_reader.GetState();
	}

	void SetCDState(const CDReader::State &state)
	{
		cd_reader.SetState(state);
	}

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

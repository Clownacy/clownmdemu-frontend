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

	void SetCD(SDL::IOStream &&stream, const std::filesystem::path &path)
	{
		cd_reader.Open(std::move(stream), path);
	}

	[[nodiscard]] bool IsCDInserted() const
	{
		return cd_reader.IsOpen();
	}
};

#endif // EMULATOR_WITH_CD_READER_H

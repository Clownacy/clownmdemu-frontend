#ifndef EMULATOR_INSTANCE_H
#define EMULATOR_INSTANCE_H

#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "common/core/clownmdemu.h"

#include "colour.h"
#include "emulator-extended.h"
#include "sdl-wrapper.h"

class EmulatorInstance final : public EmulatorExtended<Colour>
{
public:
	using InputCallback = std::function<bool(cc_u8f player_id, ClownMDEmu_Button button_id)>;

private:
	SDL::Texture &texture;
	const InputCallback input_callback;

	Emulator::Configuration emulator_configuration;

	std::vector<cc_u16l> rom_file_buffer;
	SDL::IOStream cd_stream;

	SDL::Pixel *framebuffer_texture_pixels = nullptr;
	int framebuffer_texture_pitch = 0;

	unsigned int current_screen_width = 0;
	unsigned int current_screen_height = 0;

	virtual void ScanlineRendered(cc_u16f scanline, const cc_u8l *pixels, cc_u16f left_boundary, cc_u16f right_boundary, cc_u16f screen_width, cc_u16f screen_height) override;
	virtual cc_bool InputRequested(cc_u8f player_id, ClownMDEmu_Button button_id) override;

public:
	EmulatorInstance(SDL::Texture &texture, const InputCallback &input_callback);

	void Update();
	void LoadCartridgeFile(std::vector<cc_u16l> &&file_buffer, const std::filesystem::path &path);
	void UnloadCartridgeFile();
	bool LoadCDFile(SDL::IOStream &&stream, const std::filesystem::path &path);
	void UnloadCDFile();

	bool ValidateSaveStateFile(const std::vector<unsigned char> &file_buffer);
	bool LoadSaveStateFile(const std::vector<unsigned char> &file_buffer);
	std::size_t GetSaveStateFileSize();
	bool WriteSaveStateFile(SDL::IOStream &file);

	unsigned int GetCurrentScreenWidth() const { return current_screen_width; }
	unsigned int GetCurrentScreenHeight() const { return current_screen_height; }

	const auto& GetROMBuffer() const { return rom_file_buffer; }

	std::string GetSoftwareName();
};

#endif /* EMULATOR_INSTANCE_H */

#ifndef OPTIONS_H
#define OPTIONS_H

#include <QSettings>

#include "emulator.h"

#define STRINGIFY(STRING) STRINGIFY_INTERNAL(STRING)
#define STRINGIFY_INTERNAL(STRING) #STRING

class Options
{
private:
	Emulator::Configuration emulator_configuration = {};
	bool integer_screen_scaling = false;
	bool tall_interlace_mode_2 = false;
	unsigned int keyboard_control_pad = 0;

public:
	const auto& GetEmulatorConfiguration() const
	{
		return emulator_configuration;
	}

#define DEFINE_OPTION_GETTER(IDENTIFIER, VARIABLE) \
	auto IDENTIFIER() const \
	{ \
		return VARIABLE; \
	}

#define DEFINE_OPTION_SETTER_UNSAVED(IDENTIFIER, VARIABLE) \
	void IDENTIFIER(const auto value) \
	{ \
		VARIABLE = value; \
	}

#define DEFINE_OPTION_SETTER_SAVED(IDENTIFIER, VARIABLE) \
	void IDENTIFIER(const auto value) \
	{ \
		VARIABLE = value; \
 \
		QSettings settings; \
		settings.setValue(STRINGIFY(IDENTIFIER), value); \
	}

#define DEFINE_OPTION_UNSAVED(IDENTIFIER, VARIABLE) \
	DEFINE_OPTION_GETTER(IDENTIFIER, VARIABLE) \
	DEFINE_OPTION_SETTER_UNSAVED(IDENTIFIER, VARIABLE)

#define DEFINE_OPTION_SAVED(IDENTIFIER, VARIABLE) \
	DEFINE_OPTION_GETTER(IDENTIFIER, VARIABLE) \
	DEFINE_OPTION_SETTER_SAVED(IDENTIFIER, VARIABLE)

	DEFINE_OPTION_SAVED(TVStandard, emulator_configuration.general.tv_standard)
	DEFINE_OPTION_SAVED(Region, emulator_configuration.general.region)
	DEFINE_OPTION_SAVED(CDAddonEnabled, emulator_configuration.general.cd_add_on_enabled)

	DEFINE_OPTION_SAVED(IntegerScreenScalingEnabled, integer_screen_scaling)
	DEFINE_OPTION_SAVED(TallInterlaceMode2Enabled, tall_interlace_mode_2)
	DEFINE_OPTION_SAVED(WidescreenEnabled, emulator_configuration.vdp.widescreen_enabled)

	DEFINE_OPTION_SAVED(LowPassFilterDisabled, emulator_configuration.general.low_pass_filter_disabled)
	DEFINE_OPTION_SAVED(LowVolumeDistortionDisabled, emulator_configuration.fm.ladder_effect_disabled)

	DEFINE_OPTION_SAVED(KeyboardControlPad, keyboard_control_pad)

	DEFINE_OPTION_UNSAVED(SpritesDisabled, emulator_configuration.vdp.sprites_disabled)
	DEFINE_OPTION_UNSAVED(WindowPlaneDisabled, emulator_configuration.vdp.window_disabled)
	DEFINE_OPTION_UNSAVED(ScrollPlaneADisabled, emulator_configuration.vdp.planes_disabled[0])
	DEFINE_OPTION_UNSAVED(ScrollPlaneBDisabled, emulator_configuration.vdp.planes_disabled[1])

	DEFINE_OPTION_UNSAVED(FM1Disabled, emulator_configuration.fm.fm_channels_disabled[0])
	DEFINE_OPTION_UNSAVED(FM2Disabled, emulator_configuration.fm.fm_channels_disabled[1])
	DEFINE_OPTION_UNSAVED(FM3Disabled, emulator_configuration.fm.fm_channels_disabled[2])
	DEFINE_OPTION_UNSAVED(FM4Disabled, emulator_configuration.fm.fm_channels_disabled[3])
	DEFINE_OPTION_UNSAVED(FM5Disabled, emulator_configuration.fm.fm_channels_disabled[4])
	DEFINE_OPTION_UNSAVED(FM6Disabled, emulator_configuration.fm.fm_channels_disabled[5])
	DEFINE_OPTION_UNSAVED(DACDisabled, emulator_configuration.fm.dac_channel_disabled)

	DEFINE_OPTION_UNSAVED(PSG1Disabled, emulator_configuration.psg.tone_disabled[0])
	DEFINE_OPTION_UNSAVED(PSG2Disabled, emulator_configuration.psg.tone_disabled[1])
	DEFINE_OPTION_UNSAVED(PSG3Disabled, emulator_configuration.psg.tone_disabled[2])
	DEFINE_OPTION_UNSAVED(PSGNoiseDisabled, emulator_configuration.psg.noise_disabled)

	DEFINE_OPTION_UNSAVED(PCM1Disabled, emulator_configuration.pcm.channels_disabled[0])
	DEFINE_OPTION_UNSAVED(PCM2Disabled, emulator_configuration.pcm.channels_disabled[1])
	DEFINE_OPTION_UNSAVED(PCM3Disabled, emulator_configuration.pcm.channels_disabled[2])
	DEFINE_OPTION_UNSAVED(PCM4Disabled, emulator_configuration.pcm.channels_disabled[3])
	DEFINE_OPTION_UNSAVED(PCM5Disabled, emulator_configuration.pcm.channels_disabled[4])
	DEFINE_OPTION_UNSAVED(PCM6Disabled, emulator_configuration.pcm.channels_disabled[5])
	DEFINE_OPTION_UNSAVED(PCM7Disabled, emulator_configuration.pcm.channels_disabled[6])
	DEFINE_OPTION_UNSAVED(PCM8Disabled, emulator_configuration.pcm.channels_disabled[7])
};

#endif // OPTIONS_H

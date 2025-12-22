#ifndef OPTIONS_H
#define OPTIONS_H

#include <QSettings>

#include "../emulator.h"
#include "paste.h"

#define STRINGIFY(STRING) STRINGIFY_INTERNAL(STRING)
#define STRINGIFY_INTERNAL(STRING) #STRING

class Options
{
private:
	ClownMDEmuCPP::Configuration emulator_configuration = {};
	bool integer_screen_scaling = false;
	bool tall_interlace_mode_2 = false;
	bool rewinding = false;
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

#define DEFINE_UNSAVED_OPTION_SETTER(IDENTIFIER, VARIABLE) \
	void IDENTIFIER(const auto value) \
	{ \
		VARIABLE = value; \
	}

#define DEFINE_SAVED_OPTION_SETTER(IDENTIFIER, VARIABLE) \
	void IDENTIFIER(const auto value) \
	{ \
		VARIABLE = value; \
 \
		QSettings settings; \
		settings.setValue(STRINGIFY(IDENTIFIER), VARIABLE); \
	}

#define DEFINE_UNSAVED_OPTION_GETTER_AND_SETTER(IDENTIFIER, VARIABLE) \
	DEFINE_OPTION_GETTER(IDENTIFIER, VARIABLE) \
	DEFINE_UNSAVED_OPTION_SETTER(IDENTIFIER, VARIABLE)

#define DEFINE_SAVED_OPTION_GETTER_AND_SETTER(IDENTIFIER, VARIABLE) \
	DEFINE_OPTION_GETTER(IDENTIFIER, VARIABLE) \
	DEFINE_SAVED_OPTION_SETTER(IDENTIFIER, VARIABLE)

#define DEFINE_UNSAVED_OPTIONS(...) \
	PASTE2(DEFINE_UNSAVED_OPTION_GETTER_AND_SETTER, __VA_ARGS__)

#define DEFINE_SAVED_OPTION_LOAD(IDENTIFIER, VARIABLE) \
	VARIABLE = settings.value(STRINGIFY(IDENTIFIER)).value<decltype(VARIABLE)>();

#define DEFINE_SAVED_OPTIONS(...) \
	PASTE2(DEFINE_SAVED_OPTION_GETTER_AND_SETTER, __VA_ARGS__) \
 \
	Options() \
	{ \
		QSettings settings; \
		PASTE2(DEFINE_SAVED_OPTION_LOAD, __VA_ARGS__) \
	}

	DEFINE_SAVED_OPTIONS(
		TVStandard, emulator_configuration.general.tv_standard,
		Region, emulator_configuration.general.region,
		CDAddonEnabled, emulator_configuration.general.cd_add_on_enabled,

		IntegerScreenScalingEnabled, integer_screen_scaling,
		TallInterlaceMode2Enabled, tall_interlace_mode_2,
		WidescreenEnabled, emulator_configuration.vdp.widescreen_enabled,

		LowPassFilterDisabled, emulator_configuration.general.low_pass_filter_disabled,
		LowVolumeDistortionDisabled, emulator_configuration.fm.ladder_effect_disabled,

		RewindingEnabled, rewinding,

		KeyboardControlPad, keyboard_control_pad
	)

	DEFINE_UNSAVED_OPTIONS(
		SpritesDisabled, emulator_configuration.vdp.sprites_disabled,
		WindowPlaneDisabled, emulator_configuration.vdp.window_disabled,
		ScrollPlaneADisabled, emulator_configuration.vdp.planes_disabled[0],
		ScrollPlaneBDisabled, emulator_configuration.vdp.planes_disabled[1],

		FM1Disabled, emulator_configuration.fm.fm_channels_disabled[0],
		FM2Disabled, emulator_configuration.fm.fm_channels_disabled[1],
		FM3Disabled, emulator_configuration.fm.fm_channels_disabled[2],
		FM4Disabled, emulator_configuration.fm.fm_channels_disabled[3],
		FM5Disabled, emulator_configuration.fm.fm_channels_disabled[4],
		FM6Disabled, emulator_configuration.fm.fm_channels_disabled[5],
		DACDisabled, emulator_configuration.fm.dac_channel_disabled,

		PSG1Disabled, emulator_configuration.psg.tone_disabled[0],
		PSG2Disabled, emulator_configuration.psg.tone_disabled[1],
		PSG3Disabled, emulator_configuration.psg.tone_disabled[2],
		PSGNoiseDisabled, emulator_configuration.psg.noise_disabled,

		PCM1Disabled, emulator_configuration.pcm.channels_disabled[0],
		PCM2Disabled, emulator_configuration.pcm.channels_disabled[1],
		PCM3Disabled, emulator_configuration.pcm.channels_disabled[2],
		PCM4Disabled, emulator_configuration.pcm.channels_disabled[3],
		PCM5Disabled, emulator_configuration.pcm.channels_disabled[4],
		PCM6Disabled, emulator_configuration.pcm.channels_disabled[5],
		PCM7Disabled, emulator_configuration.pcm.channels_disabled[6],
		PCM8Disabled, emulator_configuration.pcm.channels_disabled[7]
	)
};

#endif // OPTIONS_H

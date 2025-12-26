#ifndef OPTIONS_H
#define OPTIONS_H

#include <QSettings>

#include "../emulator.h"
#include "paste.h"

#define STRINGIFY(STRING) STRINGIFY_INTERNAL(STRING)
#define STRINGIFY_INTERNAL(STRING) #STRING

class OptionsPlain : public Emulator::Configuration
{
public:
	bool integer_screen_scaling = false;
	bool tall_interlace_mode_2 = false;
	bool rewinding = false;
	unsigned int keyboard_control_pad = 0;

	EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(IntegerScreenScalingEnabled, integer_screen_scaling)
	EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(TallInterlaceMode2Enabled, tall_interlace_mode_2)
	EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(RewindingEnabled, rewinding)
	EMULATOR_CONFIGURATION_GETTER_SETTER_AS_IS(KeyboardControlPad, keyboard_control_pad)
};

class Options : private OptionsPlain
{
public:
	const Emulator::Configuration& GetEmulatorConfiguration() const
	{
		return *this;
	}

#define DEFINE_SAVED_OPTION_SETTER(IDENTIFIER) \
	void IDENTIFIER(const auto value) \
	{ \
		OptionsPlain::IDENTIFIER(value); \
 \
		QSettings settings; \
		settings.setValue(STRINGIFY(IDENTIFIER), OptionsPlain::IDENTIFIER()); \
	}

#define DEFINE_UNSAVED_OPTION_GETTER_AND_SETTER(IDENTIFIER) \
	using OptionsPlain::IDENTIFIER;

#define DEFINE_SAVED_OPTION_GETTER_AND_SETTER(IDENTIFIER) \
	using OptionsPlain::IDENTIFIER; \
	DEFINE_SAVED_OPTION_SETTER(IDENTIFIER)

#define DEFINE_UNSAVED_OPTIONS(...) \
	PASTE(DEFINE_UNSAVED_OPTION_GETTER_AND_SETTER, __VA_ARGS__)

#define DEFINE_SAVED_OPTION_LOAD(IDENTIFIER) \
	IDENTIFIER(settings.value(STRINGIFY(IDENTIFIER)).value<decltype(IDENTIFIER())>());

#define DEFINE_SAVED_OPTIONS(...) \
	PASTE(DEFINE_SAVED_OPTION_GETTER_AND_SETTER, __VA_ARGS__) \
 \
	Options() \
	{ \
		QSettings settings; \
		PASTE(DEFINE_SAVED_OPTION_LOAD, __VA_ARGS__) \
	}

	DEFINE_SAVED_OPTIONS(
		TVStandard,
		Region,
		CDAddOnEnabled,

		IntegerScreenScalingEnabled,
		TallInterlaceMode2Enabled,
		WidescreenEnabled,

		LowPassFilterEnabled,
		LadderEffectEnabled,

		RewindingEnabled,

		KeyboardControlPad
	)

	DEFINE_UNSAVED_OPTIONS(
		SpritePlaneEnabled,
		WindowPlaneEnabled,
		ScrollPlaneAEnabled,
		ScrollPlaneBEnabled,

		FM1Enabled,
		FM2Enabled,
		FM3Enabled,
		FM4Enabled,
		FM5Enabled,
		FM6Enabled,
		DACEnabled,

		PSG1Enabled,
		PSG2Enabled,
		PSG3Enabled,
		PSGNoiseEnabled,

		PCM1Enabled,
		PCM2Enabled,
		PCM3Enabled,
		PCM4Enabled,
		PCM5Enabled,
		PCM6Enabled,
		PCM7Enabled,
		PCM8Enabled
	)
};

#endif // OPTIONS_H

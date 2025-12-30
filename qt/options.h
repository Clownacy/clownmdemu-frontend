#ifndef OPTIONS_H
#define OPTIONS_H

#include <QObject>
#include <QSettings>

#include "../emulator.h"
#include "paste.h"

class OptionsPlain : public Emulator::InitialConfiguration
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

class Options : public QObject, private OptionsPlain
{
	Q_OBJECT

public:
	const Emulator::InitialConfiguration& GetEmulatorConfiguration() const
	{
		return *this;
	}

#define DEFINE_UNSAVED_OPTION_SETTER(IDENTIFIER) \
	void Set##IDENTIFIER(const auto value) \
	{ \
		OptionsPlain::Set##IDENTIFIER(value); \
 \
		emit IDENTIFIER##Changed(Get##IDENTIFIER()); \
	}

#define DEFINE_SAVED_OPTION_SETTER(IDENTIFIER) \
	void Set##IDENTIFIER(const auto value) \
	{ \
		OptionsPlain::Set##IDENTIFIER(value); \
 \
		emit IDENTIFIER##Changed(Get##IDENTIFIER()); \
 \
		QSettings settings; \
		settings.setValue(CC_STRINGIFY(IDENTIFIER), OptionsPlain::Get##IDENTIFIER()); \
	}

#define DEFINE_UNSAVED_OPTION_GETTER_AND_SETTER(IDENTIFIER) \
	using OptionsPlain::Get##IDENTIFIER; \
	DEFINE_UNSAVED_OPTION_SETTER(IDENTIFIER)

#define DEFINE_SAVED_OPTION_GETTER_AND_SETTER(IDENTIFIER) \
	using OptionsPlain::Get##IDENTIFIER; \
	DEFINE_SAVED_OPTION_SETTER(IDENTIFIER)

#define DEFINE_UNSAVED_OPTIONS(...) \
	PASTE(DEFINE_UNSAVED_OPTION_GETTER_AND_SETTER, __VA_ARGS__)

#define DEFINE_SAVED_OPTION_LOAD(IDENTIFIER) \
	if (settings.contains(CC_STRINGIFY(IDENTIFIER))) \
		Set##IDENTIFIER(settings.value(CC_STRINGIFY(IDENTIFIER)).value<decltype(Get##IDENTIFIER())>());

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
		WidescreenTilePairs,

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

signals:
	void TVStandardChanged(ClownMDEmu_TVStandard tv_standard);
	void RegionChanged(ClownMDEmu_Region region);
	void CDAddOnEnabledChanged(cc_bool enabled);
	void IntegerScreenScalingEnabledChanged(cc_bool enabled);
	void TallInterlaceMode2EnabledChanged(cc_bool enabled);
	void WidescreenTilePairsChanged(unsigned int value);
	void LowPassFilterEnabledChanged(cc_bool enabled);
	void LadderEffectEnabledChanged(cc_bool enabled);
	void RewindingEnabledChanged(cc_bool enabled);
	void KeyboardControlPadChanged(unsigned int value);

	void SpritePlaneEnabledChanged(cc_bool enabled);
	void WindowPlaneEnabledChanged(cc_bool enabled);
	void ScrollPlaneAEnabledChanged(cc_bool enabled);
	void ScrollPlaneBEnabledChanged(cc_bool enabled);
	void FM1EnabledChanged(cc_bool enabled);
	void FM2EnabledChanged(cc_bool enabled);
	void FM3EnabledChanged(cc_bool enabled);
	void FM4EnabledChanged(cc_bool enabled);
	void FM5EnabledChanged(cc_bool enabled);
	void FM6EnabledChanged(cc_bool enabled);
	void DACEnabledChanged(cc_bool enabled);
	void PSG1EnabledChanged(cc_bool enabled);
	void PSG2EnabledChanged(cc_bool enabled);
	void PSG3EnabledChanged(cc_bool enabled);
	void PSGNoiseEnabledChanged(cc_bool enabled);
	void PCM1EnabledChanged(cc_bool enabled);
	void PCM2EnabledChanged(cc_bool enabled);
	void PCM3EnabledChanged(cc_bool enabled);
	void PCM4EnabledChanged(cc_bool enabled);
	void PCM5EnabledChanged(cc_bool enabled);
	void PCM6EnabledChanged(cc_bool enabled);
	void PCM7EnabledChanged(cc_bool enabled);
	void PCM8EnabledChanged(cc_bool enabled);
};

#endif // OPTIONS_H

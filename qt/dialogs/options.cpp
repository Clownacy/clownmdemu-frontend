#include "options.h"

Dialogs::Options::Options(::Options &options, QWidget* const parent)
	: QDialog(parent)
	, options(options)
{
	ui.setupUi(this);

	const auto &DoRadioButton = [this]<typename T>(QRadioButton* const radiobutton, T &variable, const T value)
	{
		radiobutton->setChecked(variable == value);
		connect(radiobutton, &QRadioButton::toggled, this, [&variable, value](const bool enabled){ if (enabled) variable = value; });
	};

	const auto &DoEnabledCheckBox = [this](QCheckBox* const checkbox, auto &boolean)
	{
		checkbox->setChecked(boolean);
		connect(checkbox, &QCheckBox::checkStateChanged, this, [&boolean](const bool enabled){boolean = enabled;});
	};

	const auto &DoDisableCheckBox = [this](QCheckBox* const checkbox, auto &boolean)
	{
		checkbox->setChecked(!boolean);
		connect(checkbox, &QCheckBox::checkStateChanged, this, [&](const bool enabled){boolean = !enabled;});
	};

	DoRadioButton(ui.ntsc, options.emulator_configuration.general.tv_standard, CLOWNMDEMU_TV_STANDARD_NTSC);
	DoRadioButton(ui.pal, options.emulator_configuration.general.tv_standard, CLOWNMDEMU_TV_STANDARD_PAL);

	DoRadioButton(ui.japan, options.emulator_configuration.general.region, CLOWNMDEMU_REGION_DOMESTIC);
	DoRadioButton(ui.elsewhere, options.emulator_configuration.general.region, CLOWNMDEMU_REGION_OVERSEAS);

	DoEnabledCheckBox(ui.cdAddon, options.emulator_configuration.general.cd_add_on_enabled);
	DoEnabledCheckBox(ui.integerScreenScaling, options.integer_screen_scaling);
	connect(ui.integerScreenScaling, &QCheckBox::checkStateChanged, this, &Options::presentationOptionChanged);
	DoEnabledCheckBox(ui.tallInterlaceMode2, options.tall_interlace_mode_2);
	connect(ui.tallInterlaceMode2, &QCheckBox::checkStateChanged, this, &Options::presentationOptionChanged);
	DoEnabledCheckBox(ui.widescreenHack, options.emulator_configuration.vdp.widescreen_enabled);
	DoDisableCheckBox(ui.lowPassFilter, options.emulator_configuration.general.low_pass_filter_disabled);
	DoDisableCheckBox(ui.lowVolumeDistortion, options.emulator_configuration.fm.ladder_effect_disabled);

	DoRadioButton(ui.controlPad1, options.keyboard_control_pad, 0u);
	DoRadioButton(ui.controlPad2, options.keyboard_control_pad, 1u);
}

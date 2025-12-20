#include "options.h"

Dialogs::Options::Options(Emulator::Configuration &configuration, QWidget* const parent)
	: QDialog(parent)
	, configuration(configuration)
{
	ui.setupUi(this);

	const auto &DoRadioButton = [this]<typename T>(QRadioButton* const radiobutton, T &variable, const T value)
	{
		radiobutton->setChecked(variable == value);
		connect(radiobutton, &QRadioButton::toggled, this, [&variable, value](const bool enabled){ if (enabled) variable = value; });
	};

	DoRadioButton(ui.ntsc, configuration.general.tv_standard, CLOWNMDEMU_TV_STANDARD_NTSC);
	DoRadioButton(ui.pal, configuration.general.tv_standard, CLOWNMDEMU_TV_STANDARD_PAL);

	DoRadioButton(ui.japan, configuration.general.region, CLOWNMDEMU_REGION_DOMESTIC);
	DoRadioButton(ui.elsewhere, configuration.general.region, CLOWNMDEMU_REGION_OVERSEAS);

	const auto &DoEnabledCheckBox = [this](QCheckBox* const checkbox, cc_bool &boolean)
	{
		checkbox->setChecked(boolean);
		connect(checkbox, &QCheckBox::checkStateChanged, this, [&boolean](const bool enabled){boolean = enabled;});
	};

	const auto &DoDisableCheckBox = [this](QCheckBox* const checkbox, cc_bool &boolean)
	{
		checkbox->setChecked(!boolean);
		connect(checkbox, &QCheckBox::checkStateChanged, this, [&](const bool enabled){boolean = !enabled;});
	};

	DoEnabledCheckBox(ui.cdAddon, configuration.general.cd_add_on_enabled);
	DoEnabledCheckBox(ui.widescreenHack, configuration.vdp.widescreen_enabled);
	DoDisableCheckBox(ui.lowPassFilter, configuration.general.low_pass_filter_disabled);
	DoDisableCheckBox(ui.lowVolumeDistortion, configuration.fm.ladder_effect_disabled);
}

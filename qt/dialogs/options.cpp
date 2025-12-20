#include "options.h"

#include "common.h"

Dialogs::Options::Options(::Options &options, QWidget* const parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	Common::BindRadioButton(this, ui.ntsc, options.emulator_configuration.general.tv_standard, CLOWNMDEMU_TV_STANDARD_NTSC);
	Common::BindRadioButton(this, ui.pal, options.emulator_configuration.general.tv_standard, CLOWNMDEMU_TV_STANDARD_PAL);

	Common::BindRadioButton(this, ui.japan, options.emulator_configuration.general.region, CLOWNMDEMU_REGION_DOMESTIC);
	Common::BindRadioButton(this, ui.elsewhere, options.emulator_configuration.general.region, CLOWNMDEMU_REGION_OVERSEAS);

	Common::BindCheckBox(this, ui.cdAddon, options.emulator_configuration.general.cd_add_on_enabled);
	Common::BindCheckBox(this, ui.integerScreenScaling, options.integer_screen_scaling);
	connect(ui.integerScreenScaling, &QCheckBox::checkStateChanged, this, &Options::presentationOptionChanged);
	Common::BindCheckBox(this, ui.tallInterlaceMode2, options.tall_interlace_mode_2);
	connect(ui.tallInterlaceMode2, &QCheckBox::checkStateChanged, this, &Options::presentationOptionChanged);
	Common::BindCheckBox(this, ui.widescreenHack, options.emulator_configuration.vdp.widescreen_enabled);
	Common::BindInverseCheckBox(this, ui.lowPassFilter, options.emulator_configuration.general.low_pass_filter_disabled);
	Common::BindInverseCheckBox(this, ui.lowVolumeDistortion, options.emulator_configuration.fm.ladder_effect_disabled);

	Common::BindRadioButton(this, ui.controlPad1, options.keyboard_control_pad, 0u);
	Common::BindRadioButton(this, ui.controlPad2, options.keyboard_control_pad, 1u);
}

#include "options.h"

#include "common.h"

Dialogs::Options::Options(::Options &options, QWidget* const parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	// Console
	DIALOGS_COMMON_BIND_RADIO_BUTTON(ui.ntsc, options, TVStandard, CLOWNMDEMU_TV_STANDARD_NTSC);
	DIALOGS_COMMON_BIND_RADIO_BUTTON(ui.pal, options, TVStandard, CLOWNMDEMU_TV_STANDARD_PAL);

	DIALOGS_COMMON_BIND_RADIO_BUTTON(ui.japan, options, Region, CLOWNMDEMU_REGION_DOMESTIC);
	DIALOGS_COMMON_BIND_RADIO_BUTTON(ui.elsewhere, options, Region, CLOWNMDEMU_REGION_OVERSEAS);

	DIALOGS_COMMON_BIND_CHECK_BOX(ui.cdAddon, options, CDAddOnEnabled);

	// Video
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.integerScreenScaling, options, IntegerScreenScalingEnabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.tallInterlaceMode2, options, TallInterlaceMode2Enabled);
	DIALOGS_COMMON_BIND_SLIDER(ui.widescreenSlider, options, WidescreenTilePairs);

	// Audio
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.lowPassFilter, options, LowPassFilterEnabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.lowVolumeDistortion, options, LadderEffectEnabled);

	// Miscellaneous
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.rewinding, options, RewindingEnabled);

	// Keyboard Input
	DIALOGS_COMMON_BIND_RADIO_BUTTON(ui.controlPad1, options, KeyboardControlPad, 0);
	DIALOGS_COMMON_BIND_RADIO_BUTTON(ui.controlPad2, options, KeyboardControlPad, 1);
}

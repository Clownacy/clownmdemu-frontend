#include "debug-toggles.h"

#include "common.h"

Dialogs::Debug::Toggles::Toggles(Options &options, QWidget *parent) :
	QDialog(parent)
{
	ui.setupUi(this);

	// VDP
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.spritePlane, options, SpritePlaneEnabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.windowPlane, options, WindowPlaneEnabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.scrollPlaneA, options, ScrollPlaneAEnabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.scrollPlaneB, options, ScrollPlaneBEnabled);

	// FM
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.fm1, options, FM1Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.fm2, options, FM2Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.fm3, options, FM3Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.fm4, options, FM4Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.fm5, options, FM5Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.fm6, options, FM6Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.dac, options, DACEnabled);

	// PSG
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.psg1, options, PSG1Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.psg2, options, PSG1Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.psg3, options, PSG1Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.psgNoise, options, PSGNoiseEnabled);

	// PCM
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.pcm1, options, PCM1Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.pcm2, options, PCM2Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.pcm3, options, PCM3Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.pcm4, options, PCM4Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.pcm5, options, PCM5Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.pcm6, options, PCM6Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.pcm7, options, PCM7Enabled);
	DIALOGS_COMMON_BIND_CHECK_BOX(ui.pcm8, options, PCM8Enabled);
}

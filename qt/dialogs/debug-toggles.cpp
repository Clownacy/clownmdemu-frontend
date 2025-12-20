#include "debug-toggles.h"

#include "common.h"

Dialogs::Debug::Toggles::Toggles(Options &options, QWidget *parent) :
	QDialog(parent)
{
	ui.setupUi(this);

	// VDP
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.spritePlane, options, SpritesDisabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.windowPlane, options, WindowPlaneDisabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.scrollPlaneA, options, ScrollPlaneADisabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.scrollPlaneB, options, ScrollPlaneBDisabled);

	// FM
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.fm1, options, FM1Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.fm2, options, FM2Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.fm3, options, FM3Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.fm4, options, FM4Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.fm5, options, FM5Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.fm6, options, FM6Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.dac, options, DACDisabled);

	// PSG
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.psg1, options, PSG1Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.psg2, options, PSG1Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.psg3, options, PSG1Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.psgNoise, options, PSGNoiseDisabled);

	// PCM
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.pcm1, options, PCM1Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.pcm2, options, PCM2Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.pcm3, options, PCM3Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.pcm4, options, PCM4Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.pcm5, options, PCM5Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.pcm6, options, PCM6Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.pcm7, options, PCM7Disabled);
	DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(ui.pcm8, options, PCM8Disabled);
}

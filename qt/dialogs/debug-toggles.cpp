#include "debug-toggles.h"

#include "common.h"

Dialogs::Debug::Toggles::Toggles(Emulator::Configuration &configuration, QWidget *parent) :
	QDialog(parent)
{
	ui.setupUi(this);

	// VDP
	Common::BindInverseCheckBox(this, ui.spritePlane, configuration.vdp.sprites_disabled);
	Common::BindInverseCheckBox(this, ui.windowPlane, configuration.vdp.window_disabled);
	Common::BindInverseCheckBox(this, ui.scrollPlaneA, configuration.vdp.planes_disabled[0]);
	Common::BindInverseCheckBox(this, ui.scrollPlaneB, configuration.vdp.planes_disabled[1]);

	// FM
	Common::BindInverseCheckBox(this, ui.fm1, configuration.fm.fm_channels_disabled[0]);
	Common::BindInverseCheckBox(this, ui.fm2, configuration.fm.fm_channels_disabled[1]);
	Common::BindInverseCheckBox(this, ui.fm3, configuration.fm.fm_channels_disabled[2]);
	Common::BindInverseCheckBox(this, ui.fm4, configuration.fm.fm_channels_disabled[3]);
	Common::BindInverseCheckBox(this, ui.fm5, configuration.fm.fm_channels_disabled[4]);
	Common::BindInverseCheckBox(this, ui.fm6, configuration.fm.fm_channels_disabled[5]);
	Common::BindInverseCheckBox(this, ui.dac, configuration.fm.dac_channel_disabled);

	// PSG
	Common::BindInverseCheckBox(this, ui.psg1, configuration.psg.tone_disabled[0]);
	Common::BindInverseCheckBox(this, ui.psg2, configuration.psg.tone_disabled[1]);
	Common::BindInverseCheckBox(this, ui.psg3, configuration.psg.tone_disabled[2]);
	Common::BindInverseCheckBox(this, ui.psgNoise, configuration.psg.noise_disabled);

	// PCM
	Common::BindInverseCheckBox(this, ui.pcm1, configuration.pcm.channels_disabled[0]);
	Common::BindInverseCheckBox(this, ui.pcm2, configuration.pcm.channels_disabled[1]);
	Common::BindInverseCheckBox(this, ui.pcm3, configuration.pcm.channels_disabled[2]);
	Common::BindInverseCheckBox(this, ui.pcm4, configuration.pcm.channels_disabled[3]);
	Common::BindInverseCheckBox(this, ui.pcm5, configuration.pcm.channels_disabled[4]);
	Common::BindInverseCheckBox(this, ui.pcm6, configuration.pcm.channels_disabled[5]);
	Common::BindInverseCheckBox(this, ui.pcm7, configuration.pcm.channels_disabled[6]);
	Common::BindInverseCheckBox(this, ui.pcm8, configuration.pcm.channels_disabled[7]);
}

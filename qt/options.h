#ifndef OPTIONS_H
#define OPTIONS_H

#include "emulator.h"

struct Options
{
	Emulator::Configuration emulator_configuration = {};
	unsigned int keyboard_control_pad = 0;
	bool tall_interlace_mode_2 = false;
};

#endif // OPTIONS_H

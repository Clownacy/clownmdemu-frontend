#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "window-popup.h"

class Disassembler : public WindowPopup
{
public:
	using WindowPopup::WindowPopup;

	bool Display();
};

#endif /* DISASSEMBLER_H */

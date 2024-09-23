#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "window-popup.h"

class Disassembler : public WindowPopup<Disassembler>
{
private:
	using Base = WindowPopup<Disassembler>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal();

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DISASSEMBLER_H */

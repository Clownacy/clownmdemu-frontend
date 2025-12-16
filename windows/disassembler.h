#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <string>

#include "common/window-popup.h"

class Disassembler : public WindowPopup<Disassembler>
{
private:
	using Base = WindowPopup<Disassembler>;

	static constexpr Uint32 window_flags = 0;

	int address;
	int current_memory;
	std::string assembly;

	void DisplayInternal();

public:
	using Base::WindowPopup;

	friend Base;

	long ReadCallback();
	void PrintCallback(const char *string);
};

#endif /* DISASSEMBLER_H */

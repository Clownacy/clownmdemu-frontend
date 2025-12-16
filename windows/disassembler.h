#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <string>

#include "common/window-popup.h"

class Disassembler : public WindowPopup<Disassembler>
{
private:
	using Base = WindowPopup<Disassembler>;

	static constexpr Uint32 window_flags = 0;

	int address_imgui;
	unsigned long address;
	int current_cpu, current_memory;
	std::string assembly;

	void DisplayInternal();

protected:
	cc_u16f ReadMemory();

	cc_u16f ReadCallback16Bit();
	cc_u8f ReadCallback8Bit();
	void PrintCallback(const char *string);

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DISASSEMBLER_H */

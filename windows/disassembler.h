#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <string>

#include "../common/core/clowncommon/clowncommon.h"

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

	void PrintCallback(const char *string);
	static long ReadCallback68000(void *user_data);
	static void PrintCallback68000(void *user_data, const char *string);
	static unsigned char ReadCallbackZ80(void *user_data);
	static void PrintCallbackZ80(void *user_data, const char *format, ...);

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DISASSEMBLER_H */

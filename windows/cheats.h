#ifndef CHEATS_H
#define CHEATS_H

#include <list>
#include <string>

#include "../common/core/clowncommon/clowncommon.h"

#include "../emulator-instance.h"
#include "common/window-popup.h"

class Cheats : public WindowPopup<Cheats>
{
private:
	using Base = WindowPopup<Cheats>;

	struct CodeSlot
	{
		enum class Status
		{
			Invalid,
			Error,
			Pending,
			Applied,
		};

		std::string code;
		Cheat_DecodedCheat decoded_cheat;
		Status status;

		CodeSlot(std::string code);
	};

	static constexpr Uint32 window_flags = 0;

	std::list<CodeSlot> codes;

	void DisplayInternal(EmulatorInstance &emulator);

public:
	using Base::WindowPopup;

	friend Base;

	void CartridgeChanged();
};

#endif /* CHEATS_H */

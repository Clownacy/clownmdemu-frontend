#ifndef EMULATOR_H
#define EMULATOR_H

#include "clownmdemu.h"

template<typename Derived>
class Emulator : public ClownMDEmuCPP::Base<Derived>
{
public:
	using Base = ClownMDEmuCPP::Base<Derived>;
	using Base::Base;
};

#endif // EMULATOR_H

#ifndef COLOUR_H
#define COLOUR_H

#include "sdl-wrapper.h"

class Colour
{
public:
	using Type = SDL::Pixel;

private:
	Type value = 0;

	static Type UnpackColour(const unsigned int packed_colour)
	{
		// Decompose XBGR4444 into individual colour channels
		const cc_u32f red = (packed_colour >> 4 * 0) & 0xF;
		const cc_u32f green = (packed_colour >> 4 * 1) & 0xF;
		const cc_u32f blue = (packed_colour >> 4 * 2) & 0xF;

		// Reassemble into ARGB8888
		return static_cast<Uint32>(0xFF000000 | (blue << 4 * 0) | (blue << 4 * 1) | (green << 4 * 2) | (green << 4 * 3) | (red << 4 * 4) | (red << 4 * 5));
	}

public:
	Colour()
	{}

	Colour(const unsigned int packed_colour)
		: value(UnpackColour(packed_colour))
	{}

	operator Type() const
	{
		return value;
	}
};


#endif // COLOUR_H

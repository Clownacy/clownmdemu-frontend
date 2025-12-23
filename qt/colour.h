#ifndef COLOUR_H
#define COLOUR_H

#include <QOpenGLWidget>

class Colour
{
public:
	using Type = GLushort;

private:
	Type value = 0;

	static Type UnpackColour(const unsigned int packed_colour)
	{
		const auto &Extract4BitChannelTo6Bit = [&](const unsigned int channel_index)
		{
			const cc_u8f channel = (packed_colour >> (channel_index * 4)) & 0xF;
			return channel << 2 | channel >> 2;
		};

		// Unpack from XBGR4444.
		const auto red   = Extract4BitChannelTo6Bit(0) >> 1;
		const auto green = Extract4BitChannelTo6Bit(1);
		const auto blue  = Extract4BitChannelTo6Bit(2) >> 1;

		// Pack into RGB565.
		return
			(red   << (0 + 6 + 5)) |
			(green << (0 + 0 + 5)) |
			(blue  << (0 + 0 + 0));
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

#ifndef DEBUG_FRONTEND_H
#define DEBUG_FRONTEND_H

#include "audio-output.h"
#include "window.h"

class DebugFrontend
{
private:
	const AudioOutput &audio_output;
	const Window &window;

public:
	DebugFrontend(const AudioOutput &audio_output, const Window &window) : audio_output(audio_output), window(window) {}
	void Display(bool &open);
};

#endif /* DEBUG_FRONTEND_H */

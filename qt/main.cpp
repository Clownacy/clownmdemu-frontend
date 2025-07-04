#include "main-window.h"

#include <QApplication>
#include <QSurfaceFormat>

#include <SDL3/SDL.h>

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_AUDIO);

	const auto result = [&](){
		// Let Qt decide our OpenGL version, since our renderer can adapt.
		QSurfaceFormat format;
	#ifndef NDEBUG
		format.setOption(QSurfaceFormat::DebugContext);
	#endif
		format.setProfile(QSurfaceFormat::NoProfile);
		format.setRenderableType(QSurfaceFormat::DefaultRenderableType);
		format.setSwapBehavior(QSurfaceFormat::SwapBehavior::DoubleBuffer);
		format.setSwapInterval(1);

		QSurfaceFormat::setDefaultFormat(format);
		QApplication a(argc, argv);
		MainWindow w;
		w.show();
		return a.exec();
	}();

	SDL_Quit();
	return result;
}

#include "main-window.h"

#include <QApplication>
#include <QSurfaceFormat>

#include <SDL3/SDL.h>

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_AUDIO);

	const auto result = [&](){
		// Set some properties which will be useful for saving settings with QSettings.
		// The version is used for displaying the version in the About menu.
		QApplication::setApplicationName("ClownMDEmu");
		QApplication::setApplicationVersion("1.5");
		QApplication::setOrganizationDomain("clownacy.com");
		QApplication::setOrganizationName("clownacy");

		// Let Qt decide our OpenGL version, since our renderer can adapt.
		QSurfaceFormat format;
	#ifndef NDEBUG
		format.setOption(QSurfaceFormat::DebugContext);
	#endif
		format.setProfile(QSurfaceFormat::NoProfile);
		format.setRenderableType(QSurfaceFormat::DefaultRenderableType);
		format.setSwapBehavior(QSurfaceFormat::SwapBehavior::DoubleBuffer);
		format.setSwapInterval(1); // TODO: Allow V-sync to be disabled?
		QSurfaceFormat::setDefaultFormat(format);

		QApplication a(argc, argv);
		MainWindow w;
		w.show();
		return a.exec();
	}();

	SDL_Quit();
	return result;
}

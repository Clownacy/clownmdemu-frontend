#include "main-window.h"

#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char *argv[])
{
	QSurfaceFormat format;
#if 1
#ifndef NDEBUG
	format.setOption(QSurfaceFormat::DebugContext);
#endif
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setRenderableType(QSurfaceFormat::OpenGLES);
	format.setSwapBehavior(QSurfaceFormat::SwapBehavior::DoubleBuffer);
	format.setSwapInterval(1);
	format.setVersion(2, 0);
#endif
	QSurfaceFormat::setDefaultFormat(format);
	QApplication a(argc, argv);
	MainWindow w;
	w.show();
	return a.exec();
}

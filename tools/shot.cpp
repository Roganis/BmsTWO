// Offscreen screenshot harness: builds the real MainWindow (App ctor applies
// the theme, opens any file passed in argv) and grabs it to a PNG so cosmetic
// changes can be reviewed without a display. Not part of the app build.
#include "MainWindow.h"
#include <QApplication>
#include <QPixmap>
#include <QString>
#include <cstdio>

int main(int argc, char **argv)
{
	App app(argc, argv); // applies Theme, creates + shows mainWindow, opens argv files
	QCoreApplication::processEvents(QEventLoop::AllEvents, 300);
	MainWindow *mw = app.GetMainWindow();
	if (!mw){ fprintf(stderr, "no mainwindow\n"); return 1; }
	mw->resize(1280, 800);
	for (int i = 0; i < 8; i++) QCoreApplication::processEvents(QEventLoop::AllEvents, 80);
	const QString out = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QStringLiteral("/tmp/bmstwo_shot.png");
	QPixmap pm = mw->grab();
	if (!pm.save(out)){ fprintf(stderr, "save failed\n"); return 1; }
	fprintf(stderr, "saved %s (%dx%d)\n", qPrintable(out), pm.width(), pm.height());
	return 0;
}

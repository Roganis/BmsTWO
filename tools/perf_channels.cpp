// Headless perf probe for high channel counts. Uses the public OpenFiles()
// (sound files -> ChannelsNew -> InsertNewSoundChannels on the live document),
// so it exercises the real per-channel view relayout. Not part of the build.
#include "MainWindow.h"
#include <QApplication>
#include <QElapsedTimer>
#include <cstdio>

static MainWindow *findMainWindow(){
    for (QWidget *w : QApplication::topLevelWidgets())
        if (auto *mw = qobject_cast<MainWindow*>(w)) return mw;
    return nullptr;
}

int main(int argc, char **argv){
    QCoreApplication::setApplicationName("BmsTWOPerf");
    App app(argc, argv);
    MainWindow *mw = findMainWindow();
    if (!mw){ fprintf(stderr, "no MainWindow\n"); return 1; }
    QApplication::processEvents();

    int total = 0;
    for (int round = 0; round < 5; round++){
        const int batch = 100;
        QStringList paths;
        for (int i = 0; i < batch; i++)
            paths << QString("r%1_ch%2.ogg").arg(round).arg(i);
        QElapsedTimer t; t.start();
        mw->OpenFiles(paths);
        QApplication::processEvents();
        total += batch;
        fprintf(stderr, "add %d (total %4d channels): %5lld ms\n", batch, total, (long long)t.elapsed());
    }
    fprintf(stderr, "DONE\n");
    return 0;
}

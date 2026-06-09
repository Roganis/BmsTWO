// Offscreen screenshot harness: builds the real MainWindow (App ctor applies
// the theme) and grabs it to a PNG so cosmetic changes can be reviewed without
// a display. Not part of the app build.
//
// Env vars:
//   SHOT_OUT   output PNG path (default /tmp/bmstwo_shot.png)
//   SHOT_DEMO  if set, populate a synthetic chart (channels + key-lane notes,
//              incl. long notes) so note styling is visible.
#include "MainWindow.h"
#include "sequence_view/SequenceView.h"
#include "document/Document.h"
#include "document/SoundChannel.h"
#include <QApplication>
#include <QPixmap>
#include <QString>
#include <cstdio>
#include <cstdlib>

static void pump() { QCoreApplication::processEvents(QEventLoop::AllEvents, 80); }

int main(int argc, char **argv)
{
	App app(argc, argv); // applies Theme, creates + shows mainWindow
	pump();
	MainWindow *mw = app.GetMainWindow();
	if (!mw){ fprintf(stderr, "no mainwindow\n"); return 1; }
	mw->resize(1280, 800);

	if (qEnvironmentVariableIsSet("SHOT_DEMO")){
		auto *doc = new Document();
		doc->Initialize();
		doc->InsertNewSoundChannels(QList<QString>() << "k1.ogg" << "k2.ogg" << "k3.ogg" << "k4.ogg" << "k5.ogg");
		pump();
		const auto chs = doc->GetSoundChannels();
		QMultiMap<SoundChannel*, SoundNote> notes;
		// Channel 0: spread across the key lanes (visible in the playing pane
		// when current); other channels get notes so their columns show their
		// categorical color.
		for (int c = 0; c < chs.size(); c++){
			SoundChannel *ch = chs[c];
			for (int i = 0; i < 24; i++){
				int loc = i * 60 + c * 15;
				int lane = c == 0 ? (i % 5) + 1 : 0; // ch0 → key lanes, others → BGM column
				int len = (c == 0 && i % 5 == 0) ? 180 : 0;
				notes.insert(ch, SoundNote(loc, lane, len, 0));
			}
		}
		doc->MultiChannelUpdateSoundNotes(notes);
		auto *sv = mw->GetActiveSequenceView();
		sv->ReplaceDocument(doc);
		sv->OnCurrentChannelChanged(0); // make the channel current so its key-lane notes render
	}

	for (int i = 0; i < 10; i++) pump();
	const QString out = qEnvironmentVariable("SHOT_OUT", QStringLiteral("/tmp/bmstwo_shot.png"));
	QPixmap pm = mw->grab();
	if (!pm.save(out)){ fprintf(stderr, "save failed\n"); return 1; }
	fprintf(stderr, "saved %s (%dx%d)\n", qPrintable(out), pm.width(), pm.height());
	return 0;
}

// Headless harness to exercise Document / EditHistory edit + undo/redo paths
// under AddressSanitizer/UBSan (issue #4 clusters: mass BPM edits, undo/redo
// storms, channel edits). Not part of the app build; see tools/run_fuzz_*.sh.
#include "document/Document.h"
#include "document/SoundChannel.h"
#include "document/History.h"
#include "document/DocumentDef.h"
#include "MainWindow.h"
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <cstdio>

static void pump() { QCoreApplication::processEvents(QEventLoop::AllEvents, 50); }

static void undoAll(EditHistory *h) {
    int guard = 0;
    while (h->CanUndo() && ++guard < 1000000) { h->Undo(); }
}
static void redoAll(EditHistory *h) {
    int guard = 0;
    while (h->CanRedo() && ++guard < 1000000) { h->Redo(); }
}
static void storm(EditHistory *h, int rounds) {
    for (int i = 0; i < rounds; i++) { undoAll(h); redoAll(h); }
    undoAll(h); // leave clean-ish
    redoAll(h);
}

int main(int argc, char **argv) {
    QCoreApplication::setApplicationName("BmsTWOFuzz");
    App app(argc, argv);

    fprintf(stderr, "=== case: bpm-mass-edit ===\n");
    {
        Document doc;
        doc.Initialize();
        EditHistory *h = doc.GetHistory();
        for (int i = 1; i <= 800; i++)
            doc.InsertBpmEvent(BpmEvent(i * 8, 60.0 + (i % 240)));
        // mass update
        QList<BpmEvent> ups;
        for (int i = 1; i <= 800; i += 2) ups.append(BpmEvent(i * 8, 150.0 + (i % 90)));
        doc.UpdateBpmEvents(ups);
        // mass remove
        QList<int> rem;
        for (int i = 2; i <= 800; i += 3) rem.append(i * 8);
        doc.RemoveBpmEvents(rem);
        storm(h, 3);
        pump();
    }

    fprintf(stderr, "=== case: barlines ===\n");
    {
        Document doc;
        doc.Initialize();
        EditHistory *h = doc.GetHistory();
        for (int i = 1; i <= 500; i++) doc.InsertBarLine(BarLine(i * 240, i % 3));
        for (int i = 1; i <= 500; i += 2) doc.RemoveBarLine(i * 240);
        storm(h, 3);
        pump();
    }

    fprintf(stderr, "=== case: channels + notes ===\n");
    {
        Document doc;
        doc.Initialize();
        EditHistory *h = doc.GetHistory();
        doc.InsertNewSoundChannels(QList<QString>() << "a.ogg" << "b.ogg" << "c.ogg");
        pump();
        const auto chs = doc.GetSoundChannels();
        fprintf(stderr, "  channels: %d\n", (int)chs.size());
        if (!chs.isEmpty()) {
            QMultiMap<SoundChannel*, SoundNote> notes;
            for (auto *ch : chs)
                for (int i = 0; i < 300; i++)
                    notes.insert(ch, SoundNote(i * 16, (i % 8) + 1, 0, 0));
            doc.MultiChannelUpdateSoundNotes(notes);
            pump();
            // delete a subset
            QMultiMap<SoundChannel*, SoundNote> del;
            for (auto *ch : chs)
                for (int i = 0; i < 300; i += 2)
                    del.insert(ch, SoundNote(i * 16, (i % 8) + 1, 0, 0));
            doc.MultiChannelDeleteSoundNotes(del);
            pump();
        }
        if (chs.size() >= 2) doc.MoveSoundChannel(0, chs.size() - 1);
        if (!doc.GetSoundChannels().isEmpty()) doc.DestroySoundChannel(0);
        storm(h, 3);
        pump();
    }

    fprintf(stderr, "=== case: interleaved storm ===\n");
    {
        Document doc;
        doc.Initialize();
        EditHistory *h = doc.GetHistory();
        for (int i = 1; i <= 200; i++) {
            doc.InsertBpmEvent(BpmEvent(i * 12, 100.0 + i % 50));
            doc.InsertBarLine(BarLine(i * 96, i % 2));
            if (i % 25 == 0) { undoAll(h); redoAll(h); }
        }
        doc.InsertNewSoundChannels(QList<QString>() << "x.ogg");
        pump();
        storm(h, 5);
        pump();
    }

    fprintf(stderr, "=== case: resolution-convert + modal-edit ===\n");
    {
        Document doc;
        doc.Initialize();
        EditHistory *h = doc.GetHistory();
        for (int i = 1; i <= 200; i++)
            doc.InsertBpmEvent(BpmEvent(i * 8, 120.0 + i % 40));
        doc.InsertNewSoundChannels(QList<QString>() << "a.ogg" << "b.ogg");
        pump();
        const auto chs = doc.GetSoundChannels();
        if (!chs.isEmpty()) {
            QMultiMap<SoundChannel*, SoundNote> notes;
            for (auto *ch : chs)
                for (int i = 0; i < 200; i++)
                    notes.insert(ch, SoundNote(i * 20, (i % 7) + 1, 0, 0));
            doc.MultiChannelUpdateSoundNotes(notes);
            pump();
            // modal edit context
            QMap<SoundChannel*, QSet<int>> locs;
            for (auto *ch : chs) { QSet<int> s; for (int i = 0; i < 50; i++) s.insert(i * 20); locs.insert(ch, s); }
            auto *ctx = doc.BeginModalEditSoundNotes(locs);
            if (ctx) {
                QMap<SoundChannel*, QMap<int, SoundNote>> upd;
                for (auto *ch : chs) { QMap<int, SoundNote> m; for (int i = 0; i < 50; i++) m.insert(i*20, SoundNote(i*20+4, (i%7)+1, 0, 0)); upd.insert(ch, m); }
                ctx->Update(upd);
                ctx->Finish();
                pump();
            }
        }
        // resolution conversion (known tricky: rescales every event/note)
        doc.ConvertResolution(480);
        pump();
        doc.ConvertResolution(240);
        pump();
        storm(h, 3);
        pump();
    }

    fprintf(stderr, "ALL CASES DONE\n");
    return 0;
}

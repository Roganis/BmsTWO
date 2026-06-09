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
                delete ctx; // caller owns the context
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

    fprintf(stderr, "=== case: ln up-field round-trip ===\n");
    {
        // `up` must survive a bmson round-trip, and must NOT be written when false.
        SoundNote n(480, 0, 240, 0, true);
        SoundNote n2(n.AsJson());
        if (!(n2.up && n2.length == 240 && n2.location == 480)) { fprintf(stderr, "FAIL: up round-trip\n"); return 1; }
        SoundNote m(480, 0, 240, 0, false);
        if (m.AsJson().contains("up")) { fprintf(stderr, "FAIL: up written when false\n"); return 1; }
        if (!(SoundNote(m.AsJson()).up == false)) { fprintf(stderr, "FAIL: up default\n"); return 1; }
    }

    fprintf(stderr, "=== case: stop events ===\n");
    {
        Document doc;
        doc.Initialize();
        EditHistory *h = doc.GetHistory();
        for (int i = 1; i <= 300; i++)
            doc.InsertStopEvent(StopEvent(i * 48, 24.0 + (i % 96)));
        if (doc.GetStopEvents().size() != 300) { fprintf(stderr, "FAIL stop count %d\n", (int)doc.GetStopEvents().size()); return 1; }
        // update path (same location, new duration)
        for (int i = 1; i <= 300; i += 2)
            doc.InsertStopEvent(StopEvent(i * 48, 200.0));
        // identical re-insert must be a no-op
        auto ev = doc.GetStopEvents().first();
        if (doc.InsertStopEvent(ev)) { fprintf(stderr, "FAIL: identical stop re-insert not a no-op\n"); return 1; }
        // remove subset
        for (int i = 2; i <= 300; i += 3) doc.RemoveStopEvent(i * 48);
        storm(h, 3);
        pump();
        undoAll(h);
        if (!doc.GetStopEvents().isEmpty()) { fprintf(stderr, "FAIL: stops not empty after full undo\n"); return 1; }
        redoAll(h);
        pump();
    }

    fprintf(stderr, "=== case: bga headers + timeline ===\n");
    {
        Document doc;
        doc.Initialize();
        EditHistory *h = doc.GetHistory();
        // headers
        for (int i = 0; i < 64; i++)
            doc.InsertBgaHeader(BgaHeader(i, QString("bmp%1.png").arg(i)));
        // re-edit some headers (update path)
        for (int i = 0; i < 64; i += 4)
            doc.InsertBgaHeader(BgaHeader(i, QString("changed%1.png").arg(i)));
        // events across all three lanes
        for (int i = 0; i < 300; i++) {
            doc.InsertBgaEvent(BgaLayer::Base,  BgaEvent(i * 24, i % 64));
            doc.InsertBgaEvent(BgaLayer::Layer, BgaEvent(i * 24 + 6, (i + 1) % 64));
            doc.InsertBgaEvent(BgaLayer::Poor,  BgaEvent(i * 24 + 12, (i + 2) % 64));
        }
        // verify state
        if (doc.GetBga().headers.size() != 64) { fprintf(stderr, "FAIL header count %d\n", doc.GetBga().headers.size()); return 1; }
        if (doc.GetBgaEvents(BgaLayer::Base).size() != 300) { fprintf(stderr, "FAIL base count\n"); return 1; }
        // remove a subset
        for (int i = 0; i < 300; i += 3) doc.RemoveBgaEvent(BgaLayer::Base, i * 24);
        for (int i = 0; i < 64; i += 5)  doc.RemoveBgaHeader(i);
        // dedup: re-inserting identical event must be a no-op
        if (!doc.GetBgaEvents(BgaLayer::Layer).isEmpty()) {
            auto ev = doc.GetBgaEvents(BgaLayer::Layer).first();
            bool changed = doc.InsertBgaEvent(BgaLayer::Layer, ev);
            if (changed) { fprintf(stderr, "FAIL: identical re-insert was not a no-op\n"); return 1; }
        }
        storm(h, 3);
        pump();
        // after a full undo we should be back to an empty BGA
        undoAll(h);
        if (!doc.GetBga().headers.isEmpty() || !doc.GetBgaEvents(BgaLayer::Base).isEmpty()
            || !doc.GetBgaEvents(BgaLayer::Layer).isEmpty() || !doc.GetBgaEvents(BgaLayer::Poor).isEmpty()) {
            fprintf(stderr, "FAIL: BGA not empty after full undo\n"); return 1;
        }
        redoAll(h);
        pump();
    }

    fprintf(stderr, "ALL CASES DONE\n");
    return 0;
}

// Headless harness to exercise Document / EditHistory edit + undo/redo paths
// under AddressSanitizer/UBSan (issue #4 clusters: mass BPM edits, undo/redo
// storms, channel edits). Not part of the app build; see tools/run_fuzz_*.sh.
#include "document/Document.h"
#include "document/SoundChannel.h"
#include "document/History.h"
#include "document/DocumentDef.h"
#include "MainWindow.h"
#include "util/ShortcutManager.h"
#include "util/SampleGrouping.h"
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QColor>
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

    fprintf(stderr, "=== case: stop affects absolute time ===\n");
    {
        auto absd = [](double x){ return x < 0 ? -x : x; };
        Document doc;
        doc.Initialize();
        doc.GetInfo()->SetInitBpm(120.0);
        const int R = doc.GetInfo()->GetResolution();
        const double spp = 60.0 / (120.0 * R); // seconds per pulse
        // before any stop, time is linear in ticks
        if (absd(doc.GetAbsoluteTime(2*R) - 2*R*spp) > 1e-9){ fprintf(stderr, "FAIL: linear time\n"); return 1; }
        // a STOP of R pulses at location R adds R pulses' worth of time afterwards
        doc.InsertStopEvent(StopEvent(R, (double)R));
        if (absd(doc.GetAbsoluteTime(2*R) - 3*R*spp) > 1e-9){ fprintf(stderr, "FAIL: stop not added\n"); return 1; }
        // arriving exactly at the stop location excludes its own pause
        if (absd(doc.GetAbsoluteTime(R) - R*spp) > 1e-9){ fprintf(stderr, "FAIL: stop counted at its own loc\n"); return 1; }
        // inverse round-trips, and a time inside the pause maps back to the stop tick
        if (absd(doc.FromAbsoluteTime(doc.GetAbsoluteTime(2*R)) - 2*R) > 1){ fprintf(stderr, "FAIL: inverse\n"); return 1; }
        if (doc.FromAbsoluteTime(R*spp + 0.5*R*spp) != R){ fprintf(stderr, "FAIL: time-in-stop maps to stop tick\n"); return 1; }
        // undo removes the stop and restores linear time
        doc.GetHistory()->Undo();
        if (absd(doc.GetAbsoluteTime(2*R) - 2*R*spp) > 1e-9){ fprintf(stderr, "FAIL: undo stop\n"); return 1; }
    }

    fprintf(stderr, "=== case: recovery snapshot round-trip (ExportTo -> LoadFile) ===\n");
    {
        // This is the core of crash recovery: ExportTo writes a snapshot without
        // touching save state, and a fresh Document must load it back intact.
        Document doc;
        doc.Initialize();
        doc.GetInfo()->SetInitBpm(150.0);
        for (int i = 1; i <= 50; i++)
            doc.InsertBpmEvent(BpmEvent(i * 24, 120.0 + (i % 30)));
        for (int i = 1; i <= 20; i++)
            doc.InsertStopEvent(StopEvent(i * 96, 48.0));
        for (int i = 0; i < 8; i++)
            doc.InsertBgaHeader(BgaHeader(i, QString("img%1.png").arg(i)));
        doc.InsertNewSoundChannels(QList<QString>() << "a.ogg" << "b.ogg");
        pump();
        const auto chs = doc.GetSoundChannels();
        QMultiMap<SoundChannel*, SoundNote> notes;
        for (auto *ch : chs)
            for (int i = 0; i < 30; i++)
                notes.insert(ch, SoundNote(i * 20, (i % 5) + 1, 0, 0));
        doc.MultiChannelUpdateSoundNotes(notes);
        pump();
        // per-channel custom color must round-trip through ExportTo/LoadFile
        if (!chs.isEmpty())
            chs.first()->SetCustomColor(QColor("#ff8800"));

        const QString snap = QDir(QDir::tempPath()).filePath("bmstwo_fuzz_recovery.bmson");
        QFile::remove(snap);
        bool exported = true;
        try { doc.ExportTo(snap); } catch (...) { exported = false; }
        if (!exported || !QFile::exists(snap)) { fprintf(stderr, "FAIL: ExportTo did not write a snapshot\n"); return 1; }
        // ExportTo must NOT clear the dirty flag (it's not a real save)
        if (!doc.GetHistory()->IsDirty()) { fprintf(stderr, "FAIL: ExportTo cleared dirty state\n"); return 1; }

        Document loaded;
        loaded.Initialize();
        bool ok = true;
        try { loaded.LoadFile(snap); } catch (...) { ok = false; }
        if (!ok) { fprintf(stderr, "FAIL: could not load snapshot back\n"); return 1; }
        pump();
        if (loaded.GetBpmEvents().size() != doc.GetBpmEvents().size()) { fprintf(stderr, "FAIL: bpm count mismatch\n"); return 1; }
        if (loaded.GetStopEvents().size() != doc.GetStopEvents().size()) { fprintf(stderr, "FAIL: stop count mismatch\n"); return 1; }
        if (loaded.GetBga().headers.size() != doc.GetBga().headers.size()) { fprintf(stderr, "FAIL: bga header count mismatch\n"); return 1; }
        if (loaded.GetSoundChannels().size() != doc.GetSoundChannels().size()) { fprintf(stderr, "FAIL: channel count mismatch\n"); return 1; }
        if (!loaded.GetSoundChannels().isEmpty()){
            QColor lc = loaded.GetSoundChannels().first()->GetCustomColor();
            if (!lc.isValid() || lc.name(QColor::HexRgb) != "#ff8800") { fprintf(stderr, "FAIL: custom channel color did not round-trip\n"); return 1; }
        }

        // SetRecoveredFilePath: adopt an original path and force dirty
        loaded.SetRecoveredFilePath("/some/original/path.bmson");
        if (loaded.GetFilePath() != "/some/original/path.bmson") { fprintf(stderr, "FAIL: recovered path not set\n"); return 1; }
        if (!loaded.GetHistory()->IsDirty()) { fprintf(stderr, "FAIL: recovered doc not dirty\n"); return 1; }
        QFile::remove(snap);
    }

    fprintf(stderr, "=== case: toggle long note (in-place length update) ===\n");
    {
        Document doc;
        doc.Initialize();
        const int R = doc.GetInfo()->GetResolution();
        doc.InsertNewSoundChannels(QList<QString>() << "a.ogg");
        pump();
        auto chs = doc.GetSoundChannels();
        if (chs.isEmpty()) { fprintf(stderr, "FAIL: no channel\n"); return 1; }
        SoundChannel *ch = chs.first();
        // sparse notes (2 beats apart) so extending by 1 beat never overlaps
        QMultiMap<SoundChannel*, SoundNote> notes;
        for (int i = 0; i < 10; i++) notes.insert(ch, SoundNote(i * 2 * R, 1, 0, 0));
        doc.MultiChannelUpdateSoundNotes(notes);
        pump();
        // toggle -> long notes (length = one beat)
        QMultiMap<SoundChannel*, SoundNote> upd;
        for (int i = 0; i < 10; i++) upd.insert(ch, SoundNote(i * 2 * R, 1, R, 0));
        doc.MultiChannelUpdateSoundNotes(upd);
        pump();
        if (ch->GetNotes().size() != 10) { fprintf(stderr, "FAIL: note count changed by LN toggle\n"); return 1; }
        for (auto it = ch->GetNotes().begin(); it != ch->GetNotes().end(); ++it)
            if (it.value().length != R) { fprintf(stderr, "FAIL: length not applied\n"); return 1; }
        // toggle back -> normal notes
        QMultiMap<SoundChannel*, SoundNote> upd2;
        for (int i = 0; i < 10; i++) upd2.insert(ch, SoundNote(i * 2 * R, 1, 0, 0));
        doc.MultiChannelUpdateSoundNotes(upd2);
        pump();
        for (auto it = ch->GetNotes().begin(); it != ch->GetNotes().end(); ++it)
            if (it.value().length != 0) { fprintf(stderr, "FAIL: length not reverted\n"); return 1; }
    }

    fprintf(stderr, "=== case: shortcut manager (register / rebind / conflict / reset) ===\n");
    {
        QMenuBar bar;
        QMenu *m = bar.addMenu("TestCat");
        QAction *a1 = m->addAction("Alpha");
        a1->setShortcut(QKeySequence("Ctrl+1"));
        QAction *a2 = m->addAction("Beta");
        a2->setShortcut(QKeySequence("Ctrl+2"));
        m->addSeparator();

        auto *sm = ShortcutManager::Instance();
        sm->RegisterMenuBar(&bar);
        bool foundAlpha = false, foundBeta = false;
        for (const auto &e : sm->Entries()){
            if (e.id == "TestCat/Alpha") foundAlpha = true;
            if (e.id == "TestCat/Beta")  foundBeta = true;
        }
        if (!foundAlpha || !foundBeta) { fprintf(stderr, "FAIL: entries not registered\n"); return 1; }
        // rebind applies live to the QAction
        sm->SetShortcut("TestCat/Alpha", QKeySequence("Ctrl+9"));
        if (a1->shortcut() != QKeySequence("Ctrl+9")) { fprintf(stderr, "FAIL: rebind not applied\n"); return 1; }
        // conflict detection
        sm->SetShortcut("TestCat/Beta", QKeySequence("Ctrl+9"));
        if (sm->Conflicts(QKeySequence("Ctrl+9"), "TestCat/Alpha").isEmpty()) { fprintf(stderr, "FAIL: conflict not detected\n"); return 1; }
        // reset restores the compiled-in default
        sm->ResetToDefault("TestCat/Alpha");
        if (a1->shortcut() != QKeySequence("Ctrl+1")) { fprintf(stderr, "FAIL: reset to default failed\n"); return 1; }
    }

    fprintf(stderr, "=== case: keying a BGM sample relocates its note in place ===\n");
    {
        // Underpins grouped-BGM charting: a channel holds one note per location,
        // so inserting a key-lane note where a BGM note already sits MOVES it
        // (same sample/time, new lane) rather than duplicating the sound.
        Document doc;
        doc.Initialize();
        doc.InsertNewSoundChannels(QList<QString>() << "piano_01.wav");
        pump();
        auto chs = doc.GetSoundChannels();
        if (chs.isEmpty()) { fprintf(stderr, "FAIL: no channel\n"); return 1; }
        SoundChannel *ch = chs.first();
        const int R = doc.GetInfo()->GetResolution();
        if (!ch->InsertNote(SoundNote(R, 0, 0, 0))) { fprintf(stderr, "FAIL: insert BGM note\n"); return 1; }
        if (ch->GetNotes().size() != 1 || ch->GetNotes().value(R).lane != 0) { fprintf(stderr, "FAIL: BGM note state\n"); return 1; }
        // key it: same location, lane 1
        if (!ch->InsertNote(SoundNote(R, 1, 0, 0))) { fprintf(stderr, "FAIL: key insert rejected\n"); return 1; }
        if (ch->GetNotes().size() != 1) { fprintf(stderr, "FAIL: keying duplicated the note (would double the sound)\n"); return 1; }
        if (ch->GetNotes().value(R).lane != 1) { fprintf(stderr, "FAIL: note not relocated to key lane\n"); return 1; }
        // un-key (the grouped-BGM "delete on chart" behavior): move back to lane 0
        if (!ch->InsertNote(SoundNote(R, 0, 0, 0))) { fprintf(stderr, "FAIL: un-key insert rejected\n"); return 1; }
        if (ch->GetNotes().size() != 1 || ch->GetNotes().value(R).lane != 0) { fprintf(stderr, "FAIL: un-key did not return sample to BGM\n"); return 1; }
    }

    fprintf(stderr, "=== case: slice / continuation (cut) note ===\n");
    {
        // A continuation note (noteType 1) coexists on the same channel as its
        // start note (noteType 0) and round-trips through the bmson `c` flag;
        // GetSampleEndTick falls back to startLoc when the wav length is unknown.
        Document doc;
        doc.Initialize();
        const int R = doc.GetInfo()->GetResolution();
        doc.InsertNewSoundChannels(QList<QString>() << "a.ogg");
        pump();
        auto chs = doc.GetSoundChannels();
        if (chs.isEmpty()) { fprintf(stderr, "FAIL: no channel\n"); return 1; }
        SoundChannel *ch = chs.first();
        if (!ch->InsertNote(SoundNote(0, 1, 0, 0))) { fprintf(stderr, "FAIL: insert start note\n"); return 1; }
        if (!ch->InsertNote(SoundNote(R, 1, 0, 1))) { fprintf(stderr, "FAIL: insert continuation\n"); return 1; }
        if (ch->GetNotes().size() != 2) { fprintf(stderr, "FAIL: continuation did not coexist (count %d)\n", (int)ch->GetNotes().size()); return 1; }
        if (ch->GetNotes().value(0).noteType != 0 || ch->GetNotes().value(R).noteType != 1) { fprintf(stderr, "FAIL: noteTypes wrong\n"); return 1; }
        // wav summary unknown in the harness -> GetSampleEndTick returns startLoc
        if (ch->GetSampleEndTick(0) != 0) { fprintf(stderr, "FAIL: GetSampleEndTick fallback\n"); return 1; }
        // bmson c-flag round-trip
        if (SoundNote(SoundNote(R, 1, 0, 1).AsJson()).noteType != 1) { fprintf(stderr, "FAIL: c=true round-trip\n"); return 1; }
        if (SoundNote(SoundNote(0, 1, 0, 0).AsJson()).noteType != 0) { fprintf(stderr, "FAIL: c=false round-trip\n"); return 1; }
        // removing the cut heals the slice: only the noteType-0 start remains
        if (!ch->RemoveNote(SoundNote(R, 1, 0, 1))) { fprintf(stderr, "FAIL: remove cut\n"); return 1; }
        if (ch->GetNotes().size() != 1 || ch->GetNotes().value(0).noteType != 0) { fprintf(stderr, "FAIL: removing cut did not restore the lone start note\n"); return 1; }
        // non-standard "stop" (x_stop) round-trips and only serializes when > 0
        {
            SoundNote s(0, 1, 0, 0, false, 480);
            SoundNote s2(s.AsJson());
            if (s2.stop != 480) { fprintf(stderr, "FAIL: stop round-trip\n"); return 1; }
            SoundNote noStop(0, 1, 0, 0);
            if (noStop.AsJson().contains("x_stop")) { fprintf(stderr, "FAIL: x_stop written when 0\n"); return 1; }
            if (SoundNote(noStop.AsJson()).stop != 0) { fprintf(stderr, "FAIL: stop default\n"); return 1; }
            if (s == noStop) { fprintf(stderr, "FAIL: stop should make notes differ (for in-place update)\n"); return 1; }
        }
    }

    fprintf(stderr, "=== case: reset all notes to BGM (un-chart) ===\n");
    {
        // Underpins Edit > Reset All Notes to BGM: every keyed note moves to lane
        // 0 (length 0), counts preserved, noteType preserved, one undo restores.
        Document doc;
        doc.Initialize();
        const int R = doc.GetInfo()->GetResolution();
        doc.InsertNewSoundChannels(QList<QString>() << "a.ogg" << "b.ogg");
        pump();
        auto chs = doc.GetSoundChannels();
        if (chs.size() != 2) { fprintf(stderr, "FAIL: channels\n"); return 1; }
        // place charted notes (lane>0, varied noteType) + one already-BGM note
        QMultiMap<SoundChannel*, SoundNote> seed;
        for (int i = 0; i < 10; i++)
            seed.insert(chs[0], SoundNote(i * 2 * R, (i % 5) + 1, 0, i % 2)); // keyed
        seed.insert(chs[1], SoundNote(0, 0, 0, 0)); // already BGM
        doc.MultiChannelUpdateSoundNotes(seed);
        pump();
        int before = chs[0]->GetNotes().size() + chs[1]->GetNotes().size();

        // build the "reset all" map exactly as SequenceView::ResetAllNotesToBgm
        QMultiMap<SoundChannel*, SoundNote> reset;
        QMap<int,int> oldType; // location -> noteType (ch0) to confirm preservation
        for (auto *ch : chs){
            const auto &ns = ch->GetNotes();
            for (auto it = ns.begin(); it != ns.end(); ++it){
                if (it.value().lane <= 0) continue;
                SoundNote n = it.value();
                if (ch == chs[0]) oldType[n.location] = n.noteType;
                n.lane = 0; n.length = 0;
                reset.insert(ch, n);
            }
        }
        doc.MultiChannelUpdateSoundNotes(reset);
        pump();
        int after = chs[0]->GetNotes().size() + chs[1]->GetNotes().size();
        if (after != before) { fprintf(stderr, "FAIL: reset changed note count %d->%d\n", before, after); return 1; }
        for (auto *ch : chs){
            for (auto it = ch->GetNotes().begin(); it != ch->GetNotes().end(); ++it){
                if (it.value().lane != 0 || it.value().length != 0) { fprintf(stderr, "FAIL: note not reset to BGM\n"); return 1; }
            }
        }
        for (auto it = chs[0]->GetNotes().begin(); it != chs[0]->GetNotes().end(); ++it){
            if (oldType.contains(it.key()) && it.value().noteType != oldType[it.key()]) { fprintf(stderr, "FAIL: noteType not preserved\n"); return 1; }
        }
        // single undo restores the charted lanes
        doc.GetHistory()->Undo();
        pump();
        bool anyKeyed = false;
        for (auto it = chs[0]->GetNotes().begin(); it != chs[0]->GetNotes().end(); ++it)
            if (it.value().lane > 0) { anyKeyed = true; break; }
        if (!anyKeyed) { fprintf(stderr, "FAIL: undo did not restore charted lanes\n"); return 1; }
    }

    fprintf(stderr, "=== case: sample grouping (name pattern + overlap sub-lanes) ===\n");
    {
        using namespace SampleGrouping;
        // group-key derivation
        if (GroupKeyOf("piano_03.wav") != "piano") { fprintf(stderr, "FAIL: key piano_03\n"); return 1; }
        if (GroupKeyOf("key_0348") != "key") { fprintf(stderr, "FAIL: key key_0348\n"); return 1; }
        if (GroupKeyOf("BASS2") != "BASS") { fprintf(stderr, "FAIL: key BASS2\n"); return 1; }
        if (GroupKeyOf("kick.ogg") != "kick") { fprintf(stderr, "FAIL: key kick\n"); return 1; }

        // non-overlapping piano_* notes collapse to a single sub-lane; drum is its own group
        {
            QStringList names; names << "piano_01.wav" << "piano_02.wav" << "drum.wav";
            QList<QList<QPair<int,int>>> notes;
            notes << (QList<QPair<int,int>>() << qMakePair(0,0) << qMakePair(480,0));
            notes << (QList<QPair<int,int>>() << qMakePair(240,0) << qMakePair(720,0));
            notes << (QList<QPair<int,int>>() << qMakePair(0,0));
            auto groups = BuildGroups(names, notes);
            if (groups.size() != 2) { fprintf(stderr, "FAIL: group count %d\n", (int)groups.size()); return 1; }
            // sorted by key: "drum", "piano"
            if (groups[0].key != "drum" || groups[1].key != "piano") { fprintf(stderr, "FAIL: group keys\n"); return 1; }
            if (groups[1].subLaneCount != 1) { fprintf(stderr, "FAIL: piano should need 1 sub-lane, got %d\n", groups[1].subLaneCount); return 1; }
            if (groups[1].placements.size() != 4) { fprintf(stderr, "FAIL: piano placement count\n"); return 1; }
        }
        // two samples at the same instant -> the group expands to 2 sub-lanes
        {
            QStringList names; names << "piano_01" << "piano_02";
            QList<QList<QPair<int,int>>> notes;
            notes << (QList<QPair<int,int>>() << qMakePair(0,0));
            notes << (QList<QPair<int,int>>() << qMakePair(0,0));
            auto groups = BuildGroups(names, notes);
            if (groups.size() != 1 || groups[0].subLaneCount != 2) { fprintf(stderr, "FAIL: overlap should need 2 sub-lanes\n"); return 1; }
            // the two notes must land on different sub-lanes
            if (groups[0].placements.size() == 2 && groups[0].placements[0].subLane == groups[0].placements[1].subLane) {
                fprintf(stderr, "FAIL: overlapping notes share a sub-lane\n"); return 1;
            }
        }
        // a long note overlapping a later point note also forces 2 sub-lanes
        {
            QStringList names; names << "pad_1" << "pad_2";
            QList<QList<QPair<int,int>>> notes;
            notes << (QList<QPair<int,int>>() << qMakePair(0,500));
            notes << (QList<QPair<int,int>>() << qMakePair(100,0));
            auto groups = BuildGroups(names, notes);
            if (groups.size() != 1 || groups[0].subLaneCount != 2) { fprintf(stderr, "FAIL: LN overlap should need 2 sub-lanes\n"); return 1; }
        }
    }

    fprintf(stderr, "ALL CASES DONE\n");
    return 0;
}

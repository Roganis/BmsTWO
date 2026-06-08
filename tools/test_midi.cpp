// Unit test for the SMF parser (MidiImport::Parse). Pure logic, no GUI.
#include "midi/MidiImport.h"
#include <QByteArray>
#include <cstdio>

static void putU32(QByteArray &b, quint32 v){ b.append(char(v>>24)); b.append(char(v>>16)); b.append(char(v>>8)); b.append(char(v)); }
static void putU16(QByteArray &b, quint16 v){ b.append(char(v>>8)); b.append(char(v)); }
static void putVar(QByteArray &b, quint32 v){
    char buf[4]; int n=0; buf[n++]=char(v&0x7F); while((v>>=7)){ buf[n++]=char((v&0x7F)|0x80); }
    for(int i=n-1;i>=0;i--) b.append(buf[i]);
}

int main(){
    // Build a format-1 SMF: PPQ=480; tempo 120 BPM at tick0; track with note-ons
    // at ticks 0, 240, 480 (and a note-off / zero-velocity that must be ignored).
    QByteArray midi;
    midi.append("MThd"); putU32(midi,6); putU16(midi,1); putU16(midi,2); putU16(midi,480);

    // Track 0: tempo
    QByteArray t0;
    putVar(t0,0); t0.append(char(0xFF)); t0.append(char(0x51)); t0.append(char(3));
    quint32 us=500000; t0.append(char(us>>16)); t0.append(char(us>>8)); t0.append(char(us)); // 120 BPM
    putVar(t0,0); t0.append(char(0xFF)); t0.append(char(0x2F)); t0.append(char(0)); // end
    midi.append("MTrk"); putU32(midi,t0.size()); midi.append(t0);

    // Track 1: notes (running status on 2nd note-on)
    QByteArray t1;
    putVar(t1,0);   t1.append(char(0x90)); t1.append(char(60)); t1.append(char(100)); // on @0
    putVar(t1,240); t1.append(char(60)); t1.append(char(100));                        // on @240 (running status)
    putVar(t1,240); t1.append(char(0x90)); t1.append(char(62)); t1.append(char(0));   // vel0 = off, ignore @480
    putVar(t1,0);   t1.append(char(0x90)); t1.append(char(64)); t1.append(char(80));  // on @480
    putVar(t1,0);   t1.append(char(0xFF)); t1.append(char(0x2F)); t1.append(char(0));
    midi.append("MTrk"); putU32(midi,t1.size()); midi.append(t1);

    MidiImport::Result r = MidiImport::Parse(midi);
    int fail = 0;
    auto check = [&](bool c, const char *m){ fprintf(stderr, "%s %s\n", c?"PASS":"FAIL", m); if(!c) fail=1; };
    check(r.ok, "parse ok");
    check(r.ppq==480, "ppq=480");
    check(r.noteOnTicks.size()==3, "3 onsets (vel0 ignored)");
    check(r.noteOnTicks.size()==3 && r.noteOnTicks[0]==0 && r.noteOnTicks[1]==240 && r.noteOnTicks[2]==480, "onset ticks 0,240,480");
    check(r.tempos.size()==1 && r.tempos[0].first==0 && r.tempos[0].second>119.9 && r.tempos[0].second<120.1, "tempo 120 @0");

    MidiImport::Result bad = MidiImport::Parse(QByteArray("not a midi file"));
    check(!bad.ok, "rejects non-midi");

    fprintf(stderr, fail ? "TEST FAILED\n" : "ALL TESTS PASSED\n");
    return fail;
}

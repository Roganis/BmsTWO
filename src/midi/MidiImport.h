#ifndef MIDIIMPORT_H
#define MIDIIMPORT_H

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>

// Minimal Standard MIDI File (SMF) reader for the "import MIDI sequence as is"
// workflow (issue #11): it extracts note-on positions and the tempo map so the
// editor can drop slice notes / BPM events that preserve the original timing.
// Pure logic (bytes in, data out) so it can be unit-tested without a GUI.
namespace MidiImport
{
	struct Result
	{
		bool ok = false;
		QString error;
		int ppq = 0;                          // ticks per quarter note (SMF division)
		QList<int> noteOnTicks;               // absolute MIDI ticks of note-ons, sorted & deduped
		QList<QPair<int, double>> tempos;     // (absolute tick, BPM), sorted by tick
	};

	// Parse an in-memory SMF (format 0/1). Returns ok=false with an error
	// message on malformed input or an unsupported (SMPTE) time division.
	Result Parse(const QByteArray &data);
}

#endif // MIDIIMPORT_H

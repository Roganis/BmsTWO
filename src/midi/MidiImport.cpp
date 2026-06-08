#include "MidiImport.h"
#include <QSet>
#include <algorithm>

namespace {

// Sequential big-endian reader with bounds checking.
class Reader
{
public:
	Reader(const QByteArray &d) : data(d), pos(0) {}
	bool atEnd() const { return pos >= data.size(); }
	int remaining() const { return data.size() - pos; }
	int tell() const { return pos; }
	void seek(int p) { pos = p; }

	bool readBytes(int n, QByteArray &out)
	{
		if (n < 0 || remaining() < n) return false;
		out = data.mid(pos, n);
		pos += n;
		return true;
	}
	bool readU8(quint8 &v)
	{
		if (remaining() < 1) return false;
		v = quint8(data[pos++]);
		return true;
	}
	bool readU16(quint32 &v)
	{
		if (remaining() < 2) return false;
		v = (quint8(data[pos]) << 8) | quint8(data[pos + 1]);
		pos += 2;
		return true;
	}
	bool readU32(quint32 &v)
	{
		if (remaining() < 4) return false;
		v = (quint8(data[pos]) << 24) | (quint8(data[pos + 1]) << 16)
		  | (quint8(data[pos + 2]) << 8) | quint8(data[pos + 3]);
		pos += 4;
		return true;
	}
	// Variable-length quantity (7 bits per byte, high bit = continue).
	bool readVarLen(quint32 &v)
	{
		v = 0;
		for (int i = 0; i < 4; i++){
			quint8 b;
			if (!readU8(b)) return false;
			v = (v << 7) | (b & 0x7F);
			if ((b & 0x80) == 0) return true;
		}
		return false; // more than 4 bytes is malformed
	}

private:
	const QByteArray &data;
	int pos;
};

} // namespace

MidiImport::Result MidiImport::Parse(const QByteArray &data)
{
	Result r;
	Reader reader(data);

	QByteArray magic;
	quint32 headerLen = 0, division32 = 0;
	quint32 format = 0, ntracks = 0;
	if (!reader.readBytes(4, magic) || magic != "MThd"){
		r.error = "Not a Standard MIDI File (missing MThd header).";
		return r;
	}
	if (!reader.readU32(headerLen) || !reader.readU16(format)
		|| !reader.readU16(ntracks) || !reader.readU16(division32)){
		r.error = "Truncated MIDI header.";
		return r;
	}
	// Skip any extra header bytes beyond the standard 6.
	if (headerLen > 6)
		reader.seek(8 + int(headerLen));

	const qint16 division = qint16(division32);
	if (division <= 0){
		r.error = "SMPTE time division is not supported; please use a PPQ-based MIDI file.";
		return r;
	}
	r.ppq = division;

	QSet<int> onsetSet;
	QList<QPair<int, double>> tempos;

	for (quint32 t = 0; t < ntracks; t++){
		QByteArray trackMagic;
		quint32 trackLen = 0;
		// Resync to next chunk; tolerate non-MTrk chunks by skipping them.
		while (true){
			if (!reader.readBytes(4, trackMagic) || !reader.readU32(trackLen)){
				r.error = "Truncated MIDI track header.";
				return r;
			}
			if (trackMagic == "MTrk")
				break;
			if (reader.remaining() < int(trackLen)){
				r.error = "Malformed MIDI chunk.";
				return r;
			}
			reader.seek(reader.tell() + int(trackLen)); // skip unknown chunk
		}
		const int trackEnd = reader.tell() + int(trackLen);
		if (trackEnd > data.size()){
			r.error = "MIDI track length exceeds file size.";
			return r;
		}

		quint32 absTick = 0;
		quint8 runningStatus = 0;
		while (reader.tell() < trackEnd){
			quint32 delta = 0;
			if (!reader.readVarLen(delta)) { r.error = "Malformed delta time."; return r; }
			absTick += delta;

			quint8 status;
			if (!reader.readU8(status)) { r.error = "Truncated event."; return r; }
			if (status < 0x80){
				// Running status: reuse previous status, rewind one data byte.
				if (runningStatus == 0) { r.error = "MIDI running status without a prior status byte."; return r; }
				reader.seek(reader.tell() - 1);
				status = runningStatus;
			}else if (status < 0xF0){
				runningStatus = status;
			}

			if (status == 0xFF){
				// Meta event.
				quint8 metaType;
				quint32 metaLen = 0;
				if (!reader.readU8(metaType) || !reader.readVarLen(metaLen)) { r.error = "Truncated meta event."; return r; }
				QByteArray metaData;
				if (!reader.readBytes(int(metaLen), metaData)) { r.error = "Truncated meta data."; return r; }
				if (metaType == 0x51 && metaData.size() == 3){
					const quint32 usPerQuarter = (quint8(metaData[0]) << 16)
											   | (quint8(metaData[1]) << 8) | quint8(metaData[2]);
					if (usPerQuarter > 0)
						tempos.append(qMakePair(int(absTick), 60000000.0 / usPerQuarter));
				}
				// 0x2F (end of track) and others: nothing to collect.
			}else if (status == 0xF0 || status == 0xF7){
				// SysEx: skip its declared length.
				quint32 sysexLen = 0;
				if (!reader.readVarLen(sysexLen)) { r.error = "Truncated sysex."; return r; }
				QByteArray dummy;
				if (!reader.readBytes(int(sysexLen), dummy)) { r.error = "Truncated sysex data."; return r; }
			}else{
				// Channel voice message.
				const quint8 hi = status & 0xF0;
				const int dataBytes = (hi == 0xC0 || hi == 0xD0) ? 1 : 2;
				quint8 d1 = 0, d2 = 0;
				if (dataBytes >= 1 && !reader.readU8(d1)) { r.error = "Truncated channel message."; return r; }
				if (dataBytes >= 2 && !reader.readU8(d2)) { r.error = "Truncated channel message."; return r; }
				// Note-on with non-zero velocity is a real onset.
				if (hi == 0x90 && d2 > 0)
					onsetSet.insert(int(absTick));
			}
		}
		reader.seek(trackEnd); // be robust if an event under-ran the track
	}

	r.noteOnTicks = onsetSet.values();
	std::sort(r.noteOnTicks.begin(), r.noteOnTicks.end());
	std::sort(tempos.begin(), tempos.end(),
			  [](const QPair<int,double> &a, const QPair<int,double> &b){ return a.first < b.first; });
	r.tempos = tempos;
	r.ok = true;
	return r;
}

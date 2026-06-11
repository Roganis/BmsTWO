#include "SampleGrouping.h"
#include <QRegularExpression>
#include <QMap>
#include <algorithm>

namespace SampleGrouping
{

QString GroupKeyOf(const QString &name)
{
	QString s = name;
	// strip a trailing file extension (".wav", ".ogg", ...)
	int dot = s.lastIndexOf('.');
	if (dot > 0)
		s = s.left(dot);
	// leading part before a trailing number (+ any separators between)
	static const QRegularExpression re("^(.*?)[\\s_\\-]*\\d+$");
	QRegularExpressionMatch m = re.match(s);
	if (m.hasMatch()){
		QString key = m.captured(1).trimmed();
		if (!key.isEmpty())
			return key;
	}
	return s.trimmed();
}

// Two notes conflict (can't share a sub-lane) if their time intervals overlap.
// Zero-length notes are given a 1-tick extent so that two samples triggered at
// the same instant are treated as simultaneous.
static bool Conflicts(int aLoc, int aLen, int bLoc, int bLen)
{
	int aEnd = aLoc + std::max(aLen, 1);
	int bEnd = bLoc + std::max(bLen, 1);
	return aLoc < bEnd && bLoc < aEnd;
}

QList<Group> BuildGroups(const QStringList &channelNames,
						 const QList<QList<QPair<int, int>>> &notesPerChannel)
{
	// bucket channels' notes by group key, preserving channel identity
	QMap<QString, QList<Placement>> buckets; // key -> raw placements (subLane unset)
	const int n = std::min(channelNames.size(), notesPerChannel.size());
	for (int ch = 0; ch < n; ch++){
		const QString key = GroupKeyOf(channelNames[ch]);
		auto &list = buckets[key];
		for (const auto &note : notesPerChannel[ch]){
			list.append(Placement{ ch, note.first, note.second, 0 });
		}
	}

	QList<Group> groups;
	for (auto it = buckets.begin(); it != buckets.end(); ++it){
		Group g;
		g.key = it.key();
		QList<Placement> notes = it.value();
		// sort by start time (then length) for a stable greedy packing
		std::sort(notes.begin(), notes.end(), [](const Placement &a, const Placement &b){
			if (a.location != b.location) return a.location < b.location;
			return a.length < b.length;
		});
		// greedy sub-lane assignment: place each note in the first sub-lane whose
		// last note does not conflict; otherwise open a new sub-lane.
		QList<QPair<int,int>> laneLastNote; // per sub-lane: (location, length) of its last note
		for (Placement &p : notes){
			int assigned = -1;
			for (int lane = 0; lane < laneLastNote.size(); lane++){
				if (!Conflicts(laneLastNote[lane].first, laneLastNote[lane].second, p.location, p.length)){
					assigned = lane;
					break;
				}
			}
			if (assigned < 0){
				assigned = laneLastNote.size();
				laneLastNote.append(qMakePair(p.location, p.length));
			}else{
				laneLastNote[assigned] = qMakePair(p.location, p.length);
			}
			p.subLane = assigned;
		}
		g.subLaneCount = std::max(1, (int)laneLastNote.size());
		g.placements = notes;
		groups.append(g);
	}

	// sort groups by key for a stable left-to-right order
	std::sort(groups.begin(), groups.end(), [](const Group &a, const Group &b){
		return a.key < b.key;
	});
	return groups;
}

} // namespace SampleGrouping

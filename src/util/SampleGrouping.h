#ifndef SAMPLEGROUPING_H
#define SAMPLEGROUPING_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>

// Groups sample channels by their name pattern for the "grouped BGM lanes" view:
// e.g. "piano_03" and "piano_04" share the "piano" group, rendered in a single
// background lane unless their notes overlap in time, in which case the group
// expands to as many sub-lanes as are needed so no two simultaneous notes share
// one. This module is pure (no Qt widgets) so it can be unit-tested headlessly.
namespace SampleGrouping
{
	// The stable group key for a sample name: the leading part before a trailing
	// number, with any file extension and trailing separators removed.
	//   "piano_03.wav" -> "piano",  "key_0348" -> "key",  "BASS2" -> "BASS",
	//   "kick" -> "kick".  Falls back to the (extension-stripped) name when there
	// is no trailing number.
	QString GroupKeyOf(const QString &name);

	// A note placed into a group, tagged with the channel it came from and the
	// sub-lane it was assigned to (0-based).
	struct Placement
	{
		int channelIndex;
		int location;
		int length;
		int subLane;
	};

	struct Group
	{
		QString key;
		int subLaneCount = 1;       // how many sub-lanes this group needs
		QList<Placement> placements; // every note in the group, with its subLane
	};

	// Build the groups from per-channel names + notes. `notesPerChannel[i]` is the
	// list of (location, length) notes for channel `i`. Groups are returned sorted
	// by key. Within each group, notes are packed greedily into the minimum number
	// of sub-lanes such that time-overlapping notes never share a sub-lane.
	QList<Group> BuildGroups(const QStringList &channelNames,
							 const QList<QList<QPair<int, int>>> &notesPerChannel);
}

#endif // SAMPLEGROUPING_H

#ifndef KEYSEQ_H
#define KEYSEQ_H

#include <QKeySequence>
#include <Qt>

// Combine keyboard modifier(s) with a key into a QKeySequence in a way that
// compiles on both Qt5 and Qt6. Qt5 rejects `modifier | key` (QFlags type
// safety) and Qt6 deletes `modifier + key`; doing the arithmetic on plain
// ints sidesteps both.
inline QKeySequence KeySeq(Qt::KeyboardModifier m, int key)
{
	return QKeySequence(int(m) | key);
}

inline QKeySequence KeySeq(Qt::KeyboardModifier m1, Qt::KeyboardModifier m2, int key)
{
	return QKeySequence(int(m1) | int(m2) | key);
}

#endif // KEYSEQ_H

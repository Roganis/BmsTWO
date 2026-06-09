#ifndef SHORTCUTMANAGER_H
#define SHORTCUTMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QKeySequence>
#include <QPointer>

class QAction;
class QMenuBar;
class QMenu;

// Central registry of rebindable menu actions. It walks the menu bar once
// (after it is built), captures each action's compiled-in shortcut as the
// default, applies any user override from QSettings, and backs the Shortcuts
// preferences page. Overrides persist under the "Shortcuts" settings group,
// keyed by a stable "<Category>/<Action>" id.
class ShortcutManager : public QObject
{
	Q_OBJECT
public:
	struct Entry
	{
		QString id;        // stable settings key, e.g. "Edit/Undo"
		QString category;  // top-level menu title, e.g. "Edit"
		QString name;      // action label (without '&' accelerators)
		QPointer<QAction> action;
		QKeySequence defaultSeq;
	};

	static ShortcutManager *Instance();

	void RegisterMenuBar(QMenuBar *bar);

	const QList<Entry> &Entries() const { return entries; }

	// Apply + persist a shortcut for an entry (an empty sequence means "unbound").
	void SetShortcut(const QString &id, const QKeySequence &seq);
	void ResetToDefault(const QString &id);
	void ResetAll();

	// ids that currently use `seq` other than `exceptId` (for conflict warnings).
	QStringList Conflicts(const QKeySequence &seq, const QString &exceptId) const;

private:
	ShortcutManager() {}
	void RegisterMenu(QMenu *menu, const QString &category);
	Entry *Find(const QString &id);
	static QString CleanText(const QString &text);

	QList<Entry> entries;
};

#endif // SHORTCUTMANAGER_H

#ifndef PREFSHORTCUTS_H
#define PREFSHORTCUTS_H

#include <QWidget>
#include <QList>

class QTreeWidget;
class QTreeWidgetItem;
class QKeySequenceEdit;
class QLabel;

// Preferences page: lists every menu command grouped by category with a
// click-to-capture editor, live conflict detection, and reset-to-defaults.
class PrefShortcutsPage : public QWidget
{
	Q_OBJECT
public:
	PrefShortcutsPage(QWidget *parent);

	void load();
	void store();

private slots:
	void OnResetAll();
	void CheckConflicts();

private:
	struct Row
	{
		QString id;
		QTreeWidgetItem *item;
		QKeySequenceEdit *editor;
	};
	QTreeWidget *tree;
	QLabel *conflictLabel;
	QList<Row> rows;
};

#endif // PREFSHORTCUTS_H

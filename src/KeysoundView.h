#ifndef KEYSOUNDVIEW_H
#define KEYSOUNDVIEW_H

#include <QWidget>

class MainWindow;
class Document;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTimer;

// Sabun-friendly consolidated keysound-list manager: lists every sound channel
// with its note count, and offers add / remove / move / replace in one place
// (operations that otherwise live in per-channel context menus). Double-click a
// row to make it the channel being edited.
class KeysoundView : public QWidget
{
	Q_OBJECT
public:
	KeysoundView(MainWindow *mainWindow);

	void ReplaceDocument(Document *newDocument);

private slots:
	void ScheduleRefresh();
	void Refresh();
	void HighlightCurrent(int index);
	void OnItemActivated(QListWidgetItem *item);
	void OnAdd();
	void OnRemove();
	void OnMoveUp();
	void OnMoveDown();
	void OnReplace();

private:
	int SelectedIndex() const;

	MainWindow *mainWindow;
	Document *document;
	QTimer *refreshTimer;
	QListWidget *list;
	QPushButton *addButton;
	QPushButton *removeButton;
	QPushButton *upButton;
	QPushButton *downButton;
	QPushButton *replaceButton;
};

#endif // KEYSOUNDVIEW_H

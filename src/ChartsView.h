#ifndef CHARTSVIEW_H
#define CHARTSVIEW_H

#include <QWidget>

class MainWindow;
class Document;
class QListWidget;
class QListWidgetItem;

// Sabun-friendly "Charts" dock: lists the sibling chart files (.bmson/.bms/...)
// in the current song folder. Double-click to switch charts (with the usual
// save prompt). Refreshes when the document or its file path changes.
class ChartsView : public QWidget
{
	Q_OBJECT
public:
	ChartsView(MainWindow *mainWindow);

	void ReplaceDocument(Document *newDocument);

private slots:
	void Refresh();
	void OnItemActivated(QListWidgetItem *item);

private:
	MainWindow *mainWindow;
	Document *document;
	QListWidget *list;
};

#endif // CHARTSVIEW_H

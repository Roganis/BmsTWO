#ifndef SELECTIONVIEW_H
#define SELECTIONVIEW_H

#include <QWidget>

class MainWindow;
class SequenceView;
class QTableWidget;
class QLabel;
class QTableWidgetItem;

// Read-only dock listing the currently selected notes. Sortable by any column
// (position / time / lane / channel); double-click a row to jump to that note.
class SelectionView : public QWidget
{
	Q_OBJECT
public:
	SelectionView(MainWindow *mainWindow);

	void Bind(SequenceView *sview);

public slots:
	void Refresh();

private slots:
	void OnCellDoubleClicked(int row, int column);

private:
	MainWindow *mainWindow;
	SequenceView *sequenceView;
	QLabel *countLabel;
	QTableWidget *table;
};

#endif // SELECTIONVIEW_H

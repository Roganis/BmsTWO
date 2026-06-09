#ifndef STOPVIEW_H
#define STOPVIEW_H

#include <QWidget>
#include "document/Document.h"

class MainWindow;
class QTableWidget;
class QPushButton;

// Dock widget for editing STOP events (issue #3): a table of (location, pulse
// duration) rows with Add/Remove and inline editing. All edits route through
// the Document's undoable STOP API. Note: stops are saved to bmson but the
// timing engine does not yet pause playback at them.
class StopView : public QWidget
{
	Q_OBJECT

private:
	MainWindow *mainWindow;
	Document *document;

	QTableWidget *table;
	QPushButton *addButton;
	QPushButton *removeButton;

	bool updating; // guards against cellChanged reentrancy while we repopulate

	void Reload();
	int NextFreeLocation() const;
	void AddStop();
	void RemoveStop();
	void CellChanged(int row, int column);

private slots:
	void OnStopsChanged();

public:
	StopView(MainWindow *mainWindow = nullptr);
	virtual ~StopView();

	void ReplaceDocument(Document *newDocument);
};

#endif // STOPVIEW_H

#ifndef BGAVIEW_H
#define BGAVIEW_H

#include <QWidget>
#include "document/Document.h"

class MainWindow;
class QTableWidget;
class QTabWidget;
class QPushButton;

// Dock widget for editing BGA data (issue #2): a "Pictures" tab managing BGA
// headers (id -> image file) and one tab per event lane (BGA / Layer / Poor)
// listing image-change events along the timeline. All edits route through the
// Document's undoable BGA API.
class BgaView : public QWidget
{
	Q_OBJECT

private:
	struct LaneTab
	{
		BgaLayer layer;
		QTableWidget *table;
		QPushButton *addButton;
		QPushButton *removeButton;
	};

	MainWindow *mainWindow;
	Document *document;

	QTabWidget *tabs;
	QTableWidget *headerTable;
	QPushButton *addHeaderButton;
	QPushButton *removeHeaderButton;
	LaneTab lanes[3];

	bool updating; // guards against cellChanged reentrancy while we repopulate

	void ReloadHeaders();
	void ReloadLane(const LaneTab &lane);
	void ReloadAll();
	int NextFreeHeaderId() const;
	int NextFreeEventLocation(BgaLayer layer) const;

	void AddHeader();
	void RemoveHeader();
	void HeaderCellChanged(int row, int column);
	void AddEvent(const LaneTab &lane);
	void RemoveEvent(const LaneTab &lane);
	void EventCellChanged(const LaneTab &lane, int row, int column);

private slots:
	void OnBgaChanged();

public:
	BgaView(MainWindow *mainWindow = nullptr);
	virtual ~BgaView();

	void ReplaceDocument(Document *newDocument);
};

#endif // BGAVIEW_H

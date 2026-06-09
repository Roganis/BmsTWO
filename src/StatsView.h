#ifndef STATSVIEW_H
#define STATSVIEW_H

#include <QWidget>

class MainWindow;
class Document;
class QLabel;
class QTableWidget;
class QTimer;

// Read-only dock summarizing the current chart: note counts, density / NPS,
// and per-lane distribution. Recomputed (debounced) whenever the document
// changes.
class StatsView : public QWidget
{
	Q_OBJECT
public:
	StatsView(MainWindow *mainWindow);

	void ReplaceDocument(Document *newDocument);

private slots:
	void ScheduleRefresh();
	void Refresh();

private:
	MainWindow *mainWindow;
	Document *document;
	QTimer *refreshTimer;

	QLabel *totalValue;
	QLabel *playableValue;
	QLabel *bgmValue;
	QLabel *channelsValue;
	QLabel *lengthValue;
	QLabel *avgNpsValue;
	QLabel *peakNpsValue;
	QTableWidget *laneTable;
};

#endif // STATSVIEW_H

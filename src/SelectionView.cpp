#include "SelectionView.h"
#include "MainWindow.h"
#include "sequence_view/SequenceView.h"
#include "util/Theme.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>

SelectionView::SelectionView(MainWindow *mainWindow)
	: QWidget(mainWindow)
	, mainWindow(mainWindow)
	, sequenceView(nullptr)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);

	countLabel = new QLabel(tr("No selection"));
	layout->addWidget(countLabel);

	table = new QTableWidget(0, 4, this);
	table->setHorizontalHeaderLabels(QStringList() << tr("Pulse") << tr("Time (s)") << tr("Lane") << tr("Channel"));
	table->horizontalHeader()->setStretchLastSection(true);
	table->verticalHeader()->setVisible(false);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setSortingEnabled(true);
	if (Theme::IsModern())
		table->setFont(Theme::MonospaceFont());
	layout->addWidget(table, 1);

	connect(table, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(OnCellDoubleClicked(int,int)));
}

void SelectionView::Bind(SequenceView *sview)
{
	sequenceView = sview;
	Refresh();
}

// Numeric-sorting table item: keeps the sort key in EditRole so QTableWidget
// orders by value, not lexicographically.
static QTableWidgetItem *NumItem(double value, const QString &text)
{
	auto *item = new QTableWidgetItem();
	item->setData(Qt::DisplayRole, text);
	item->setData(Qt::EditRole, value);
	return item;
}

void SelectionView::Refresh()
{
	if (!sequenceView){
		table->setRowCount(0);
		countLabel->setText(tr("No selection"));
		return;
	}
	const auto infos = sequenceView->GetSelectedNotesInfo();

	const bool wasSorting = table->isSortingEnabled();
	table->setSortingEnabled(false); // don't re-sort while populating
	table->setRowCount(infos.size());
	for (int row = 0; row < infos.size(); row++){
		const auto &info = infos[row];
		table->setItem(row, 0, NumItem(info.location, QString::number(info.location)));
		table->setItem(row, 1, NumItem(info.seconds, QString::number(info.seconds, 'f', 3)));
		const QString laneText = info.lane == 0 ? tr("BGM") : QString::number(info.lane);
		table->setItem(row, 2, NumItem(info.lane, laneText));
		auto *chItem = new QTableWidgetItem(info.channel);
		table->setItem(row, 3, chItem);
	}
	table->setSortingEnabled(wasSorting);

	countLabel->setText(infos.isEmpty()
						? tr("No selection")
						: tr("%n note(s) selected", "", infos.size()));
}

void SelectionView::OnCellDoubleClicked(int row, int /*column*/)
{
	if (!sequenceView)
		return;
	QTableWidgetItem *item = table->item(row, 0);
	if (!item)
		return;
	const int location = item->data(Qt::EditRole).toInt();
	sequenceView->GoToLocation(location);
}

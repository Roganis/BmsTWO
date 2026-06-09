#include "StopView.h"
#include "MainWindow.h"
#include "util/Theme.h"
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

static const int LocationRole = Qt::UserRole + 1;

StopView::StopView(MainWindow *mainWindow)
	: QWidget(mainWindow)
	, mainWindow(mainWindow)
	, document(nullptr)
	, updating(false)
{
	table = new QTableWidget(0, 2, this);
	table->setHorizontalHeaderLabels(QStringList() << tr("Location") << tr("Duration (pulses)"));
	table->horizontalHeader()->setStretchLastSection(true);
	table->verticalHeader()->setVisible(false);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	if (Theme::IsModern())
		table->setFont(Theme::MonospaceFont()); // numeric location/duration

	addButton = new QPushButton(tr("Add"), this);
	removeButton = new QPushButton(tr("Remove"), this);
	auto *buttons = new QHBoxLayout();
	buttons->addWidget(addButton);
	buttons->addWidget(removeButton);
	buttons->addStretch(1);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(2, 2, 2, 2);
	layout->addWidget(table);
	layout->addLayout(buttons);

	connect(addButton, &QPushButton::clicked, this, [this](){ AddStop(); });
	connect(removeButton, &QPushButton::clicked, this, [this](){ RemoveStop(); });
	connect(table, &QTableWidget::cellChanged, this, [this](int r, int c){ CellChanged(r, c); });

	setEnabled(false);
}

StopView::~StopView()
{
}

void StopView::ReplaceDocument(Document *newDocument)
{
	// The previous document has already been deleted by MainWindow before this
	// is called, so we must NOT disconnect from it (freed pointer). Qt drops the
	// old connections when the document is destroyed; we only bind to the new one.
	document = newDocument;
	if (document){
		connect(document, SIGNAL(StopEventsChanged()), this, SLOT(OnStopsChanged()));
	}
	setEnabled(document != nullptr);
	Reload();
}

void StopView::OnStopsChanged()
{
	Reload();
}

int StopView::NextFreeLocation() const
{
	if (!document)
		return 0;
	const auto &stops = document->GetStopEvents();
	if (stops.isEmpty())
		return 0;
	int resolution = document->GetInfo()->GetResolution();
	if (resolution <= 0)
		resolution = 1;
	return stops.lastKey() + resolution;
}

void StopView::Reload()
{
	updating = true;
	table->setRowCount(0);
	if (document){
		const auto &stops = document->GetStopEvents();
		int row = 0;
		for (auto i = stops.begin(); i != stops.end(); i++, row++){
			table->insertRow(row);
			auto *locItem = new QTableWidgetItem(QString::number(i.key()));
			locItem->setData(LocationRole, i.key());
			table->setItem(row, 0, locItem);
			table->setItem(row, 1, new QTableWidgetItem(QString::number(i.value().value)));
		}
	}
	updating = false;
}

void StopView::AddStop()
{
	if (!document)
		return;
	int resolution = document->GetInfo()->GetResolution();
	if (resolution <= 0)
		resolution = 1;
	// Default to a quarter-beat stop, a reasonable starting duration.
	document->InsertStopEvent(StopEvent(NextFreeLocation(), resolution));
}

void StopView::RemoveStop()
{
	if (!document)
		return;
	auto rows = table->selectionModel()->selectedRows();
	QList<int> locations;
	for (const auto &index : rows){
		auto *item = table->item(index.row(), 0);
		if (item)
			locations.append(item->data(LocationRole).toInt());
	}
	for (int location : locations)
		document->RemoveStopEvent(location);
}

void StopView::CellChanged(int row, int column)
{
	if (updating || !document)
		return;
	auto *locItem = table->item(row, 0);
	auto *durItem = table->item(row, 1);
	if (!locItem || !durItem)
		return;
	int oldLocation = locItem->data(LocationRole).toInt();
	bool okLoc = false, okDur = false;
	int newLocation = locItem->text().toInt(&okLoc);
	double duration = durItem->text().toDouble(&okDur);
	if (!okLoc || !okDur || newLocation < 0 || duration < 0){
		Reload(); // revert invalid input from the model
		return;
	}
	if (column == 0 && newLocation != oldLocation){
		document->RemoveStopEvent(oldLocation);
	}
	document->InsertStopEvent(StopEvent(newLocation, duration));
}

#include "BgaView.h"
#include "MainWindow.h"
#include "util/Theme.h"
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

static const int LocationRole = Qt::UserRole + 1;

BgaView::BgaView(MainWindow *mainWindow)
	: QWidget(mainWindow)
	, mainWindow(mainWindow)
	, document(nullptr)
	, updating(false)
{
	tabs = new QTabWidget(this);

	// Pictures tab (BGA headers).
	{
		auto *page = new QWidget(this);
		auto *layout = new QVBoxLayout(page);
		layout->setContentsMargins(2, 2, 2, 2);
		headerTable = new QTableWidget(0, 2, page);
		headerTable->setHorizontalHeaderLabels(QStringList() << tr("ID") << tr("Image File"));
		headerTable->horizontalHeader()->setStretchLastSection(true);
		headerTable->verticalHeader()->setVisible(false);
		headerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
		layout->addWidget(headerTable);
		auto *buttons = new QHBoxLayout();
		addHeaderButton = new QPushButton(tr("Add"), page);
		removeHeaderButton = new QPushButton(tr("Remove"), page);
		buttons->addWidget(addHeaderButton);
		buttons->addWidget(removeHeaderButton);
		buttons->addStretch(1);
		layout->addLayout(buttons);
		tabs->addTab(page, tr("Pictures"));

		connect(addHeaderButton, &QPushButton::clicked, this, [this](){ AddHeader(); });
		connect(removeHeaderButton, &QPushButton::clicked, this, [this](){ RemoveHeader(); });
		connect(headerTable, &QTableWidget::cellChanged, this, [this](int r, int c){ HeaderCellChanged(r, c); });
	}

	// Event lanes.
	struct LaneDef { BgaLayer layer; const char *name; } defs[3] = {
		{ BgaLayer::Base,  QT_TR_NOOP("BGA") },
		{ BgaLayer::Layer, QT_TR_NOOP("Layer") },
		{ BgaLayer::Poor,  QT_TR_NOOP("Poor") },
	};
	for (int i = 0; i < 3; i++){
		auto *page = new QWidget(this);
		auto *layout = new QVBoxLayout(page);
		layout->setContentsMargins(2, 2, 2, 2);
		auto *table = new QTableWidget(0, 2, page);
		table->setHorizontalHeaderLabels(QStringList() << tr("Location") << tr("Picture ID"));
		table->horizontalHeader()->setStretchLastSection(true);
		table->verticalHeader()->setVisible(false);
		table->setSelectionBehavior(QAbstractItemView::SelectRows);
		if (Theme::IsModern())
			table->setFont(Theme::MonospaceFont()); // numeric location/picture id
		layout->addWidget(table);
		auto *buttons = new QHBoxLayout();
		auto *addButton = new QPushButton(tr("Add"), page);
		auto *removeButton = new QPushButton(tr("Remove"), page);
		buttons->addWidget(addButton);
		buttons->addWidget(removeButton);
		buttons->addStretch(1);
		layout->addLayout(buttons);
		tabs->addTab(page, tr(defs[i].name));

		lanes[i].layer = defs[i].layer;
		lanes[i].table = table;
		lanes[i].addButton = addButton;
		lanes[i].removeButton = removeButton;

		const LaneTab &lane = lanes[i];
		connect(addButton, &QPushButton::clicked, this, [this, lane](){ AddEvent(lane); });
		connect(removeButton, &QPushButton::clicked, this, [this, lane](){ RemoveEvent(lane); });
		connect(table, &QTableWidget::cellChanged, this, [this, lane](int r, int c){ EventCellChanged(lane, r, c); });
	}

	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->addWidget(tabs);

	setEnabled(false);
}

BgaView::~BgaView()
{
}

void BgaView::ReplaceDocument(Document *newDocument)
{
	// The previous document has already been deleted by MainWindow before this
	// is called, so we must NOT disconnect from it (that would dereference a
	// freed pointer). Qt drops the old connections when the document is
	// destroyed; we only bind to the new one.
	document = newDocument;
	if (document){
		connect(document, SIGNAL(BgaChanged()), this, SLOT(OnBgaChanged()));
	}
	setEnabled(document != nullptr);
	ReloadAll();
}

void BgaView::OnBgaChanged()
{
	ReloadAll();
}

int BgaView::NextFreeHeaderId() const
{
	if (!document)
		return 0;
	const auto &headers = document->GetBga().headers;
	int id = 0;
	while (headers.contains(id))
		id++;
	return id;
}

int BgaView::NextFreeEventLocation(BgaLayer layer) const
{
	if (!document)
		return 0;
	const auto &events = document->GetBgaEvents(layer);
	if (events.isEmpty())
		return 0;
	int resolution = document->GetInfo()->GetResolution();
	if (resolution <= 0)
		resolution = 1;
	return events.lastKey() + resolution;
}

void BgaView::ReloadAll()
{
	ReloadHeaders();
	for (int i = 0; i < 3; i++)
		ReloadLane(lanes[i]);
}

void BgaView::ReloadHeaders()
{
	updating = true;
	headerTable->setRowCount(0);
	if (document){
		const auto &headers = document->GetBga().headers;
		int row = 0;
		for (auto i = headers.begin(); i != headers.end(); i++, row++){
			headerTable->insertRow(row);
			auto *idItem = new QTableWidgetItem(QString::number(i.key()));
			idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
			idItem->setData(LocationRole, i.key());
			headerTable->setItem(row, 0, idItem);
			headerTable->setItem(row, 1, new QTableWidgetItem(i.value().name));
		}
	}
	updating = false;
}

void BgaView::ReloadLane(const LaneTab &lane)
{
	updating = true;
	lane.table->setRowCount(0);
	if (document){
		const auto &events = document->GetBgaEvents(lane.layer);
		const auto &headers = document->GetBga().headers;
		int row = 0;
		for (auto i = events.begin(); i != events.end(); i++, row++){
			lane.table->insertRow(row);
			auto *locItem = new QTableWidgetItem(QString::number(i.key()));
			locItem->setData(LocationRole, i.key());
			lane.table->setItem(row, 0, locItem);
			auto *idItem = new QTableWidgetItem(QString::number(i.value().id));
			if (headers.contains(i.value().id))
				idItem->setToolTip(headers[i.value().id].name);
			lane.table->setItem(row, 1, idItem);
		}
	}
	updating = false;
}

void BgaView::AddHeader()
{
	if (!document)
		return;
	document->InsertBgaHeader(BgaHeader(NextFreeHeaderId(), QString()));
}

void BgaView::RemoveHeader()
{
	if (!document)
		return;
	auto rows = headerTable->selectionModel()->selectedRows();
	QList<int> ids;
	for (const auto &index : rows){
		auto *item = headerTable->item(index.row(), 0);
		if (item)
			ids.append(item->data(LocationRole).toInt());
	}
	for (int id : ids)
		document->RemoveBgaHeader(id);
}

void BgaView::HeaderCellChanged(int row, int column)
{
	if (updating || !document || column != 1)
		return;
	auto *idItem = headerTable->item(row, 0);
	auto *nameItem = headerTable->item(row, 1);
	if (!idItem || !nameItem)
		return;
	int id = idItem->data(LocationRole).toInt();
	document->InsertBgaHeader(BgaHeader(id, nameItem->text()));
}

void BgaView::AddEvent(const LaneTab &lane)
{
	if (!document)
		return;
	int id = document->GetBga().headers.isEmpty() ? 0 : document->GetBga().headers.firstKey();
	document->InsertBgaEvent(lane.layer, BgaEvent(NextFreeEventLocation(lane.layer), id));
}

void BgaView::RemoveEvent(const LaneTab &lane)
{
	if (!document)
		return;
	auto rows = lane.table->selectionModel()->selectedRows();
	QList<int> locations;
	for (const auto &index : rows){
		auto *item = lane.table->item(index.row(), 0);
		if (item)
			locations.append(item->data(LocationRole).toInt());
	}
	for (int location : locations)
		document->RemoveBgaEvent(lane.layer, location);
}

void BgaView::EventCellChanged(const LaneTab &lane, int row, int column)
{
	if (updating || !document)
		return;
	auto *locItem = lane.table->item(row, 0);
	auto *idItem = lane.table->item(row, 1);
	if (!locItem || !idItem)
		return;
	int oldLocation = locItem->data(LocationRole).toInt();
	bool okLoc = false, okId = false;
	int newLocation = locItem->text().toInt(&okLoc);
	int id = idItem->text().toInt(&okId);
	if (!okLoc || !okId || newLocation < 0){
		// invalid input: revert by reloading from the model
		ReloadLane(lane);
		return;
	}
	if (column == 0 && newLocation != oldLocation){
		document->RemoveBgaEvent(lane.layer, oldLocation);
	}
	document->InsertBgaEvent(lane.layer, BgaEvent(newLocation, id));
}

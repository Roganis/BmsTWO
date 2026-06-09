#include "ChartsView.h"
#include "MainWindow.h"
#include "document/Document.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QDir>
#include <QFileInfo>

ChartsView::ChartsView(MainWindow *mainWindow)
	: QWidget(mainWindow)
	, mainWindow(mainWindow)
	, document(nullptr)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);

	list = new QListWidget(this);
	list->setSelectionMode(QAbstractItemView::SingleSelection);
	layout->addWidget(list, 1);

	auto *buttons = new QHBoxLayout();
	buttons->addStretch(1);
	auto *refreshButton = new QPushButton(tr("Refresh"), this);
	buttons->addWidget(refreshButton);
	layout->addLayout(buttons);

	connect(list, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(OnItemActivated(QListWidgetItem*)));
	connect(list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnItemActivated(QListWidgetItem*)));
	connect(refreshButton, SIGNAL(clicked()), this, SLOT(Refresh()));
}

void ChartsView::ReplaceDocument(Document *newDocument)
{
	// Old document already deleted by MainWindow; bind only to the new one.
	document = newDocument;
	if (document)
		connect(document, SIGNAL(FilePathChanged()), this, SLOT(Refresh()));
	Refresh();
}

void ChartsView::Refresh()
{
	list->clear();
	if (!document)
		return;
	QDir dir = document->GetProjectDirectory(QDir::root());
	if (dir.isRoot())
		return; // untitled / unsaved: no folder to list yet

	QStringList filters;
	filters << "*.bmson" << "*.bms" << "*.bme" << "*.bml" << "*.pms";
	const QString current = document->GetFilePath().isEmpty()
			? QString()
			: QFileInfo(document->GetFilePath()).absoluteFilePath();

	for (const QFileInfo &fi : dir.entryInfoList(filters, QDir::Files, QDir::Name)){
		auto *item = new QListWidgetItem(fi.fileName());
		item->setData(Qt::UserRole, fi.absoluteFilePath());
		if (!current.isEmpty() && fi.absoluteFilePath() == current){
			QFont f = item->font();
			f.setBold(true);
			item->setFont(f);
			list->setCurrentItem(item);
		}
		list->addItem(item);
	}
}

void ChartsView::OnItemActivated(QListWidgetItem *item)
{
	if (!item || !document)
		return;
	const QString path = item->data(Qt::UserRole).toString();
	if (path.isEmpty())
		return;
	// already the open chart -> do nothing
	if (!document->GetFilePath().isEmpty()
		&& QFileInfo(document->GetFilePath()).absoluteFilePath() == QFileInfo(path).absoluteFilePath()){
		return;
	}
	mainWindow->OpenFiles(QStringList() << path);
}

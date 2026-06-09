#include "KeysoundView.h"
#include "MainWindow.h"
#include "document/Document.h"
#include "document/SoundChannel.h"
#include "document/History.h"
#include "sequence_view/SequenceView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QFileDialog>
#include <QDir>

KeysoundView::KeysoundView(MainWindow *mainWindow)
	: QWidget(mainWindow)
	, mainWindow(mainWindow)
	, document(nullptr)
{
	refreshTimer = new QTimer(this);
	refreshTimer->setSingleShot(true);
	refreshTimer->setInterval(150);
	connect(refreshTimer, SIGNAL(timeout()), this, SLOT(Refresh()));

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);

	list = new QListWidget(this);
	list->setSelectionMode(QAbstractItemView::SingleSelection);
	layout->addWidget(list, 1);

	auto *row = new QHBoxLayout();
	addButton = new QPushButton(tr("Add..."), this);
	removeButton = new QPushButton(tr("Remove"), this);
	upButton = new QPushButton(tr("Up"), this);
	downButton = new QPushButton(tr("Down"), this);
	replaceButton = new QPushButton(tr("Replace..."), this);
	row->addWidget(addButton);
	row->addWidget(removeButton);
	row->addWidget(upButton);
	row->addWidget(downButton);
	row->addWidget(replaceButton);
	layout->addLayout(row);

	connect(addButton, SIGNAL(clicked()), this, SLOT(OnAdd()));
	connect(removeButton, SIGNAL(clicked()), this, SLOT(OnRemove()));
	connect(upButton, SIGNAL(clicked()), this, SLOT(OnMoveUp()));
	connect(downButton, SIGNAL(clicked()), this, SLOT(OnMoveDown()));
	connect(replaceButton, SIGNAL(clicked()), this, SLOT(OnReplace()));
	connect(list, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(OnItemActivated(QListWidgetItem*)));
	connect(list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnItemActivated(QListWidgetItem*)));

	connect(mainWindow, SIGNAL(CurrentChannelChanged(int)), this, SLOT(HighlightCurrent(int)));

	setEnabled(false);
}

void KeysoundView::ReplaceDocument(Document *newDocument)
{
	// Old document already deleted by MainWindow; bind only to the new one.
	document = newDocument;
	if (document)
		connect(document->GetHistory(), SIGNAL(OnHistoryChanged()), this, SLOT(ScheduleRefresh()));
	setEnabled(document != nullptr);
	Refresh();
}

void KeysoundView::ScheduleRefresh()
{
	refreshTimer->start();
}

void KeysoundView::Refresh()
{
	list->clear();
	if (!document)
		return;
	const auto &channels = document->GetSoundChannels();
	for (int i = 0; i < channels.size(); i++){
		SoundChannel *ch = channels[i];
		list->addItem(tr("%1: %2  (%3 notes)")
					  .arg(i + 1)
					  .arg(ch->GetName())
					  .arg(ch->GetNotes().size()));
	}
	HighlightCurrent(mainWindow->GetCurrentChannel());
}

void KeysoundView::HighlightCurrent(int index)
{
	for (int i = 0; i < list->count(); i++){
		QFont f = list->item(i)->font();
		f.setBold(i == index);
		list->item(i)->setFont(f);
	}
	if (index >= 0 && index < list->count())
		list->setCurrentRow(index);
}

int KeysoundView::SelectedIndex() const
{
	return list->currentRow();
}

void KeysoundView::OnItemActivated(QListWidgetItem *item)
{
	if (!item || !document)
		return;
	int idx = list->row(item);
	mainWindow->SelectChannelIndex(idx);
}

void KeysoundView::OnAdd()
{
	mainWindow->AddKeysoundsInteractive();
}

void KeysoundView::OnRemove()
{
	int idx = SelectedIndex();
	if (!document || idx < 0 || idx >= document->GetSoundChannels().size())
		return;
	document->DestroySoundChannel(idx);
}

void KeysoundView::OnMoveUp()
{
	int idx = SelectedIndex();
	if (!document || idx <= 0)
		return;
	document->MoveSoundChannel(idx, idx - 1);
}

void KeysoundView::OnMoveDown()
{
	int idx = SelectedIndex();
	if (!document || idx < 0 || idx >= document->GetSoundChannels().size() - 1)
		return;
	document->MoveSoundChannel(idx, idx + 1);
}

void KeysoundView::OnReplace()
{
	int idx = SelectedIndex();
	if (!document || idx < 0 || idx >= document->GetSoundChannels().size())
		return;
	QString filters = tr("sound files (*.wav *.ogg)" ";;" "all files (*.*)");
	QString dir = document->GetProjectDirectory(QDir::home()).absolutePath();
	QString fileName = QFileDialog::getOpenFileName(this, tr("Replace Sound File"), dir, filters);
	if (fileName.isEmpty())
		return;
	document->GetSoundChannels()[idx]->SetSourceFile(fileName);
}

#include "StatsView.h"
#include "MainWindow.h"
#include "document/Document.h"
#include "document/SoundChannel.h"
#include "document/History.h"
#include "util/Theme.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <QMap>
#include <algorithm>

StatsView::StatsView(MainWindow *mainWindow)
	: QWidget(mainWindow)
	, mainWindow(mainWindow)
	, document(nullptr)
{
	refreshTimer = new QTimer(this);
	refreshTimer->setSingleShot(true);
	refreshTimer->setInterval(200); // debounce edit storms
	connect(refreshTimer, SIGNAL(timeout()), this, SLOT(Refresh()));

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(6, 6, 6, 6);

	auto *form = new QFormLayout();
	form->addRow(tr("Notes (total):"), totalValue = new QLabel("-"));
	form->addRow(tr("Playable notes:"), playableValue = new QLabel("-"));
	form->addRow(tr("BGM notes:"), bgmValue = new QLabel("-"));
	form->addRow(tr("Keysound channels:"), channelsValue = new QLabel("-"));
	form->addRow(tr("Length:"), lengthValue = new QLabel("-"));
	form->addRow(tr("Average NPS:"), avgNpsValue = new QLabel("-"));
	form->addRow(tr("Peak NPS (1s):"), peakNpsValue = new QLabel("-"));
	layout->addLayout(form);

	if (Theme::IsModern()){
		QFont mono = Theme::MonospaceFont();
		for (QLabel *l : { totalValue, playableValue, bgmValue, channelsValue, lengthValue, avgNpsValue, peakNpsValue })
			l->setFont(mono);
	}

	auto *laneLabel = new QLabel(tr("Per-lane distribution:"));
	layout->addWidget(laneLabel);
	laneTable = new QTableWidget(0, 2, this);
	laneTable->setHorizontalHeaderLabels(QStringList() << tr("Lane") << tr("Notes"));
	laneTable->horizontalHeader()->setStretchLastSection(true);
	laneTable->verticalHeader()->setVisible(false);
	laneTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	laneTable->setSelectionMode(QAbstractItemView::NoSelection);
	if (Theme::IsModern())
		laneTable->setFont(Theme::MonospaceFont());
	layout->addWidget(laneTable, 1);

	setEnabled(false);
}

void StatsView::ReplaceDocument(Document *newDocument)
{
	// Old document is already deleted by MainWindow; do not touch it (Qt drops
	// the connections automatically). Bind only to the new one.
	document = newDocument;
	if (document){
		connect(document->GetHistory(), SIGNAL(OnHistoryChanged()), this, SLOT(ScheduleRefresh()));
	}
	setEnabled(document != nullptr);
	Refresh();
}

void StatsView::ScheduleRefresh()
{
	refreshTimer->start();
}

void StatsView::Refresh()
{
	if (!document){
		totalValue->setText("-");
		playableValue->setText("-");
		bgmValue->setText("-");
		channelsValue->setText("-");
		lengthValue->setText("-");
		avgNpsValue->setText("-");
		peakNpsValue->setText("-");
		laneTable->setRowCount(0);
		return;
	}

	const auto &channels = document->GetSoundChannels();
	int total = 0, bgm = 0, playable = 0;
	QMap<int, int> perLane;          // lane -> count
	QList<double> playableTimes;     // absolute seconds of playable notes

	for (SoundChannel *ch : channels){
		const auto &notes = ch->GetNotes();
		for (auto it = notes.begin(); it != notes.end(); ++it){
			const SoundNote &note = it.value();
			total++;
			perLane[note.lane]++;
			if (note.lane == 0){
				bgm++;
			}else{
				playable++;
				playableTimes.append(document->GetAbsoluteTime(note.location));
			}
		}
	}

	totalValue->setText(QString::number(total));
	playableValue->setText(QString::number(playable));
	bgmValue->setText(QString::number(bgm));
	channelsValue->setText(QString::number(channels.size()));

	// Length: absolute time at the end of the chart.
	const double lengthSec = document->GetAbsoluteTime(document->GetTotalLength());
	const int mins = int(lengthSec) / 60;
	const double secs = lengthSec - mins * 60;
	lengthValue->setText(QString("%1:%2 (%3 s)")
						 .arg(mins)
						 .arg(secs, 5, 'f', 2, QChar('0'))
						 .arg(lengthSec, 0, 'f', 2));

	avgNpsValue->setText(lengthSec > 0.0
						 ? QString::number(playable / lengthSec, 'f', 2)
						 : QString("-"));

	// Peak NPS: max number of playable notes in any 1-second sliding window.
	int peak = 0;
	if (!playableTimes.isEmpty()){
		std::sort(playableTimes.begin(), playableTimes.end());
		int lo = 0;
		for (int hi = 0; hi < playableTimes.size(); hi++){
			while (playableTimes[hi] - playableTimes[lo] >= 1.0)
				lo++;
			peak = std::max(peak, hi - lo + 1);
		}
	}
	peakNpsValue->setText(QString::number(peak));

	// Per-lane table (lane 0 shown as BGM).
	laneTable->setRowCount(perLane.size());
	int row = 0;
	for (auto it = perLane.begin(); it != perLane.end(); ++it, ++row){
		const QString laneName = it.key() == 0 ? tr("BGM") : QString::number(it.key());
		laneTable->setItem(row, 0, new QTableWidgetItem(laneName));
		laneTable->setItem(row, 1, new QTableWidgetItem(QString::number(it.value())));
	}
}

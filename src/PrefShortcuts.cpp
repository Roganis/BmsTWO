#include "PrefShortcuts.h"
#include "util/ShortcutManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QHash>
#include <QSet>
#include <QAction>
#include <QHeaderView>

PrefShortcutsPage::PrefShortcutsPage(QWidget *parent)
	: QWidget(parent)
{
	auto layout = new QVBoxLayout();

	tree = new QTreeWidget();
	tree->setColumnCount(2);
	tree->setHeaderLabels(QStringList() << tr("Command") << tr("Shortcut"));
	tree->setRootIsDecorated(true);
	tree->setSelectionMode(QAbstractItemView::NoSelection);
	tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	layout->addWidget(tree, 1);

	conflictLabel = new QLabel();
	conflictLabel->setStyleSheet("color:#d05050;");
	conflictLabel->setWordWrap(true);
	layout->addWidget(conflictLabel);

	auto buttons = new QHBoxLayout();
	buttons->addStretch(1);
	auto resetButton = new QPushButton(tr("Reset All to Defaults"));
	buttons->addWidget(resetButton);
	layout->addLayout(buttons);

	setLayout(layout);

	connect(resetButton, SIGNAL(clicked()), this, SLOT(OnResetAll()));
}

void PrefShortcutsPage::load()
{
	tree->clear();
	rows.clear();

	QHash<QString, QTreeWidgetItem*> categoryItems;
	for (const auto &e : ShortcutManager::Instance()->Entries()){
		QTreeWidgetItem *parent = categoryItems.value(e.category, nullptr);
		if (!parent){
			parent = new QTreeWidgetItem(tree, QStringList() << e.category);
			parent->setFirstColumnSpanned(true);
			parent->setExpanded(true);
			categoryItems.insert(e.category, parent);
		}
		auto *item = new QTreeWidgetItem(parent, QStringList() << e.name);
		auto *editor = new QKeySequenceEdit(tree);
		editor->setKeySequence(e.action ? e.action->shortcut() : e.defaultSeq);
		tree->setItemWidget(item, 1, editor);
		rows.append({ e.id, item, editor });
		connect(editor, SIGNAL(keySequenceChanged(QKeySequence)), this, SLOT(CheckConflicts()));
	}
	CheckConflicts();
}

void PrefShortcutsPage::store()
{
	for (const Row &r : rows){
		ShortcutManager::Instance()->SetShortcut(r.id, r.editor->keySequence());
	}
}

void PrefShortcutsPage::OnResetAll()
{
	ShortcutManager::Instance()->ResetAll();
	load(); // re-read the (now default) sequences back into the editors
}

void PrefShortcutsPage::CheckConflicts()
{
	// Group rows by their (non-empty) sequence; any group with >1 member is a clash.
	QHash<QString, QList<int>> bySeq;
	for (int i = 0; i < rows.size(); i++){
		const QKeySequence seq = rows[i].editor->keySequence();
		if (!seq.isEmpty())
			bySeq[seq.toString()].append(i);
	}
	QStringList clashing;
	QSet<int> conflictedRows;
	for (auto it = bySeq.begin(); it != bySeq.end(); ++it){
		if (it.value().size() > 1){
			clashing.append(it.key());
			for (int i : it.value())
				conflictedRows.insert(i);
		}
	}
	for (int i = 0; i < rows.size(); i++){
		QColor bg = conflictedRows.contains(i) ? QColor(120, 40, 40) : QColor(Qt::transparent);
		rows[i].item->setBackground(0, bg);
		rows[i].item->setBackground(1, bg);
	}
	if (clashing.isEmpty()){
		conflictLabel->clear();
	}else{
		conflictLabel->setText(tr("Conflicting shortcuts: %1").arg(clashing.join(", ")));
	}
}

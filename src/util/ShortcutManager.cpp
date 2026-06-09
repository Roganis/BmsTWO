#include "ShortcutManager.h"
#include "../MainWindow.h" // App::Instance()->GetSettings()
#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QSettings>

static const char *SettingsGroup = "Shortcuts";

ShortcutManager *ShortcutManager::Instance()
{
	static ShortcutManager instance;
	return &instance;
}

QString ShortcutManager::CleanText(const QString &text)
{
	QString s = text;
	s.remove('&');     // drop menu-accelerator markers
	return s.trimmed();
}

void ShortcutManager::RegisterMenuBar(QMenuBar *bar)
{
	if (!bar)
		return;
	for (QAction *a : bar->actions()){
		if (a->menu()){
			RegisterMenu(a->menu(), CleanText(a->text()));
		}
	}
}

void ShortcutManager::RegisterMenu(QMenu *menu, const QString &category)
{
	QSettings *settings = App::Instance()->GetSettings();
	for (QAction *a : menu->actions()){
		if (a->isSeparator())
			continue;
		if (a->menu()){
			// nested submenu: keep grouping under the same top-level category
			RegisterMenu(a->menu(), category);
			continue;
		}
		const QString name = CleanText(a->text());
		if (name.isEmpty())
			continue;
		const QString id = category + "/" + name;
		// de-dup: an action object may be referenced from more than one menu
		bool dup = false;
		for (const Entry &e : entries){
			if (e.id == id || e.action == a){ dup = true; break; }
		}
		if (dup)
			continue;

		Entry entry;
		entry.id = id;
		entry.category = category;
		entry.name = name;
		entry.action = a;
		entry.defaultSeq = a->shortcut();
		entries.append(entry);

		// apply a saved override if one exists (empty value == intentionally unbound)
		const QString key = QString(SettingsGroup) + "/" + id;
		if (settings->contains(key)){
			a->setShortcut(QKeySequence::fromString(settings->value(key).toString()));
		}
	}
}

ShortcutManager::Entry *ShortcutManager::Find(const QString &id)
{
	for (Entry &e : entries){
		if (e.id == id)
			return &e;
	}
	return nullptr;
}

void ShortcutManager::SetShortcut(const QString &id, const QKeySequence &seq)
{
	Entry *e = Find(id);
	if (!e || !e->action)
		return;
	e->action->setShortcut(seq);
	QSettings *settings = App::Instance()->GetSettings();
	const QString key = QString(SettingsGroup) + "/" + id;
	if (seq == e->defaultSeq){
		settings->remove(key); // back to default -> no override stored
	}else{
		settings->setValue(key, seq.toString());
	}
}

void ShortcutManager::ResetToDefault(const QString &id)
{
	Entry *e = Find(id);
	if (!e || !e->action)
		return;
	e->action->setShortcut(e->defaultSeq);
	App::Instance()->GetSettings()->remove(QString(SettingsGroup) + "/" + id);
}

void ShortcutManager::ResetAll()
{
	for (Entry &e : entries){
		if (e.action)
			e.action->setShortcut(e.defaultSeq);
	}
	App::Instance()->GetSettings()->remove(SettingsGroup);
}

QStringList ShortcutManager::Conflicts(const QKeySequence &seq, const QString &exceptId) const
{
	QStringList result;
	if (seq.isEmpty())
		return result;
	for (const Entry &e : entries){
		if (e.id == exceptId || !e.action)
			continue;
		if (e.action->shortcut() == seq)
			result.append(e.id);
	}
	return result;
}

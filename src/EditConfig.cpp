#include "EditConfig.h"
#include "MainWindow.h"
#include <algorithm>

EditConfig *EditConfig::instance = nullptr;

EditConfig::EditConfig()
{
	snappedHitTestInEditMode = App::Instance()->GetSettings()->value(SettingsSnappedHitTestInEditModeKey, false).toBool();
	alwaysShowCursorLineInEditMode = App::Instance()->GetSettings()->value(SettingsAlwaysShowCursorLineInEditModeKey, false).toBool();
	snappedSelectionInEditMode = App::Instance()->GetSettings()->value(SettingsSnappedSelectionInEditModeKey, true).toBool();
	lockKeysoundLane = App::Instance()->GetSettings()->value(SettingsLockKeysoundLaneKey, false).toBool();
	// autosave config is read on demand (see GetEnableAutosave / GetAutosaveInterval)
}

EditConfig *EditConfig::Instance()
{
	if (!instance){
		instance = new EditConfig();
	}
	return instance;
}


const char* EditConfig::SettingsEnableMasterChannelKey = "Edit/EnableMasterChannel";
const char* EditConfig::SettingsShowMasterLaneKey = "SequenceView/ShowMasterLane";
const char* EditConfig::SettingsShowMiniMapKey = "SequenceView/ShowMiniMap";
const char* EditConfig::SettingsFixMiniMapKey = "SequenceView/FixMiniMap";
const char* EditConfig::SettingsMiniMapOpacityKey = "SequenceView/MiniMapOpacity";

const char* EditConfig::SettingsSnappedHitTestInEditModeKey = "SequenceView/SnappedHitTestInEditMode";
const char* EditConfig::SettingsAlwaysShowCursorLineInEditModeKey = "SequenceView/AlwaysShowCursorLineInEditMode";
const char* EditConfig::SettingsSnappedSelectionInEditModeKey = "SequenceView/SnappedSelectionInEditMode";
const char* EditConfig::SettingsLockKeysoundLaneKey = "SequenceView/LockKeysoundLane";
const char* EditConfig::SettingsEnableAutosaveKey = "Edit/EnableAutosave";
const char* EditConfig::SettingsAutosaveIntervalKey = "Edit/AutosaveIntervalSec";


bool EditConfig::GetEnableMasterChannel()
{
	return App::Instance()->GetSettings()->value(SettingsEnableMasterChannelKey, true).toBool();
}

bool EditConfig::GetShowMasterLane()
{
	return App::Instance()->GetSettings()->value(SettingsShowMasterLaneKey, true).toBool();
}

bool EditConfig::GetShowMiniMap()
{
	return App::Instance()->GetSettings()->value(SettingsShowMiniMapKey, true).toBool();
}

bool EditConfig::GetFixMiniMap()
{
	return App::Instance()->GetSettings()->value(SettingsFixMiniMapKey, false).toBool();
}

double EditConfig::GetMiniMapOpacity()
{
	return App::Instance()->GetSettings()->value(SettingsMiniMapOpacityKey, 0.67).toDouble();
}

void EditConfig::SetEnableMasterChannel(bool value)
{
	App::Instance()->GetSettings()->setValue(SettingsEnableMasterChannelKey, value);
	emit Instance()->EnableMasterChannelChanged(value);
	emit Instance()->CanShowMiniMapChanged(value && GetShowMiniMap());
	emit Instance()->CanShowMasterLaneChanged(value && GetShowMasterLane());
}

void EditConfig::SetShowMasterLane(bool value)
{
	App::Instance()->GetSettings()->setValue(SettingsShowMasterLaneKey, value);
	emit Instance()->ShowMasterLaneChanged(value);
	emit Instance()->CanShowMasterLaneChanged(value && GetEnableMasterChannel());
}

void EditConfig::SetShowMiniMap(bool value)
{
	App::Instance()->GetSettings()->setValue(SettingsShowMiniMapKey, value);
	emit Instance()->ShowMiniMapChanged(value);
	emit Instance()->CanShowMiniMapChanged(value && GetEnableMasterChannel());
}

void EditConfig::SetFixMiniMap(bool value)
{
	App::Instance()->GetSettings()->setValue(SettingsFixMiniMapKey, value);
	emit Instance()->FixMiniMapChanged(value);
}

void EditConfig::SetMiniMapOpacity(double value)
{
	App::Instance()->GetSettings()->setValue(SettingsMiniMapOpacityKey, value);
	emit Instance()->MiniMapOpacityChanged(value);
}




bool EditConfig::CanShowMasterLane()
{
	return GetShowMasterLane() && GetEnableMasterChannel();
}

bool EditConfig::CanShowMiniMap()
{
	return GetShowMiniMap() && GetEnableMasterChannel();
}




bool EditConfig::SnappedHitTestInEditMode()
{
	return Instance()->snappedHitTestInEditMode;
}

bool EditConfig::AlwaysShowCursorLineInEditMode()
{
	return Instance()->alwaysShowCursorLineInEditMode;
}

bool EditConfig::SnappedSelectionInEditMode()
{
	return Instance()->snappedSelectionInEditMode;
}


void EditConfig::SetSnappedHitTestInEditMode(bool value)
{
	App::Instance()->GetSettings()->setValue(SettingsSnappedHitTestInEditModeKey, Instance()->snappedHitTestInEditMode = value);
}

void EditConfig::SetAlwaysShowCursorLineInEditMode(bool value)
{
	App::Instance()->GetSettings()->setValue(SettingsAlwaysShowCursorLineInEditModeKey, Instance()->alwaysShowCursorLineInEditMode = value);
}

void EditConfig::SetSnappedSelectionInEditMode(bool value)
{
	App::Instance()->GetSettings()->setValue(SettingsSnappedSelectionInEditModeKey, Instance()->snappedSelectionInEditMode = value);
}

bool EditConfig::LockKeysoundLane()
{
	return Instance()->lockKeysoundLane;
}

void EditConfig::SetLockKeysoundLane(bool value)
{
	App::Instance()->GetSettings()->setValue(SettingsLockKeysoundLaneKey, Instance()->lockKeysoundLane = value);
}

bool EditConfig::GetEnableAutosave()
{
	return App::Instance()->GetSettings()->value(SettingsEnableAutosaveKey, true).toBool();
}

int EditConfig::GetAutosaveInterval()
{
	int seconds = App::Instance()->GetSettings()->value(SettingsAutosaveIntervalKey, 180).toInt();
	// clamp to a sane range: 15s .. 1h
	return std::max(15, std::min(3600, seconds));
}

void EditConfig::SetEnableAutosave(bool value)
{
	App::Instance()->GetSettings()->setValue(SettingsEnableAutosaveKey, value);
	emit Instance()->AutosaveConfigChanged();
}

void EditConfig::SetAutosaveInterval(int seconds)
{
	App::Instance()->GetSettings()->setValue(SettingsAutosaveIntervalKey, std::max(15, std::min(3600, seconds)));
	emit Instance()->AutosaveConfigChanged();
}




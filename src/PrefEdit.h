#ifndef PREFEDIT
#define PREFEDIT

#include <QObject>
#include <QWidget>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>

class Preferences;

class PrefEditPage : public QWidget
{
	Q_OBJECT

private:
	QCheckBox *master;
	QWidget *masterGroup;
	QCheckBox *showMasterLane;
	QCheckBox *showMiniMap;
	QWidget *miniMapGroup;
	QCheckBox *fixMiniMap;
	QSlider *miniMapOpacity;
	QSlider *laneBackgroundBrightness;
	QCheckBox *snappedHitTestInEditMode;
	QCheckBox *alwaysShowCursorLineInEditMode;
	QCheckBox *snappedSelectionInEditMode;
	QCheckBox *enableAutosave;
	QSpinBox *autosaveInterval;

public:
	PrefEditPage(QWidget *parent);

	void load();
	void store();

private slots:
	void MasterChanged(bool value);
	void ShowMiniMapChanged(bool value);
	void FixMiniMapChanged(bool value);
};


#endif // PREFEDIT

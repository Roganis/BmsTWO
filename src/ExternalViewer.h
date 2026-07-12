#ifndef EXTERNALVIEWER_H
#define EXTERNALVIEWER_H

#include <QObject>
#include <QFile>
#include <QProcess>
#include <QMessageBox>
#include <QTimer>


class Document;
class MainWindow;
class QSettings;

struct ExternalViewerConfig
{
	QString displayName;
	QString programPath;
	QString iconPath;
	QString argumentFormatPlayBeg;
	QString argumentFormatPlayHere;
	QString argumentFormatPlayRegion; // optional; empty = PlayHere + timed Stop
	QString argumentFormatStop;
	QString executionDirectory;
};


class ExternalViewer : public QObject
{
	Q_OBJECT

private:
	MainWindow *mainWindow;
	QSettings *settingsCache; // App-owned settings, cached for safe use in dtor
	Document *document;
	QString tempFilePath;
	QFile tempFile;
	QTimer stopTimer;

	QList<ExternalViewerConfig> config;
	int configIndex;

	static const char *SettingsGroup;
	static const char *SettingsViewerCountKey;
	static const char *SettingsCurrentViewerKey;
	static const char *SettingsViewerGroupFormat;
	static const char *SettingsViewerDisplayNameKey;
	static const char *SettingsViewerProgramPathKey;
	static const char *SettingsViewerIconPathKey;
	static const char *SettingsViewerArgumentFormatPlayBegKey;
	static const char *SettingsViewerArgumentFormatPlayHereKey;
	static const char *SettingsViewerArgumentFormatPlayRegionKey;
	static const char *SettingsViewerArgumentFormatStopKey;
	static const char *SettingsViewerExecutionDirectoryFormatKey;

public:
	explicit ExternalViewer(MainWindow *mainWindow = nullptr);
	~ExternalViewer();

	void ReplaceDocument(Document *document);
	QList<ExternalViewerConfig> GetConfig() const { return config; }
	int GetCurrentConfigIndex() const { return configIndex; }
	void SetConfig(QList<ExternalViewerConfig> config, int index);
	void SetConfigIndex(int index);
	bool IsPlayable() const;

	int GetBarNumber(int time) const;
	int GetBarStartTick(int barNumber) const; // -1 if barNumber is beyond the last bar line

	static ExternalViewerConfig CreateNewConfig();

signals:
	void ConfigChanged();
	void ConfigIndexChanged(int index);

public slots:
	void PlayBeg();
	void Play(int time=0);
	void PlayRegion(int timeBegin, int timeEnd); // timeEnd < 0 = end of chart
	void Stop();

private:
	void RunCommand(QString path, QString argument, QString executeDir);
	QString EvalArgument(QString argumentFormat, QMap<QString, QString> env);
	QMap<QString, QString> Environment(int time=-1, int timeEnd=-1);
	bool PrepareTempFile();
	bool Clean();
};


#endif // EXTERNALVIEWER_H

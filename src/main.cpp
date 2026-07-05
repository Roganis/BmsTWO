#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>
#include <QLibraryInfo>
#include "MainWindow.h"
#include "AppInfo.h"
#include <QApplication>
#include <QMetaType>
#include "document/SoundChannelDef.h"
#include "util/CrashHandler.h"
#include "util/Theme.h"


int main(int argc, char *argv[])
{
	// Install crash handler first so even early faults produce a stack trace.
	CrashHandler::Install(QStandardPaths::writableLocation(QStandardPaths::TempLocation));

	qRegisterMetaType<QList<RmsCacheEntry>>("QList<RmsCacheEntry>");
	qRegisterMetaType<QList<QString>>("QList<QString>");
	qRegisterMetaType<BmsonIO::BmsonVersion>("BmsonIO::BmsonVersion");

    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	// Crisp rendering on fractional-DPI displays (blurriness reads as "old").
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

	Q_INIT_RESOURCE(bmstwo);

	App app(argc, argv);
	return app.exec();
}


const char* App::SettingsVersionKey = "ConfigVersion";
const char* App::SettingsLanguageKey = "Language";
const char* App::SettingsModernThemeKey = "ModernTheme";
const int App::SettingsVersion = 1;

App::App(int argc, char *argv[])
	: QApplication(argc, argv)
	, settings(nullptr)
	, mainWindow(nullptr)
{
	QString settingsDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
	if (settingsDir.isEmpty()){
		settings = new QSettings(ORGANIZATION_NAME, APP_NAME);
	}else{
		settings = new QSettings(QDir(settingsDir).filePath("Settings.ini"), QSettings::IniFormat);
	}
	int version = settings->value(SettingsVersionKey, 0).toInt();
	if (version != SettingsVersion){
		settings->clear();
		settings->setValue(SettingsVersionKey, SettingsVersion);
	}
	QString systemLocale = QLocale::system().name();
	QString locale = settings->value(SettingsLanguageKey).toString();
	if (locale.isNull() || locale.isEmpty()){
		locale = systemLocale;
	}

	QTranslator *translator = new QTranslator(this);
	if (translator->load(":/i18n/" + locale)){
		installTranslator(translator);
	}

	QTranslator *qtTranslator = new QTranslator(this);
	if (qtTranslator->load(locale, "qt", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath))){
		installTranslator(qtTranslator);
	}

	QTranslator *qtBaseTranslator = new QTranslator(this);
	if (qtBaseTranslator->load("qtbase_" + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath))){
		installTranslator(qtBaseTranslator);
	}

	// Phase 4: modern dark look via Fusion + a centralized dark palette,
	// toggleable in Preferences (Classic = the original native look).
	Theme::SetModern(settings->value(SettingsModernThemeKey, true).toBool());
	Theme::Apply(this);

	mainWindow = new MainWindow(settings);
	const bool autoExportMode = qEnvironmentVariableIsSet("BMSTWO_AUTOEXPORT");
	// Offer to recover work left behind by a crashed session before we touch
	// any file the user asked to open on the command line. Skipped in headless
	// auto-export mode, where a modal recovery prompt would block forever.
	bool recovered = autoExportMode ? false : mainWindow->CheckForCrashRecovery();
	if (arguments().size() > 1 && !recovered){
		QStringList filePaths = arguments().mid(1);
		mainWindow->OpenFiles(filePaths);
	}
	mainWindow->show();

	// Diagnostics/automation: BMSTWO_AUTOEXPORT=<out.wav> exports the just-opened
	// chart through the real export path, then quits. No GUI interaction needed.
	QByteArray autoExport = qgetenv("BMSTWO_AUTOEXPORT");
	if (!autoExport.isEmpty()){
		QString outPath = QString::fromLocal8Bit(autoExport);
		MainWindow *mw = mainWindow;
		// Give the just-opened document a moment to settle before exporting.
		QTimer::singleShot(5000, mw, [mw, outPath]{ mw->HeadlessExport(outPath); });
	}
}

App::~App()
{
	if (mainWindow) delete mainWindow;
	if (settings) delete settings;
}

App *App::Instance()
{
	return dynamic_cast<App*>(qApp);
}

bool App::event(QEvent *e)
{
	if (e->type() == QEvent::FileOpen){
		QFileOpenEvent *fileOpenEvent = dynamic_cast<QFileOpenEvent*>(e);
		QStringList filePaths;
		filePaths.append(fileOpenEvent->file());
		mainWindow->OpenFiles(filePaths);
		return true;
	}
	return QApplication::event(e);
}




#if 0

void dummy4tr(){
	QScrollBar::tr("Scroll here");
	QScrollBar::tr("Top");
	QScrollBar::tr("Bottom");
	QScrollBar::tr("Scroll up");
	QScrollBar::tr("Scroll down");
	QScrollBar::tr("Page up");
	QScrollBar::tr("Page down");
	QScrollBar::tr("Left edge");
	QScrollBar::tr("Right edge");
	QScrollBar::tr("Scroll left");
	QScrollBar::tr("Scroll right");
	QScrollBar::tr("Page left");
	QScrollBar::tr("Page right");

	QMessageBox::tr("Save");
	QMessageBox::tr("Discard");
	QMessageBox::tr("Cancel");
}

#endif

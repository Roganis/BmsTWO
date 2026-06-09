#include "MainWindow.h"
#include "sequence_view/SequenceView.h"
#include "InfoView.h"
#include "ChannelInfoView.h"
#include "BgaView.h"
#include "StopView.h"
#include "StatsView.h"
#include "SelectionView.h"
#include "ChartsView.h"
#include "BpmEditTool.h"
#include "NoteEditTool.h"
#include "document/History.h"
#include <QActionGroup> // Qt6: no longer pulled in transitively
#include <QtMultimedia/QMediaPlayer>
#include "util/UIDef.h"
#include "util/KeySeq.h"
#include "AppInfo.h"
#include "util/SymbolIconManager.h"
#include "Preferences.h"
#include "ViewMode.h"
#include "ExternalViewer.h"
#include "ExternalViewerTools.h"
#include "EditConfig.h"
#include "util/ShortcutManager.h"
#include "MasterOutDialog.h"
#include "bmson/Bmson.h"
#include "bms/Bms.h"
#include "bms/BmsImportDialog.h"
#include <QTimer>
#include <QLockFile>
#include <QDialog>
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>


const char* MainWindow::SettingsGroup = "MainWindow";
const char* MainWindow::SettingsGeometryKey = "Geometry";
const char* MainWindow::SettingsWindowStateKey = "WindowState";
const char* MainWindow::SettingsWidgetsStateKey = "WidgetsState";
const char* MainWindow::SettingsHideInactiveSelectedViewKey = "HideInactiveSelectedView";

const QSize UIUtil::ToolBarIconSize(18, 18);
const int UIUtil::LightAnimationInterval = 8;
const int UIUtil::HeavyAnimationInterval = 16;

MainWindow::MainWindow(QSettings *settings)
	: QMainWindow()
	, settings(settings)
	, document(nullptr)
    , viewMode(ViewMode::ViewModeEZ5k())
	, currentChannel(-1)
	, closing(false)
	, autosaveTimer(nullptr)
	, recoveryLock(nullptr)
{
	UIUtil::SetFontMainWindow(this);
	setWindowIcon(QIcon(":/images/bmstwo64.png"));
	resize(960,640);
	setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AllowNestedDocks | QMainWindow::GroupedDragging);
	setUnifiedTitleAndToolBarOnMac(true);
	setAcceptDrops(true);

	externalViewer = new ExternalViewer(this);

	actionFileNew = new QAction(tr("New"), this);
	actionFileNew->setIcon(SymbolIconManager::GetIcon(SymbolIconManager::Icon::New));
	actionFileNew->setShortcut(QKeySequence::New);
	SharedUIHelper::RegisterGlobalShortcut(actionFileNew);
	QObject::connect(actionFileNew, SIGNAL(triggered()), this, SLOT(FileNew()));

	actionFileOpen = new QAction(tr("Open..."), this);
	actionFileOpen->setIcon(SymbolIconManager::GetIcon(SymbolIconManager::Icon::Open));
	actionFileOpen->setShortcut(QKeySequence::Open);
	SharedUIHelper::RegisterGlobalShortcut(actionFileOpen);
	QObject::connect(actionFileOpen, SIGNAL(triggered()), this, SLOT(FileOpen()));

	actionFileSave = new QAction(tr("Save"), this);
	actionFileSave->setIcon(SymbolIconManager::GetIcon(SymbolIconManager::Icon::Save));
	actionFileSave->setShortcut(QKeySequence::Save);
	SharedUIHelper::RegisterGlobalShortcut(actionFileSave);
	QObject::connect(actionFileSave, SIGNAL(triggered()), this, SLOT(FileSave()));

	actionFileSaveAs = new QAction(tr("Save As..."), this);
	actionFileSaveAs->setShortcut(QKeySequence::SaveAs);
	SharedUIHelper::RegisterGlobalShortcut(actionFileSaveAs);
	QObject::connect(actionFileSaveAs, SIGNAL(triggered()), this, SLOT(FileSaveAs()));

	actionFileMasterOut = new QAction(tr("Export WAV..."), this);
	actionFileMasterOut->setShortcut(KeySeq(Qt::ShiftModifier, Qt::Key_F5));
	SharedUIHelper::RegisterGlobalShortcut(actionFileMasterOut);
	QObject::connect(actionFileMasterOut, SIGNAL(triggered(bool)), this, SLOT(MasterOut()));

	actionFileQuit = new QAction(tr("Quit"), this);
#ifdef Q_OS_WIN
	actionFileQuit->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_Q));
#endif
	SharedUIHelper::RegisterGlobalShortcut(actionFileQuit);
	QObject::connect(actionFileQuit, SIGNAL(triggered()), this, SLOT(close()));

	actionEditUndo = new QAction(tr("Undo"), this);
	actionEditUndo->setIcon(SymbolIconManager::GetIcon(SymbolIconManager::Icon::Undo));
	actionEditUndo->setShortcuts(QKeySequence::Undo);
	SharedUIHelper::RegisterGlobalShortcut(actionEditUndo);
	QObject::connect(actionEditUndo, SIGNAL(triggered()), this, SLOT(EditUndo()));

	actionEditRedo = new QAction(tr("Redo"), this);
	actionEditRedo->setIcon(SymbolIconManager::GetIcon(SymbolIconManager::Icon::Redo));
	actionEditRedo->setShortcuts(QKeySequence::Redo);
	SharedUIHelper::RegisterGlobalShortcut(actionEditRedo);
	QObject::connect(actionEditRedo, SIGNAL(triggered()), this, SLOT(EditRedo()));

	actionEditCut = new QAction(tr("Cut"), this);
	actionEditCut->setShortcut(QKeySequence::Cut);
	SharedUIHelper::RegisterGlobalShortcut(actionEditCut);
	actionEditCut->setEnabled(false);

	actionEditCopy = new QAction(tr("Copy"), this);
	actionEditCopy->setShortcut(QKeySequence::Copy);
	SharedUIHelper::RegisterGlobalShortcut(actionEditCopy);
	actionEditCopy->setEnabled(false);

	actionEditPaste = new QAction(tr("Paste"), this);
	actionEditPaste->setShortcut(QKeySequence::Paste);
	SharedUIHelper::RegisterGlobalShortcut(actionEditPaste);
	actionEditPaste->setEnabled(false);

	actionEditSelectAll = new QAction(tr("Select All"), this);
	actionEditSelectAll->setShortcut(QKeySequence::SelectAll);
	SharedUIHelper::RegisterGlobalShortcut(actionEditSelectAll);
	actionEditSelectAll->setEnabled(false);

	actionEditDelete = new QAction(tr("Delete"), this);
	//actionEditDelete->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_D));
	SharedUIHelper::RegisterGlobalShortcut(actionEditDelete);
	actionEditDelete->setEnabled(false);

	actionEditTransferToKey = new QAction(tr("Move to Key Lanes"), this);
	//actionEditTransferToKey->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_G));
	SharedUIHelper::RegisterGlobalShortcut(actionEditTransferToKey);
	actionEditTransferToKey->setEnabled(false);

	actionEditTransferToBgm = new QAction(tr("Move to BGM Lanes"), this);
	//actionEditTransfer->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_G));
	SharedUIHelper::RegisterGlobalShortcut(actionEditTransferToBgm);
	actionEditTransferToBgm->setEnabled(false);

	actionEditSeparateLayeredNotes = new QAction(tr("Separate Layered Notes"), this);
	SharedUIHelper::RegisterGlobalShortcut(actionEditSeparateLayeredNotes);
	actionEditSeparateLayeredNotes->setEnabled(false);

	actionEditFillNotes = new QAction(tr("Fill Notes"), this);
	actionEditFillNotes->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_F));
	SharedUIHelper::RegisterGlobalShortcut(actionEditFillNotes);
	actionEditFillNotes->setEnabled(false);

	actionEditToggleBarLine = new QAction(tr("Toggle Bar Line at Cursor"), this);
	actionEditToggleBarLine->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_B));
	SharedUIHelper::RegisterGlobalShortcut(actionEditToggleBarLine);
	actionEditToggleBarLine->setEnabled(false);

	actionEditModeEdit = new QAction(tr("Edit Mode"), this);
	SharedUIHelper::RegisterGlobalShortcut(actionEditModeEdit);
	actionEditModeEdit->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_1));

	actionEditModeWrite = new QAction(tr("Write Mode"), this);
	SharedUIHelper::RegisterGlobalShortcut(actionEditModeWrite);
	actionEditModeWrite->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_2));

	actionEditModeInteractive = new QAction(tr("Interactive Mode"), this);
	SharedUIHelper::RegisterGlobalShortcut(actionEditModeInteractive);
	actionEditModeInteractive->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_3));

	actionEditLockCreation = new QAction(tr("Lock Note Creation"), this);
	SharedUIHelper::RegisterGlobalShortcut(actionEditLockCreation);
	actionEditLockCreation->setShortcut(KeySeq(Qt::AltModifier, Qt::ShiftModifier, Qt::Key_N));

	actionEditLockDeletion = new QAction(tr("Lock Note Deletion"), this);
	SharedUIHelper::RegisterGlobalShortcut(actionEditLockDeletion);
	actionEditLockDeletion->setShortcut(KeySeq(Qt::AltModifier, Qt::ShiftModifier, Qt::Key_D));

	actionEditLockVerticalMove = new QAction(tr("Lock Keysound (Lane Move)"), this);
	SharedUIHelper::RegisterGlobalShortcut(actionEditLockVerticalMove);
	actionEditLockVerticalMove->setShortcut(KeySeq(Qt::AltModifier, Qt::ShiftModifier, Qt::Key_V));
	actionEditLockVerticalMove->setCheckable(true);
	actionEditLockVerticalMove->setChecked(EditConfig::LockKeysoundLane());
	actionEditLockVerticalMove->setStatusTip(tr("Prevent dragging notes from changing their lane / keysound assignment"));
	connect(actionEditLockVerticalMove, &QAction::triggered, this, [](bool checked){
		EditConfig::SetLockKeysoundLane(checked);
	});

	actionEditGoTo = new QAction(tr("Go To..."), this);
	SharedUIHelper::RegisterGlobalShortcut(actionEditGoTo);
	actionEditGoTo->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_G));
	connect(actionEditGoTo, SIGNAL(triggered()), this, SLOT(EditGoTo()));

	actionEditToggleLongNote = new QAction(tr("Toggle Long Note"), this);
	SharedUIHelper::RegisterGlobalShortcut(actionEditToggleLongNote);
	actionEditToggleLongNote->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_L));
	actionEditToggleLongNote->setStatusTip(tr("Flip selected notes between normal and long notes"));
	connect(actionEditToggleLongNote, &QAction::triggered, this, [this](){
		if (sequenceView) sequenceView->ToggleLongNotes();
	});

	actionEditPlay = new QAction(tr("Play"), this);
	SharedUIHelper::RegisterGlobalShortcut(actionEditPlay);
	actionEditPlay->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_P));

	actionEditClearMasterCache = new QAction(tr("Clear Master Cache"), this);
	SharedUIHelper::RegisterGlobalShortcut(actionEditClearMasterCache);
	connect(actionEditClearMasterCache, SIGNAL(triggered(bool)), this, SLOT(ClearMasterCache()));

	actionEditPreferences = new QAction(tr("Preferences..."), this);
	actionEditPreferences->setIcon(SymbolIconManager::GetIcon(SymbolIconManager::Icon::Settings));
	actionEditPreferences->setShortcut(QKeySequence::Preferences);
	SharedUIHelper::RegisterGlobalShortcut(actionEditPreferences);
	connect(actionEditPreferences, SIGNAL(triggered()), this, SLOT(EditPreferences()));

	actionViewFullScreen = new QAction(tr("Toggle Full Screen"), this);
	actionViewFullScreen->setShortcut(QKeySequence::FullScreen);
	SharedUIHelper::RegisterGlobalShortcut(actionViewFullScreen);
	connect(actionViewFullScreen, SIGNAL(triggered()), this, SLOT(ViewFullScreen()));

	actionViewSnapToGrid = new QAction(tr("Snap to Grid"), this);
	actionViewSnapToGrid->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_T));
	SharedUIHelper::RegisterGlobalShortcut(actionViewSnapToGrid);

	actionViewDarkenNotesInInactiveChannels = new QAction(tr("Darken Notes in Inactive Channels"), this);
	actionViewDarkenNotesInInactiveChannels->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_D));
	SharedUIHelper::RegisterGlobalShortcut(actionViewDarkenNotesInInactiveChannels);

	actionViewZoomIn = new QAction(tr("Zoom In"), this);
	actionViewZoomIn->setShortcut(QKeySequence::ZoomIn);
	SharedUIHelper::RegisterGlobalShortcut(actionViewZoomIn);

	actionViewZoomOut = new QAction(tr("Zoom Out"), this);
	actionViewZoomOut->setShortcut(QKeySequence::ZoomOut);
	SharedUIHelper::RegisterGlobalShortcut(actionViewZoomOut);

	actionViewZoomReset = new QAction(tr("Reset", "Zoom"), this);
	actionViewZoomReset->setShortcut(KeySeq(Qt::ControlModifier, Qt::Key_0));
	SharedUIHelper::RegisterGlobalShortcut(actionViewZoomReset);

	actionChannelNew = new QAction(tr("Add..."), this);
	actionChannelNew->setShortcut(KeySeq(Qt::ControlModifier, Qt::ShiftModifier, Qt::Key_N));
	SharedUIHelper::RegisterGlobalShortcut(actionChannelNew);
	connect(actionChannelNew, SIGNAL(triggered()), this, SLOT(ChannelNew()));

	actionChannelImportMidi = new QAction(tr("Import MIDI to Channel..."), this);
	SharedUIHelper::RegisterGlobalShortcut(actionChannelImportMidi);
	connect(actionChannelImportMidi, SIGNAL(triggered()), this, SLOT(ChannelImportMidi()));

	actionChannelPrev = new QAction(tr("Select Previous"), this);
	actionChannelPrev->setShortcut(QKeySequence::Back);
	SharedUIHelper::RegisterGlobalShortcut(actionChannelPrev);
	connect(actionChannelPrev, SIGNAL(triggered()), this, SLOT(ChannelPrev()));

	actionChannelNext = new QAction(tr("Select Next"), this);
	actionChannelNext->setShortcut(QKeySequence::Forward);
	SharedUIHelper::RegisterGlobalShortcut(actionChannelNext);
	connect(actionChannelNext, SIGNAL(triggered()), this, SLOT(ChannelNext()));

	actionChannelMoveLeft = new QAction(tr("Move Left"), this);
	actionChannelMoveLeft->setShortcut(KeySeq(Qt::ControlModifier, Qt::AltModifier, Qt::Key_Left));
	SharedUIHelper::RegisterGlobalShortcut(actionChannelMoveLeft);
	connect(actionChannelMoveLeft, SIGNAL(triggered()), this, SLOT(ChannelMoveLeft()));

	actionChannelMoveRight = new QAction(tr("Move Right"), this);
	actionChannelMoveRight->setShortcut(KeySeq(Qt::ControlModifier, Qt::AltModifier, Qt::Key_Right));
	SharedUIHelper::RegisterGlobalShortcut(actionChannelMoveRight);
	connect(actionChannelMoveRight, SIGNAL(triggered()), this, SLOT(ChannelMoveRight()));

	actionChannelDestroy = new QAction(tr("Delete"), this);
	SharedUIHelper::RegisterGlobalShortcut(actionChannelDestroy);
	connect(actionChannelDestroy, SIGNAL(triggered()), this, SLOT(ChannelDestroy()));

	actionChannelSelectFile = new QAction(tr("Select File..."), this);
	SharedUIHelper::RegisterGlobalShortcut(actionChannelSelectFile);
	connect(actionChannelSelectFile, SIGNAL(triggered()), this, SLOT(ChannelSelectFile()));

	actionChannelPreviewSource = new QAction(tr("Preview Source Sound"), this);
	actionChannelPreviewSource->setIcon(SymbolIconManager::GetIcon(SymbolIconManager::Icon::Sound));
	SharedUIHelper::RegisterGlobalShortcut(actionChannelPreviewSource);
	connect(actionChannelPreviewSource, SIGNAL(triggered(bool)), this, SLOT(ChannelPreviewSource()));

	actionHelpAbout = new QAction(tr("About BmsTWO..."), this);
	SharedUIHelper::RegisterGlobalShortcut(actionHelpAbout);
	connect(actionHelpAbout, SIGNAL(triggered()), this, SLOT(HelpAbout()));
#if defined(Q_OS_OSX) || defined(Q_OS_MACOS)
	actionHelpAboutQt = new QAction(tr("Qt..."), this);
#else
	actionHelpAboutQt = new QAction(tr("About Qt..."), this);
#endif
	SharedUIHelper::RegisterGlobalShortcut(actionHelpAboutQt);
	connect(actionHelpAboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

	auto *menuFile = menuBar()->addMenu(tr("File"));
	menuFile->addAction(actionFileNew);
	menuFile->addAction(actionFileOpen);
	menuFile->addSeparator();
	menuFile->addAction(actionFileSave);
	menuFile->addAction(actionFileSaveAs);
	menuFile->addSeparator();
	menuFile->addAction(actionFileMasterOut);
#ifdef Q_OS_WIN
	menuFile->addSeparator();
	menuFile->addAction(actionFileQuit);
#endif
	auto *menuEdit = menuBar()->addMenu(tr("Edit"));
	menuEdit->addAction(actionEditUndo);
	menuEdit->addAction(actionEditRedo);
	menuEdit->addSeparator();
	menuEdit->addAction(actionEditCut);
	menuEdit->addAction(actionEditCopy);
	menuEdit->addAction(actionEditPaste);
#if 0 // Select All is not implemented yet
	menuEdit->addSeparator();
	menuEdit->addAction(actionEditSelectAll);
#endif
	menuEdit->addSeparator();
	menuEdit->addAction(actionEditDelete);
	menuEdit->addAction(actionEditTransferToKey);
	menuEdit->addAction(actionEditTransferToBgm);
	menuEdit->addAction(actionEditSeparateLayeredNotes);
	menuEdit->addAction(actionEditFillNotes);
	menuEdit->addSeparator();
	menuEdit->addAction(actionEditToggleBarLine);
	menuEdit->addAction(actionEditToggleLongNote);
	menuEdit->addSeparator();
	menuEdit->addAction(actionEditModeEdit);
	menuEdit->addAction(actionEditModeWrite);
	//menuEdit->addAction(actionEditModeInteractive);
	menuEdit->addSeparator();
	menuEdit->addAction(actionEditLockVerticalMove); // keysound/lane lock (#1)
	menuEdit->addAction(actionEditGoTo);
#if 0
	menuEdit->addSeparator();
	menuEdit->addAction(actionEditLockCreation);
	menuEdit->addAction(actionEditLockDeletion);
	menuEdit->addSeparator();
	menuEdit->addAction(actionEditPlay);
#endif
	menuEdit->addSeparator();
	menuEdit->addAction(actionEditClearMasterCache);
	menuEdit->addSeparator();
	menuEdit->addAction(actionEditPreferences);
	menuView = menuBar()->addMenu(tr("View"));
	auto *menuViewDockBars = menuView->addMenu(tr("Views"));
	actionViewDockSeparator = menuViewDockBars->addSeparator();
	auto *menuViewToolBars = menuView->addMenu(tr("Toolbars"));
	actionViewTbSeparator = menuViewToolBars->addSeparator();
	menuView->addSeparator();
	menuViewMode = menuView->addMenu(tr("View Mode"));
	actionViewSkinSettingSeparator = menuView->addSeparator();
	menuView->addAction(actionViewDarkenNotesInInactiveChannels);
	menuView->addAction(actionViewSnapToGrid);
	auto *menuViewZoom = menuView->addMenu(tr("Zoom"));
	menuViewZoom->addAction(actionViewZoomIn);
	menuViewZoom->addAction(actionViewZoomOut);
	menuViewZoom->addSeparator();
	menuViewZoom->addAction(actionViewZoomReset);
	menuViewChannelLane = menuView->addMenu(tr("Sound Channel Lanes Display"));
	menuView->addSeparator();
	menuView->addAction(actionViewFullScreen);

	auto *menuChannel = menuBar()->addMenu(tr("Channel"));
	menuChannel->addAction(actionChannelNew);
	menuChannel->addSeparator();
	menuChannel->addAction(actionChannelPrev);
	menuChannel->addAction(actionChannelNext);
	menuChannelFind = menuChannel->addMenu(tr("Find"));
	menuChannel->addSeparator();
	menuChannel->addAction(actionChannelMoveLeft);
	menuChannel->addAction(actionChannelMoveRight);
	menuChannel->addSeparator();
	menuChannel->addAction(actionChannelDestroy);
	menuChannel->addSeparator();
	menuChannel->addAction(actionChannelSelectFile);
	menuChannel->addAction(actionChannelPreviewSource);
	menuChannel->addSeparator();
	menuChannel->addAction(actionChannelImportMidi);

	menuPreview = menuBar()->addMenu(tr("Preview"));

	auto *menuHelp = menuBar()->addMenu(tr("Help"));
	menuHelp->addAction(actionHelpAbout);
	menuHelp->addAction(actionHelpAboutQt);

	{
		auto viewModeGroup = new QActionGroup(this);
		auto viewModes = ViewMode::GetAllViewModes();
		for (auto mode : viewModes){
			auto action = menuViewMode->addAction(mode->GetName());
			action->setCheckable(true);
			action->setChecked(mode == viewMode);
			connect(action, &QAction::triggered, this, [mode, this](){
				viewMode = mode;
				emit ViewModeChanged(mode);
			});
			viewModeGroup->addAction(action);
			viewModeActionMap.insert(mode, action);
		}
        delete viewModeGroup;
	}

	//setStatusBar(statusBar = new StatusBar(this));
	auto *statusToolBar = new QToolBar(tr("Status Bar"));
	UIUtil::SetFont(statusToolBar);
	statusToolBar->setObjectName("Status Bar");
	statusToolBar->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
	statusToolBar->addWidget(statusBar = new StatusBar(this));
	addToolBar(Qt::BottomToolBarArea, statusToolBar);
	menuViewToolBars->insertAction(actionViewTbSeparator, statusToolBar->toggleViewAction());

	auto *fileTools = new QToolBar(tr("File"));
	UIUtil::SetFont(fileTools);
	fileTools->setObjectName("File Tools");
	fileTools->addAction(actionFileNew);
	fileTools->addAction(actionFileOpen);
	fileTools->addAction(actionFileSave);
	fileTools->setIconSize(UIUtil::ToolBarIconSize);
	addToolBar(fileTools);
	menuViewToolBars->insertAction(actionViewTbSeparator, fileTools->toggleViewAction());

	auto *editTools = new QToolBar(tr("Edit"));
	UIUtil::SetFont(editTools);
	editTools->setObjectName("Edit Tools");
	editTools->addAction(actionEditUndo);
	editTools->addAction(actionEditRedo);
	editTools->setIconSize(UIUtil::ToolBarIconSize);
	addToolBar(editTools);
	menuViewToolBars->insertAction(actionViewTbSeparator, editTools->toggleViewAction());

	sequenceTools = new SequenceTools("Sequence Tools", tr("Sequence Tools"), this);
	UIUtil::SetFont(sequenceTools);
	sequenceTools->setIconSize(UIUtil::ToolBarIconSize);
	addToolBar(sequenceTools);
	menuViewToolBars->insertAction(actionViewTbSeparator, sequenceTools->toggleViewAction());

	audioPlayer = new AudioPlayer(this, "Sound Output", tr("Sound Output"));
	UIUtil::SetFont(audioPlayer);
	audioPlayer->setIconSize(UIUtil::ToolBarIconSize);
	addToolBar(audioPlayer);
	menuViewToolBars->insertAction(actionViewTbSeparator, audioPlayer->toggleViewAction());

	externalViewerTools = new ExternalViewerTools("External Viewer Tools", tr("Preview"), this);
	UIUtil::SetFont(externalViewerTools);
	externalViewerTools->setIconSize(UIUtil::ToolBarIconSize);
	addToolBar(externalViewerTools);
	menuViewToolBars->insertAction(actionViewTbSeparator, externalViewerTools->toggleViewAction());

	channelFindTools = new ChannelFindTools("Channel Find Tools", tr("Find Channels"), this);
	UIUtil::SetFont(channelFindTools);
	channelFindTools->setIconSize(UIUtil::ToolBarIconSize);
	addToolBar(channelFindTools);
	menuViewToolBars->insertAction(actionViewTbSeparator, channelFindTools->toggleViewAction());

	sequenceView = new SequenceView(this);
	setCentralWidget(sequenceView);
	//sequenceView->installEventFilter(this);

	preferences = new Preferences(this);
	UIUtil::SetFont(preferences);
	connect(BmsonIO::Instance(), SIGNAL(SaveFormatChanged(BmsonIO::BmsonVersion)), this, SLOT(SaveFormatChanged(BmsonIO::BmsonVersion)));

	auto dock = new QDockWidget(tr("Info"));
	UIUtil::SetFont(dock);
	dock->setObjectName("Info");
	dock->setWidget(infoView = new InfoView(this));
	addDockWidget(Qt::LeftDockWidgetArea, dock);
	menuViewDockBars->insertAction(actionViewDockSeparator, dock->toggleViewAction());

	auto dock2 = new QDockWidget(tr("Channel"));
	UIUtil::SetFont(dock2);
	dock2->setObjectName("Channel");
	dock2->setWidget(channelInfoView = new ChannelInfoView(this));
	addDockWidget(Qt::LeftDockWidgetArea, dock2);
	dock2->resize(334, dock2->height());
	menuViewDockBars->insertAction(actionViewDockSeparator, dock2->toggleViewAction());

	auto dockBga = new QDockWidget(tr("BGA"));
	UIUtil::SetFont(dockBga);
	dockBga->setObjectName("Bga");
	dockBga->setWidget(bgaView = new BgaView(this));
	addDockWidget(Qt::LeftDockWidgetArea, dockBga);
	dockBga->hide(); // off by default; available from View > Docks
	menuViewDockBars->insertAction(actionViewDockSeparator, dockBga->toggleViewAction());

	auto dockStop = new QDockWidget(tr("Stops"));
	UIUtil::SetFont(dockStop);
	dockStop->setObjectName("Stops");
	dockStop->setWidget(stopView = new StopView(this));
	addDockWidget(Qt::LeftDockWidgetArea, dockStop);
	dockStop->hide(); // off by default; available from View > Docks
	menuViewDockBars->insertAction(actionViewDockSeparator, dockStop->toggleViewAction());

	auto dockStats = new QDockWidget(tr("Statistics"));
	UIUtil::SetFont(dockStats);
	dockStats->setObjectName("Statistics");
	dockStats->setWidget(statsView = new StatsView(this));
	addDockWidget(Qt::LeftDockWidgetArea, dockStats);
	dockStats->hide(); // off by default; available from View > Docks
	menuViewDockBars->insertAction(actionViewDockSeparator, dockStats->toggleViewAction());

	auto dockSelection = new QDockWidget(tr("Selection"));
	UIUtil::SetFont(dockSelection);
	dockSelection->setObjectName("Selection");
	dockSelection->setWidget(selectionView = new SelectionView(this));
	addDockWidget(Qt::LeftDockWidgetArea, dockSelection);
	dockSelection->hide(); // off by default; available from View > Docks
	menuViewDockBars->insertAction(actionViewDockSeparator, dockSelection->toggleViewAction());
	selectionView->Bind(sequenceView);
	connect(sequenceView, SIGNAL(SelectionChanged()), selectionView, SLOT(Refresh()));

	auto dockCharts = new QDockWidget(tr("Charts"));
	UIUtil::SetFont(dockCharts);
	dockCharts->setObjectName("Charts");
	dockCharts->setWidget(chartsView = new ChartsView(this));
	addDockWidget(Qt::LeftDockWidgetArea, dockCharts);
	dockCharts->hide(); // off by default; available from View > Docks
	menuViewDockBars->insertAction(actionViewDockSeparator, dockCharts->toggleViewAction());

	selectedObjectsDockWidget = new QDockWidget(tr("Selected Objects"));
	UIUtil::SetFont(selectedObjectsDockWidget);
	selectedObjectsDockWidget->setObjectName("SelectedObjectsDock");
	addDockWidget(Qt::LeftDockWidgetArea, selectedObjectsDockWidget);
	menuViewDockBars->insertAction(actionViewDockSeparator, selectedObjectsDockWidget->toggleViewAction());

	bpmEditView = new BpmEditView(this);
	connect(bpmEditView, SIGNAL(Updated()), this, SLOT(OnBpmEdited()));

	noteEditView = new NoteEditView(this);
	//connect(noteEditView, SIGNAL(Updated()), this, SLOT());

	// View Mdoe Binding
	connect(this, SIGNAL(ViewModeChanged(ViewMode*)), sequenceView, SLOT(ViewModeChanged(ViewMode*)));

	// Current Channel Binding
	connect(channelInfoView, SIGNAL(CurrentChannelChanged(int)), this, SLOT(OnCurrentChannelChanged(int)));
	connect(sequenceView, SIGNAL(CurrentChannelChanged(int)), this, SLOT(OnCurrentChannelChanged(int)));
	connect(this, SIGNAL(CurrentChannelChanged(int)), channelInfoView, SLOT(OnCurrentChannelChanged(int)));
	connect(this, SIGNAL(CurrentChannelChanged(int)), sequenceView, SLOT(OnCurrentChannelChanged(int)));
	connect(channelInfoView, SIGNAL(CurrentChannelChanged(int)), sequenceView, SLOT(OnCurrentChannelChanged(int)));
	connect(sequenceView, SIGNAL(CurrentChannelChanged(int)), channelInfoView, SLOT(OnCurrentChannelChanged(int)));

	// SequenceView-SequenceTools Initial Binding
	sequenceTools->ReplaceSequenceView(sequenceView);
	channelFindTools->ReplaceSequenceView(sequenceView);
	noteEditView->ReplaceSequenceView(sequenceView);

	// Other Binding
	connect(EditConfig::Instance(), SIGNAL(EnableMasterChannelChanged(bool)), this, SLOT(EnableMasterChannelChanged(bool)));
	EnableMasterChannelChanged(EditConfig::Instance()->GetEnableMasterChannel());

	// Initial Document
	auto newDocument = new Document(this);
	newDocument->Initialize();
	ReplaceDocument(newDocument);

	SetupAutosave();


	// Load Settings for MainWindow
	settings->beginGroup(SettingsGroup);
	{
		Qt::WindowStates statesMask = Qt::WindowMaximized | Qt::WindowFullScreen;
		auto states = (Qt::WindowStates)settings->value(SettingsWindowStateKey, 0).toInt() & statesMask;
		if (states != 0){
			// do not set position
			setWindowState(states);
		}else{
			if (settings->contains(SettingsGeometryKey)){
				setGeometry(settings->value(SettingsGeometryKey).toRect());
			}
		}

		if (settings->contains(SettingsWidgetsStateKey)){
			restoreState(settings->value(SettingsWidgetsStateKey).toByteArray());
		}else{
			restoreState(saveState());
		}

		//if (settings->value(SettingsHideInactiveSelectedViewKey, true).toBool()){
		//	selectedObjectView->hide();
		//}
	}
	settings->endGroup(); // MainWindow

	// preparation for startup
	bpmEditView->hide();
	noteEditView->hide();

	// Register all menu commands for user-configurable shortcuts and apply any
	// saved overrides (done last so dynamically-added submenu items are included).
	ShortcutManager::Instance()->RegisterMenuBar(menuBar());
}

MainWindow::~MainWindow()
{
	settings->beginGroup(SettingsGroup);
	settings->setValue(SettingsGeometryKey, geometry());
	settings->setValue(SettingsWindowStateKey, (int)windowState());
	settings->setValue(SettingsWidgetsStateKey, saveState());
	settings->endGroup(); // MainWindow

	if (recoveryLock){
		recoveryLock->unlock();
		delete recoveryLock;
		recoveryLock = nullptr;
	}
}

// ===================== Autosave / crash recovery =====================

QString MainWindow::RecoveryDir()
{
	QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if (base.isEmpty())
		base = QDir::tempPath();
	return QDir(base).filePath("Recovery");
}

void MainWindow::SetupAutosave()
{
	recoverySessionId = QString::number(QCoreApplication::applicationPid())
			+ "_" + QString::number(QDateTime::currentMSecsSinceEpoch());
	autosaveTimer = new QTimer(this);
	connect(autosaveTimer, &QTimer::timeout, this, &MainWindow::PerformAutosave);
	connect(EditConfig::Instance(), &EditConfig::AutosaveConfigChanged, this, &MainWindow::ReconfigureAutosave);
	ReconfigureAutosave();
}

void MainWindow::ReconfigureAutosave()
{
	if (!autosaveTimer)
		return;
	if (EditConfig::GetEnableAutosave()){
		autosaveTimer->start(EditConfig::GetAutosaveInterval() * 1000);
	}else{
		autosaveTimer->stop();
		DiscardRecoverySnapshot();
	}
}

void MainWindow::PerformAutosave()
{
	if (!document)
		return;
	if (!document->GetHistory()->IsDirty()){
		// nothing unsaved -> make sure no stale snapshot lingers
		DiscardRecoverySnapshot();
		return;
	}
	QDir dir(RecoveryDir());
	if (!dir.exists())
		QDir().mkpath(dir.path());
	// Hold a session lock so a future launch can tell a crash (dead owner) from
	// a still-running instance (live owner). Acquired lazily on first snapshot.
	if (!recoveryLock){
		recoveryLock = new QLockFile(dir.filePath("recovery_" + recoverySessionId + ".lock"));
		recoveryLock->tryLock(10);
	}
	const QString snap = dir.filePath("recovery_" + recoverySessionId + ".bmson");
	try{
		document->ExportTo(snap); // does not touch the document's own save state
	}catch(...){
		return; // best-effort: never interrupt editing
	}
	recoverySnapshotPath = snap;
	settings->beginGroup("Recovery");
	settings->beginGroup(recoverySessionId);
	settings->setValue("snapshot", snap);
	settings->setValue("original", document->GetFilePath());
	settings->setValue("time", QDateTime::currentDateTime().toString(Qt::ISODate));
	settings->endGroup();
	settings->endGroup();
	settings->sync(); // make sure the registry survives a subsequent crash
}

void MainWindow::DiscardRecoverySnapshot()
{
	if (!recoverySnapshotPath.isEmpty()){
		QFile::remove(recoverySnapshotPath);
		recoverySnapshotPath.clear();
	}
	if (!recoverySessionId.isEmpty()){
		settings->beginGroup("Recovery");
		settings->remove(recoverySessionId);
		settings->endGroup();
		settings->sync();
	}
}

void MainWindow::OpenRecovery(const QString &snapshotPath, const QString &originalPath)
{
	if (!EnsureClosingFile())
		return;
	try{
		auto *doc = new Document(this);
		doc->LoadFile(snapshotPath);
		ReplaceDocument(doc);
		// point it at the original location (or leave untitled) and mark dirty
		doc->SetRecoveredFilePath(originalPath);
	}catch(...){
		QMessageBox::critical(this, tr("Error"), tr("Failed to load the recovery snapshot."));
	}
}

bool MainWindow::CheckForCrashRecovery()
{
	settings->beginGroup("Recovery");
	const QStringList sessions = settings->childGroups();
	settings->endGroup();
	if (sessions.isEmpty())
		return false;

	QDir dir(RecoveryDir());
	bool recovered = false;
	for (const QString &sid : sessions){
		if (sid == recoverySessionId)
			continue; // never our own live session

		settings->beginGroup("Recovery");
		settings->beginGroup(sid);
		const QString snap = settings->value("snapshot").toString();
		const QString original = settings->value("original").toString();
		const QString time = settings->value("time").toString();
		settings->endGroup();
		settings->endGroup();

		// Liveness check: if the owning instance is still running we can't take
		// its lock -> it didn't crash, leave it alone.
		QLockFile probe(dir.filePath("recovery_" + sid + ".lock"));
		probe.setStaleLockTime(1000);
		if (!probe.tryLock(10))
			continue;
		probe.unlock();

		auto forget = [&](){
			QFile::remove(dir.filePath("recovery_" + sid + ".lock"));
			settings->beginGroup("Recovery");
			settings->remove(sid);
			settings->endGroup();
		};

		if (snap.isEmpty() || !QFile::exists(snap)){
			forget();
			continue;
		}

		const QString name = original.isEmpty() ? tr("(untitled)") : QFileInfo(original).fileName();
		auto res = QMessageBox::question(
			this, tr("Recover unsaved work?"),
			tr("BmsTWO did not close normally.\n\nRecover unsaved changes to \"%1\"?\n(autosaved %2)")
				.arg(name, time),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
		if (res == QMessageBox::Yes){
			OpenRecovery(snap, original);
			QFile::remove(snap);
			forget();
			recovered = true;
			break; // single-document: recover one, leave any others for next launch
		}else{
			QFile::remove(snap);
			forget();
		}
	}
	return recovered;
}

void MainWindow::FileNew()
{
	if (!EnsureClosingFile())
		return;
	auto *newDocument = new Document(this);
	newDocument->Initialize();
	ReplaceDocument(newDocument);
}

void MainWindow::FileOpen()
{
	if (!EnsureClosingFile())
		return;
	QString filters = tr("bmson files (*.bmson)"
						 ";;" "legacy bms files (*.bms *.bme *.bml *.pms)"
						 ";;" "all files (*.*)");
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open"), QString(), filters, 0);
	if (fileName.isEmpty())
		return;
	QString ext = QFileInfo(fileName).suffix().toLower();
	if (BmsIO::IsBmsFileExtension(ext)){
		OpenBms(fileName);
	}else{
		OpenBmson(fileName);
	}
}

void MainWindow::FileOpen(QString path)
{
	if (path.isEmpty())
		return;
	if (!EnsureClosingFile())
		return;
	QString ext = QFileInfo(path).suffix().toLower();
	if (BmsIO::IsBmsFileExtension(ext)){
		OpenBms(path);
	}else{
		OpenBmson(path);
	}
}

void MainWindow::FileSave()
{
	Save();
}

void MainWindow::FileSaveAs()
{
	SharedUIHelper::CommitDirtyEdit();
	try{
		QString filters = tr("bmson files (*.bmson)"
							 ";;" "all files (*.*)");
		QString defaultPath = document->GetFilePath();
		QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"), defaultPath.isEmpty() ? document->GetProjectDirectory().path() : defaultPath, filters, 0);
		if (fileName.isEmpty())
			return;
		document->SaveAs(fileName);
	}catch(...){
		QMessageBox *msgbox = new QMessageBox(
					QMessageBox::Warning,
					tr("Error"),
					tr("Failed to save file."),
					QMessageBox::Ok,
					this);
		msgbox->show();
	}
}

void MainWindow::MasterOut()
{
	if (!document)
		return;
	auto dialog = new MasterOutDialog(document, this);
	dialog->exec();
	delete dialog;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	if (closing){
		event->accept();
		return;
	}
	if (!EnsureClosingFile()){
		event->ignore();
		return;
	}
	closing = true;
	// Clean shutdown: drop the recovery snapshot so we don't offer to recover
	// on the next launch.
	DiscardRecoverySnapshot();
	event->accept();
}

SequenceView *MainWindow::GetActiveSequenceView() const
{
	return sequenceView;
}

void MainWindow::SetSelectedObjectsView(QWidget *view)
{
	selectedObjectsDockWidget->setWidget(view);
	if (view != nullptr){
		selectedObjectsDockWidget->show();
		selectedObjectsDockWidget->activateWindow();
		selectedObjectsDockWidget->raise();
	}
}

void MainWindow::UnsetSelectedObjectsView(QWidget *view)
{
	if (selectedObjectsDockWidget->widget() == view){
		selectedObjectsDockWidget->setWidget(nullptr);
	}
}

void MainWindow::EditUndo()
{
	SharedUIHelper::CommitDirtyEdit();
	if (!document->GetHistory()->CanUndo())
		return;
	document->GetHistory()->Undo();
}

void MainWindow::EditRedo()
{
	SharedUIHelper::CommitDirtyEdit();
	if (!document->GetHistory()->CanRedo())
		return;
	document->GetHistory()->Redo();
}

void MainWindow::EditPreferences()
{
	preferences->exec();
}

void MainWindow::ViewFullScreen()
{
	if (isFullScreen()){
		showNormal();
	}else{
		showFullScreen();
	}
}

void MainWindow::ChannelNew()
{
	if (!document)
		return;
	QString filters = tr("sound files (*.wav *.ogg)"
						 ";;" "all files (*.*)");
	QString dir = document->GetProjectDirectory(QDir::home()).absolutePath();
	QStringList fileNames = QFileDialog::getOpenFileNames(this, tr("Select Sound Files"), dir, filters, 0);
	if (fileNames.empty())
		return;
	auto traversalPaths = document->FindTraversalFilePaths(fileNames);
	if (!traversalPaths.isEmpty()){
		if (!WarningFileTraversals(traversalPaths))
			return;
	}
	document->InsertNewSoundChannels(fileNames);
}

void MainWindow::ChannelPrev()
{
	if (!document || currentChannel < 0)
		return;
	if (currentChannel == 0){
		currentChannel = document->GetSoundChannels().size()-1;
	}else{
		currentChannel--;
	}
	emit CurrentChannelChanged(currentChannel);
}

void MainWindow::ChannelNext()
{
	if (!document || currentChannel < 0)
		return;
	if (currentChannel == document->GetSoundChannels().size()-1){
		currentChannel = 0;
	}else{
		currentChannel++;
	}
	emit CurrentChannelChanged(currentChannel);
}

void MainWindow::ChannelMoveLeft()
{
	if (!document || currentChannel < 0)
		return;
	document->MoveSoundChannel(currentChannel, currentChannel-1);
}

void MainWindow::ChannelMoveRight()
{
	if (!document || currentChannel < 0)
		return;
	document->MoveSoundChannel(currentChannel, currentChannel+1);
}

void MainWindow::ChannelDestroy()
{
	if (!document || currentChannel < 0)
		return;
	document->DestroySoundChannel(currentChannel);
}

void MainWindow::ChannelSelectFile()
{
	if (!document || currentChannel < 0)
		return;
	channelInfoView->SelectSourceFile();
}

void MainWindow::ChannelPreviewSource()
{
	if (!document || currentChannel < 0)
		return;
	channelInfoView->PreviewSound();
}

void MainWindow::ChannelImportMidi()
{
	if (!document)
		return;
	SequenceView *sview = GetActiveSequenceView();
	if (!sview)
		return;
	QString filters = tr("MIDI files (*.mid *.midi)") + ";;" + tr("all files (*.*)");
	QString dir = document->GetProjectDirectory(QDir::home()).absolutePath();
	QString fileName = QFileDialog::getOpenFileName(this, tr("Import MIDI to Channel"), dir, filters);
	if (fileName.isEmpty())
		return;
	sview->ImportMidi(fileName);
}

void MainWindow::ChannelsNew(QList<QString> filePaths)
{
	if (!document)
		return;
	auto traversalPaths = document->FindTraversalFilePaths(filePaths);
	if (!traversalPaths.isEmpty()){
		if (!WarningFileTraversals(traversalPaths))
			return;
	}
	document->InsertNewSoundChannels(filePaths);
}

void MainWindow::HelpAbout()
{
	QString text = "<h2>" APP_NAME "</h2>"
			"<p>version " APP_VERSION_STRING " " APP_PLATFORM_NAME " (" __DATE__ " " __TIME__ ")</p>"
			"<p><a href=\"" APP_URL "\">" APP_NAME "</a> is a continuation fork of "
			"<a href=\"https://github.com/excln/BmsONE\">BmsONE</a> by exclusion (Copyright 2015-2017), "
			"by way of the <a href=\"https://github.com/djkero/BmsONE\">djkero</a> fork.</p>"
			"<hr>"
			"Libraries Information:"
			"<ul>"
			"<li>" "<a href=\"" QT_URL "\">Qt</a> " QT_VERSION_STR
			"<li>" "<a href=\"" OGG_URL "\">libogg</a> " OGG_VERSION_STRING
			"<li>" "<a href=\"" VORBIS_URL "\">libvorbis</a> " + QString(vorbis_version_string()) +
			"</ul>";
	QMessageBox::about(this, tr("About BmsTWO"), text);
}

void MainWindow::FilePathChanged()
{
	QString title = document->GetFilePath();
	if (title.isEmpty()){
		title = tr("untitled");
	}else{
		title = QDir::toNativeSeparators(title);
	}
	title += " [*] - " APP_NAME;
	setWindowTitle(title);
	setWindowModified(document->GetHistory()->IsDirty());
}

void MainWindow::HistoryChanged()
{
	if (!document)
		return;
	QString undoName, redoName;
	if (document->GetHistory()->CanUndo(&undoName)){
		actionEditUndo->setEnabled(true);
		actionEditUndo->setText(tr("Undo - ") + undoName);
	}else{
		actionEditUndo->setEnabled(false);
		actionEditUndo->setText(tr("Undo"));
	}
	if (document->GetHistory()->CanRedo(&redoName)){
		actionEditRedo->setEnabled(true);
		actionEditRedo->setText(tr("Redo - ") + redoName);
	}else{
		actionEditRedo->setEnabled(false);
		actionEditRedo->setText(tr("Redo"));
	}
	FilePathChanged();
}

void MainWindow::OnCurrentChannelChanged(int ichannel)
{
	currentChannel = ichannel;
	if (currentChannel >= 0){
		actionChannelMoveLeft->setEnabled(true);
		actionChannelMoveRight->setEnabled(true);
		actionChannelDestroy->setEnabled(true);
		actionChannelSelectFile->setEnabled(true);
		actionChannelPreviewSource->setEnabled(true);
	}else{
		actionChannelMoveLeft->setEnabled(false);
		actionChannelMoveRight->setEnabled(false);
		actionChannelDestroy->setEnabled(false);
		actionChannelSelectFile->setEnabled(false);
		actionChannelPreviewSource->setEnabled(false);
	}
}

void MainWindow::OnTimeMappingChanged()
{
	if (!document)
		return;
	// don't manage bpmEditView's BPM events
	// (active SequenceView should do)
}

void MainWindow::OnNotesEdited()
{
	if (!document)
		return;
}

void MainWindow::OnBpmEdited()
{
	if (!document)
		return;
	auto bpmEvents = bpmEditView->GetBpmEvents();
	document->UpdateBpmEvents(bpmEvents);
}

void MainWindow::SaveFormatChanged(BmsonIO::BmsonVersion version)
{
	if (!document)
		return;
	document->SetOutputVersion(version);
}


void MainWindow::ChannelFindKeywordChanged(QString keyword)
{
	if (currentChannel < 0)
		return;
	if (SequenceViewUtil::MatchChannelNameKeyword(document->GetSoundChannels()[currentChannel]->GetName(), keyword))
		return;
	// similar to ChannelFindNext but does not beep
	QPair<int, int> range = sequenceView->GetVisibleRangeExtended();
	for (int i=currentChannel+1; i<document->GetSoundChannels().size(); i++){
		auto channel = document->GetSoundChannels()[i];
		if (SequenceViewUtil::MatchChannelNameKeyword(channel->GetName(), keyword)
			&& (!channelFindTools->FiltersActive() || channel->IsActiveInRegion(range.first, range.second)))
		{
			currentChannel = i;
			emit CurrentChannelChanged(currentChannel);
			return;
		}
	}
	for (int i=0; i<currentChannel; i++){
		auto channel = document->GetSoundChannels()[i];
		if (SequenceViewUtil::MatchChannelNameKeyword(channel->GetName(), keyword)
			&& (!channelFindTools->FiltersActive() || channel->IsActiveInRegion(range.first, range.second)))
		{
			currentChannel = i;
			emit CurrentChannelChanged(currentChannel);
			return;
		}
	}
}

void MainWindow::ChannelFindNext(QString keyword)
{
	if (currentChannel < 0)
		return;
	QPair<int, int> range = sequenceView->GetVisibleRangeExtended();
	for (int i=currentChannel+1; i<document->GetSoundChannels().size(); i++){
		auto channel = document->GetSoundChannels()[i];
		if (SequenceViewUtil::MatchChannelNameKeyword(channel->GetName(), keyword)
			&& (!channelFindTools->FiltersActive() || channel->IsActiveInRegion(range.first, range.second)))
		{
			currentChannel = i;
			emit CurrentChannelChanged(currentChannel);
			return;
		}
	}
	for (int i=0; i<=currentChannel; i++){
		auto channel = document->GetSoundChannels()[i];
		if (SequenceViewUtil::MatchChannelNameKeyword(channel->GetName(), keyword)
			&& (!channelFindTools->FiltersActive() || channel->IsActiveInRegion(range.first, range.second)))
		{
			currentChannel = i;
			emit CurrentChannelChanged(currentChannel);
			return;
		}
	}
	qApp->beep();
}

void MainWindow::ChannelFindPrev(QString keyword)
{
	if (currentChannel < 0)
		return;
	QPair<int, int> range = sequenceView->GetVisibleRangeExtended();
	for (int i=currentChannel-1; i>=0; i--){
		auto channel = document->GetSoundChannels()[i];
		if (SequenceViewUtil::MatchChannelNameKeyword(channel->GetName(), keyword)
			&& (!channelFindTools->FiltersActive() || channel->IsActiveInRegion(range.first, range.second)))
		{
			currentChannel = i;
			emit CurrentChannelChanged(currentChannel);
			return;
		}
	}
	for (int i=document->GetSoundChannels().size()-1; i>=currentChannel; i--){
		auto channel = document->GetSoundChannels()[i];
		if (SequenceViewUtil::MatchChannelNameKeyword(channel->GetName(), keyword)
			&& (!channelFindTools->FiltersActive() || channel->IsActiveInRegion(range.first, range.second)))
		{
			currentChannel = i;
			emit CurrentChannelChanged(currentChannel);
			return;
		}
	}
	qApp->beep();
}

void MainWindow::EnableMasterChannelChanged(bool value)
{
	actionEditClearMasterCache->setEnabled(value);
}

void MainWindow::ClearMasterCache()
{
	if (!document)
		return;
	// show confirmation?
	document->ReconstructMasterCache();
}

void MainWindow::SetViewMode(ViewMode *mode)
{
	if (viewModeActionMap.contains(mode)){
		viewModeActionMap[mode]->setChecked(true);
	}else{
		// hidden view mode?
		if (viewModeActionMap.contains(viewMode)){
			viewModeActionMap[viewMode]->setChecked(false);
		}
	}
	viewMode = mode;
	emit ViewModeChanged(mode);
}


void MainWindow::ReplaceDocument(Document *newDocument)
{
	if (document){
		bpmEditView->UnsetBpmEvents();
		noteEditView->UnsetNotes();
		delete document;
	}
	document = newDocument;
	if (!document)
		return;

	connect(document, &Document::FilePathChanged, this, &MainWindow::FilePathChanged);
	connect(document, &Document::TimeMappingChanged, this, &MainWindow::OnTimeMappingChanged);
	connect(document->GetHistory(), &EditHistory::OnHistoryChanged, this, &MainWindow::HistoryChanged);

	infoView->ReplaceDocument(document);
	channelInfoView->ReplaceDocument(document);
	bgaView->ReplaceDocument(document);
	stopView->ReplaceDocument(document);
	statsView->ReplaceDocument(document);
	chartsView->ReplaceDocument(document);
	sequenceView->ReplaceDocument(document);
	externalViewer->ReplaceDocument(document);

	document->SetOutputVersion(BmsonIO::GetSaveFormat());

	HistoryChanged(); // calls FilePathChanged()
	OnCurrentChannelChanged(-1);

	// begin interaction
	SetViewMode(ViewMode::GetViewMode(document->GetInfo()->GetModeHint()));
	channelInfoView->Begin();
}

bool MainWindow::Save()
{
	try{
		SharedUIHelper::CommitDirtyEdit();
		if (document->GetFilePath().isEmpty()){
			QString filters = tr("bmson files (*.bmson)"
								 ";;" "all files (*.*)");
			QString defaultPath = document->GetFilePath();
			QString fileName = QFileDialog::getSaveFileName(this, tr("Save"), defaultPath.isEmpty() ? document->GetProjectDirectory().path() : defaultPath, filters, 0);
			if (fileName.isEmpty())
				return false;
			document->SaveAs(fileName);
		}else{
			document->Save();
		}
		// Saved successfully: there is nothing left to recover.
		DiscardRecoverySnapshot();
		return true;
	}catch(...){
		QMessageBox *msgbox = new QMessageBox(
					QMessageBox::Warning,
					tr("Error"),
					tr("Failed to save file."),
					QMessageBox::Ok,
					this);
		msgbox->show();
		return false;
	}
}

bool MainWindow::EnsureClosingFile()
{
	if (!document->GetHistory()->IsDirty())
		return true;
	QMessageBox::StandardButton res = QMessageBox::question(
				this,
				tr("Confirmation"),
				tr("Save changes before closing?"),
				QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
				QMessageBox::Save);
	switch (res){
	case QMessageBox::Save:
		return Save();
	case QMessageBox::Discard:
		return true;
	case QMessageBox::Cancel:
	default:
		return false;
	}
}

void MainWindow::EditGoTo()
{
	if (!document)
		return;
	QDialog dlg(this);
	dlg.setWindowTitle(tr("Go To"));
	auto *form = new QFormLayout(&dlg);
	auto *combo = new QComboBox(&dlg);
	combo->addItem(tr("Measure"));        // 0
	combo->addItem(tr("Pulse (tick)"));   // 1
	combo->addItem(tr("Time (seconds)")); // 2
	auto *spin = new QDoubleSpinBox(&dlg);
	spin->setRange(0, 1e9);
	form->addRow(tr("Go to:"), combo);
	form->addRow(tr("Value:"), spin);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	form->addRow(buttons);
	connect(buttons, SIGNAL(accepted()), &dlg, SLOT(accept()));
	connect(buttons, SIGNAL(rejected()), &dlg, SLOT(reject()));
	auto applyMode = [spin](int idx){ spin->setDecimals(idx == 2 ? 3 : 0); };
	connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, applyMode);
	applyMode(0);

	if (dlg.exec() != QDialog::Accepted)
		return;

	int tick = 0;
	switch (combo->currentIndex()){
	case 0: { // measure -> the location of that bar line
		const QList<int> bars = document->GetBarLines().keys();
		if (!bars.isEmpty()){
			int m = qBound(0, int(spin->value()), bars.size() - 1);
			tick = bars[m];
		}
		break;
	}
	case 1: // pulse
		tick = int(spin->value());
		break;
	case 2: // seconds
		tick = document->FromAbsoluteTime(spin->value());
		break;
	}
	sequenceView->GoToLocation(tick);
}

bool MainWindow::IsSourceFileExtension(const QString &ext)
{
	if (BmsonIO::IsBmsonFileExtension(ext))
		return true;
	if (BmsIO::IsBmsFileExtension(ext))
		return true;
	return false;
}

bool MainWindow::IsSoundFileExtension(const QString &ext)
{
	if (ext == "wav" || ext == "ogg"){
		return true;
	}
	return false;
}

/*
bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
	switch (event->type()){
	case QEvent::DragEnter:
		dragEnterEvent(dynamic_cast<QDragEnterEvent*>(event));
		break;
		//return true;
	case QEvent::DragMove:
		dragMoveEvent(dynamic_cast<QDragMoveEvent*>(event));
		break;
		//return true;
	case QEvent::Drop:
		dropEvent(dynamic_cast<QDropEvent*>(event));
		break;
		//return true;
	}
	return false;
}*/

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls()){
		event->setDropAction(Qt::CopyAction);
		event->accept();
		return;
	}
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
	if (event->mimeData()->hasUrls()){
		event->setDropAction(Qt::CopyAction);
		event->accept();
		return;
	}
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
	event->accept();
}

void MainWindow::dropEvent(QDropEvent *event)
{
	const QMimeData* mimeData = event->mimeData();
	if (mimeData->hasUrls()){
		QStringList filePaths;
		for (auto url : mimeData->urls()){
			filePaths.append(url.toLocalFile());
		}
		QTimer::singleShot(10, [=](){
			OpenFiles(filePaths);
		});
		event->setDropAction(Qt::CopyAction);
		event->accept();
	}
}

void MainWindow::OpenFiles(QStringList filePaths)
{
	QString ext = QFileInfo(filePaths[0]).suffix().toLower();
	if (IsSourceFileExtension(ext)){
		//QMetaObject::invokeMethod(this, "FileOpen", Qt::QueuedConnection, Q_ARG(QString, filePaths[0]));
		FileOpen(filePaths[0]);
	}else if (IsSoundFileExtension(ext)){
		//QMetaObject::invokeMethod(this, "ChannelsNew", Qt::QueuedConnection, Q_ARG(QList<QString>, filePaths));
		ChannelsNew(filePaths);
	}else{
		QMessageBox *msgbox = new QMessageBox(
					QMessageBox::Warning,
					tr("Error"),
					tr("Unknown File Type"),
					QMessageBox::Ok,
					this);
		msgbox->show();
	}
}

void MainWindow::OpenBmson(QString path)
{
	try{
		Document *newEditor = new Document(this);
		newEditor->LoadFile(path);
		ReplaceDocument(newEditor);
	}catch(QString message){
		QMessageBox *msgbox = new QMessageBox(
					QMessageBox::Warning,
					tr("Error"),
					message,
					QMessageBox::Ok,
					this);
		msgbox->show();
	}
}

void MainWindow::OpenBms(QString path)
{
	Bms::BmsReader *reader = BmsIO::LoadBms(path);
	BmsImportDialog *dialog = new BmsImportDialog(this, *reader);
	dialog->exec();
	if (dialog->result() == QDialog::Accepted && dialog->IsSucceeded()){
		auto newEditor = new Document(this);
		newEditor->LoadBms(reader->GetBms());
		ReplaceDocument(newEditor);
	}
	delete dialog;
	delete reader;
}

bool MainWindow::WarningFileTraversals(QStringList filePaths)
{
	QString paths;
	int i=0;
	for (auto path : filePaths){
		paths += QDir::toNativeSeparators(path) += "\n";
		if (++i >= 10){
			paths += "...";
			break;
		}
	}
	QMessageBox msg(this);
	msg.setWindowTitle(tr("Warning"));
	msg.setText(tr("The following files are not in the current bmson directory. Are you sure to add these files?"));
	msg.setInformativeText(paths);
	msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
	msg.setDefaultButton(QMessageBox::No);
	int r = msg.exec();
	return r == QMessageBox::Yes;
}




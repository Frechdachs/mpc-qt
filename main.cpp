#include <QDebug>
#include <clocale>
#include <QApplication>
#include <QDesktopWidget>
#include <QLocalSocket>
#include <QFileDialog>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>
#include <QJsonDocument>
#include <QTimer>
#include "main.h"
#include "storage.h"
#include "mainwindow.h"
#include "manager.h"
#include "settingswindow.h"
#include "mpvwidget.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationDomain("cmdrkotori.mpc-qt");
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/images/bitmaps/icon.png"));

    // Qt sets the locale in the QApplication constructor, but libmpv requires
    // the LC_NUMERIC category to be set to "C", so change it back.
    std::setlocale(LC_NUMERIC, "C");

    // Register the error code type so that signals/slots will work with it
    qRegisterMetaType<MpvErrorCode>("MpvErrorCode");

    Flow f;
    if (!f.hasPrevious())
        return f.run();
    else
        return 0;
}

Flow::Flow(QObject *owner) :
    QObject(owner), server(NULL), mpvServer(NULL), mainWindow(NULL),
    playbackManager(NULL), settingsWindow(NULL)
{
    mainWindow = new MainWindow();
    playbackManager = new PlaybackManager(this);
    playbackManager->setMpvWidget(mainWindow->mpvWidget(), true);
    playbackManager->setPlaylistWindow(mainWindow->playlistWindow());
    settingsWindow = new SettingsWindow();
    settingsWindow->setWindowModality(Qt::WindowModal);

    server = new MpcQtServer(mainWindow, playbackManager, this);
    hasPrevious_ = server->sendPayload(makePayload());
    if (hasPrevious_)
        return;
    mpvServer = new MpvServer(playbackManager, mainWindow->mpvWidget(), this);

    // mainwindow -> manager
    connect(mainWindow, &MainWindow::severalFilesOpened,
            playbackManager, &PlaybackManager::openSeveralFiles);
    connect(mainWindow, &MainWindow::fileOpened,
            playbackManager, &PlaybackManager::openFile);
    connect(mainWindow, &MainWindow::dvdbdOpened,
            playbackManager, &PlaybackManager::playDiscFiles);
    connect(mainWindow, &MainWindow::streamOpened,
            playbackManager, &PlaybackManager::playStream);
    connect(mainWindow, &MainWindow::paused,
            playbackManager, &PlaybackManager::pausePlayer);
    connect(mainWindow, &MainWindow::unpaused,
            playbackManager, &PlaybackManager::unpausePlayer);
    connect(mainWindow, &MainWindow::stopped,
            playbackManager, &PlaybackManager::stopPlayer);
    connect(mainWindow, &MainWindow::stepBackward,
            playbackManager, &PlaybackManager::stepBackward);
    connect(mainWindow, &MainWindow::stepForward,
            playbackManager, &PlaybackManager::stepForward);
    connect(mainWindow, &MainWindow::speedDown,
            playbackManager, &PlaybackManager::speedDown);
    connect(mainWindow, &MainWindow::speedUp,
            playbackManager, &PlaybackManager::speedUp);
    connect(mainWindow, &MainWindow::speedReset,
            playbackManager, &PlaybackManager::speedReset);
    connect(mainWindow, &MainWindow::relativeSeek,
            playbackManager, &PlaybackManager::relativeSeek);
    connect(mainWindow, &MainWindow::audioTrackSelected,
            playbackManager, &PlaybackManager::setAudioTrack);
    connect(mainWindow, &MainWindow::subtitleTrackSelected,
            playbackManager, &PlaybackManager::setSubtitleTrack);
    connect(mainWindow, &MainWindow::videoTrackSelected,
            playbackManager, &PlaybackManager::setVideoTrack);
    connect(mainWindow, &MainWindow::volumeChanged,
            playbackManager, &PlaybackManager::setVolume);
    connect(mainWindow, &MainWindow::volumeMuteChanged,
            playbackManager, &PlaybackManager::setMute);
    connect(mainWindow, &MainWindow::chapterPrevious,
            playbackManager, &PlaybackManager::navigateToPrevChapter);
    connect(mainWindow, &MainWindow::chapterNext,
            playbackManager, &PlaybackManager::navigateToNextChapter);
    connect(mainWindow, &MainWindow::chapterSelected,
            playbackManager, &PlaybackManager::navigateToChapter);
    connect(mainWindow, &MainWindow::timeSelected,
            playbackManager, &PlaybackManager::navigateToTime);

    // playlistwindow -> mainwindow
    connect(mainWindow->playlistWindow(), &PlaylistWindow::visibilityChanged,
            mainWindow, &MainWindow::setPlaylistVisibleState);

    // mainwindow -> playlistwindow
    connect(mainWindow, &MainWindow::playCurrentItemRequested,
            mainWindow->playlistWindow(), &PlaylistWindow::playCurrentItem);

    // manager -> mainwindow
    connect(playbackManager, &PlaybackManager::timeChanged,
            mainWindow, &MainWindow::setTime);
    connect(playbackManager, &PlaybackManager::titleChanged,
            mainWindow, &MainWindow::setMediaTitle);
    connect(playbackManager, &PlaybackManager::chapterTitleChanged,
            mainWindow, &MainWindow::setChapterTitle);
    connect(playbackManager, &PlaybackManager::videoSizeChanged,
            mainWindow, &MainWindow::setVideoSize);
    connect(playbackManager, &PlaybackManager::stateChanged,
            mainWindow, &MainWindow::setPlaybackState);
    connect(playbackManager, &PlaybackManager::typeChanged,
            mainWindow, &MainWindow::setPlaybackType);
    connect(playbackManager, &PlaybackManager::chaptersAvailable,
            mainWindow, &MainWindow::setChapters);
    connect(playbackManager, &PlaybackManager::audioTracksAvailable,
            mainWindow, &MainWindow::setAudioTracks);
    connect(playbackManager, &PlaybackManager::videoTracksAvailable,
            mainWindow, &MainWindow::setVideoTracks);
    connect(playbackManager, &PlaybackManager::subtitleTracksAvailable,
            mainWindow, &MainWindow::setSubtitleTracks);
    connect(playbackManager, &PlaybackManager::fpsChanged,
            mainWindow, &MainWindow::setFps);
    connect(playbackManager, &PlaybackManager::avsyncChanged,
            mainWindow, &MainWindow::setAvsync);
    connect(playbackManager, &PlaybackManager::displayFramedropsChanged,
            mainWindow, &MainWindow::setDisplayFramedrops);
    connect(playbackManager, &PlaybackManager::decoderFramedropsChanged,
            mainWindow, &MainWindow::setDecoderFramedrops);

    // mainwindow -> settings
    connect(mainWindow, &MainWindow::volumeChanged,
            settingsWindow, &SettingsWindow::setVolume);
    connect(mainWindow, &MainWindow::zoomPresetChanged,
            settingsWindow, &SettingsWindow::setZoomPreset);

    // settings -> mainwindow
    connect(settingsWindow, &SettingsWindow::mouseWindowedMap,
            mainWindow, &MainWindow::setWindowedMouseMap);
    connect(settingsWindow, &SettingsWindow::mouseFullscreenMap,
            mainWindow, &MainWindow::setFullscreenMouseMap);
    connect(settingsWindow, &SettingsWindow::volume,
            mainWindow, &MainWindow::setVolume);
    connect(settingsWindow, &SettingsWindow::zoomPreset,
            mainWindow, &MainWindow::setZoomPreset);
    connect(settingsWindow, &SettingsWindow::zoomCenter,
            mainWindow, &MainWindow::setZoomCenter);
    connect(settingsWindow, &SettingsWindow::hideMethod,
            mainWindow, &MainWindow::setBottomAreaBehavior);
    connect(settingsWindow, &SettingsWindow::hideTime,
            mainWindow, &MainWindow::setBottomAreaHideTime);
    connect(settingsWindow, &SettingsWindow::hidePanels,
            mainWindow, &MainWindow::setFullscreenHidePanels);

    // settings -> mpvwidget
    auto mpvw = mainWindow->mpvWidget();
    connect(settingsWindow, &SettingsWindow::logoSource,
            mpvw, &MpvWidget::setLogoUrl);
    connect(settingsWindow, &SettingsWindow::volume,
            mpvw, &MpvWidget::setVolume);
    connect(settingsWindow, &SettingsWindow::voCommandLine,
            mpvw, &MpvWidget::setVOCommandLine);
    connect(settingsWindow, &SettingsWindow::framedropMode,
            mpvw, &MpvWidget::setFramedropMode);
    connect(settingsWindow, &SettingsWindow::decoderDropMode,
            mpvw, &MpvWidget::setDecoderDropMode);
    connect(settingsWindow, &SettingsWindow::displaySyncMode,
            mpvw, &MpvWidget::setDisplaySyncMode);
    connect(settingsWindow, &SettingsWindow::audioDropSize,
            mpvw, &MpvWidget::setAudioDropSize);
    connect(settingsWindow, &SettingsWindow::maximumAudioChange,
            mpvw, &MpvWidget::setMaximumAudioChange);
    connect(settingsWindow, &SettingsWindow::maximumVideoChange,
            mpvw, &MpvWidget::setMaximumVideoChange);
    connect(settingsWindow, &SettingsWindow::playbackLoopImages,
            mpvw, &MpvWidget::setLoopImages);
    connect(settingsWindow, &SettingsWindow::subsAreGray,
            mpvw, &MpvWidget::setSubsAreGray);
    connect(settingsWindow, &SettingsWindow::screenshotFormat,
            mpvw, &MpvWidget::setScreenshotFormat);
    connect(settingsWindow, &SettingsWindow::screenshotJpegQuality,
            mpvw, &MpvWidget::setScreenshotJpegQuality);
    connect(settingsWindow, &SettingsWindow::screenshotJpegSmooth,
            mpvw, &MpvWidget::setScreenshotJpegSmooth);
    connect(settingsWindow, &SettingsWindow::screenshotJpegSourceChroma,
            mpvw, &MpvWidget::setScreenshotJpegSourceChroma);
    connect(settingsWindow, &SettingsWindow::screenshotPngCompression,
            mpvw, &MpvWidget::setScreenshotPngCompression);
    connect(settingsWindow, &SettingsWindow::screenshotPngFilter,
            mpvw, &MpvWidget::setScreenshotPngFilter);
    connect(settingsWindow, &SettingsWindow::screenshotPngColorspace,
            mpvw, &MpvWidget::setScreenshotPngColorspace);
    connect(settingsWindow, &SettingsWindow::clientDebuggingMessages,
            mpvw, &MpvWidget::setClientDebuggingMessages);
    connect(settingsWindow, &SettingsWindow::mpvLogLevel,
            mpvw, &MpvWidget::setMpvLogLevel);

    // settings -> playlistWindow
    connect(settingsWindow, &SettingsWindow::playlistFormat,
            mainWindow->playlistWindow(), &PlaylistWindow::setDisplayFormatSpecifier);

    // settings -> manager
    connect(settingsWindow, &SettingsWindow::playbackPlayTimes,
            playbackManager, &PlaybackManager::setPlaybackPlayTimes);

    // manager -> settings
    connect(playbackManager, &PlaybackManager::playerSettingsRequested,
            settingsWindow, &SettingsWindow::sendSignals);

    // mainwindow -> this
    connect(mainWindow, &MainWindow::recentOpened,
            this, &Flow::mainwindow_recentOpened);
    connect(mainWindow, &MainWindow::recentClear,
            this, &Flow::mainwindow_recentClear);
    connect(mainWindow, &MainWindow::takeImage,
            this, &Flow::mainwindow_takeImage);
    connect(mainWindow, &MainWindow::takeImageAutomatically,
            this, &Flow::mainwindow_takeImageAutomatically);
    connect(mainWindow, &MainWindow::optionsOpenRequested,
            this, &Flow::mainwindow_optionsOpenRequested);
    connect(mainWindow, &MainWindow::applicationShouldQuit,
            this, &Flow::mainwindow_applicationShouldQuit);

    // manager -> this
    connect(playbackManager, &PlaybackManager::nowPlayingChanged,
            this, &Flow::manager_nowPlayingChanged);

    // settings -> this
    connect(settingsWindow, &SettingsWindow::settingsData,
            this, &Flow::settingswindow_settingsData);
    connect(settingsWindow, &SettingsWindow::keyMapData,
            this, &Flow::settingswindow_keymapData);
    connect(settingsWindow, &SettingsWindow::rememberWindowGeometry,
            this, &Flow::settingswindow_rememberWindowGeometry);
    connect(settingsWindow, &SettingsWindow::screenshotDirectory,
            this, &Flow::settingswindow_screenshotDirectory);
    connect(settingsWindow, &SettingsWindow::encodeDirectory,
            this, &Flow::settingswindow_encodeDirectory);
    connect(settingsWindow, &SettingsWindow::screenshotTemplate,
            this, &Flow::settingswindow_screenshotTemplate);
    connect(settingsWindow, &SettingsWindow::encodeTemplate,
            this, &Flow::settingswindow_encodeTemplate);
    connect(settingsWindow, &SettingsWindow::screenshotFormat,
            this, &Flow::settingswindow_screenshotFormat);

    // playlistwindow -> this.storage
    connect(mainWindow->playlistWindow(), &PlaylistWindow::importPlaylist,
            this, &Flow::importPlaylist);
    connect(mainWindow->playlistWindow(), &PlaylistWindow::exportPlaylist,
            this, &Flow::exportPlaylist);

    // this -> mainwindow
    connect(this, &Flow::recentFilesChanged,
            mainWindow, &MainWindow::setRecentDocuments);

    // update player framework
    settingsWindow->takeActions(mainWindow->editableActions());
    recentFromVList(storage.readVList("recent"));
    mainWindow->setRecentDocuments(recentFiles);
    settings = storage.readVMap("settings");
    keyMap = storage.readVMap("keys");
    settingsWindow->takeSettings(settings);
    settingsWindow->setMouseMapDefaults(mainWindow->mouseMapDefaults());
    settingsWindow->takeKeyMap(keyMap);
    settingsWindow->setServerName(server->fullServerName());
    settingsWindow->sendSignals();

    // Push all our windows on to the same (moused) screen... similar code is
    // in mainwindow.cpp.  Prevents jumping screens on multiple screens for
    // whatever underlying reason.
    QDesktopWidget *desktop = qApp->desktop();
    QRect available = desktop->availableGeometry(desktop->screenNumber(QCursor::pos()));
    settingsWindow->setGeometry(QStyle::alignedRect(Qt::LeftToRight,
                                                    Qt::AlignCenter,
                                                    settingsWindow->size(),
                                                    available));
    mainWindow->setGeometry(QStyle::alignedRect(Qt::LeftToRight,
                                                    Qt::AlignCenter,
                                                    mainWindow->size(),
                                                    available));
}

Flow::~Flow()
{
    if (server) {
        delete server;
        server = NULL;
    }
    if (mpvServer) {
        delete mpvServer;
        mpvServer = NULL;
    }
    if (mainWindow) {
        storage.writeVList("playlists", mainWindow->playlistWindow()->tabsToVList());
        delete mainWindow;
        mainWindow = NULL;
    }
    if (playbackManager) {
        delete playbackManager;
        playbackManager = NULL;
    }
    if (settingsWindow) {
        delete settingsWindow;
        settingsWindow = NULL;
    }
}

int Flow::run()
{
    mainWindow->playlistWindow()->tabsFromVList(storage.readVList("playlists"));
    QTimer::singleShot(50, [this]() {
        // wait for the internal geometry to update, then perform a resize
        restoreWindows(storage.readVMap("geometry"));
        if (!hasPrevious_)
            server->fakePayload(makePayload());
    });
    return qApp->exec();
}

bool Flow::hasPrevious()
{
    return hasPrevious_;
}

QByteArray Flow::makePayload() const
{
    QVariantMap map({
        {"command", QVariant("playFiles")},
        {"directory", QVariant(QDir::currentPath())},
        {"files", QVariant(QCoreApplication::arguments().mid(1))}
    });
    return QJsonDocument::fromVariant(map).toJson();
}

QString Flow::pictureTemplate(Helpers::DisabledTrack tracks, Helpers::Subtitles subs) const
{
    double playTime = mainWindow->mpvWidget()->playTime();
    QUrl nowPlaying = playbackManager->nowPlaying();
    QString basename = QFileInfo(nowPlaying.toDisplayString().split('/').last())
                       .completeBaseName();

    QString fileName = Helpers::parseFormat(screenshotTemplate, basename,
                                            tracks,
                                            subs,
                                            playTime, 0, 0);
    QString filePath = screenshotDirectory;
    if (filePath.isEmpty()) {
        if (nowPlaying.isLocalFile())
            filePath = QFileInfo(nowPlaying.toLocalFile()).path();
        else
            filePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }
    QDir().mkpath(filePath);
    return filePath + "/" + fileName;
}

QVariantList Flow::recentToVList() const
{
    QVariantList l;
    for (TrackInfo track : recentFiles)
        l.append(track.toVMap());
    return l;
}

void Flow::recentFromVList(const QVariantList &list)
{
    recentFiles.clear();
    for (QVariant item : list) {
        TrackInfo t;
        t.fromVMap(item.toMap());
        recentFiles.append(t);
    }
}

QVariantMap Flow::saveWindows()
{
    return QVariantMap {
        {
            "mainWindow", QVariantMap {
                { "geometry", Helpers::rectToVmap(mainWindow->geometry()) },
                { "state", mainWindow->state() }
            }
        },
        {
            "playlistWindow", QVariantMap {
                { "geometry", Helpers::rectToVmap(mainWindow->playlistWindow()->window()->geometry()) },
                { "floating", mainWindow->playlistWindow()->isFloating() }
            }
        }
    };
}

void Flow::restoreWindows(const QVariantMap &map)
{
    QVariantMap mainWindowMap = map["mainWindow"].toMap();
    QVariantMap playlistWindowMap = map["playlistWindow"].toMap();
    if (rememberWindowGeometry && mainWindowMap.contains("geometry")
            && playlistWindowMap.contains("geometry")) {
        if (playlistWindowMap["floating"].toBool()) {
            mainWindow->playlistWindow()->setFloating(true);
            mainWindow->playlistWindow()->window()->setGeometry(Helpers::vmapToRect(playlistWindowMap["geometry"].toMap()));
        }
        mainWindow->setGeometry(Helpers::vmapToRect(mainWindowMap["geometry"].toMap()));
        QTimer::singleShot(50, [=]() {
            // once again, wait for the internal geometery to change, then
            // show the window.
            mainWindow->show();
            mainWindow->setState(mainWindowMap["state"].toMap());
        });
    } else {
        mainWindow->fireUpdateSize();
        mainWindow->show();
        mainWindow->setState(mainWindowMap["state"].toMap());
    }
}

void Flow::mainwindow_applicationShouldQuit()
{
    storage.writeVMap("settings", settings);
    storage.writeVMap("keys", keyMap);
    storage.writeVList("recent", recentToVList());
    storage.writeVMap("geometry", saveWindows());
    qApp->quit();
}

void Flow::mainwindow_recentOpened(const TrackInfo &track)
{
    // attempt to play the old one if possible, otherwise pretend it's new
    QUrl old = mainWindow->playlistWindow()->getUrlOf(track.list, track.item);
    if (!old.isEmpty())
        playbackManager->playItem(track.list, track.item);
    else
        playbackManager->openFile(track.url);
}

void Flow::mainwindow_recentClear()
{
    recentFiles.clear();
    mainWindow->setRecentDocuments(recentFiles);
}

void Flow::mainwindow_takeImage()
{
    QString tempFile = QString("/dev/shm/mpc-qt_%1").arg(QUuid::createUuid().toString());
    mainWindow->mpvWidget()->screenshot(tempFile, true);
    tempFile += "." + screenshotFormat;

    QString fileName = pictureTemplate(Helpers::DisabledAudio, Helpers::SubtitlesPresent);
    QString picFile;
    picFile = QFileDialog::getSaveFileName(this->mainWindow, tr("Save Image"),
                                           fileName);
    if (picFile.isEmpty()) {
        QFile(tempFile).remove();
        return;
    }
    QFileInfo qfi(picFile);
    QString dest = qfi.absolutePath() + "/" + qfi.completeBaseName() + "." + screenshotFormat;
    QFile(tempFile).copy(dest);
}

void Flow::mainwindow_takeImageAutomatically()
{
    mainWindow->mpvWidget()->screenshot(pictureTemplate(Helpers::DisabledAudio, Helpers::SubtitlesPresent), true);
}

void Flow::mainwindow_optionsOpenRequested()
{
    settingsWindow->takeSettings(settings);
    settingsWindow->takeKeyMap(keyMap);
    settingsWindow->show();
    settingsWindow->raise();
}

void Flow::manager_nowPlayingChanged(QUrl url, QUuid listUuid, QUuid itemUuid)
{
    TrackInfo track(url, listUuid, itemUuid);
    if (recentFiles.contains(track)) {
        recentFiles.removeAll(track);
    }
    recentFiles.insert(0, track);
    if (recentFiles.size() > 10)
        recentFiles.removeLast();

    emit recentFilesChanged(recentFiles);
}

void Flow::settingswindow_settingsData(const QVariantMap &settings)
{
    this->settings = settings;
}

void Flow::settingswindow_rememberWindowGeometry(bool yes)
{
    this->rememberWindowGeometry = yes;
}

void Flow::settingswindow_keymapData(const QVariantMap &keyMap)
{
    this->keyMap = keyMap;
}

void Flow::settingswindow_screenshotDirectory(const QString &where)
{
    this->screenshotDirectory = where;
}

void Flow::settingswindow_encodeDirectory(const QString &where)
{
    this->encodeDirectory = where;
}

void Flow::settingswindow_screenshotTemplate(const QString &fmt)
{
    this->screenshotTemplate = fmt;
}

void Flow::settingswindow_encodeTemplate(const QString &fmt)
{
    this->encodeTemplate = fmt;
}

void Flow::settingswindow_screenshotFormat(const QString &fmt)
{
    this->screenshotFormat = fmt;
}

void Flow::importPlaylist(QString fname)
{
    emit mainWindow->playlistWindow()->addSimplePlaylist(storage.readM3U(fname));
}

void Flow::exportPlaylist(QString fname, QStringList items)
{
    storage.writeM3U(fname, items);
}


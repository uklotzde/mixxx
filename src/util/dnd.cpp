#include "util/dnd.h"

#include <QScopedPointer>

#include "control/controlobject.h"
#include "sources/soundsourceproxy.h"
#include "library/parser.h"
#include "library/parserm3u.h"
#include "library/parserpls.h"
#include "library/parsercsv.h"
#include "util/sandbox.h"
#include "mixer/playermanager.h"
#include "widget/trackdroptarget.h"
#include "track/trackref.h"


namespace {

QDrag* dragUrls(
        const QList<QUrl>& locationUrls,
        QWidget* pDragSource,
        QString sourceIdentifier) {
    if (locationUrls.isEmpty()) {
        return NULL;
    }

    QMimeData* mimeData = new QMimeData();
    mimeData->setUrls(locationUrls);
    mimeData->setText(sourceIdentifier);

    QDrag* drag = new QDrag(pDragSource);
    drag->setMimeData(mimeData);
    drag->setPixmap(QPixmap(":/images/library/ic_library_drag_and_drop.svg"));
    drag->exec(Qt::CopyAction);

    return drag;
}

bool addFileToList(
        const QString& file,
        QList<QFileInfo>* files) {
    QFileInfo fileInfo(file);

    // Since the user just dropped these files into Mixxx we have permission
    // to touch the file. Create a security token to keep this permission
    // across reboots.
    Sandbox::createSecurityToken(fileInfo);

    if (!fileInfo.exists()) {
        return false;
    }

    // Filter out invalid URLs (eg. files that aren't supported audio
    // filetypes, etc.)
    if (!SoundSourceProxy::isFileSupported(fileInfo)) {
        return false;
    }

    files->append(fileInfo);
    return true;
}

} // anonymous namespace

// static
QList<QFileInfo> DragAndDropHelper::supportedTracksFromUrls(
        const QList<QUrl>& urls,
        bool firstOnly,
        bool acceptPlaylists) {
    QList<QFileInfo> fileLocations;
    foreach (const QUrl& url, urls) {

        // XXX: Possible WTF alert - Previously we thought we needed
        // toString() here but what you actually want in any case when
        // converting a QUrl to a file system path is
        // QUrl::toLocalFile(). This is the second time we have flip-flopped
        // on this, but I think toLocalFile() should work in any
        // case. toString() absolutely does not work when you pass the
        // result to a (this comment was never finished by the original
        // author).
        QString file(url.toLocalFile());

        // If the file is on a network share, try just converting the URL to
        // a string...
        if (file.isEmpty()) {
            file = url.toString();
        }

        if (file.isEmpty()) {
            continue;
        }

        if (acceptPlaylists && (file.endsWith(".m3u") || file.endsWith(".m3u8"))) {
            QScopedPointer<ParserM3u> playlist_parser(new ParserM3u());
            QList<QString> track_list = playlist_parser->parse(file);
            foreach (const QString& playlistFile, track_list) {
                addFileToList(playlistFile, &fileLocations);
            }
        } else if (acceptPlaylists && url.toString().endsWith(".pls")) {
            QScopedPointer<ParserPls> playlist_parser(new ParserPls());
            QList<QString> track_list = playlist_parser->parse(file);
            foreach (const QString& playlistFile, track_list) {
                addFileToList(playlistFile, &fileLocations);
            }
        } else {
            addFileToList(file, &fileLocations);
        }

        if (firstOnly && !fileLocations.isEmpty()) {
            break;
        }
    }

    return fileLocations;
}

//static
bool DragAndDropHelper::allowLoadToPlayer(
        const QString& group,
        UserSettingsPointer pConfig) {
    // Always allow loads to preview decks.
    if (PlayerManager::isPreviewDeckGroup(group)) {
        return true;
    }

    const bool isPlaying = ControlObject::get(ConfigKey(group, "play")) > 0.0;
    return !isPlaying || pConfig->getValueString(
            ConfigKey("[Controls]",
                      "AllowTrackLoadToPlayingDeck")).toInt();
}

//static
bool DragAndDropHelper::allowDeckCloneAttempt(const QDropEvent& event,
                               const QString& group) {
    // only allow clones to decks
    if (!PlayerManager::isDeckGroup(group, nullptr)) {
        return false;
    }

    // forbid clone if shift is pressed
    if (event.keyboardModifiers().testFlag(Qt::ShiftModifier)) {
        return false;
    }

    if (!event.mimeData()->hasText() ||
             // prevent cloning to ourself
             event.mimeData()->text() == group ||
             // only allow clone from decks
             !PlayerManager::isDeckGroup(event.mimeData()->text(), nullptr)) {
        return false;
    }

    return true;
}

//static
QList<QFileInfo> DragAndDropHelper::dropEventFiles(const QMimeData& mimeData,
                                       const QString& sourceIdentifier,
                                       bool firstOnly,
                                       bool acceptPlaylists) {
    if (!mimeData.hasUrls() ||
            (mimeData.hasText() && mimeData.text() == sourceIdentifier)) {
        return QList<QFileInfo>();
    }

    QList<QFileInfo> files = DragAndDropHelper::supportedTracksFromUrls(
            mimeData.urls(), firstOnly, acceptPlaylists);
    return files;
}

//static
QDrag* DragAndDropHelper::dragTrack(
        TrackPointer pTrack,
        QWidget* pDragSource,
        QString sourceIdentifier) {
    QList<QUrl> locationUrls;
    locationUrls.append(pTrack->getLocationUrl());
    return dragUrls(locationUrls, pDragSource, sourceIdentifier);
}

//static
QDrag* DragAndDropHelper::dragTrackLocations(
        const QList<QString>& locations,
        QWidget* pDragSource,
        QString sourceIdentifier) {
    QList<QUrl> locationUrls;
    foreach (QString location, locations) {
        locationUrls.append(TrackRef::locationUrl(location));
    }
    return dragUrls(locationUrls, pDragSource, sourceIdentifier);
}

//static
void DragAndDropHelper::handleTrackDropEvent(
        QDropEvent* event,
        TrackDropTarget& target,
        const QString& group,
        UserSettingsPointer pConfig) {
    if (allowLoadToPlayer(group, pConfig)) {
        if (allowDeckCloneAttempt(*event, group)) {
            event->accept();
            emit(target.cloneDeck(event->mimeData()->text(), group));
            return;
        } else {
            QList<QFileInfo> files = dropEventFiles(
                    *event->mimeData(), group, true, false);
            if (!files.isEmpty()) {
                event->accept();
                emit(target.trackDropped(files.at(0).absoluteFilePath(), group));
                return;
            }
        }
    }
    event->ignore();
}

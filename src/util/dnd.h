#pragma once

#include <QUrl>
#include <QDrag>
#include <QDropEvent>
#include <QMimeData>
#include <QList>
#include <QString>
#include <QFileInfo>

#include "preferences/usersettings.h"
#include "track/track.h"

class TrackDropTarget;

class DragAndDropHelper {
  public:
    static QList<QFileInfo> supportedTracksFromUrls(
            const QList<QUrl>& urls,
            bool firstOnly,
            bool acceptPlaylists);

    // Allow loading to a player if the player isn't playing or the settings
    // allow interrupting a playing player.
    static bool allowLoadToPlayer(
            const QString& group,
            UserSettingsPointer pConfig);

    static bool allowDeckCloneAttempt(
            const QDropEvent& event,
            const QString& group);

    static bool dragEnterAccept(
            const QMimeData& mimeData,
            const QString& sourceIdentifier,
            bool firstOnly,
            bool acceptPlaylists) {
        return !dropEventFiles(
                mimeData,
                sourceIdentifier,
                firstOnly,
                acceptPlaylists).isEmpty();
    }

    static QList<QFileInfo> dropEventFiles(
            const QMimeData& mimeData,
            const QString& sourceIdentifier,
            bool firstOnly,
           bool acceptPlaylists);

    static QDrag* dragTrack(
            TrackPointer pTrack,
            QWidget* pDragSource,
            QString sourceIdentifier);

    static QDrag* dragTrackLocations(
            const QList<QString>& locations,
            QWidget* pDragSource,
            QString sourceIdentifier);

    static void handleTrackDragEnterEvent(
            QDragEnterEvent* event,
            const QString& group,
            UserSettingsPointer pConfig) {
        if (allowLoadToPlayer(group, pConfig) &&
                dragEnterAccept(*event->mimeData(), group, true, false)) {
            event->acceptProposedAction();
        } else {
            event->ignore();
        }
    }

    static void handleTrackDropEvent(
            QDropEvent* event,
            TrackDropTarget& target,
            const QString& group,
            UserSettingsPointer pConfig);
};

#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>

#include "track/playcounter.h"
#include "track/trackid.h"

class TrackCollectionManager;

namespace aoide {

class SessionCache {
  public:
    SessionCache() = default;

    const QDateTime& startedAt() const {
        return m_startedAt;
    }

    void restart(
            const TrackCollectionManager& trackCollectionManager,
            QDateTime startedAt = {});

    bool updateTrack(
            TrackId trackId,
            QString trackLocation,
            PlayCounter trackPlayCounter);

    void removeTrackById(
            TrackId trackId);

    bool isPlayed(
            const PlayCounter& playCounter) const;

    bool isTrackPlayed(
            TrackId trackId) const;

    bool isTrackLocationPlayed(
            const QString& trackLocation) const;

  private:
    QDateTime m_startedAt;
    QHash<QString, TrackId> m_trackLocations;
    QHash<TrackId, PlayCounter> m_trackPlayCounters;
};

} // namespace aoide

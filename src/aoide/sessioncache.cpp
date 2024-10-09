#include "aoide/sessioncache.h"

#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "track/track.h"
#include "util/assert.h"

namespace aoide {

void SessionCache::restart(
        const TrackCollectionManager& trackCollectionManager,
        QDateTime startedAt) {
    qInfo() << "Restarting session cache";
    if (!startedAt.isValid() || !m_startedAt.isValid() || startedAt < m_startedAt) {
        auto& trackDao = trackCollectionManager.internalCollection()->getTrackDAO();
        const auto sinceLastPlayedAt = startedAt.addDays(-1);
        const auto recentlyPlayedTracks = trackDao.findRecentlyPlayedTracks(sinceLastPlayedAt);
        qInfo() << "Updating session cache from"
                << recentlyPlayedTracks.size()
                << "track(s) that have been played since"
                << qPrintable(sinceLastPlayedAt.toString(Qt::ISODate));
        for (const auto& recentlyPlayedTrack : recentlyPlayedTracks) {
            updateTrack(
                    recentlyPlayedTrack.id,
                    recentlyPlayedTrack.location,
                    recentlyPlayedTrack.playCounter);
        }
    }
    m_startedAt = startedAt;
    if (m_startedAt.isValid()) {
        qInfo() << "Restarted session cache at" << qPrintable(m_startedAt.toString(Qt::ISODate));
    } else {
        qInfo() << "Restarted session cache";
    }
}

bool SessionCache::updateTrack(
        TrackId trackId,
        QString trackLocation,
        PlayCounter trackPlayCounter) {
    DEBUG_ASSERT(trackId.isValid());
    auto modified = false;
    {
        const auto iTrackLocation = m_trackLocations.find(trackLocation);
        if (iTrackLocation == m_trackLocations.end()) {
            // Insert.
            m_trackLocations.emplace(std::move(trackLocation), trackId);
        } else {
            // Update.
            const auto oldTrackId = iTrackLocation.value();
            if (oldTrackId != trackId) {
                // Very unlikely case that a file location changed its identity.
                removeTrackById(oldTrackId);
                m_trackLocations.emplace(std::move(trackLocation), trackId);
                modified = true;
            }
        }
    }
    const auto iTrackPlayCounter = m_trackPlayCounters.find(trackId);
    if (iTrackPlayCounter == m_trackPlayCounters.end()) {
        // Insert.
        m_trackPlayCounters.emplace(trackId, std::move(trackPlayCounter));
        modified = true;
    } else if (iTrackPlayCounter.value() != trackPlayCounter) {
        // Update.
        iTrackPlayCounter.value() = std::move(trackPlayCounter);
        modified = true;
    }
    DEBUG_ASSERT(m_trackPlayCounters.size() == m_trackLocations.size());
    return modified;
}

void SessionCache::removeTrackById(TrackId trackId) {
    for (auto i = m_trackLocations.begin(); i != m_trackLocations.end(); ++i) {
        if (i.value() == trackId) {
            m_trackLocations.erase(i);
            break;
        }
    }
    m_trackPlayCounters.remove(trackId);
    DEBUG_ASSERT(m_trackPlayCounters.size() == m_trackLocations.size());
}

bool SessionCache::isPlayed(
        const PlayCounter& playCounter) const {
    if (playCounter.isPlayed()) {
        return true;
    }
    if (!m_startedAt.isValid()) {
        DEBUG_ASSERT(m_startedAt.isNull());
        return false;
    }
    const auto lastPlayedAt = playCounter.getLastPlayedAt();
    if (!lastPlayedAt.isValid()) {
        DEBUG_ASSERT(lastPlayedAt.isNull());
        DEBUG_ASSERT(playCounter.getTimesPlayed() == 0);
        return false;
    }
    DEBUG_ASSERT(playCounter.getTimesPlayed() > 0);
    return lastPlayedAt >= m_startedAt;
}

bool SessionCache::isTrackPlayed(
        TrackId trackId) const {
    const auto i = m_trackPlayCounters.find(trackId);
    if (i == m_trackPlayCounters.end()) {
        return false;
    }
    return isPlayed(i.value());
}

bool SessionCache::isTrackLocationPlayed(
        const QString& trackLocation) const {
    const auto i = m_trackLocations.find(trackLocation);
    if (i == m_trackLocations.end()) {
        return false;
    }
    return isTrackPlayed(i.value());
}

} // namespace aoide

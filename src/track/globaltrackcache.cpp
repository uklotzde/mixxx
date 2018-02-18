#include "track/globaltrackcache.h"

#include <QApplication>
#include <QThread>

#include "util/assert.h"
#include "util/logger.h"


namespace {

constexpr std::size_t kUnorderedCollectionMinCapacity = 1024;

const mixxx::Logger kLogger("GlobalTrackCache");

//static
GlobalTrackCache* volatile s_pInstance = nullptr;

// Enforce logging during tests
constexpr bool kLogEnabled = false;

inline bool debugLogEnabled() {
    return kLogEnabled || kLogger.debugEnabled();
}

inline bool traceLogEnabled() {
    return kLogEnabled || kLogger.traceEnabled();
}

constexpr bool kLogStats = false;

inline
TrackRef createTrackRef(const Track& track) {
    return TrackRef::fromFileInfo(track.getFileInfo(), track.getId());
}

void deleteTrack(Track* plainPtr) {
    if (plainPtr) {
        if (traceLogEnabled()) {
            plainPtr->dumpObjectInfo();
        }
        plainPtr->deleteLater();
    }
}

} // anonymous namespace

GlobalTrackCacheLocker::GlobalTrackCacheLocker()
        : m_pInstance(nullptr) {
    lockCache();
}

GlobalTrackCacheLocker::GlobalTrackCacheLocker(
        GlobalTrackCacheLocker&& moveable)
        : m_pInstance(std::move(moveable.m_pInstance)) {
    moveable.m_pInstance = nullptr;
}

GlobalTrackCacheLocker::~GlobalTrackCacheLocker() {
    unlockCache();
}

void GlobalTrackCacheLocker::lockCache() {
    DEBUG_ASSERT(s_pInstance);
    DEBUG_ASSERT(!m_pInstance);
    if (traceLogEnabled()) {
        kLogger.trace() << "Locking cache";
    }
    s_pInstance->m_mutex.lock();
    if (traceLogEnabled()) {
        kLogger.trace() << "Cache is locked";
    }
    m_pInstance = s_pInstance;
}

void GlobalTrackCacheLocker::unlockCache() {
    if (m_pInstance) {
        // Verify consistency before unlocking the cache
        DEBUG_ASSERT(m_pInstance->verifyConsistency());
        if (traceLogEnabled()) {
            kLogger.trace() << "Unlocking cache";
        }
        if (kLogStats && debugLogEnabled()) {
            kLogger.debug()
                    << "#tracksById ="
                    << m_pInstance->m_tracksById.size()
                    << "/ #tracksByCanonicalLocation ="
                    << m_pInstance->m_tracksByCanonicalLocation.size();
        }
        m_pInstance->m_mutex.unlock();
        if (traceLogEnabled()) {
            kLogger.trace() << "Cache is unlocked";
        }
        m_pInstance = nullptr;
    }
}

void GlobalTrackCacheLocker::resetCache() const {
    DEBUG_ASSERT(m_pInstance);
    m_pInstance->reset();
}

void GlobalTrackCacheLocker::deactivateCache() const {
    DEBUG_ASSERT(m_pInstance);
    m_pInstance->deactivate();
}

bool GlobalTrackCacheLocker::isEmpty() const {
    DEBUG_ASSERT(m_pInstance);
    return m_pInstance->isEmpty();
}

TrackPointer GlobalTrackCacheLocker::lookupTrackById(
        const TrackId& trackId) const {
    DEBUG_ASSERT(m_pInstance);
    return m_pInstance->lookupById(trackId);
}

TrackPointer GlobalTrackCacheLocker::lookupTrackByRef(
        const TrackRef& trackRef) const {
    DEBUG_ASSERT(m_pInstance);
    return m_pInstance->lookupByRef(trackRef);
}

GlobalTrackCacheResolver::GlobalTrackCacheResolver(
        QFileInfo fileInfo,
        SecurityTokenPointer pSecurityToken)
        : m_lookupResult(GlobalTrackCacheLookupResult::NONE) {
    DEBUG_ASSERT(m_pInstance);
    m_pInstance->resolve(this, std::move(fileInfo), TrackId(), std::move(pSecurityToken));
}

GlobalTrackCacheResolver::GlobalTrackCacheResolver(
        QFileInfo fileInfo,
        TrackId trackId,
        SecurityTokenPointer pSecurityToken)
        : m_lookupResult(GlobalTrackCacheLookupResult::NONE) {
    DEBUG_ASSERT(m_pInstance);
    m_pInstance->resolve(this, std::move(fileInfo), std::move(trackId), std::move(pSecurityToken));
}

void GlobalTrackCacheResolver::initLookupResult(
        GlobalTrackCacheLookupResult lookupResult,
        TrackPointer&& strongPtr,
        TrackRef&& trackRef) {
    DEBUG_ASSERT(m_pInstance);
    DEBUG_ASSERT(GlobalTrackCacheLookupResult::NONE == m_lookupResult);
    DEBUG_ASSERT(!m_strongPtr);
    m_lookupResult = lookupResult;
    m_strongPtr = std::move(strongPtr);
    m_trackRef = std::move(trackRef);
}

void GlobalTrackCacheResolver::initTrackIdAndUnlockCache(TrackId trackId) {
    DEBUG_ASSERT(m_pInstance);
    DEBUG_ASSERT(GlobalTrackCacheLookupResult::NONE != m_lookupResult);
    DEBUG_ASSERT(m_strongPtr);
    DEBUG_ASSERT(trackId.isValid());
    if (m_trackRef.getId().isValid()) {
        // Ignore initializing the same id twice
        DEBUG_ASSERT(m_trackRef.getId() == trackId);
    } else {
        m_trackRef = m_pInstance->initTrackId(
                m_strongPtr,
                m_trackRef,
                trackId);
    }
    unlockCache();
    DEBUG_ASSERT(m_trackRef == createTrackRef(*m_strongPtr));
}

//static
void GlobalTrackCache::createInstance(GlobalTrackCacheEvictor* pEvictor) {
    DEBUG_ASSERT(QApplication::instance()->thread() == QThread::currentThread());
    DEBUG_ASSERT(!s_pInstance);
    s_pInstance = new GlobalTrackCache(pEvictor);
}

//static
void GlobalTrackCache::destroyInstance() {
    DEBUG_ASSERT(QApplication::instance()->thread() == QThread::currentThread());
    DEBUG_ASSERT(s_pInstance);
    GlobalTrackCache* pInstance = s_pInstance;
    // Reset the static/global pointer before entering the destructor
    s_pInstance = nullptr;
    // Delete the singular instance
    delete pInstance;
}

//static
void GlobalTrackCache::saver(TrackUseTracked* pTrack) {
    if (!pTrack) {
        return;
    }
    if (s_pInstance) {
        s_pInstance->evictAndSaveIfLast(pTrack);
    } else if (pTrack->removeUse() == 1) {
        VERIFY_OR_DEBUG_ASSERT(!pTrack->isDirty()) {
            // We are the last user. Simply omit saving when the cache is no
            // longer available. This should not happen.
            kLogger.warning()
                    << "Omit saving uncached track"
                    << pTrack->getCanonicalLocation();
        }
    }
}

//static
void GlobalTrackCache::deleter(TrackUseTracked* plainPtr) {
    DEBUG_ASSERT(plainPtr);
    deleteTrack(plainPtr);
}

GlobalTrackCache::GlobalTrackCache(GlobalTrackCacheEvictor* pEvictor)
    : m_mutex(QMutex::Recursive),
      m_pEvictor(pEvictor),
      m_tracksById(kUnorderedCollectionMinCapacity, DbId::hash_fun) {
    DEBUG_ASSERT(m_pEvictor);
    DEBUG_ASSERT(verifyConsistency());
}

GlobalTrackCache::~GlobalTrackCache() {
    deactivate();
}

bool GlobalTrackCache::verifyConsistency() {
    {
        TracksById::iterator i = m_tracksById.begin();
        while (i != m_tracksById.end()) {
            const std::shared_ptr<Track> strongPtr = (*i).second.lock();
            VERIFY_OR_DEBUG_ASSERT(strongPtr) {
                i = m_tracksById.erase(i);
                continue;
            }
            const TrackRef trackRef = createTrackRef(*strongPtr);
            VERIFY_OR_DEBUG_ASSERT(trackRef.getId().isValid()) {
                return false;
            }
            VERIFY_OR_DEBUG_ASSERT(trackRef.getId() == (*i).first) {
                return false;
            }
            const QString canonicalLocation = trackRef.getCanonicalLocation();
            if (!canonicalLocation.isEmpty()) {
                VERIFY_OR_DEBUG_ASSERT(
                        1 == m_tracksByCanonicalLocation.count(canonicalLocation)) {
                    return false;
                }
                TracksByCanonicalLocation::const_iterator j =
                        m_tracksByCanonicalLocation.find(canonicalLocation);
                VERIFY_OR_DEBUG_ASSERT(m_tracksByCanonicalLocation.end() != j) {
                    return false;
                }
                VERIFY_OR_DEBUG_ASSERT((*j).second.lock() == strongPtr) {
                    return false;
                }
            }
            ++i;
        }
    }
    {
        TracksByCanonicalLocation::iterator i = m_tracksByCanonicalLocation.begin();
        while (i != m_tracksByCanonicalLocation.end()) {
            const std::shared_ptr<Track> strongPtr = (*i).second.lock();
            VERIFY_OR_DEBUG_ASSERT(strongPtr) {
                i = m_tracksByCanonicalLocation.erase(i);
                continue;
            }
            const TrackRef trackRef = createTrackRef(*strongPtr);
            VERIFY_OR_DEBUG_ASSERT(!trackRef.getCanonicalLocation().isEmpty()) {
                return false;
            }
            VERIFY_OR_DEBUG_ASSERT(trackRef.getCanonicalLocation() == (*i).first) {
                return false;
            }
            VERIFY_OR_DEBUG_ASSERT(1 == m_tracksByCanonicalLocation.count(trackRef.getCanonicalLocation())) {
                return false;
            }
            TracksById::const_iterator j = m_tracksById.find(trackRef.getId());
            VERIFY_OR_DEBUG_ASSERT(
                    (m_tracksById.end() == j) || ((*j).second.lock() == strongPtr)) {
                return false;
            }
            ++i;
        }
    }
    return true;
}

void GlobalTrackCache::deactivate() {
    DEBUG_ASSERT(verifyConsistency());
    // Ideally the cache should be empty when destroyed.
    // But since this is difficult to achieve all remaining
    // cached tracks will evicted no matter if they are still
    // referenced or not. This ensures that the eviction
    // callback is triggered for all modified tracks before
    // exiting the application.
    while (!m_tracksById.empty()) {
        evictAndSave(
                nullptr,
                m_tracksById.begin()->second.lock().get());
    }
    while (!m_tracksByCanonicalLocation.empty()) {
        evictAndSave(
                nullptr,
                m_tracksByCanonicalLocation.begin()->second.lock().get());
    }
    m_pEvictor = nullptr;
}

void GlobalTrackCache::reset() {
    deactivate();
}

bool GlobalTrackCache::isEmpty() const {
    return m_tracksById.empty() && m_tracksByCanonicalLocation.empty();
}

TrackPointer GlobalTrackCache::lookupById(
        const TrackId& trackId) {
    const auto trackById(m_tracksById.find(trackId));
    if (m_tracksById.end() != trackById) {
        // Cache hit
        TrackPointer strongPtr(trackById->second);
        VERIFY_OR_DEBUG_ASSERT(strongPtr) {
            m_tracksById.erase(trackById);
        } else {
            if (traceLogEnabled()) {
                qDebug()
                        << "Cache hit for"
                        << trackId
                        << strongPtr.get();
            }
            return strongPtr;
        }
    }

    // Cache miss
    if (traceLogEnabled()) {
        kLogger.trace()
                << "Cache miss for"
                << trackId;
    }
    return TrackPointer();
}

TrackPointer GlobalTrackCache::lookupByRef(
        const TrackRef& trackRef) {
    if (trackRef.hasId()) {
        return lookupById(trackRef.getId());
    } else {
        const auto canonicalLocation = trackRef.getCanonicalLocation();
        const auto trackByCanonicalLocation(
                m_tracksByCanonicalLocation.find(canonicalLocation));
        if (m_tracksByCanonicalLocation.end() != trackByCanonicalLocation) {
            // Cache hit
            TrackPointer strongPtr(trackByCanonicalLocation->second);
            VERIFY_OR_DEBUG_ASSERT(strongPtr) {
                m_tracksByCanonicalLocation.erase(trackByCanonicalLocation);
            } else {
                if (traceLogEnabled()) {
                    kLogger.trace()
                            << "Cache hit for"
                            << canonicalLocation
                            << strongPtr.get();
                }
                return strongPtr;
            }
        }

        // Cache miss
        if (traceLogEnabled()) {
            kLogger.trace()
                    << "Cache miss for"
                    << canonicalLocation;
        }
        return TrackPointer();
    }
}

void GlobalTrackCache::resolve(
        GlobalTrackCacheResolver* /*in/out*/ pCacheResolver,
        QFileInfo /*in*/ fileInfo,
        TrackId trackId,
        SecurityTokenPointer pSecurityToken) {
    DEBUG_ASSERT(pCacheResolver);
    // Primary lookup by id (if available)
    if (trackId.isValid()) {
        if (debugLogEnabled()) {
            kLogger.debug()
                    << "Resolving track by id"
                    << trackId;
        }
        auto strongPtr = lookupById(trackId);
        if (strongPtr) {
            if (debugLogEnabled()) {
                kLogger.debug()
                        << "Cache hit - found track by id"
                        << trackId
                        << strongPtr.get();
            }
            TrackRef trackRef = createTrackRef(*strongPtr);
            pCacheResolver->initLookupResult(
                    GlobalTrackCacheLookupResult::HIT,
                    std::move(strongPtr),
                    std::move(trackRef));
            return;
        }
    }
    // Secondary lookup by canonical location
    // The TrackRef is constructed now after the lookup by ID failed to
    // avoid calculating the canonical file path if it is not needed.
    TrackRef trackRef = TrackRef::fromFileInfo(fileInfo, trackId);
    if (trackRef.hasCanonicalLocation()) {
        if (debugLogEnabled()) {
            kLogger.debug()
                    << "Resolving track by canonical location"
                    << trackRef.getCanonicalLocation();
        }
        auto strongPtr = lookupByRef(trackRef);
        if (strongPtr) {
            // Cache hit
            if (debugLogEnabled()) {
                kLogger.debug()
                        << "Cache hit - found track by canonical location"
                        << trackRef.getCanonicalLocation()
                        << strongPtr.get();
            }
            pCacheResolver->initLookupResult(
                    GlobalTrackCacheLookupResult::HIT,
                    std::move(strongPtr),
                    std::move(trackRef));
            return;
        }
    }
    if (!m_pEvictor) {
        // Do not allocate any new tracks once the cache
        // has been deactivated
        DEBUG_ASSERT(isEmpty());
        kLogger.warning()
                << "Cache miss - caching has already been deactivated"
                << trackRef;
        return;
    }
    if (debugLogEnabled()) {
        kLogger.debug()
                << "Cache miss - allocating track"
                << trackRef;
    }
    auto strongPtr = TrackPointer(
            new TrackUseTracked(
                    std::move(fileInfo),
                    std::move(pSecurityToken),
                    std::move(trackId),
                    saver),
            deleter);
    // Track objects live together with the cache on the main thread
    // and will be deleted later within the event loop. But this
    // function might be called from any thread, even from worker
    // threads without an event loop. We need to move the newly
    // created object to the target thread.
    strongPtr->moveToThread(QApplication::instance()->thread());
    if (debugLogEnabled()) {
        kLogger.debug()
                << "Cache miss - inserting new track into cache"
                << trackRef
                << strongPtr.get();
    }
    TrackWeakPointer weakPtr(strongPtr);
    if (trackRef.hasId()) {
        // Insert item by id
        DEBUG_ASSERT(m_tracksById.find(
                trackRef.getId()) == m_tracksById.end());
        m_tracksById.insert(std::make_pair(
                trackRef.getId(),
                weakPtr));
    }
    if (trackRef.hasCanonicalLocation()) {
        // Insert item by track location
        DEBUG_ASSERT(m_tracksByCanonicalLocation.find(
                trackRef.getCanonicalLocation()) == m_tracksByCanonicalLocation.end());
        m_tracksByCanonicalLocation.insert(std::make_pair(
                trackRef.getCanonicalLocation(),
                weakPtr));
    }
    pCacheResolver->initLookupResult(
            GlobalTrackCacheLookupResult::MISS,
            std::move(strongPtr),
            std::move(trackRef));
}

TrackRef GlobalTrackCache::initTrackId(
        const TrackPointer& strongPtr,
        TrackRef trackRef,
        TrackId trackId) {
    DEBUG_ASSERT(strongPtr);
    DEBUG_ASSERT(!trackRef.getId().isValid());
    DEBUG_ASSERT(trackId.isValid());
    DEBUG_ASSERT(m_tracksByCanonicalLocation.find(
            trackRef.getCanonicalLocation()) != m_tracksByCanonicalLocation.end());
    TrackRef trackRefWithId(trackRef, trackId);

    // Insert item by id
    DEBUG_ASSERT(m_tracksById.find(trackId) == m_tracksById.end());
    m_tracksById.insert(std::make_pair(
            trackId,
            TrackWeakPointer(strongPtr)));

    strongPtr->initId(trackId);
    DEBUG_ASSERT(createTrackRef(*strongPtr) == trackRefWithId);

    return trackRefWithId;
}

void GlobalTrackCache::afterEvicted(
        GlobalTrackCacheLocker* pCacheLocker,
        Track* pEvictedTrack) {
    DEBUG_ASSERT(pEvictedTrack);

    // It can produce dangerous signal loops if the track is still
    // sending signals while being saved! All references to this
    // track have been dropped at this point, so there is no need
    // to send any signals.
    // See: https://bugs.launchpad.net/mixxx/+bug/136578
    // NOTE(uklotzde, 2018-02-03): Simply disconnecting all receivers
    // doesn't seem to work reliably. Emitting the clean() signal from
    // a track that is about to deleted may cause access violations!!
    pEvictedTrack->blockSignals(true);

    // Keep the cache locked while evicting the track object!
    // The callback is given the chance to unlock the cache
    // after all operations that rely on managed track ownership
    // have been done, e.g. exporting track metadata into a file.
    m_pEvictor->afterEvictedTrackFromCache(
            pCacheLocker,
            pEvictedTrack);

    // At this point the cache might have been unlocked by the
    // callback!!
}

bool GlobalTrackCache::evictAndSaveIfLast(
        TrackUseTracked* pTrack) {
    GlobalTrackCacheLocker cacheLocker;

    // While saving only a single owner is allowed to
    // guarantee exclusive access to the track and the
    // corresponding file.

    int use_count = pTrack->removeUse();
    //kLogger.debug() << "Saving track with use_count" << use_count;
    if (use_count != 1) {
        // we are not the last user, skip saving
        return false;
    }
    evictAndSave(
            &cacheLocker,
            pTrack);
    return true;
}

void GlobalTrackCache::evictAndSave(
        GlobalTrackCacheLocker* pCacheLocker,
        Track* pTrack) {
    if (!m_pEvictor) {
        // cache is already disabled
        return;
    }

    DEBUG_ASSERT(pTrack);
    const auto trackRef = createTrackRef(*pTrack);
    if (debugLogEnabled()) {
        kLogger.debug()
                << "Evicting indexed track"
                << trackRef
                << pTrack;
    }

    evict(trackRef, pTrack);
    DEBUG_ASSERT(verifyConsistency());
    // The evicted entry must not be accessible anymore!
    DEBUG_ASSERT(!lookupByRef(trackRef));
    afterEvicted(pCacheLocker, pTrack);
    // After returning from the callback the lock might have
    // already been released!
    if (debugLogEnabled()) {
        kLogger.debug()
                << "Deleting evicted track"
                << trackRef
                << pTrack;
    }
}

void GlobalTrackCache::evict(
        const TrackRef& trackRef,
        Track* pTrack) {
    DEBUG_ASSERT(pTrack);
    if (trackRef.hasId()) {
        const auto trackById = m_tracksById.find(trackRef.getId());
        if (trackById != m_tracksById.end()) {
            m_tracksById.erase(trackById);
        }
    }
    if (trackRef.hasCanonicalLocation()) {
        const auto trackByCanonicalLocation(
                m_tracksByCanonicalLocation.find(trackRef.getCanonicalLocation()));
        if (m_tracksByCanonicalLocation.end() != trackByCanonicalLocation) {
            m_tracksByCanonicalLocation.erase(
                    trackByCanonicalLocation);
        }
    }
}


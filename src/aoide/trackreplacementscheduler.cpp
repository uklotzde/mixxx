#include "aoide/trackreplacementscheduler.h"

#include <QMetaMethod>
#include <QThread>

#include <mutex>

#include "aoide/gateway.h"
#include "aoide/translator.h"

#include "library/asynctrackloader.h"

#include "util/logger.h"

namespace mixxx {

namespace aoide {

namespace {

const Logger kLogger("aoide TrackReplacementScheduler");

const int kMaxLoading = 8;

// The JSON representation of a track has a size of 2 to 5 kB depending
// on the amount of metadata. Batching requests help to reduce network
// traffic with a small chance that a whole batch might fail. Within
// a batch individual tracks might be rejected or skipped by the server
// without failing the whole batch.
// TODO:
//  - Define an initial batch size, e.g. 8 items
//  - Define a max. batch size, e.g. 64 items (~ 5kB serialized JSON data per track)
//  - Define a max. latency, e.g. 500 ms
//  - Measure actual mean latency as a moving average
//  - Dynamically either halve or double the batch size
//    after measuring a sufficient number of latencies with
//    the current batch size if the actual mean latency
//    is too high or low respectively.
const int kBatchSize = 64;

// 2 pending batches + some slots for loading tracks
const int kMaxPending = 2 * (kMaxLoading + kBatchSize);

std::once_flag registerMetaTypesOnceFlag;

void registerMetaTypes() {
    qRegisterMetaType<QList<TrackRef>>();
}

} // anonymous namespace

TrackReplacementScheduler::TrackReplacementScheduler(
        QPointer<Gateway> gateway, QPointer<AsyncTrackLoader> trackLoader, QObject* parent)
    : QObject(parent),
      m_gateway(gateway),
      m_trackLoader(trackLoader),
      m_pendingCounter(0),
      m_succeededCounter(0),
      m_failedCounter(0) {
    m_loadingTrackRefs.reserve(kMaxLoading);
    DEBUG_ASSERT(kMaxLoading <= kMaxPending);
    DEBUG_ASSERT(kBatchSize <= kMaxPending);
    std::call_once(registerMetaTypesOnceFlag, registerMetaTypes);
}

void TrackReplacementScheduler::connectPeers() {
    connect(m_gateway, &Gateway::networkRequestFailed, this,
            &TrackReplacementScheduler::networkRequestFailed);
    connect(m_gateway, &Gateway::replaceTracksResult, this,
            &TrackReplacementScheduler::replaceTracksResult);
    connect(m_trackLoader, &AsyncTrackLoader::trackLoaded, this,
            &TrackReplacementScheduler::trackLoaded);
}

bool TrackReplacementScheduler::isLoading(const TrackRef& trackRef) const {
    int count = m_loadingTrackRefs.count(trackRef);
    DEBUG_ASSERT(count <= 1);
    return count > 0;
}

bool TrackReplacementScheduler::enterLoading(const TrackRef& trackRef) {
    if (isLoading(trackRef)) {
        return false;
    } else {
        m_loadingTrackRefs.append(trackRef);
        return true;
    }
}

bool TrackReplacementScheduler::leaveLoading(const TrackRef& trackRef) {
    int removed = m_loadingTrackRefs.removeAll(trackRef);
    DEBUG_ASSERT(removed <= 1);
    return removed > 0;
}

void TrackReplacementScheduler::replaceTracksAsync(
        QString collectionUid, QList<TrackRef> trackRefs) {
    QMetaObject::invokeMethod(
            this, "replaceTracks",
            Qt::QueuedConnection, // async
            Q_ARG(QString, std::move(collectionUid)), Q_ARG(QList<TrackRef>, std::move(trackRefs)));
}

void TrackReplacementScheduler::replaceTracks(QString collectionUid, QList<TrackRef> trackRefs) {
    DEBUG_ASSERT(thread() == QThread::currentThread());
    VERIFY_OR_DEBUG_ASSERT(!collectionUid.isEmpty()) {
        kLogger.warning() << "Cannot replace" << trackRefs.size()
                          << "tracks without a collection UID";
    }
    if (m_collectionUid.isEmpty() || (m_collectionUid == collectionUid)) {
        m_collectionUid = std::move(collectionUid);
        m_queuedTrackRefs.append(std::move(trackRefs));
        makeProgress();
    } else {
        m_deferredRequests.append(std::make_pair(std::move(collectionUid), std::move(trackRefs)));
    }
}

void TrackReplacementScheduler::cancelAsync() {
    QMetaObject::invokeMethod(this, "cancel",
                              Qt::QueuedConnection); // async
}

void TrackReplacementScheduler::cancel() {
    DEBUG_ASSERT(thread() == QThread::currentThread());
    m_deferredRequests.clear();
    m_collectionUid.clear();
    m_queuedTrackRefs.clear();
    makeProgress();
}

void TrackReplacementScheduler::trackLoaded(TrackRef trackRef, TrackPointer trackPtr) {
    if (!leaveLoading(trackRef)) {
        if (kLogger.debugEnabled()) {
            kLogger.debug() << "Ignoring loaded track" << trackRef;
        }
        return;
    }
    if (!trackPtr) {
        kLogger.warning() << "Failed to load track" << trackRef;
        --m_pendingCounter;
        ++m_failedCounter;
        emitProgress();
        return;
    }
    if (!m_gateway || m_collectionUid.isEmpty()) {
        kLogger.warning() << "Skipping loaded track" << trackRef;
        --m_pendingCounter;
        ++m_failedCounter;
        emitProgress();
        return;
    }

    const Translator translator(m_collectionUid);
    m_bufferedRequests += translator.exportTrack(*trackPtr);
    if ((m_bufferedRequests.size() >= kBatchSize) ||
        (m_queuedTrackRefs.isEmpty() && m_loadingTrackRefs.isEmpty())) {
        AsyncRestClient::RequestId requestId =
                m_gateway->replaceTracksAsync(translator.collectionUid(), m_bufferedRequests);
        DEBUG_ASSERT(!m_pendingRequests.contains(requestId));
        m_pendingRequests.insert(std::move(requestId));
        m_bufferedRequests.clear();
    }

    makeProgress();
}

void TrackReplacementScheduler::replaceTracksResult(
        AsyncRestClient::RequestId requestId, QJsonObject result) {
    DEBUG_ASSERT(m_pendingRequests.contains(requestId));
    m_pendingRequests.remove(requestId);
    DEBUG_ASSERT(result.contains("created"));
    DEBUG_ASSERT(result.value("created").isArray());
    int created = result.value("created").toArray().size();
    DEBUG_ASSERT(created >= 0);
    DEBUG_ASSERT(result.contains("updated"));
    DEBUG_ASSERT(result.value("updated").isArray());
    int updated = result.value("updated").toArray().size();
    DEBUG_ASSERT(updated >= 0);
    DEBUG_ASSERT(result.contains("skipped"));
    DEBUG_ASSERT(result.value("skipped").isArray());
    int skipped = result.value("skipped").toArray().size();
    DEBUG_ASSERT(skipped >= 0);
    int replaced = created + updated + skipped;
    DEBUG_ASSERT(replaced <= m_pendingCounter);
    m_pendingCounter -= replaced;
    m_succeededCounter += replaced;
    DEBUG_ASSERT(result.contains("rejected"));
    DEBUG_ASSERT(result.value("rejected").isArray());
    int rejected = result.value("rejected").toArray().size();
    DEBUG_ASSERT(rejected >= 0);
    DEBUG_ASSERT(result.contains("discarded"));
    DEBUG_ASSERT(result.value("discarded").isArray());
    int discarded = result.value("discarded").toArray().size();
    DEBUG_ASSERT(discarded == 0); // none expected
    int failed = rejected + discarded;
    DEBUG_ASSERT(failed <= m_pendingCounter);
    m_pendingCounter -= failed;
    m_failedCounter += failed;
    if (kLogger.debugEnabled()) {
        kLogger.debug() << "Replaced" << replaced << "track(s) (" << created << "created +"
                        << updated << "updated +" << skipped << "skipped )";
    }
    if (failed > 0) {
        kLogger.warning() << "Failed to replace" << failed << "tracks";
    }
    emitProgress();
    makeProgress();
}

void TrackReplacementScheduler::networkRequestFailed(
        AsyncRestClient::RequestId requestId, QString errorMessage) {
    if (m_pendingRequests.contains(requestId)) {
        m_pendingRequests.remove(requestId);
        kLogger.warning() << "Failed to replace track entity" << errorMessage;
        --m_pendingCounter;
        ++m_failedCounter;
        emitProgress();
        makeProgress();
    }
}

void TrackReplacementScheduler::makeProgress() {
    while (!m_collectionUid.isEmpty()) {
        while (!m_queuedTrackRefs.isEmpty() && (m_loadingTrackRefs.size() < kMaxLoading) &&
               (m_pendingCounter < kMaxPending)) {
            TrackRef trackRef = m_queuedTrackRefs.dequeue();
            if (enterLoading(trackRef)) {
                VERIFY_OR_DEBUG_ASSERT(m_trackLoader) {
                    kLogger.warning() << "Cannot load track";
                    dumpObjectInfo();
                    ++m_failedCounter;
                    break;
                }
                ++m_pendingCounter;
                m_trackLoader->loadTrackAsync(std::move(trackRef));
            } else {
                // We can safely skip this dequeued track
                if (kLogger.debugEnabled()) {
                    kLogger.debug() << "Track is already loading" << trackRef;
                }
            }
        }
        DEBUG_ASSERT(m_pendingCounter >= 0);
        DEBUG_ASSERT(m_succeededCounter >= 0);
        DEBUG_ASSERT(m_failedCounter >= 0);
        if (m_queuedTrackRefs.isEmpty() && (m_pendingCounter == 0)) {
            // Idle -> reset
            m_collectionUid.clear();
            m_succeededCounter = 0;
            m_failedCounter = 0;
        }
        emitProgress();
        if (m_collectionUid.isEmpty() && !m_deferredRequests.isEmpty()) {
            // Idle -> next batch
            DEBUG_ASSERT(m_queuedTrackRefs.isEmpty());
            auto nextBatch = m_deferredRequests.dequeue();
            m_collectionUid = std::move(nextBatch.first);
            m_queuedTrackRefs.append(std::move(nextBatch.second));
        } else {
            // Return to event loop
            break;
        }
    }
}

void TrackReplacementScheduler::emitProgress() {
    const int queued = m_queuedTrackRefs.size();
    if (kLogger.debugEnabled()) {
        kLogger.debug() << "Emitting progress"
                        << ": queued" << queued << "/ pending" << m_pendingCounter << "/ succeeded"
                        << m_succeededCounter << "/ failed" << m_failedCounter;
    }
    emit progress(queued, m_pendingCounter, m_succeededCounter, m_failedCounter);
}

} // namespace aoide

} // namespace mixxx

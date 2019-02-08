#include "library/asynctrackloader.h"

#include <QMetaMethod>
#include <QThread>

#include <mutex>

#include "library/trackcollection.h"
#include "util/logger.h"


namespace mixxx {

namespace {

const Logger kLogger("AsyncTrackLoader");

std::once_flag registerMetaTypesOnceFlag;

void registerMetaTypes() {
    qRegisterMetaType<TrackId>();
    qRegisterMetaType<TrackRef>();
    qRegisterMetaType<TrackPointer>();
}

QPointer<TrackCollection> getTrackCollection(
        QPointer<TrackCollection> trackCollection) {
    if (trackCollection) {
        VERIFY_OR_DEBUG_ASSERT(QThread::currentThread() == trackCollection->thread()) {
            kLogger.critical()
                    << "Execution in different threads is not supported:"
                    << QThread::currentThread()
                    << "<>"
                    << trackCollection->thread();
            return QPointer<TrackCollection>();
        }
    }
    return trackCollection;
}

} // anonymous namespace

AsyncTrackLoader::AsyncTrackLoader(
        QPointer<TrackCollection> trackCollection,
        QObject* parent)
    : QObject(parent),
      m_trackCollection(std::move(trackCollection)) {
    std::call_once(registerMetaTypesOnceFlag, registerMetaTypes);
}

void AsyncTrackLoader::loadTrackAsync(
        TrackRef trackRef) {
    QMetaObject::invokeMethod(
            this,
            "loadTrack",
            Qt::QueuedConnection, // async,
            Q_ARG(TrackRef, std::move(trackRef)));
}

void AsyncTrackLoader::loadTrack(
        TrackRef trackRef) {
    DEBUG_ASSERT(thread() == QThread::currentThread());
    TrackPointer trackPtr;
    const auto trackCollection = getTrackCollection(m_trackCollection);
    if (trackCollection) {
        if (trackRef.hasId()) {
            trackPtr = trackCollection->getTrackDAO().getTrack(trackRef.getId());
        }
        if (!trackPtr && trackRef.hasLocation()) {
            trackPtr = trackCollection->getTrackDAO().getOrAddTrack(trackRef.getLocation(), false, nullptr);
        }
    } else {
        kLogger.warning()
                << "Track collection not accessible";
    }
    emit trackLoaded(std::move(trackRef), std::move(trackPtr));
}

} // namespace mixxx

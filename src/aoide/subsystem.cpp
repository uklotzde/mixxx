#include "aoide/subsystem.h"

#include <QMetaObject>

#include "aoide/gateway.h"
#include "aoide/trackreplacementscheduler.h"

#include "library/asynctrackloader.h"

#include "util/logger.h"


namespace mixxx {

namespace aoide {

namespace {

const Logger kLogger("aoide Subsystem");

const QString kThreadName = "aoide";

const AoideCollectionEntity DEFAULT_COLLECTION_ENTITY = AoideCollectionEntity();

bool findCollectionByUid(
        const QVector<AoideCollectionEntity>& allCollections,
        const QString& collectionUid,
        AoideCollectionEntity* result = nullptr) {
    for (const auto& collection: allCollections) {
        if (collectionUid == collection.header().uid()) {
            if (result) {
                *result = collection;
            }
            return true;
        }
    }
    if (result) {
        *result = DEFAULT_COLLECTION_ENTITY;
    }
    return false;
}

} // anonymous namespace

Subsystem::Subsystem(
        UserSettingsPointer userSettings,
        QPointer<AsyncTrackLoader> trackLoader,
        QObject* parent)
    : QObject(parent),
      m_settings(std::move(userSettings)),
      m_networkAccessManager(new QNetworkAccessManager),
      m_gateway(new mixxx::aoide::Gateway(m_settings, m_networkAccessManager)),
      m_trackReplacementScheduler(new TrackReplacementScheduler(m_gateway, trackLoader)) {
    m_thread.setObjectName(kThreadName);

    m_networkAccessManager->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_networkAccessManager, &QObject::deleteLater);

    m_gateway->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_gateway, &QObject::deleteLater);
    m_gateway->connectPeers();

    m_trackReplacementScheduler->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_trackReplacementScheduler, &QObject::deleteLater);
    m_trackReplacementScheduler->connectPeers();
}

void Subsystem::startup(
        QThread::Priority threadPriority) {
    kLogger.info() << "Starting up";
    DEBUG_ASSERT(thread() == QThread::currentThread());
    m_thread.start(threadPriority);
    connect(m_gateway, &Gateway::listCollectionsResult, this, &Subsystem::listCollectionsResultResponse);
    connect(m_gateway, &Gateway::createCollectionResult, this, &Subsystem::createCollectionResultResponse);
    connect(m_gateway, &Gateway::updateCollectionResult, this, &Subsystem::updateCollectionResultResponse);
    connect(m_gateway, &Gateway::deleteCollectionResult, this, &Subsystem::deleteCollectionResultResponse);
    connect(m_gateway, &Gateway::searchTracksResult, this, &Subsystem::searchTracksResultResponse);
    connect(m_trackReplacementScheduler, &TrackReplacementScheduler::progress, this, &Subsystem::replaceTracksProgress); // signal/signal pass-through
    refreshCollectionsAsync();
}

void Subsystem::shutdown() {
    kLogger.info() << "Shutting down";
    DEBUG_ASSERT(thread() == QThread::currentThread());
    cancelReplacingTracksAsync();
    m_thread.quit();
    m_thread.wait();
}

void Subsystem::selectActiveCollection(
        const QString& collectionUid) {
    DEBUG_ASSERT(thread() == QThread::currentThread());
    QString activeCollectionUidBefore = m_activeCollection.header().uid();
    findCollectionByUid(m_allCollections, collectionUid, &m_activeCollection);
    QString activeCollectionUidAfter = m_activeCollection.header().uid();
    if (activeCollectionUidBefore != activeCollectionUidAfter) {
        if (hasActiveCollection()) {
            // Only overwrite the settings if a different collection
            // has actually been selected!
            m_settings.setCollectionUid(activeCollectionUidAfter);
            kLogger.info()
                    << "Selected active collection:"
                    << m_activeCollection;
        }
        emit collectionsChanged(
                CollectionsChangedFlags::ACTIVE_COLLECTION);
    }
}

void Subsystem::refreshCollectionsAsync() {
    m_gateway->listCollectionsAsync();
}

void Subsystem::createCollectionAsync(
        AoideCollection collection) {
    m_gateway->createCollectionAsync(collection);
}

void Subsystem::updateCollectionAsync(
        AoideCollectionEntity collectionEntity) {
    m_gateway->updateCollectionAsync(collectionEntity);
}

void Subsystem::deleteCollectionAsync(
        QString collectionUid) {
    m_gateway->deleteCollectionAsync(collectionUid);
}

void Subsystem::searchTracksAsync(
        QString phraseQuery,
        AoidePagination pagination) {
    // Accesses mutable member variables -> not thread-safe
    DEBUG_ASSERT(thread() == QThread::currentThread());
    VERIFY_OR_DEBUG_ASSERT(hasActiveCollection()) {
        kLogger.warning()
                << "Cannot search tracks without active collection";
        return;
    }
    const auto requestId = m_gateway->searchTracksAsync(
            m_activeCollection.header().uid(),
            std::move(phraseQuery),
            std::move(pagination));
    if (kLogger.debugEnabled()) {
        kLogger.debug()
                << "Sent request"
                << requestId
                << "to search for tracks";
    }
}

void Subsystem::replaceTracksAsync(
        QList<TrackRef> trackRefs) {
    // Accesses mutable member variables -> not thread-safe
    DEBUG_ASSERT(thread() == QThread::currentThread());
    VERIFY_OR_DEBUG_ASSERT(hasActiveCollection()) {
        kLogger.warning()
                << "Cannot replace tracks without active collection";
        return;
    }
    m_trackReplacementScheduler->replaceTracksAsync(
            m_activeCollection.header().uid(),
            std::move(trackRefs));
}

void Subsystem::cancelReplacingTracksAsync() {
    m_trackReplacementScheduler->cancelAsync();
}

void Subsystem::listCollectionsResultResponse(
        mixxx::AsyncRestClient::RequestId /*requestId*/,
        QVector<AoideCollectionEntity> result) {
    m_allCollections = std::move(result);
    int changedFlags = CollectionsChangedFlags::ALL_COLLECTIONS;
    if (hasActiveCollection()) {
        if (!findCollectionByUid(m_allCollections, m_activeCollection.header().uid(), &m_activeCollection)) {
            // active collection has been reset
            kLogger.info()
                    << "Deselected active collection";
            changedFlags |= CollectionsChangedFlags::ACTIVE_COLLECTION;
        }
    } else {
        auto settingsCollectionUid = m_settings.collectionUid();
        for (auto&& collection: qAsConst(m_allCollections)) {
            if (collection.header().uid() == settingsCollectionUid) {
                m_activeCollection = collection;
                kLogger.info()
                        << "Reselected active collection:"
                        << m_activeCollection;
                changedFlags |= CollectionsChangedFlags::ACTIVE_COLLECTION;
                break; // exit loop
            }
        }
    }
    emit collectionsChanged(changedFlags);
}

void Subsystem::createCollectionResultResponse(
        mixxx::AsyncRestClient::RequestId /*requestId*/,
        AoideEntityHeader /*result*/) {
    refreshCollectionsAsync();
}

void Subsystem::updateCollectionResultResponse(
        mixxx::AsyncRestClient::RequestId /*requestId*/,
        AoideEntityHeader /*result*/) {
    refreshCollectionsAsync();
}

void Subsystem::deleteCollectionResultResponse(
        mixxx::AsyncRestClient::RequestId /*requestId*/) {
    refreshCollectionsAsync();
}

void Subsystem::searchTracksResultResponse(
        mixxx::AsyncRestClient::RequestId requestId,
        QVector<AoideTrackEntity> result) {
    if (kLogger.debugEnabled()) {
        kLogger.debug()
                << "Received result of tracks search"
                << requestId;
    }
    emit searchTracksResult(std::move(result));
}

} // namespace aoide

} // namespace mixxx

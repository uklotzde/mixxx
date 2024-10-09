#include "aoide/gateway.h"

#include <QMetaObject>
#include <QThread>
#include <mutex> // std::once_flag

#include "aoide/web/exporttrackfilestask.h"
#include "aoide/web/listcollectionstask.h"
#include "aoide/web/listplayliststask.h"
#include "aoide/web/searchtrackstask.h"
#include "aoide/web/shutdowntask.h"
#include "util/encodedurl.h"
#include "util/logger.h"

namespace {

const mixxx::Logger kLogger("aoide Gateway");

std::once_flag registerMetaTypesOnceFlag;

} // anonymous namespace

namespace aoide {

namespace {

void registerMetaTypesOnce() {
    qRegisterMetaType<Pagination>("aoide::Pagination");

    qRegisterMetaType<json::EntityUid>("aoide::json::EntityUid");
    qRegisterMetaType<json::EntityRevision>("aoide::json::EntityRevision");
    qRegisterMetaType<json::EntityHeader>("aoide::json::EntityHeader");

    qRegisterMetaType<json::Collection>("aoide::json::Collection");
    qRegisterMetaType<json::CollectionEntity>("aoide::json::CollectionEntity");
    qRegisterMetaType<QVector<json::CollectionEntity>>("aoide::QVector<json::CollectionEntity>");

    qRegisterMetaType<json::Title>("aoide::json::Title");
    qRegisterMetaType<json::TitleVector>("aoide::json::TitleVector");
    qRegisterMetaType<json::Actor>("aoide::json::Actor");
    qRegisterMetaType<json::ActorVector>("aoide::json::ActorVector");

    qRegisterMetaType<json::TrackOrAlbum>("aoide::json::TrackOrAlbum");
    qRegisterMetaType<json::Album>("aoide::json::Album");
    qRegisterMetaType<json::Track>("aoide::json::Track");
    qRegisterMetaType<json::TrackEntity>("aoide::json::TrackEntity");
    qRegisterMetaType<QVector<json::TrackEntity>>("aoide::QVector<json::TrackEntity>");

    qRegisterMetaType<json::Playlist>("aoide::json::Playlist");
    qRegisterMetaType<json::PlaylistEntry>("aoide::json::PlaylistEntry");
    qRegisterMetaType<json::PlaylistEntity>("aoide::json::PlaylistEntity");
    qRegisterMetaType<QVector<json::PlaylistEntity>>("aoide::QVector<json::PlaylistEntity>");
    qRegisterMetaType<json::PlaylistWithEntriesSummary>("aoide::json::PlaylistWithEntriesSummary");
    qRegisterMetaType<json::PlaylistWithEntriesSummary>("aoide::json::PlaylistWithEntriesSummary");
    qRegisterMetaType<json::PlaylistWithEntriesSummaryEntity>(
            "aoide::json::PlaylistWithEntriesSummaryEntity");
    qRegisterMetaType<QVector<json::PlaylistWithEntriesSummaryEntity>>(
            "aoide::QVector<json::PlaylistWithEntriesSummaryEntity>");

    qRegisterMetaType<aoide::TrackSearchOverlayFilter>(
            "aoide::TrackSearchOverlayFilter");
}

} // anonymous namespace

Gateway::Gateway(
        const QUrl& baseUrl,
        QObject* parent)
        : QObject(parent),
          m_baseUrl(std::move(baseUrl)),
          m_networkAccessManager(new QNetworkAccessManager(this)),
          m_pendingWriteTasks(0),
          m_state(State::Active),
          m_shutdownTimeoutMillis(0) {
    std::call_once(registerMetaTypesOnceFlag, registerMetaTypesOnce);
    DEBUG_ASSERT(m_baseUrl.isValid());
}

Gateway::~Gateway() = default;

void Gateway::invokeShutdown(int timeoutMillis) {
    QMetaObject::invokeMethod(
            this,
            [this, timeoutMillis] {
                this->slotShutdown(timeoutMillis);
            });
}

void Gateway::slotShutdown(int timeoutMillis) {
    if (m_state == State::ShuttingDown) {
        return;
    }
    m_state = State::ShutdownPending;
    m_shutdownTimeoutMillis = timeoutMillis;
    const auto pendingWriteTasks = m_pendingWriteTasks.loadAcquire();
    if (pendingWriteTasks > 0) {
        kLogger.info()
                << "Delaying shutdown until"
                << pendingWriteTasks
                << "pending write task(s) have been finished";
        return;
    }
    kLogger.info()
            << "Shutting down";
    auto* task = new ShutdownTask(
            m_networkAccessManager,
            m_baseUrl);
    m_state = State::ShuttingDown;
    // The started task will be deleted implicitly after
    // receiving a reply.
    task->invokeStart(
            m_shutdownTimeoutMillis);
    emit shuttingDown();
}

void Gateway::slotNetworkTaskAborted(
        const QUrl& requestUrl) {
    auto* const pWebTask =
            qobject_cast<mixxx::network::WebTask*>(sender());
    VERIFY_OR_DEBUG_ASSERT(pWebTask) {
        return;
    }
    DEBUG_ASSERT(pWebTask->parent() == this);
    kLogger.info()
            << pWebTask
            << "Web task aborted"
            << requestUrl;
    pWebTask->deleteLater();
}

void Gateway::slotWebTaskNetworkError(
        QNetworkReply::NetworkError errorCode,
        const QString& errorString,
        const mixxx::network::WebResponseWithContent responseWithContent) {
    auto* const pWebTask =
            qobject_cast<mixxx::network::WebTask*>(sender());
    VERIFY_OR_DEBUG_ASSERT(pWebTask) {
        return;
    }
    DEBUG_ASSERT(pWebTask->parent() == this);
    kLogger.warning()
            << pWebTask
            << "Web task failed with network error"
            << errorCode
            << errorString
            << responseWithContent;
    pWebTask->deleteLater();
}

void Gateway::slotJsonWebTaskFailed(
        const mixxx::network::JsonWebResponse& response) {
    auto* const pJsonWebTask =
            qobject_cast<mixxx::network::JsonWebTask*>(sender());
    VERIFY_OR_DEBUG_ASSERT(pJsonWebTask) {
        return;
    }
    DEBUG_ASSERT(pJsonWebTask->parent() == this);
    kLogger.warning()
            << pJsonWebTask
            << "JSON web task failed"
            << response;
    pJsonWebTask->deleteLater();
}

void Gateway::slotWriteTaskDestroyed() {
    const auto pendingWriteTasks = m_pendingWriteTasks.fetchAndSubRelease(1);
    DEBUG_ASSERT(pendingWriteTasks > 0);
    Q_UNUSED(pendingWriteTasks)
    if (m_state == State::ShutdownPending) {
        // Retry and continue delayed shutdown
        slotShutdown(m_shutdownTimeoutMillis);
    }
}

void Gateway::connectPendingWriteTask(
        mixxx::network::NetworkTask* task) {
    // Will be invoked from multiple threads and must be thread-safe!
    connect(task,
            &QObject::destroyed,
            this,
            &Gateway::slotWriteTaskDestroyed);
    m_pendingWriteTasks.fetchAndAddAcquire(1);
}

ListCollectionsTask* Gateway::listCollections(
        const QString& kind,
        const Pagination& pagination) {
    return newReadingJsonWebTask<ListCollectionsTask>(
            m_networkAccessManager,
            m_baseUrl,
            kind,
            pagination);
}

SearchTracksTask* Gateway::searchTracks(
        const QString& collectionUid,
        const QJsonObject& baseQuery,
        const TrackSearchOverlayFilter& overlayFilter,
        const QStringList& searchTerms,
        const Pagination& pagination) {
    return newReadingJsonWebTask<SearchTracksTask>(
            m_networkAccessManager,
            m_baseUrl,
            collectionUid,
            baseQuery,
            overlayFilter,
            searchTerms,
            pagination);
}

ExportTrackFilesTask* Gateway::exportTrackFiles(
        const QString& collectionUid,
        const QJsonObject& trackFilter,
        const QString& targetRootPath) {
    return newReadingJsonWebTask<ExportTrackFilesTask>(
            m_networkAccessManager,
            m_baseUrl,
            collectionUid,
            trackFilter,
            targetRootPath);
}

ListPlaylistsTask* Gateway::listPlaylists(
        const QString& collectionUid,
        const QString& kind) {
    return newReadingJsonWebTask<ListPlaylistsTask>(
            m_networkAccessManager,
            m_baseUrl,
            collectionUid,
            kind);
}

} // namespace aoide

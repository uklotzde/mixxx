#pragma once

#include <QByteArray>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStringList>
#include <QUrl>

#include "aoide/json/collection.h"
#include "aoide/json/playlist.h"
#include "aoide/json/track.h"
#include "aoide/settings.h"
#include "aoide/util.h"
#include "network/jsonwebtask.h"
#include "util/optional.h"
#include "util/parented_ptr.h"

class Track;

namespace aoide {

class CreateCollectionTask;
class CreateCollectedPlaylistTask;
class DeleteCollectionTask;
class DeletePlaylistTask;
class ListCollectionsTask;
class ListTagsTask;
class ListTagsFacetsTask;
class ListCollectedPlaylistsTask;
class PlaylistAppendTrackEntriesByUrlTask;
class PurgeTracksTask;
class RelocateCollectedTracksTask;
class ReplaceCollectedTracksTask;
class ResolveCollectedTracksTask;
class SearchCollectedTracksTask;
class UpdateCollectionTask;

class Gateway : public QObject {
    Q_OBJECT

  public:
    Gateway(
            const QUrl& baseUrl,
            Settings settings,
            QObject* parent = nullptr);
    ~Gateway() override;

    const Settings& settings() const {
        return m_settings;
    }

    void invokeShutdown(
            int timeoutMillis = 0);

    ListCollectionsTask* listCollections(
            const QString& kind = QString(),
            const Pagination& pagination = Pagination());

    CreateCollectionTask* createCollection(
            json::Collection collection);
    UpdateCollectionTask* updateCollection(
            json::CollectionEntity collectionEntity);
    DeleteCollectionTask* deleteCollection(
            QString collectionUid);

    SearchCollectedTracksTask* searchTracks(
            const QString& collectionUid,
            const QJsonObject& baseQuery = QJsonObject(),
            const QStringList& searchTerms = QStringList(),
            const Pagination& pagination = Pagination());

    ResolveCollectedTracksTask* resolveTracksByUrl(
            const QString& collectionUid,
            QList<QUrl> trackUrls);

    ReplaceCollectedTracksTask* replaceTracks(
            const QString& collectionUid,
            const QList<json::Track>& tracks);
    RelocateCollectedTracksTask* relocateTracks(
            const QString& collectionUid,
            const QList<QPair<QString, QString>>& trackRelocations);
    RelocateCollectedTracksTask* relocateAllTracks(
            const QString& collectionUid,
            const QDir& oldRootDir,
            const QDir& newRootDir);
    PurgeTracksTask* purgeTracks(
            const QString& collectionUid,
            const QStringList& trackLocations);
    PurgeTracksTask* purgeAllTracks(
            const QString& collectionUid,
            const QDir& rootDir);

    ListCollectedPlaylistsTask* listCollectedPlaylists(
            const QString& collectionUid,
            const QString& kind);

    CreateCollectedPlaylistTask* createPlaylist(
            const QString& collectionUid,
            const json::Playlist& playlist);
    DeletePlaylistTask* deletePlaylist(
            QString playlistUid);
    PlaylistAppendTrackEntriesByUrlTask* playlistAppendTrackEntriesByUrl(
            QString collectionUid,
            json::EntityHeader playlistEntityHeader,
            QList<QUrl> trackUrls);

    ListTagsTask* listTags(
            const QString& collectionUid,
            const std::optional<QStringList>& facets = std::nullopt,
            const Pagination& pagination = Pagination());
    ListTagsFacetsTask* listTagsFacets(
            const QString& collectionUid,
            const std::optional<QStringList>& facets = std::nullopt,
            const Pagination& pagination = Pagination());

  public slots:
    void slotShutdown(
            int timeoutMillis);

  signals:
    void shuttingDown();

  private slots:
    void slotNetworkTaskAborted(
            const QUrl& requestUrl);
    void slotWebTaskNetworkError(
            QNetworkReply::NetworkError errorCode,
            const QString& errorString,
            const mixxx::network::WebResponseWithContent responseWithContent);
    void slotJsonWebTaskFailed(
            const mixxx::network::JsonWebResponse& response);
    void slotWriteTaskDestroyed();

  private:
    template<typename T, typename... Args>
    T* newReadingNetworkTask(Args&&... args) {
        auto* const pNetworkTask = new T(std::forward<Args>(args)...);
        pNetworkTask->moveToThread(thread());
        pNetworkTask->setParent(this);
        connect(pNetworkTask,
                &mixxx::network::NetworkTask::aborted,
                this,
                &Gateway::slotNetworkTaskAborted);
        return pNetworkTask;
    }
    template<typename T, typename... Args>
    T* newReadingWebTask(Args&&... args) {
        auto* const pWebTask = newReadingNetworkTask<T>(std::forward<Args>(args)...);
        connect(pWebTask,
                &mixxx::network::WebTask::networkError,
                this,
                &Gateway::slotWebTaskNetworkError);
        return pWebTask;
    }
    template<typename T, typename... Args>
    T* newReadingJsonWebTask(Args&&... args) {
        auto* const pJsonWebTask = newReadingWebTask<T>(std::forward<Args>(args)...);
        connect(pJsonWebTask,
                &mixxx::network::JsonWebTask::failed,
                this,
                &Gateway::slotJsonWebTaskFailed);
        return pJsonWebTask;
    }
    template<typename T, typename... Args>
    T* newWritingNetworkTask(Args&&... args) {
        auto* const pNetworkTask = newReadingNetworkTask<T>(std::forward<Args>(args)...);
        connectPendingWriteTask(pNetworkTask);
        return pNetworkTask;
    }
    template<typename T, typename... Args>
    T* newWritingJsonWebTask(Args&&... args) {
        auto* const pJsonWebTask = newReadingJsonWebTask<T>(std::forward<Args>(args)...);
        connectPendingWriteTask(pJsonWebTask);
        return pJsonWebTask;
    }

    void connectPendingWriteTask(
            mixxx::network::NetworkTask* task);

    const QUrl m_baseUrl;

    const Settings m_settings;

    const parented_ptr<QNetworkAccessManager> m_networkAccessManager;

    enum class State {
        Idle,
        ShutdownPending,
        ShuttingDown,
    };
    State m_state;

    int m_shutdownTimeoutMillis;

    int m_pendingWriteTasks;
};

} // namespace aoide

#pragma once

#include <QAtomicInteger>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStringList>
#include <QUrl>

#include "aoide/json/collection.h"
#include "aoide/json/playlist.h"
#include "aoide/json/track.h"
#include "aoide/tracksearchoverlayfilter.h"
#include "aoide/util.h"
#include "network/jsonwebtask.h"
#include "util/optional.h"
#include "util/parented_ptr.h"

class Track;

namespace aoide {

class ExportTrackFilesTask;
class ListCollectionsTask;
class ListPlaylistsTask;
class SearchTracksTask;

/// Manages network communication with an aoide service.
///
/// It ensures that all pending write tasks are finished before
/// shutting down the client.
///
/// All public methods are thread-safe and can be invoked from
/// any thread. The network tasks live in the same thread as
/// the gateway instance.
class Gateway : public QObject {
    Q_OBJECT

  public:
    explicit Gateway(
            const QUrl& baseUrl,
            QObject* parent = nullptr);
    ~Gateway() override;

    void invokeShutdown(
            int timeoutMillis = 0);

    ListCollectionsTask* listCollections(
            const QString& kind = {},
            const Pagination& pagination = {});

    SearchTracksTask* searchTracks(
            const QString& collectionUid,
            const QJsonObject& baseQuery = {},
            const TrackSearchOverlayFilter& overlayFilter = {},
            const QStringList& searchTerms = {},
            const Pagination& pagination = {});

    ExportTrackFilesTask* exportTrackFiles(
            const QString& collectionUid,
            const QJsonObject& trackFilter,
            const QString& targetRootPath);

    ListPlaylistsTask* listPlaylists(
            const QString& collectionUid,
            const QString& kind);

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

    const parented_ptr<QNetworkAccessManager> m_networkAccessManager;

    QAtomicInteger<quint32> m_pendingWriteTasks;

    enum class State {
        Active,
        ShutdownPending,
        ShuttingDown,
    };
    State m_state;

    int m_shutdownTimeoutMillis;
};

} // namespace aoide

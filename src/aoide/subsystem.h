#pragma once

#include <QPointer>
#include <QProcess>
#include <QThread>

#include "aoide/gateway.h"
#include "aoide/json/collection.h"
#include "aoide/json/playlist.h"
#include "aoide/json/track.h"
#include "aoide/settings.h"
#include "aoide/util.h"
#include "track/trackref.h"

namespace mixxx {

class TrackLoader;

}

namespace aoide {

class Subsystem : public QObject {
    Q_OBJECT

  public:
    Subsystem(
            UserSettingsPointer userSettings,
            mixxx::TrackLoader* trackLoader,
            QObject* parent = nullptr);
    ~Subsystem() override;

    void startUp();
    void invokeShutdown();

    const Settings& settings() const {
        return m_settings;
    }

    Gateway* gateway() const {
        return m_gateway.data();
    }

    bool isConnected() const {
        return gateway() != nullptr;
    }

    ///////////////////////////////////////////////////////////////////
    // Collections
    ///////////////////////////////////////////////////////////////////

    void invokeRefreshCollections();

    const QVector<json::CollectionEntity>& allCollections() const {
        return m_allCollections;
    }

    const std::optional<json::CollectionEntity>& activeCollection() const {
        return m_activeCollection;
    }
    void selectActiveCollection(
            const QString& collectionUid = {});

    enum CollectionsChangedFlags {
        ALL_COLLECTIONS = 0x01,
        ACTIVE_COLLECTION = 0x02,
    };

    ///////////////////////////////////////////////////////////////////
    // Track tasks
    ///////////////////////////////////////////////////////////////////

    SearchTracksTask* searchTracks(
            const QJsonObject& baseQuery,
            const TrackSearchOverlayFilter& overlayFilter,
            const QStringList& searchTerms,
            const Pagination& pagination);

    ExportTrackFilesTask* exportTrackFiles(
            const QJsonObject& trackFilter,
            const QString& targetRootPath);

    ///////////////////////////////////////////////////////////////////
    // Playlist tasks
    ///////////////////////////////////////////////////////////////////

    ListPlaylistsTask* listPlaylists(
            const QString& kind = {});

    ///////////////////////////////////////////////////////////////////

  signals:
    void connected();
    void disconnected();

    void collectionsChanged(int flags);

    void replacingTracksProgress(int queued, int pending, int succeeded, int failed);

  private slots:
    void onReadyReadStandardOutputFromProcess();
    void onReadyReadStandardErrorFromProcess();

    void slotGatewayShuttingDown();

    void slotListCollectionsSucceeded(
            QVector<json::CollectionEntity> result);

  private:
    void connectProcess(
            QString endpointAddr);

    void startThread();
    void stopThread();

    const Settings m_settings;

    const QPointer<mixxx::TrackLoader> m_trackLoader;

    QProcess m_process;

    QByteArray m_bufferedStandardErrorFromProcess;

    QVector<json::CollectionEntity> m_allCollections;

    std::optional<json::CollectionEntity> m_activeCollection;

    ///////////////////////////////////////////////////////////////////
    // SubsystemThread
    ///////////////////////////////////////////////////////////////////

    QThread m_thread;

    QPointer<Gateway> m_gateway;
};

} // namespace aoide
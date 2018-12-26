#pragma once

#include <QNetworkAccessManager>
#include <QThread>

#include "aoide/domain.h"
#include "aoide/settings.h"

#include "network/asyncrestclient.h"

#include "track/trackref.h"


namespace mixxx {

class AsyncTrackLoader;

namespace aoide {

class Gateway;
class TrackReplacementScheduler;

class Subsystem: public QObject {
    Q_OBJECT

  public:
    Subsystem(
            UserSettingsPointer userSettings,
            QPointer<AsyncTrackLoader> trackLoader,
            QObject* parent = nullptr);
    ~Subsystem() override = default;

    void startup(
            QThread::Priority threadPriority = QThread::LowPriority);
    void shutdown();

    const QVector<AoideCollectionEntity>& allCollections() const {
        return m_allCollections;
    }

    bool hasActiveCollection() const {
        return !m_activeCollection.header().uid().isEmpty();
    }
    const AoideCollectionEntity& activeCollection() const {
        return m_activeCollection;
    }
    void selectActiveCollection(const QString& collectionUid = QString());

    void refreshCollectionsAsync();
    void createCollectionAsync(
            AoideCollection collection);
    void updateCollectionAsync(
            AoideCollectionEntity collectionEntity);
    void deleteCollectionAsync(
            QString collectionUid);

    // Not thread-safe despite async -> use active collection!
    void searchTracksAsync(
            QString phraseQuery = QString(),
            AoidePagination pagination = AoidePagination());
    void replaceTracksAsync(
            QList<TrackRef> trackRefs);

    void cancelReplacingTracksAsync();

    enum CollectionsChangedFlags {
        ALL_COLLECTIONS = 0x01,
        ACTIVE_COLLECTION = 0x02,
    };

  signals:
    void collectionsChanged(int flags);

    void searchTracksResult(QVector<AoideTrackEntity> result);

    void replaceTracksProgress(int queued, int pending, int succeeded, int failed);

  private /*peer*/ slots:
    void /*Gateway*/ listCollectionsResultResponse(
            mixxx::AsyncRestClient::RequestId requestId,
            QVector<AoideCollectionEntity> result);
    void /*Gateway*/ createCollectionResultResponse(
            mixxx::AsyncRestClient::RequestId requestId,
            AoideEntityHeader result);
    void /*Gateway*/ updateCollectionResultResponse(
            mixxx::AsyncRestClient::RequestId requestId,
            AoideEntityHeader result);
    void /*Gateway*/ deleteCollectionResultResponse(
            mixxx::AsyncRestClient::RequestId requestId);

    void /*Gateway*/ searchTracksResultResponse(
            mixxx::AsyncRestClient::RequestId requestId,
            QVector<AoideTrackEntity> result);

  private:
    QThread m_thread;

    const Settings m_settings;

    const QPointer<QNetworkAccessManager> m_networkAccessManager;

    const QPointer<Gateway> m_gateway;

    const QPointer<TrackReplacementScheduler> m_trackReplacementScheduler;

    QVector<AoideCollectionEntity> m_allCollections;

    AoideCollectionEntity m_activeCollection;
};

} // namespace aoide

} // namespace mixxx

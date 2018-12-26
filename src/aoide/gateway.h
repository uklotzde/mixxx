#pragma once

#include <QSharedPointer>
#include <QStringList>

#include "aoide/domain.h"
#include "aoide/settings.h"

#include "network/asyncrestclient.h"


namespace mixxx {

namespace aoide {

class Gateway: public QObject {
    Q_OBJECT

  public:
    Gateway(
            Settings settings,
            QPointer<QNetworkAccessManager> networkAccessManager,
            QObject* parent = nullptr);
    ~Gateway() override = default;

    void connectPeers();

    typedef AsyncRestClient::RequestId RequestId;

    RequestId listCollectionsAsync(
            AoidePagination pagination = AoidePagination());
    RequestId createCollectionAsync(
            AoideCollection collection);
    RequestId updateCollectionAsync(
            AoideCollectionEntity collectionEntity);
    RequestId deleteCollectionAsync(
            QString collectionUid);

    RequestId searchTracksAsync(
            QString collectionUid,
            QString phraseQuery,
            AoidePagination pagination = AoidePagination());
    RequestId replaceTracksAsync(
            QString collectionUid,
            QList<AoideTrack> tracks);

    RequestId listTagsFacetsAsync(
            QString collectionUid,
            QSharedPointer<QStringList> facets = QSharedPointer<QStringList>(),
            AoidePagination pagination = AoidePagination());
    RequestId listTagsAsync(
            QString collectionUid,
            QSharedPointer<QStringList> facets = QSharedPointer<QStringList>(),
            AoidePagination pagination = AoidePagination());

  public slots:
    void listCollections(
            mixxx::AsyncRestClient::RequestId requestId,
            AoidePagination pagination);

    void createCollection(
            mixxx::AsyncRestClient::RequestId requestId,
            AoideCollection collection);

    void updateCollection(
            mixxx::AsyncRestClient::RequestId requestId,
            AoideCollectionEntity collectionEntity);

    void deleteCollection(
            mixxx::AsyncRestClient::RequestId requestId,
            QString collectionUid);

    void searchTracks(
            mixxx::AsyncRestClient::RequestId requestId,
            QString collectionUid,
            QString phraseQuery,
            AoidePagination pagination);
    void replaceTracks(
            mixxx::AsyncRestClient::RequestId requestId,
            QString collectionUid,
            QList<AoideTrack> tracks);

    void listTagsFacets(
            mixxx::AsyncRestClient::RequestId requestId,
            QString collectionUid,
            QSharedPointer<QStringList> facets,
            AoidePagination pagination);

    void listTags(
            mixxx::AsyncRestClient::RequestId requestId,
            QString collectionUid,
            QSharedPointer<QStringList> facets,
            AoidePagination pagination);

  signals:
    void networkRequestFailed(
            mixxx::AsyncRestClient::RequestId requestId,
            QString errorMessage);

    void listCollectionsResult(
            mixxx::AsyncRestClient::RequestId requestId,
            QVector<AoideCollectionEntity> result);

    void createCollectionResult(
            mixxx::AsyncRestClient::RequestId requestId,
            AoideEntityHeader result);

    void updateCollectionResult(
            mixxx::AsyncRestClient::RequestId requestId,
            AoideEntityHeader result);

    void deleteCollectionResult(
            mixxx::AsyncRestClient::RequestId requestId);

    void searchTracksResult(
            mixxx::AsyncRestClient::RequestId requestId,
            QVector<AoideTrackEntity> result);

    void replaceTracksResult(
            mixxx::AsyncRestClient::RequestId requestId,
            QJsonObject result);

    void listTagsFacetsResult(
            mixxx::AsyncRestClient::RequestId requestId,
            QVector<AoideScoredTagFacetCount> result);

    void listTagsResult(
            mixxx::AsyncRestClient::RequestId requestId,
            QVector<AoideScoredTagCount> result);

  private /*peer*/ slots:
    void listCollectionsNetworkReplyFinished();
    void createCollectionNetworkReplyFinished();
    void updateCollectionNetworkReplyFinished();
    void deleteCollectionNetworkReplyFinished();

    void searchTracksNetworkReplyFinished();
    void replaceTracksNetworkReplyFinished();

    void listTagsFacetsNetworkReplyFinished();
    void listTagsNetworkReplyFinished();

  private:
    const Settings m_settings;

    const QPointer<AsyncRestClient> m_restClient;
};

} // namespace aoide

} // namespace mixxx

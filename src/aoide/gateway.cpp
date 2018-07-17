#include "aoide/gateway.h"

#include <QMetaObject>
#include <QThread>

#include <mutex>

#include "aoide/customtransformers.h"
#include "aoide/transformers.h"

#include "util/logger.h"

namespace mixxx {

namespace {

std::once_flag registerMetaTypesOnceFlag;

void registerMetaTypes() {
    qRegisterMetaType<AoidePagination>();

    qRegisterMetaType<AoideEntityRevision>();
    qRegisterMetaType<AoideEntityHeader>();

    qRegisterMetaType<AoideCollection>();
    qRegisterMetaType<AoideCollectionEntity>();
    qRegisterMetaType<QVector<AoideCollectionEntity>>();

    qRegisterMetaType<AoideTitle>();
    qRegisterMetaType<AoideTitleVector>();
    qRegisterMetaType<AoideActor>();
    qRegisterMetaType<AoideActorVector>();
    qRegisterMetaType<AoideScoredTag>();
    qRegisterMetaType<AoideScoredTagVector>();
    qRegisterMetaType<AoideComment>();
    qRegisterMetaType<AoideCommentVector>();
    qRegisterMetaType<AoideRating>();
    qRegisterMetaType<AoideRatingVector>();

    qRegisterMetaType<AoideTrackOrAlbum>();
    qRegisterMetaType<AoideRelease>();
    qRegisterMetaType<AoideAlbum>();
    qRegisterMetaType<AoideTrack>();
    qRegisterMetaType<AoideTrackEntity>();
    qRegisterMetaType<QVector<AoideTrackEntity>>();

    qRegisterMetaType<AoideScoredTagFacetCount>();
    qRegisterMetaType<QVector<AoideScoredTagFacetCount>>();
    qRegisterMetaType<AoideScoredTagCount>();
    qRegisterMetaType<QVector<AoideScoredTagCount>>();
}

const Logger kLogger("aoide Gateway");

const QString kReplaceMode = "update-or-create";

} // anonymous namespace

namespace aoide {

Gateway::Gateway(
        Settings settings, QPointer<QNetworkAccessManager> networkAccessManager, QObject* parent)
    : QObject(parent),
      m_settings(std::move(settings)),
      m_restClient(new AsyncRestClient(std::move(networkAccessManager), this)) {
    std::call_once(registerMetaTypesOnceFlag, registerMetaTypes);
}

void Gateway::connectPeers() {
    connect(m_restClient, &AsyncRestClient::networkRequestFailed, this,
            &Gateway::networkRequestFailed); // signal/signal pass-through
}

Gateway::RequestId Gateway::listCollectionsAsync(AoidePagination pagination) {
    AsyncRestClient::RequestId requestId = m_restClient->nextRequestId();
    QMetaObject::invokeMethod(
            this, "listCollections",
            Qt::QueuedConnection, // async,
            Q_ARG(mixxx::AsyncRestClient::RequestId, std::move(requestId)),
            Q_ARG(AoidePagination, std::move(pagination)));
    return requestId;
}

void Gateway::listCollections(AsyncRestClient::RequestId requestId, AoidePagination pagination) {
    DEBUG_ASSERT(thread() == QThread::currentThread());

    const QPointer<QNetworkAccessManager> networkAccessManager =
            m_restClient->accessNetwork(requestId);
    if (!networkAccessManager) {
        return;
    }

    QUrl url = m_settings.url();
    url.setPath("/collections");
    QUrlQuery query;
    pagination.addToQuery(&query);
    url.setQuery(query);

    QNetworkRequest request(url);

    QNetworkReply* reply = networkAccessManager->get(request);
    m_restClient->afterNetworkRequestSent(requestId, reply);

    connect(reply, &QNetworkReply::finished, this, &Gateway::listCollectionsNetworkReplyFinished);
}

void Gateway::listCollectionsNetworkReplyFinished() {
    auto reply = static_cast<QNetworkReply*>(sender());

    QJsonDocument jsonContent;
    const std::pair<RequestId, int> response =
            m_restClient->receiveNetworkReply(reply, &jsonContent);
    const RequestId requestId = response.first;
    if (!AsyncRestClient::isValidRequestId(requestId)) {
        return;
    }
    const int statusCode = response.second;
    VERIFY_OR_DEBUG_ASSERT(AsyncRestClient::isStatusCodeSuccess(statusCode)) {
        kLogger.warning() << "Network request" << requestId << "-" << statusCode
                          << "Failed to list collections";
        return;
    }
    DEBUG_ASSERT(statusCode == HttpStatusCodes::Ok);

    QVector<AoideCollectionEntity> result;
    VERIFY_OR_DEBUG_ASSERT(jsonContent.isArray()) {
        kLogger.warning() << "Invalid JSON content" << jsonContent;
    }
    else {
        const auto jsonArray = jsonContent.array();
        result.reserve(jsonArray.size());
        for (const auto& jsonValue : jsonArray) {
            DEBUG_ASSERT(jsonValue.isObject());
            result.append(AoideCollectionEntity(jsonValue.toObject()));
        }
    }

    emit listCollectionsResult(requestId, std::move(result));
}

Gateway::RequestId Gateway::createCollectionAsync(AoideCollection collection) {
    AsyncRestClient::RequestId requestId = m_restClient->nextRequestId();
    QMetaObject::invokeMethod(
            this, "createCollection",
            Qt::QueuedConnection, // async,
            Q_ARG(mixxx::AsyncRestClient::RequestId, std::move(requestId)),
            Q_ARG(AoideCollection, std::move(collection)));
    return requestId;
}

void Gateway::createCollection(AsyncRestClient::RequestId requestId, AoideCollection collection) {
    DEBUG_ASSERT(thread() == QThread::currentThread());

    const QPointer<QNetworkAccessManager> networkAccessManager =
            m_restClient->accessNetwork(requestId);
    if (!networkAccessManager) {
        return;
    }

    QUrl url = m_settings.url();
    url.setPath("/collections");

    QNetworkRequest jsonRequest = m_restClient->newJsonRequest(url);

    QByteArray jsonContent = QJsonDocument(collection).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = networkAccessManager->post(jsonRequest, jsonContent);
    m_restClient->afterNetworkRequestSent(requestId, reply);

    connect(reply, &QNetworkReply::finished, this, &Gateway::createCollectionNetworkReplyFinished);
}

void Gateway::createCollectionNetworkReplyFinished() {
    auto reply = static_cast<QNetworkReply*>(sender());

    QJsonDocument jsonContent;
    const std::pair<RequestId, int> response =
            m_restClient->receiveNetworkReply(reply, &jsonContent);
    const RequestId requestId = response.first;
    if (!AsyncRestClient::isValidRequestId(requestId)) {
        return;
    }
    const int statusCode = response.second;
    VERIFY_OR_DEBUG_ASSERT(AsyncRestClient::isStatusCodeSuccess(statusCode)) {
        kLogger.warning() << "Network request" << requestId << "-" << statusCode
                          << "Failed to create collection";
    }
    DEBUG_ASSERT(statusCode == HttpStatusCodes::Created);

    AoideEntityHeader result;
    VERIFY_OR_DEBUG_ASSERT(jsonContent.isObject()) {
        kLogger.warning() << "Invalid JSON content" << jsonContent;
    }
    else {
        result = AoideEntityHeader(jsonContent.object());
    }

    emit createCollectionResult(requestId, std::move(result));
}

Gateway::RequestId Gateway::updateCollectionAsync(AoideCollectionEntity collectionEntity) {
    AsyncRestClient::RequestId requestId = m_restClient->nextRequestId();
    QMetaObject::invokeMethod(
            this, "updateCollection",
            Qt::QueuedConnection, // async,
            Q_ARG(mixxx::AsyncRestClient::RequestId, std::move(requestId)),
            Q_ARG(AoideCollectionEntity, std::move(collectionEntity)));
    return requestId;
}

void Gateway::updateCollection(
        AsyncRestClient::RequestId requestId, AoideCollectionEntity collectionEntity) {
    DEBUG_ASSERT(thread() == QThread::currentThread());

    const QPointer<QNetworkAccessManager> networkAccessManager =
            m_restClient->accessNetwork(requestId);
    if (!networkAccessManager) {
        return;
    }

    QUrl url = m_settings.url();
    url.setPath("/collections/" + collectionEntity.header().uid());

    QNetworkRequest jsonRequest = m_restClient->newJsonRequest(url);

    QByteArray jsonContent = QJsonDocument(collectionEntity).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = networkAccessManager->put(jsonRequest, jsonContent);
    m_restClient->afterNetworkRequestSent(requestId, reply);

    connect(reply, &QNetworkReply::finished, this, &Gateway::updateCollectionNetworkReplyFinished);
}

void Gateway::updateCollectionNetworkReplyFinished() {
    auto reply = static_cast<QNetworkReply*>(sender());

    QJsonDocument jsonContent;
    const std::pair<RequestId, int> response =
            m_restClient->receiveNetworkReply(reply, &jsonContent);
    const RequestId requestId = response.first;
    if (!AsyncRestClient::isValidRequestId(requestId)) {
        return;
    }
    const int statusCode = response.second;
    VERIFY_OR_DEBUG_ASSERT(AsyncRestClient::isStatusCodeSuccess(statusCode)) {
        kLogger.warning() << "Network request" << requestId << "-" << statusCode
                          << "Failed to update collection";
    }
    DEBUG_ASSERT(statusCode == HttpStatusCodes::Ok);

    AoideEntityHeader result;
    VERIFY_OR_DEBUG_ASSERT(jsonContent.isObject()) {
        kLogger.warning() << "Invalid JSON content" << jsonContent;
    }
    else {
        result = AoideEntityHeader(jsonContent.object());
    }

    emit updateCollectionResult(requestId, std::move(result));
}

Gateway::RequestId Gateway::deleteCollectionAsync(QString collectionUid) {
    AsyncRestClient::RequestId requestId = m_restClient->nextRequestId();
    QMetaObject::invokeMethod(
            this, "deleteCollection",
            Qt::QueuedConnection, // async,
            Q_ARG(mixxx::AsyncRestClient::RequestId, std::move(requestId)),
            Q_ARG(QString, std::move(collectionUid)));
    return requestId;
}

void Gateway::deleteCollection(AsyncRestClient::RequestId requestId, QString collectionUid) {
    DEBUG_ASSERT(thread() == QThread::currentThread());

    const QPointer<QNetworkAccessManager> networkAccessManager =
            m_restClient->accessNetwork(requestId);
    if (!networkAccessManager) {
        return;
    }

    QUrl url = m_settings.url();
    url.setPath("/collections/" + collectionUid);

    QNetworkRequest request(url);

    QNetworkReply* reply = networkAccessManager->deleteResource(request);
    m_restClient->afterNetworkRequestSent(requestId, reply);

    connect(reply, &QNetworkReply::finished, this, &Gateway::deleteCollectionNetworkReplyFinished);
}

void Gateway::deleteCollectionNetworkReplyFinished() {
    auto reply = static_cast<QNetworkReply*>(sender());

    const std::pair<RequestId, int> response = m_restClient->receiveNetworkReply(reply);
    const RequestId requestId = response.first;
    if (!AsyncRestClient::isValidRequestId(requestId)) {
        return;
    }
    const int statusCode = response.second;
    VERIFY_OR_DEBUG_ASSERT(AsyncRestClient::isStatusCodeSuccess(statusCode)) {
        kLogger.warning() << "Network request" << requestId << "-" << statusCode
                          << "Failed to delete collection";
    }
    DEBUG_ASSERT(statusCode == HttpStatusCodes::NoContent);

    emit deleteCollectionResult(requestId);
}

Gateway::RequestId Gateway::searchTracksAsync(
        QString collectionUid, QString phraseQuery, AoidePagination pagination) {
    AsyncRestClient::RequestId requestId = m_restClient->nextRequestId();
    QMetaObject::invokeMethod(
            this, "searchTracks",
            Qt::QueuedConnection, // async,
            Q_ARG(mixxx::AsyncRestClient::RequestId, std::move(requestId)),
            Q_ARG(QString, std::move(collectionUid)), Q_ARG(QString, std::move(phraseQuery)),
            Q_ARG(AoidePagination, std::move(pagination)));
    return requestId;
}

void Gateway::searchTracks(
        AsyncRestClient::RequestId requestId, QString collectionUid, QString phraseQuery,
        AoidePagination pagination) {
    DEBUG_ASSERT(thread() == QThread::currentThread());

    const QPointer<QNetworkAccessManager> networkAccessManager =
            m_restClient->accessNetwork(requestId);
    if (!networkAccessManager) {
        return;
    }

    QUrl url = m_settings.url();
    url.setPath("/tracks/search");

    QUrlQuery query;
    query.addQueryItem("collectionUid", collectionUid);
    pagination.addToQuery(&query);
    url.setQuery(query);

    QNetworkRequest jsonRequest = m_restClient->newJsonRequest(url);

    QByteArray jsonContent = QJsonDocument(QJsonObject{{
                                                   "phraseFilter",
                                                   QJsonObject{{"query", phraseQuery}},
                                           }})
                                     .toJson(QJsonDocument::Compact);

    QNetworkReply* reply = networkAccessManager->post(jsonRequest, jsonContent);
    m_restClient->afterNetworkRequestSent(requestId, reply);

    connect(reply, &QNetworkReply::finished, this, &Gateway::searchTracksNetworkReplyFinished);
}

void Gateway::searchTracksNetworkReplyFinished() {
    auto reply = static_cast<QNetworkReply*>(sender());

    QJsonDocument jsonContent;
    const std::pair<RequestId, int> response =
            m_restClient->receiveNetworkReply(reply, &jsonContent);
    const RequestId requestId = response.first;
    if (!AsyncRestClient::isValidRequestId(requestId)) {
        return;
    }
    const int statusCode = response.second;
    VERIFY_OR_DEBUG_ASSERT(AsyncRestClient::isStatusCodeSuccess(statusCode)) {
        kLogger.warning() << "Network request" << requestId << "-" << statusCode
                          << "Failed to search tracks";
        return;
    }
    DEBUG_ASSERT(statusCode == HttpStatusCodes::Ok);

    QVector<AoideTrackEntity> result;
    VERIFY_OR_DEBUG_ASSERT(jsonContent.isArray()) {
        kLogger.warning() << "Invalid JSON content" << jsonContent;
    }
    else {
        const auto jsonArray = jsonContent.array();
        result.reserve(jsonArray.size());
        for (const auto& jsonValue : jsonArray) {
            DEBUG_ASSERT(jsonValue.isObject());
            AoideTrackEntity trackEntity(jsonValue.toObject());
            AoideTrack track = trackEntity.body();
            track = MultiGenreTagger(m_settings).importTrack(std::move(track));
            // TODO: Add more transformers here
            trackEntity.setBody(std::move(track));
            result.append(std::move(trackEntity));
        }
    }

    emit searchTracksResult(requestId, std::move(result));
}

Gateway::RequestId Gateway::replaceTracksAsync(QString collectionUid, QList<AoideTrack> tracks) {
    AsyncRestClient::RequestId requestId = m_restClient->nextRequestId();
    QMetaObject::invokeMethod(
            this, "replaceTracks",
            Qt::QueuedConnection, // async,
            Q_ARG(mixxx::AsyncRestClient::RequestId, std::move(requestId)),
            Q_ARG(QString, std::move(collectionUid)), Q_ARG(QList<AoideTrack>, std::move(tracks)));
    return requestId;
}

void Gateway::replaceTracks(
        AsyncRestClient::RequestId requestId, QString collectionUid, QList<AoideTrack> tracks) {
    DEBUG_ASSERT(thread() == QThread::currentThread());

    QJsonArray jsonReplacements;
    for (auto track : qAsConst(tracks)) {
        track = MultiGenreTagger(m_settings).exportTrack(std::move(track));
        // TODO: Add more transformers here

        jsonReplacements += QJsonObject{
                {"uri", track.contentUri()},
                {"track", track.intoJsonObject()},
        };
    }
    QByteArray jsonContent = QJsonDocument(QJsonObject{
                                                   {"mode", kReplaceMode},
                                                   {"replacements", jsonReplacements},
                                           })
                                     .toJson(QJsonDocument::Compact);
    //qDebug() << jsonContent.toStdString().c_str();

    const QPointer<QNetworkAccessManager> networkAccessManager =
            m_restClient->accessNetwork(requestId);
    if (!networkAccessManager) {
        return;
    }

    QUrl url = m_settings.url();
    url.setPath("/tracks/replace");

    QUrlQuery query;
    query.addQueryItem("collectionUid", collectionUid);
    url.setQuery(query);

    QNetworkRequest jsonRequest = m_restClient->newJsonRequest(url);

    QNetworkReply* reply = networkAccessManager->post(jsonRequest, jsonContent);
    m_restClient->afterNetworkRequestSent(requestId, reply);

    connect(reply, &QNetworkReply::finished, this, &Gateway::replaceTracksNetworkReplyFinished);
}

void Gateway::replaceTracksNetworkReplyFinished() {
    auto reply = static_cast<QNetworkReply*>(sender());

    QJsonDocument jsonContent;
    const std::pair<RequestId, int> response =
            m_restClient->receiveNetworkReply(reply, &jsonContent);
    const RequestId requestId = response.first;
    if (!AsyncRestClient::isValidRequestId(requestId)) {
        return;
    }
    const int statusCode = response.second;
    VERIFY_OR_DEBUG_ASSERT(AsyncRestClient::isStatusCodeSuccess(statusCode)) {
        kLogger.warning() << "Network request" << requestId << "-" << statusCode
                          << "Failed to replace tracks";
    }
    DEBUG_ASSERT(statusCode == HttpStatusCodes::Ok);
    VERIFY_OR_DEBUG_ASSERT(AsyncRestClient::isStatusCodeSuccess(statusCode)) {
        kLogger.warning() << "Network request" << requestId << "-" << statusCode
                          << "Failed to replace tracks";
    }

    QJsonObject result;
    VERIFY_OR_DEBUG_ASSERT(jsonContent.isObject()) {
        kLogger.warning() << "Invalid JSON content" << jsonContent;
    }
    else {
        result = jsonContent.object();
    }

    emit replaceTracksResult(requestId, result);
}

Gateway::RequestId Gateway::listTagsFacetsAsync(
        QString collectionUid, QSharedPointer<QStringList> facets, AoidePagination pagination) {
    AsyncRestClient::RequestId requestId = m_restClient->nextRequestId();
    QMetaObject::invokeMethod(
            this, "listTagsFacets",
            Qt::QueuedConnection, // async,
            Q_ARG(mixxx::AsyncRestClient::RequestId, std::move(requestId)),
            Q_ARG(QString, std::move(collectionUid)),
            Q_ARG(QSharedPointer<QStringList>, std::move(facets)),
            Q_ARG(AoidePagination, std::move(pagination)));
    return requestId;
}

void Gateway::listTagsFacets(
        AsyncRestClient::RequestId requestId, QString collectionUid,
        QSharedPointer<QStringList> facets, AoidePagination pagination) {
    DEBUG_ASSERT(thread() == QThread::currentThread());

    const QPointer<QNetworkAccessManager> networkAccessManager =
            m_restClient->accessNetwork(requestId);
    if (!networkAccessManager) {
        return;
    }

    QUrl url = m_settings.url();
    url.setPath("/tags/facets");
    QUrlQuery query;
    query.addQueryItem("collectionUid", collectionUid);
    if (facets) {
        query.addQueryItem("facet", facets->join(','));
    }
    pagination.addToQuery(&query);
    url.setQuery(query);

    QNetworkRequest request(url);

    QNetworkReply* reply = networkAccessManager->get(request);
    m_restClient->afterNetworkRequestSent(requestId, reply);

    connect(reply, &QNetworkReply::finished, this, &Gateway::listTagsFacetsNetworkReplyFinished);
}

void Gateway::listTagsFacetsNetworkReplyFinished() {
    auto reply = static_cast<QNetworkReply*>(sender());

    QJsonDocument jsonContent;
    const std::pair<RequestId, int> response =
            m_restClient->receiveNetworkReply(reply, &jsonContent);
    const RequestId requestId = response.first;
    if (!AsyncRestClient::isValidRequestId(requestId)) {
        return;
    }
    const int statusCode = response.second;
    VERIFY_OR_DEBUG_ASSERT(AsyncRestClient::isStatusCodeSuccess(statusCode)) {
        kLogger.warning() << "Network request" << requestId << "-" << statusCode
                          << "Failed to list facets of tags";
    }
    DEBUG_ASSERT(statusCode == HttpStatusCodes::Ok);

    QVector<AoideScoredTagFacetCount> result;
    VERIFY_OR_DEBUG_ASSERT(jsonContent.isArray()) {
        kLogger.warning() << "Invalid JSON content" << jsonContent;
    }
    else {
        const auto jsonArray = jsonContent.array();
        result.reserve(jsonArray.size());
        for (const auto& jsonValue : jsonArray) {
            DEBUG_ASSERT(jsonValue.isObject());
            result.append(AoideScoredTagFacetCount(jsonValue.toObject()));
        }
    }

    emit listTagsFacetsResult(requestId, std::move(result));
}

Gateway::RequestId Gateway::listTagsAsync(
        QString collectionUid, QSharedPointer<QStringList> facets, AoidePagination pagination) {
    AsyncRestClient::RequestId requestId = m_restClient->nextRequestId();
    QMetaObject::invokeMethod(
            this, "listTags",
            Qt::QueuedConnection, // async,
            Q_ARG(mixxx::AsyncRestClient::RequestId, std::move(requestId)),
            Q_ARG(QString, std::move(collectionUid)),
            Q_ARG(QSharedPointer<QStringList>, std::move(facets)),
            Q_ARG(AoidePagination, std::move(pagination)));
    return requestId;
}

void Gateway::listTags(
        AsyncRestClient::RequestId requestId, QString collectionUid,
        QSharedPointer<QStringList> facets, AoidePagination pagination) {
    DEBUG_ASSERT(thread() == QThread::currentThread());

    const QPointer<QNetworkAccessManager> networkAccessManager =
            m_restClient->accessNetwork(requestId);
    if (!networkAccessManager) {
        return;
    }

    QUrl url = m_settings.url();
    url.setPath("/tags");
    QUrlQuery query;
    query.addQueryItem("collectionUid", collectionUid);
    if (facets) {
        query.addQueryItem("facet", facets->join(','));
    }
    pagination.addToQuery(&query);
    url.setQuery(query);

    QNetworkRequest request(url);

    QNetworkReply* reply = networkAccessManager->get(request);
    m_restClient->afterNetworkRequestSent(requestId, reply);

    connect(reply, &QNetworkReply::finished, this, &Gateway::listTagsNetworkReplyFinished);
}

void Gateway::listTagsNetworkReplyFinished() {
    auto reply = static_cast<QNetworkReply*>(sender());

    QJsonDocument jsonContent;
    const std::pair<RequestId, int> response =
            m_restClient->receiveNetworkReply(reply, &jsonContent);
    const RequestId requestId = response.first;
    if (!AsyncRestClient::isValidRequestId(requestId)) {
        return;
    }
    const int statusCode = response.second;
    VERIFY_OR_DEBUG_ASSERT(AsyncRestClient::isStatusCodeSuccess(statusCode)) {
        kLogger.warning() << "Network request" << requestId << "-" << statusCode
                          << "Failed to list tags";
    }
    DEBUG_ASSERT(statusCode == HttpStatusCodes::Ok);

    QVector<AoideScoredTagCount> result;
    VERIFY_OR_DEBUG_ASSERT(jsonContent.isArray()) {
        kLogger.warning() << "Invalid JSON content" << jsonContent;
    }
    else {
        const auto jsonArray = jsonContent.array();
        result.reserve(jsonArray.size());
        for (const auto& jsonValue : jsonArray) {
            DEBUG_ASSERT(jsonValue.isObject());
            result.append(AoideScoredTagCount(jsonValue.toObject()));
        }
    }

    emit listTagsResult(requestId, std::move(result));
}

} // namespace aoide

} // namespace mixxx

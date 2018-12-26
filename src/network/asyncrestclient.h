#pragma once

#include <QAtomicInteger>
#include <QJsonDocument>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPointer>

#include "network/httpstatuscode.h"

namespace mixxx {

class AsyncRestClient: public QObject {
    Q_OBJECT

  public:
    AsyncRestClient(
            QPointer<QNetworkAccessManager> networkAccessManager,
            QObject* parent = nullptr);
    ~AsyncRestClient() override = default;

    typedef quint32 RequestId;

    static bool isValidRequestId(
            RequestId requestId);

    RequestId nextRequestId();

    QPointer<QNetworkAccessManager> accessNetwork(
            RequestId requestId);

    static QNetworkRequest newJsonRequest(
            QUrl url);

    static int isStatusCodeSuccess(
            int statusCode) {
        return statusCode >= 200 && statusCode < 300;
    }

    void afterNetworkRequestSent(
            RequestId requestId,
            QNetworkReply* reply);
    std::pair<RequestId, int> receiveNetworkReply(
            QNetworkReply* reply,
            QJsonDocument* jsonResponse = nullptr);

  signals:
    void networkRequestFailed(
            RequestId requestId,
            QString errorMessage);

  private:
    const QPointer<QNetworkAccessManager> m_networkAccessManager;

    QAtomicInteger<RequestId> m_requestId;

    QMap<RequestId, QNetworkReply*> m_pendingRequests;
    QMap<QNetworkReply*, RequestId> m_pendingReplies;
};

} // namespace mixxx

Q_DECLARE_METATYPE(mixxx::AsyncRestClient::RequestId);

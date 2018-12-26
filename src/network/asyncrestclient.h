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

    class RequestId {
      public:
        typedef quint32 value_t;
        explicit RequestId(value_t value = 0): m_value(value) {}
        operator value_t() const { return m_value; }
        bool isValid() const { return m_value != 0; }
        void reset() { m_value = 0; }
      private:
        value_t m_value;
    };

    static RequestId nextRequestId();

    static QNetworkRequest newJsonRequest(
            QUrl url);

    static int isStatusCodeSuccess(
            int statusCode) {
        return statusCode >= 200 && statusCode < 300;
    }

    bool isRequestPending(RequestId requestId) const {
        return m_pendingRequests.contains(requestId);
    }

    QPointer<QNetworkAccessManager> accessNetwork(
            RequestId requestId);

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
    static QAtomicInteger<RequestId::value_t> s_requestId;

    const QPointer<QNetworkAccessManager> m_networkAccessManager;

    QMap<RequestId, QNetworkReply*> m_pendingRequests;
    QMap<QNetworkReply*, RequestId> m_pendingReplies;
};

} // namespace mixxx

Q_DECLARE_METATYPE(mixxx::AsyncRestClient::RequestId);

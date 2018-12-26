#include "network/asyncrestclient.h"

#include <QMimeDatabase>
#include <QThread>

#include <mutex>

#include "util/assert.h"
#include "util/logger.h"


namespace {

std::once_flag registerMetaTypesOnceFlag;

void registerMetaTypes() {
    qRegisterMetaType<mixxx::AsyncRestClient::RequestId>("mixxx::AsyncRestClient::RequestId");
}

} // anonymous namespace

namespace mixxx {

namespace {

const Logger kLogger("AsyncRestClient");

const AsyncRestClient::RequestId INVALID_REQUEST_ID = 0;

const QString JSON_CONTENT_TYPE = "application/json";

const QMimeType JSON_MIME_TYPE = QMimeDatabase().mimeTypeForName(JSON_CONTENT_TYPE);

const QString TEXT_CONTENT_TYPE = "text/plain";

const QMimeType TEXT_MIME_TYPE = QMimeDatabase().mimeTypeForName(TEXT_CONTENT_TYPE);

bool readStatusCode(
        AsyncRestClient::RequestId requestId,
        const QNetworkReply* reply,
        int* statusCode) {
    DEBUG_ASSERT(statusCode);
    const QVariant statusCodeAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    bool statusCodeValid = false;
    const int statusCodeValue = statusCodeAttr.toInt(&statusCodeValid);
    VERIFY_OR_DEBUG_ASSERT(statusCodeValid && HttpStatusCode_isValid(statusCodeValue)) {
        kLogger.warning()
                << "Invalid or missing status code attribute"
                << statusCodeAttr
                << "in reply for network request"
                << requestId;
    } else {
        *statusCode = statusCodeValue;
    }
    return statusCodeValid;
}

QMimeType readContentType(
        AsyncRestClient::RequestId requestId,
        const QNetworkReply* reply) {
    DEBUG_ASSERT(reply);
    const QVariant contentTypeHeader = reply->header(QNetworkRequest::ContentTypeHeader);
    const QString contentTypeString = contentTypeHeader.toString();
    const QString contentTypeWithoutParams = contentTypeString.left(contentTypeString.indexOf(';'));
    const QMimeType contentType = QMimeDatabase().mimeTypeForName(contentTypeWithoutParams);
    if (!contentType.isValid()) {
        kLogger.warning()
                << "Unknown content type"
                << contentTypeHeader
                << "in reply for network request"
                << requestId;
    }
    return contentType;
}

bool readTextContent(
        AsyncRestClient::RequestId requestId,
        QNetworkReply* reply,
        QString* textContent) {
    DEBUG_ASSERT(reply);
    DEBUG_ASSERT(textContent);
    DEBUG_ASSERT(TEXT_MIME_TYPE.isValid());
    if (readContentType(requestId, reply) == TEXT_MIME_TYPE) {
        // TODO: Verify that charset=utf-8?
        *textContent = QString::fromUtf8(reply->readAll());
        return true;
    } else {
        return false;
    }
}

bool readJsonContent(
        AsyncRestClient::RequestId requestId,
        QNetworkReply* reply,
        QJsonDocument* jsonContent) {
    DEBUG_ASSERT(reply);
    DEBUG_ASSERT(jsonContent);
    DEBUG_ASSERT(JSON_MIME_TYPE.isValid());
    const auto contentType = readContentType(requestId, reply);
    if (contentType == JSON_MIME_TYPE) {
        *jsonContent = QJsonDocument::fromJson(reply->readAll());
        return true;
    } else {
        return false;
    }
}

} // anonymous namespace

//static
bool AsyncRestClient::isValidRequestId(RequestId requestId) {
    return requestId != INVALID_REQUEST_ID;
}

AsyncRestClient::AsyncRestClient(
        QPointer<QNetworkAccessManager> networkAccessManager,
        QObject* parent)
    : QObject(parent),
      m_networkAccessManager(std::move(networkAccessManager)),
      m_requestId(INVALID_REQUEST_ID) {
    DEBUG_ASSERT(!isValidRequestId(m_requestId));
    std::call_once(registerMetaTypesOnceFlag, registerMetaTypes);
}

AsyncRestClient::RequestId AsyncRestClient::nextRequestId() {
    RequestId requestId;
    do {
        requestId = ++m_requestId;
    } while (!isValidRequestId(requestId));
    return requestId;
}

QPointer<QNetworkAccessManager> AsyncRestClient::accessNetwork(
        RequestId requestId) {
    DEBUG_ASSERT(thread() == QThread::currentThread());
    if (m_networkAccessManager) {
        VERIFY_OR_DEBUG_ASSERT(thread() == m_networkAccessManager->thread()) {
            kLogger.critical()
                    << "Network access from different threads is not supported:"
                    << thread()
                    << "<>"
                    << m_networkAccessManager->thread();
            emit networkRequestFailed(requestId, "Invalid thread");
            return QPointer<QNetworkAccessManager>();
        }
        if (m_networkAccessManager->networkAccessible() == QNetworkAccessManager::NotAccessible) {
            QLatin1String errorMessage("Network not accessible");
            kLogger.warning() << errorMessage;
            emit networkRequestFailed(requestId, errorMessage);
            return QPointer<QNetworkAccessManager>();
        }
    } else {
        QLatin1String errorMessage("No network access");
        kLogger.warning() << errorMessage;
        emit networkRequestFailed(requestId, errorMessage);
    }
    return m_networkAccessManager;
}

QNetworkRequest AsyncRestClient::newJsonRequest(QUrl url) {
    QNetworkRequest request(std::move(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, JSON_CONTENT_TYPE);
    return request;
}

void AsyncRestClient::afterNetworkRequestSent(
        RequestId requestId,
        QNetworkReply* reply) {
    DEBUG_ASSERT(isValidRequestId(requestId));
    DEBUG_ASSERT(reply);
    DEBUG_ASSERT(!m_pendingRequests.values().contains(reply));
    DEBUG_ASSERT(!m_pendingReplies.contains(reply));
    DEBUG_ASSERT(m_pendingRequests.size() == m_pendingReplies.size());

    VERIFY_OR_DEBUG_ASSERT(!m_pendingRequests.contains(requestId)) {
        kLogger.critical()
                << "Duplicate request identifier"
                << requestId
                << "for pending network reply detected"
                << "- discarding pending request/reply pair";
        // Remove conflicting entries
        QNetworkReply* pendingReply = m_pendingRequests.take(requestId);
        DEBUG_ASSERT(pendingReply);
        DEBUG_ASSERT(m_pendingReplies.value(pendingReply) == requestId);
        m_pendingReplies.remove(pendingReply);
    }
    m_pendingRequests.insert(requestId, reply);
    m_pendingReplies.insert(reply, requestId);

    if (kLogger.debugEnabled()) {
        kLogger.debug()
                << "Sent network request"
                << requestId
                << reply->request().url();
    }
}

std::pair<AsyncRestClient::RequestId, int> AsyncRestClient::receiveNetworkReply(
        QNetworkReply* reply,
        QJsonDocument* jsonResponse) {
    DEBUG_ASSERT(reply);
    reply->deleteLater();

    DEBUG_ASSERT(m_pendingRequests.size() == m_pendingReplies.size());
    RequestId requestId = m_pendingReplies.value(reply, INVALID_REQUEST_ID);
    VERIFY_OR_DEBUG_ASSERT(isValidRequestId(requestId) &&
            (m_pendingRequests.value(requestId, nullptr) == reply)) {
        kLogger.critical()
                << "Missing or mismatching request identifier"
                << requestId
                << "for received network reply detected";
        return std::make_pair(INVALID_REQUEST_ID, HttpStatusCodes::Invalid);
    }
    m_pendingRequests.remove(requestId);
    m_pendingReplies.remove(reply);

    if (reply->error() != QNetworkReply::NetworkError::NoError) {
        QString errorMessage = reply->errorString();
        QString textContent;
        if (readTextContent(requestId, reply, &textContent) && !textContent.isEmpty()) {
            errorMessage += " -- ";
            errorMessage += textContent;
        }
        DEBUG_ASSERT(!errorMessage.isEmpty());
        kLogger.warning()
                << "Network request"
                << requestId
                << "failed:"
                << errorMessage;
        emit networkRequestFailed(requestId, std::move(errorMessage));
        return std::make_pair(INVALID_REQUEST_ID, HttpStatusCodes::Invalid);
    }

    if (kLogger.debugEnabled()) {
        if (reply->url() == reply->request().url()) {
            kLogger.debug()
                    << "Received network reply"
                    << requestId
                    << reply->url();
        } else {
            kLogger.debug()
                    << "Received network reply"
                    << requestId
                    << reply->request().url()
                    << "->"
                    << reply->url();
        }
    }

    int statusCode = HttpStatusCodes::Invalid;
    readStatusCode(requestId, reply, &statusCode);

    if (jsonResponse && !readJsonContent(requestId, reply, jsonResponse)) {
        kLogger.warning()
                << "Missing or invalid JSON response in reply for network request"
                << requestId;
    }

    return std::make_pair(requestId, statusCode);
}

} // namespace mixxx

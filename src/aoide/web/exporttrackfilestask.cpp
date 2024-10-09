#include "aoide/web/exporttrackfilestask.h"

#include <QMetaMethod>

#include "util/logger.h"

namespace {

const mixxx::Logger kLogger("aoide ExportTrackFilesTask");

} // anonymous namespace

namespace aoide {

namespace {

mixxx::network::JsonWebRequest buildRequest(
        const QString& collectionUid,
        const QJsonObject& trackFilter,
        const QString& targetRootPath) {
    const auto requestBody = QJsonObject{
            {
                    QLatin1String("filter"),
                    trackFilter,
            },
            {
                    QLatin1String("targetRootPath"),
                    targetRootPath,
            },
            {
                    QLatin1String("purgeOtherFiles"),
                    true,
            },
    };

    return mixxx::network::JsonWebRequest{
            mixxx::network::HttpRequestMethod::Post,
            QStringLiteral("/api/c/") +
                    collectionUid +
                    QStringLiteral("/t/export-vfs"),
            QUrlQuery{},
            QJsonDocument(requestBody),
    };
}

} // anonymous namespace

ExportTrackFilesTask::ExportTrackFilesTask(
        QNetworkAccessManager* networkAccessManager,
        QUrl baseUrl,
        const QString& collectionUid,
        const QJsonObject& trackFilter,
        const QString& targetRootPath)
        : mixxx::network::JsonWebTask(
                  networkAccessManager,
                  std::move(baseUrl),
                  buildRequest(collectionUid, trackFilter, targetRootPath)) {
}

void ExportTrackFilesTask::onFinished(
        const mixxx::network::JsonWebResponse& jsonResponse) {
    if (!jsonResponse.isStatusCodeSuccess()) {
        kLogger.warning()
                << "Request failed with HTTP status code"
                << jsonResponse.statusCode();
        emitFailed(jsonResponse);
        return;
    }
    VERIFY_OR_DEBUG_ASSERT(jsonResponse.statusCode() == mixxx::network::kHttpStatusCodeOk) {
        kLogger.warning()
                << "Unexpected HTTP status code"
                << jsonResponse.statusCode();
        emitFailed(jsonResponse);
        return;
    }

    VERIFY_OR_DEBUG_ASSERT(jsonResponse.content().isObject()) {
        kLogger.warning()
                << "Invalid JSON content"
                << jsonResponse.content();
        emitFailed(jsonResponse);
        return;
    }
    const auto outcome = jsonResponse.content().object();

    emitSucceeded(outcome);
}

void ExportTrackFilesTask::emitSucceeded(
        const QJsonObject& responseBody) {
    const auto signal = QMetaMethod::fromSignal(
            &ExportTrackFilesTask::succeeded);
    DEBUG_ASSERT(receivers(signal.name()) <= 1); // unique connection
    if (isSignalConnected(signal)) {
        emit succeeded(responseBody);
    } else {
        deleteLater();
    }
}

} // namespace aoide

#pragma once

#include <QJsonObject>
#include <QString>

#include "network/jsonwebtask.h"

namespace aoide {

class ExportTrackFilesTask : public mixxx::network::JsonWebTask {
    Q_OBJECT

  public:
    ExportTrackFilesTask(
            QNetworkAccessManager* networkAccessManager,
            QUrl baseUrl,
            const QString& collectionUid,
            const QJsonObject& trackFilter,
            const QString& targetRootPath);
    ~ExportTrackFilesTask() override = default;

  signals:
    void succeeded(
            const QJsonObject& responseBody);

  private:
    void onFinished(
            const mixxx::network::JsonWebResponse& jsonResponse) override;

    void emitSucceeded(
            const QJsonObject& responseBody);
};

} // namespace aoide

#pragma once

#include <QVector>

#include "aoide/json/playlist.h"
#include "aoide/util.h"
#include "network/jsonwebtask.h"

namespace aoide {

class ListPlaylistsTask : public mixxx::network::JsonWebTask {
    Q_OBJECT

  public:
    ListPlaylistsTask(
            QNetworkAccessManager* networkAccessManager,
            QUrl baseUrl,
            const QString& collectionUid,
            const QString& kind = QString(),
            const Pagination& pagination = Pagination());
    ~ListPlaylistsTask() override = default;

  signals:
    void succeeded(
            const QVector<aoide::json::PlaylistWithEntriesSummaryEntity>& result);

  private:
    void onFinished(
            const mixxx::network::JsonWebResponse& jsonResponse) override;

    void emitSucceeded(
            const QVector<aoide::json::PlaylistWithEntriesSummaryEntity>& result);
};

} // namespace aoide
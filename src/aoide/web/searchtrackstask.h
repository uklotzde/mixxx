#pragma once

#include <QStringList>
#include <QVector>

#include "aoide/json/track.h"
#include "aoide/tracksearchoverlayfilter.h"
#include "aoide/util.h"
#include "library/tags/facetid.h"
#include "network/jsonwebtask.h"

namespace aoide {

class SearchTracksTask : public mixxx::network::JsonWebTask {
    Q_OBJECT

  public:
    SearchTracksTask(
            QNetworkAccessManager* networkAccessManager,
            QUrl baseUrl,
            const QString& collectionUid,
            const QJsonObject& baseQuery,
            const TrackSearchOverlayFilter& overlayFilter,
            const QStringList& searchTerms,
            const Pagination& pagination = {});
    ~SearchTracksTask() override = default;

  signals:
    void succeeded(
            const QJsonArray& searchResults);

  private:
    void onFinished(
            const mixxx::network::JsonWebResponse& jsonResponse) override;

    void emitSucceeded(
            const QJsonArray& searchResults);
};

} // namespace aoide

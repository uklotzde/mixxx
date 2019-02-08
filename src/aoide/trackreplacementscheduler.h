#pragma once

#include <QList>
#include <QQueue>
#include <QSet>
#include <QVector>

#include "aoide/domain.h"

#include "network/asyncrestclient.h"

#include "track/track.h"
#include "track/trackref.h"


namespace mixxx {

class AsyncTrackLoader;

namespace aoide {

class Gateway;

class TrackReplacementScheduler: public QObject {
    Q_OBJECT

  public:
    TrackReplacementScheduler(
            QPointer<Gateway> gateway,
            QPointer<AsyncTrackLoader> trackLoader,
            QObject* parent = nullptr);
    ~TrackReplacementScheduler() override = default;

    void connectPeers();

    void replaceTracksAsync(
            QString collectionUid,
            QList<TrackRef> trackRefs);

    void cancelAsync();

  public slots:
    void replaceTracks(
            QString collectionUid,
            QList<TrackRef> trackRefs);

    void cancel();

  signals:
    // total = queued + pending + succeeded + failed
    void progress(int queued, int pending, int succeeded, int failed);

  private /*peer*/ slots:
    void /*AsyncTrackLoader*/ trackLoaded(
            TrackRef trackRef,
            TrackPointer trackPtr);

    void /*Gateway*/ networkRequestFailed(
            mixxx::AsyncRestClient::RequestId requestId,
            QString errorMessage);

    void /*Gateway*/ replaceTracksResult(
            mixxx::AsyncRestClient::RequestId requestId,
            QJsonObject result);

  /*private peer*/ signals:
    void /*AsyncTrackLoader*/ loadTrack(
            TrackRef trackRef);

  private:
    void makeProgress();
    void emitProgress();

    const QPointer<Gateway> m_gateway;

    QPointer<AsyncTrackLoader> m_trackLoader;

    // Requests for different collections
    QQueue<std::pair<QString, QList<TrackRef>>> m_deferredRequests;

    QString m_collectionUid;

    QQueue<TrackRef> m_queuedTrackRefs;

    QVector<TrackRef> m_loadingTrackRefs;

    bool isLoading(
            const TrackRef& trackRef) const;
    bool enterLoading(
            const TrackRef& trackRef);
    bool leaveLoading(
            const TrackRef& trackRef);

    QList<AoideTrack> m_bufferedRequests;

    QSet<AsyncRestClient::RequestId> m_pendingRequests;

    int m_pendingCounter;
    int m_succeededCounter;
    int m_failedCounter;
};

} // namespace aoide

} // namespace mixxx

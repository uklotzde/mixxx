#pragma once

#include <QObject>
#include <QPointer>

#include "track/track.h"
#include "track/trackref.h"

class TrackCollection;

namespace mixxx {

// Loads tracks asynchronously from the local collection. Must be
// located in the same thread where TrackCollection lives!
class AsyncTrackLoader: public QObject {
    Q_OBJECT

  public:
    explicit AsyncTrackLoader(
            QPointer<TrackCollection> trackCollection,
            QObject* parent = nullptr);
    ~AsyncTrackLoader() override = default;

    // Thread-safe async invocation
    void loadTrackAsync(
            TrackRef trackRef);

  public slots:
    void loadTrack(
            TrackRef trackRef);

  signals:
    // A nullptr indicates failure
    void trackLoaded(
            TrackRef trackRef,
            TrackPointer trackPtr);

  private:
    const QPointer<TrackCollection> m_trackCollection;
};

} // namespace mixxx

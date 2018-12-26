#pragma once

#include <QPointer>

#include "aoide/domain.h"

namespace mixxx {

namespace aoide {

class Subsystem;

class Agent: public QObject {
    Q_OBJECT

  public:
    explicit Agent(
            QPointer<Subsystem> subsystem,
            QObject* parent = nullptr);
    ~Agent() override = default;

  private /*peer*/ slots:
    void /*Subsystem*/ collectionsChanged(
            int flags);
    void /*Subsystem*/ searchTracksResult(
            QVector<AoideTrackEntity> result);

  private:
    const QPointer<Subsystem> m_subsystem;
};

} // namespace aoide

} // namespace mixxx

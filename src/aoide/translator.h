#pragma once

#include "aoide/domain.h"

class Track;


namespace mixxx {

namespace aoide {

class Translator {
  public:
    explicit Translator(
            QString collectionUid);

    const QString& collectionUid() const {
        return m_collectionUid;
    }

    AoideTrack exportTrack(
            const Track& track) const;

  private:
    const QString m_collectionUid;
};

} // namespace aoide

} // namespace mixxx

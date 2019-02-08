#pragma once

#include "aoide/domain.h"


namespace mixxx {

namespace aoide {

class Settings;

class /*interface*/ TrackTransformer {
  public:
    virtual AoideTrack importTrack(AoideTrack track) const = 0;
    virtual AoideTrack exportTrack(AoideTrack track) const = 0;
};

class MultiGenreTagger: public virtual /*implements*/ TrackTransformer {
  public:
    explicit MultiGenreTagger(
            const Settings& settings);

    AoideTrack importTrack(AoideTrack track) const override;
    AoideTrack exportTrack(AoideTrack track) const override;

  private:
    const QString m_multiGenreSeparator;
    const double m_multiGenreAttenuation;
};

} // namespace aoide

} // namespace mixxx

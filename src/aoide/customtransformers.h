#pragma once

#include "aoide/transformers.h"


namespace mixxx {

namespace aoide {

class CustomCommentsTransformer: public virtual /*implements*/ TrackTransformer {
  public:
    AoideTrack importTrack(AoideTrack track) const override;
    AoideTrack exportTrack(AoideTrack track) const override;
};

} // namespace aoide

} // namespace mixxx

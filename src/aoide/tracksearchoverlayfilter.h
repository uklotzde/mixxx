#pragma once

#include <QStringList>

#include "library/tags/label.h"
#include "track/bpm.h"

namespace aoide {

struct TrackSearchOverlayFilter {
    mixxx::Bpm minBpm;
    mixxx::Bpm maxBpm;
    mixxx::library::tags::LabelVector anyGenreLabels;
    mixxx::library::tags::LabelVector allHashtagLabels;
    mixxx::library::tags::LabelVector allCommentTerms;
    mixxx::library::tags::LabelVector anyCommentTerms;

    bool isEmpty() const {
        return !minBpm.isValid() &&
                !maxBpm.isValid() &&
                anyGenreLabels.isEmpty() &&
                allHashtagLabels.isEmpty() &&
                allCommentTerms.isEmpty() &&
                anyCommentTerms.isEmpty();
    }
};

inline bool operator==(const TrackSearchOverlayFilter& lhs, const TrackSearchOverlayFilter& rhs) {
    return lhs.minBpm == rhs.minBpm &&
            lhs.maxBpm == rhs.maxBpm &&
            lhs.anyGenreLabels == rhs.anyGenreLabels &&
            lhs.allHashtagLabels == rhs.allHashtagLabels &&
            lhs.allCommentTerms == rhs.allCommentTerms &&
            lhs.anyCommentTerms == rhs.anyCommentTerms;
}

inline bool operator!=(const TrackSearchOverlayFilter& lhs, const TrackSearchOverlayFilter& rhs) {
    return !(lhs == rhs);
}

inline QDebug operator<<(QDebug dbg, const TrackSearchOverlayFilter& arg) {
    const QDebugStateSaver saver(dbg);
    dbg = dbg.maybeSpace() << "TrackSearchOverlayFilter";
    return dbg.nospace()
            << '{'
            << arg.minBpm
            << ','
            << arg.maxBpm
            << ','
            << arg.anyGenreLabels
            << ','
            << arg.allHashtagLabels
            << ','
            << arg.allCommentTerms
            << ','
            << arg.anyCommentTerms
            << '}';
}

} // namespace aoide

Q_DECLARE_METATYPE(aoide::TrackSearchOverlayFilter)

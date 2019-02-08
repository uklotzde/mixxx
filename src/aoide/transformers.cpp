#include "aoide/transformers.h"

#include "aoide/settings.h"

#include "util/logger.h"

namespace mixxx {

namespace aoide {

namespace {

const Logger kLogger("aoide Transformers");

} // anonymous namespace

MultiGenreTagger::MultiGenreTagger(const Settings& settings)
    : m_multiGenreSeparator(settings.multiGenreSeparator()),
      m_multiGenreAttenuation(settings.multiGenreAttenuation()) {
    DEBUG_ASSERT(!m_multiGenreSeparator.isEmpty());
    DEBUG_ASSERT(m_multiGenreAttenuation > 0);
    DEBUG_ASSERT(m_multiGenreAttenuation <= 1);
}

AoideTrack MultiGenreTagger::importTrack(AoideTrack track) const {
    // TODO: Sort and concatenate genre tags into a single tag?
    return track;
}

AoideTrack MultiGenreTagger::exportTrack(AoideTrack track) const {
    QStringList genreNames;
    for (auto&& genreTag : track.removeTags(AoideScoredTag::kFacetGenre)) {
        DEBUG_ASSERT(genreTag.score() == 1);
        genreNames += genreTag.term().split(m_multiGenreSeparator, QString::SkipEmptyParts);
    }
    AoideScoredTagVector genreTags;
    genreTags.reserve(genreNames.size());
    double score = 1;
    for (auto&& genreName : genreNames) {
        AoideScoredTag genreTag;
        genreTag.setScore(score);
        genreTag.setTerm(genreName.trimmed());
        genreTag.setFacet(AoideScoredTag::kFacetGenre);
        genreTags += std::move(genreTag);
        score *= m_multiGenreAttenuation;
        DEBUG_ASSERT(score > 0);
    }
    track.addTags(genreTags);
    return track;
}

} // namespace aoide

} // namespace mixxx

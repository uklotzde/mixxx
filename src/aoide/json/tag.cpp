#include "aoide/json/tag.h"

#include "library/tags/facets.h"

namespace aoide {

// MusicBrainz: "comment:description"
const mixxx::library::tags::FacetId kFacetComment =
        mixxx::library::tags::FacetId::staticConst(QStringLiteral("comm"));

const mixxx::library::tags::FacetId kFacetGenre =
        mixxx::library::tags::FacetId::staticConst(QStringLiteral("gnre"));

const mixxx::library::tags::FacetId kFacetGrouping =
        mixxx::library::tags::FacetId::staticConst(QStringLiteral("cgrp"));

const mixxx::library::tags::FacetId kFacetMood =
        mixxx::library::tags::FacetId::staticConst(QStringLiteral("mood"));

const QString kGenreSeparator = QStringLiteral(";");

const QString kMoodSeparator = QStringLiteral(";");

namespace json {

SimplifiedTags::SimplifiedTags(QJsonObject jsonObject) {
    auto const facets = mixxx::library::tags::Facets::fromJsonObject(jsonObject)
                                .value_or(mixxx::library::tags::Facets{});
    const auto genreTags = facets.collectTagsOrdered(
            mixxx::library::tags::Facets::ScoreOrdering::Descending,
            kFacetGenre);
    m_genres.reserve(genreTags.size());
    for (const auto& genreTag : genreTags) {
        m_genres.append(genreTag.getLabel().value());
    }
    const auto moodTags = facets.collectTagsOrdered(
            mixxx::library::tags::Facets::ScoreOrdering::Descending,
            kFacetGenre);
    m_moods.reserve(moodTags.size());
    for (const auto& moodTag : moodTags) {
        m_moods.append(moodTag.getLabel().value());
    }
    m_comment =
            facets.getSingleTagLabel(
                          kFacetComment)
                    .value_or(mixxx::library::tags::Label{});
    m_grouping =
            facets.getSingleTagLabel(
                          kFacetGrouping)
                    .value_or(mixxx::library::tags::Label{});
}

QString SimplifiedTags::joinGenres() const {
    return m_genres.join(kGenreSeparator);
}

QString SimplifiedTags::joinMoods() const {
    return m_moods.join(kMoodSeparator);
}

} // namespace json

} // namespace aoide

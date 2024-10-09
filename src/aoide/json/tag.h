#pragma once

#include "aoide/json/json.h"
#include "library/tags/facetid.h"

namespace aoide {

extern const mixxx::library::tags::FacetId kFacetGenre;    // multi-valued genre(s)
extern const mixxx::library::tags::FacetId kFacetMood;     // multi-valued mood(s)
extern const mixxx::library::tags::FacetId kFacetComment;  // single-valued comment
extern const mixxx::library::tags::FacetId kFacetGrouping; // single-valued grouping (content group)

extern const QString kGenreSeparator;

extern const QString kMoodSeparator;

namespace json {

/// Selected tags that are supported by Mixxx
///
/// The track search must include the query parameter `encodeGigtags=true`
/// such that all custom tags are encoded as a string in the grouping field.
class SimplifiedTags : public Object {
  public:
    explicit SimplifiedTags(
            QJsonObject jsonObject = QJsonObject());

    const QStringList& genres() const {
        return m_genres;
    }
    QString joinGenres() const;

    const QStringList& moods() const {
        return m_moods;
    }
    QString joinMoods() const;

    const QString& comment() const {
        return m_comment;
    }

    const QString& grouping() const {
        return m_grouping;
    }

  private:
    QStringList m_genres;
    QStringList m_moods;
    QString m_comment;
    QString m_grouping;
};

} // namespace json

} // namespace aoide

Q_DECLARE_METATYPE(aoide::json::SimplifiedTags);

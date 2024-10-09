#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

#include "util/assert.h"
#include "util/compatibility/qhash.h"

namespace mixxx {

namespace library {

namespace tags {

/// An identifier for referencing tag categories.
///
/// Facets are used for grouping/categorizing and providing context or meaning.
///
/// Serves as a symbolic, internal identifier that is not intended to be displayed
/// literally in the UI. The restrictive naming constraints ensure that they are
/// not used for storing arbitrary text. Instead facet identifiers should be mapped
/// to translated display strings, e.g. the facet "gnre" could be mapped to "Genre"
/// in English and the facet "venue" could be mapped to "Veranstaltungsort" in German.
///
/// Value constraints:
///   - charset/alphabet: +-./0123456789@[]_abcdefghijklmnopqrstuvwxyz
///   - no leading/trailing/inner whitespace
///
/// Rationale for the value constraints:
///   - Facet identifiers are intended to be created, shared, and parsed worldwide
///   - The Lingua franca of IT is English
///   - ASCII characters can be encoded by a single byte in UTF-8
///
/// References:
///   - https://en.wikipedia.org/wiki/Faceted_classification
class FacetId final {
  public:
    typedef QString value_t;

    /// The alphabet of facets
    ///
    /// All valid characters, ordered by their ASCII codes.
    static constexpr const char* kAlphabet = "+-./0123456789@[]_abcdefghijklmnopqrstuvwxyz";

    static bool isValidValue(
            const value_t& value);

    /// Convert the given string into lowercase and then
    /// removes all whitespace and non-ASCII characters.
    static value_t convertIntoValidValue(
            const value_t& value);

    /// Ensure that empty values are always null
    static value_t filterEmptyValue(
            value_t value) {
        // std::move() is required despite Return Value Optimization (RVO)
        // to avoid clazy warnings!
        return value.isEmpty() ? value_t{} : std::move(value);
    }

    /// Default constructor.
    FacetId() = default;

    /// Create a new instance.
    ///
    /// This constructor must not be used for static constants!
    explicit FacetId(
            value_t value)
            : m_value(std::move(value)) {
        DEBUG_ASSERT(isValid());
    }

    /// Type-tag for creating non-validated, static constants.
    ///
    /// The regular expressions required for validation are also
    /// static constant defined in this compilation unit. The
    /// initialization order between compilation units is undefined!
    enum struct StaticCtor {};

    /// Constructor for creating non-validated, static constants.
    FacetId(
            StaticCtor,
            value_t value)
            : m_value(std::move(value)) {
    }

    static FacetId staticConst(value_t value) {
        return FacetId(StaticCtor{}, std::move(value));
    }

    FacetId(const FacetId&) = default;
    FacetId(FacetId&&) = default;

    FacetId& operator=(const FacetId&) = default;
    FacetId& operator=(FacetId&&) = default;

    bool isValid() const {
        return isValidValue(m_value);
    }

    bool isEmpty() const {
        DEBUG_ASSERT(isValid());
        return m_value.isEmpty();
    }

    const value_t& value() const {
        DEBUG_ASSERT(isValid());
        return m_value;
    }
    operator const value_t&() const {
        return value();
    }

  private:
    value_t m_value;
};

inline bool operator==(
        const FacetId& lhs,
        const FacetId& rhs) {
    return lhs.value() == rhs.value();
}

inline bool operator!=(
        const FacetId& lhs,
        const FacetId& rhs) {
    return !(lhs == rhs);
}

inline bool operator<(
        const FacetId& lhs,
        const FacetId& rhs) {
    return lhs.value() < rhs.value();
}

inline bool operator>(
        const FacetId& lhs,
        const FacetId& rhs) {
    return !(lhs < rhs);
}

inline bool operator<=(
        const FacetId& lhs,
        const FacetId& rhs) {
    return !(lhs > rhs);
}

inline bool operator>=(
        const FacetId& lhs,
        const FacetId& rhs) {
    return !(lhs < rhs);
}

inline qhash_seed_t qHash(
        const FacetId& facetId,
        qhash_seed_t seed = 0) {
    return qHash(facetId.value(), seed);
}

} // namespace tags

} // namespace library

} // namespace mixxx

Q_DECLARE_METATYPE(mixxx::library::tags::FacetId)

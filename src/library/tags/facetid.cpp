#include "library/tags/facetid.h"

#include <QRegularExpression>

namespace {

const QRegularExpression kValidFacetStringNotEmpty(
        QStringLiteral("^[a-z][\\+\\-\\./0-9@a-z\\[\\]_]*"));
const QRegularExpression kInvalidFacetChars(
        QStringLiteral("[^\\+\\-\\./0-9@a-z\\[\\]_]*"));

} // anonymous namespace

namespace mixxx {

namespace library {

namespace tags {

// static
bool FacetId::isValidValue(
        const value_t& value) {
    if (value.isNull()) {
        return true;
    }
    if (value.isEmpty()) {
        // for disambiguation with null
        return false;
    }
    const auto match = kValidFacetStringNotEmpty.match(value);
    DEBUG_ASSERT(match.isValid());
    DEBUG_ASSERT(value.length() > 0);
    // match = exact match
    return match.capturedLength() == value.length();
}

// static
FacetId::value_t FacetId::convertIntoValidValue(
        const value_t& value) {
    const auto validChars = value.toLower().remove(kInvalidFacetChars);
    int offset = 0;
    while (offset < validChars.size() && !validChars[offset].isLetter()) {
        ++offset;
    }
    DEBUG_ASSERT(offset <= validChars.size());
    if (offset >= validChars.size()) {
        return {};
    }
    const auto validValue = validChars.right(validChars.size() - offset);
    DEBUG_ASSERT(!validValue.isEmpty());
    DEBUG_ASSERT(isValidValue(validValue));
    return validValue;
}

} // namespace tags

} // namespace library

} // namespace mixxx

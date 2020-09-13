#include "tagging/customtagsfromtrackcommentextractor.h"

#include <QRegularExpression>

#include "library/trackcollectionmanager.h"
#include "track/track.h"
#include "util/logger.h"

namespace mixxx {

namespace {

const Logger kLogger("CustomTagsFromTrackCommentExtractor");

const QString kTokenSeparator = " ";

const QRegularExpression kRegexpEquiv("^~|=|==$");

const QRegularExpression kRegexpDecade(
        "^[1-2][0-9]{2}0s$"); // calendar year decades in the format YYYYs

const QRegularExpression kRegexpOccasion(
        "^B(irth)?day|Cruise|Chillout|Driving|Holiday|Motivation|Casual|(Apres)"
        "?Gaudi|Enjoy|Love|Relax|Running|Summer|Vacation|"
        "Wedding.*|Workout|Xmas|RocknRoll|Bar|Pub|Beach|Dinner|Club|Lounge|"
        "Party|Soundcheck(:.+)?|NewYearsEve|Meditation|SundayMorning$");

const QRegularExpression kRegexpDjSet(
        "^Arrival|LadyKiller|Warmup|(Floor)?Filler|Open(er|ing)|Clos(er|ing)|"
        "Peak(Time)?|AfterHour|Afterhours?$");

const QRegularExpression kRegexpExclude("^Cue(\\+|-).+|Intro(\\+|-).+|Outro(\\+|-).+$");

const QMap<QString, QString> kTagLabelReplacements = {
        {"Birthday", "Bday"},
        {"Enjoy", "Casual"},
        {"Filler", "FloorFiller"},
        {"Peak", "PeakTime"},
        {"Driving", "Cruise"},
        {"WeddingClassic", "Wedding"},
        {"Afterhours", "AfterHour"},
        {"Closer", "Closing"},
        {"Opener", "Opening"},
        {"Gaudi", "ApresGaudi"},
        {"Charts", "Top40"},
        {"Lounge", "Lounge&Bar"},
        {"Bar", "Lounge&Bar"},
};

//const int kRatingValueMin = 0;
//const int kRatingValueMax = 5;
const QRegularExpression kRegexpRating("^r([0-5])$"); // "r" like "r"ating

//const int kEnergyValueMin = 0;
//const int kEnergyValueMax = 5;
const QRegularExpression kRegexpEnergy("^e([0-5])$"); // "e" like in "e"nergy

//const int kValenceValueMin = 0;
//const int kValenceValueMax = 5;
const QRegularExpression kRegexpValence("^m([0-5])$"); // "m" like in "m"ood -> valence

//const int kDanceabilityValueMin = 0;
//const int kDanceabilityValueMax = 5;
const QRegularExpression kRegexpDanceability("^d([0-5])$"); // "d" like in "d"anceability

const QString kRegexpPrefixTemplate = "^%1(.*)$";

const TagFacet kTagFacetDjSet = TagFacet("dj-set");
const TagFacet kTagFacetSelection = TagFacet("collection");
const TagFacet kTagFacetCovers = TagFacet("covers");
const TagFacet kTagFacetCoveredBy = TagFacet("covered-by");
const TagFacet kTagFacetDecade = TagFacet("decade");
const TagFacet kTagFacetLyrics = TagFacet("lyrics");
const TagFacet kTagFacetOccasion = TagFacet("occasion");
const TagFacet kTagFacetPlayAfter = TagFacet("play-after");
const TagFacet kTagFacetPlayBefore = TagFacet("play-before");
const TagFacet kTagFacetRating = TagFacet("rating");
const TagFacet kTagFacetSamples = TagFacet("samples");
const TagFacet kTagFacetSampledBy = TagFacet("sampled-by");
const TagFacet kTagFacetSoundtrack = TagFacet("soundtrack");

const TagFacet kFacetEquiv = TagFacet("equiv");

void replaceScoredTag(CustomTags* pCustomTags,
        const TagFacet& facet,
        double score,
        QString label = QString()) {
    if (kTagLabelReplacements.contains(label)) {
        label = kTagLabelReplacements.value(label);
    }
    pCustomTags->addOrReplaceTag(
            Tag(
                    TagLabel(TagLabel::clampValue(label)),
                    TagScore(TagScore::clampValue(score))),
            TagFacet(TagFacet::clampValue(facet)));
}

void replaceLabeledTag(
        CustomTags* pCustomTags,
        const QString& label,
        const TagFacet& facet = TagFacet()) {
    replaceScoredTag(
            pCustomTags,
            facet,
            TagScore::kDefaultValue,
            label);
}

bool transformTokenTag(
        CustomTags* pCustomTags,
        const QString& token,
        const QRegularExpression& regexp,
        const TagFacet& facet = TagFacet()) {
    const QRegularExpressionMatch regexpMatch = regexp.match(token);
    if (regexpMatch.hasMatch()) {
        replaceLabeledTag(pCustomTags, token, facet);
        return true;
    } else {
        return false;
    }
}

bool transformPrefixedTag(
        CustomTags* pCustomTags,
        const QString& token,
        const QString& regexpPrefix,
        const TagFacet& tagFacet = TagFacet()) {
    const QRegularExpression regexp(kRegexpPrefixTemplate.arg(regexpPrefix));
    const QRegularExpressionMatch regexpMatch = regexp.match(token);
    if (!regexpMatch.hasMatch()) {
        return false;
    }
    QString label = regexpMatch.captured(1).trimmed();
    if (label.isEmpty()) {
        kLogger.warning() << "Prefixed token with empty label:" << token;
        return false;
    }
    replaceLabeledTag(pCustomTags, label, tagFacet);
    return true;
}

bool transformToken(
        CustomTags* pCustomTags,
        const QString& token) {
    DEBUG_ASSERT(token.trimmed() == token);
    if (token.isEmpty()) {
        return false;
    }
    QRegularExpressionMatch matchRating = kRegexpRating.match(token);
    if (matchRating.hasMatch()) {
        DEBUG_ASSERT(matchRating.lastCapturedIndex() == 1);
        return false;
        /*
        bool valid = false;
        int value = matchRating.captured(1).toInt(&valid);
        if (!valid && value < kRatingValueMin && value > kRatingValueMax) {
            kLogger.warning() << "Invalid rating token:" << token;
            return false;
        }
        double score = static_cast<double>(value) / (kRatingValueMax - kRatingValueMin);
        replaceScoredTag(pCustomTags, kTagFacetRating, score);
        return true;
        */
    }

    QRegularExpressionMatch matchEnergy = kRegexpEnergy.match(token);
    if (matchEnergy.hasMatch()) {
        DEBUG_ASSERT(matchEnergy.lastCapturedIndex() == 1);
        return false;
        /*
        bool valid = false;
        int value = matchEnergy.captured(1).toInt(&valid);
        if (!valid && value < kEnergyValueMin && value > kEnergyValueMax) {
            kLogger.warning() << "Invalid energy token:" << token;
            return false;
        }
        double score = static_cast<double>(value) / (kEnergyValueMax - kEnergyValueMin);
        replaceScoredTag(pCustomTags, CustomTags::kReservedFacetEnergy, score);
        return true;
        */
    }

    QRegularExpressionMatch matchValence = kRegexpValence.match(token);
    if (matchValence.hasMatch()) {
        DEBUG_ASSERT(matchValence.lastCapturedIndex() == 1);
        return false;
        /*
        bool valid = false;
        int value = matchValence.captured(1).toInt(&valid);
        if (!valid && value < kValenceValueMin && value > kValenceValueMax) {
            kLogger.warning() << "Invalid valence token:" << token;
            return false;
        }
        double score = static_cast<double>(value) / (kValenceValueMax - kValenceValueMin);
        replaceScoredTag(pCustomTags, CustomTags::kReservedFacetValence, score);
        return true;
        */
    }

    QRegularExpressionMatch matchDanceability = kRegexpDanceability.match(token);
    if (matchDanceability.hasMatch()) {
        DEBUG_ASSERT(matchDanceability.lastCapturedIndex() == 1);
        return false;
        /*
        bool valid = false;
        int value = matchDanceability.captured(1).toInt(&valid);
        if (!valid && value < kDanceabilityValueMin && value > kDanceabilityValueMax) {
            kLogger.warning() << "Invalid danceability token:" << token;
            return false;
        }
        double score = static_cast<double>(value) / (kDanceabilityValueMax - kDanceabilityValueMin);
        replaceScoredTag(pCustomTags, CustomTags::kReservedFacetDanceability, score);
        return true;
        */
    }

    // The prefixed tokens have to be processed before processing
    // the remaining tokens without a prefix!
    if (transformPrefixedTag(pCustomTags, token, "<", kTagFacetPlayBefore) ||
            transformPrefixedTag(pCustomTags, token, ">", kTagFacetPlayAfter) ||
            transformPrefixedTag(pCustomTags, token, "Selection:", kTagFacetSelection) ||
            transformPrefixedTag(pCustomTags, token, "Covers?:", kTagFacetCovers) ||
            transformPrefixedTag(pCustomTags, token, "CoveredBy:", kTagFacetCoveredBy) ||
            transformPrefixedTag(pCustomTags, token, "Lyrics:", kTagFacetLyrics) ||
            transformPrefixedTag(pCustomTags, token, "Samples?:", kTagFacetSamples) ||
            transformPrefixedTag(pCustomTags, token, "SampledBy:", kTagFacetSampledBy) ||
            transformPrefixedTag(pCustomTags, token, "Soundtrack:", kTagFacetSoundtrack)) {
        return true;
    }

    if (transformTokenTag(pCustomTags, token, kRegexpEquiv, kFacetEquiv) ||
            transformTokenTag(pCustomTags, token, kRegexpDecade, kTagFacetDecade) ||
            transformTokenTag(pCustomTags, token, kRegexpOccasion, kTagFacetOccasion) ||
            transformTokenTag(pCustomTags, token, kRegexpDjSet, kTagFacetDjSet)) {
        return true;
    }

    QRegularExpressionMatch matchExclude = kRegexpExclude.match(token);
    if (matchExclude.hasMatch()) {
        return false;
    } else {
        // Any other not excluded token becomes a free tag (without a facet)
        replaceLabeledTag(pCustomTags, token);
        return true;
    }
}

} // anonymous namespace

void CustomTagsFromTrackCommentExtractor::doApply(
        const TrackPointer& pTrack) const {
    auto customTags = pTrack->getCustomTags();
    const auto oldComment = pTrack->getComment();
    QStringList tokens =
            oldComment.split(
                    kTokenSeparator,
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                    Qt::SkipEmptyParts);
#else
                    QString::SkipEmptyParts);
#endif
    QStringList unparsedTokens;
    unparsedTokens.reserve(tokens.size());
    for (auto&& token : tokens) {
        token = token.trimmed();
        if (!token.isEmpty() && !transformToken(&customTags, token)) {
            unparsedTokens += std::move(token);
        }
    }
    pTrack->updateCustomTags(
            m_pTrackCollectionManager->taggingConfig(),
            customTags);
    QString newComment;
    if (!unparsedTokens.isEmpty()) {
        newComment = unparsedTokens.join(kTokenSeparator);
        DEBUG_ASSERT(newComment.trimmed() == newComment);
    }
    pTrack->setComment(newComment);
}

void TrackFileMover::doApply(
        const TrackPointer& pTrack) const {
    const auto oldFileInfo = pTrack->getFileInfo();
    VERIFY_OR_DEBUG_ASSERT(oldFileInfo.isAbsolute()) {
        kLogger.warning()
                << "File path"
                << oldFileInfo
                << "is not absolute";
        return;
    }
    const auto oldLocation = oldFileInfo.location();
    const auto oldLocationPrefixPos = oldLocation.indexOf(m_fileLocationReplacement.oldPrefix);
    if (oldLocationPrefixPos != 0) {
        kLogger.info()
                << "Track location"
                << oldLocation
                << "does not start with"
                << m_fileLocationReplacement.oldPrefix;
        return;
    }
    DEBUG_ASSERT(m_fileLocationReplacement.oldPrefix.size() <= oldLocation.size());
    const auto oldLocationSuffixSize =
            oldLocation.size() - m_fileLocationReplacement.oldPrefix.size();
    const auto oldLocationSuffix =
            oldLocation.right(oldLocationSuffixSize);
    const QString newLocation =
            m_fileLocationReplacement.newPrefix +
            oldLocationSuffix;
    if (oldLocation == newLocation) {
        kLogger.info()
                << "Track file remains at its current location"
                << oldLocation;
        return;
    }
    const auto newFileInfo = mixxx::FileInfo(newLocation);
    if (oldFileInfo.completeSuffix() != newFileInfo.completeSuffix()) {
        kLogger.warning()
                << "Mismatching file suffixes"
                << oldLocation
                << newLocation;
        return;
    }
    VERIFY_OR_DEBUG_ASSERT(newFileInfo.isAbsolute()) {
        kLogger.warning()
                << "File path"
                << newFileInfo
                << "is not absolute";
        return;
    }
    if (newFileInfo.exists()) {
        kLogger.warning()
                << "New track location"
                << newLocation
                << "already exists";
        return;
    }
    kLogger.info()
            << "Moving track file from"
            << oldLocation
            << "to"
            << newLocation;
    const auto targetPath = newFileInfo.locationPath();
    if (!QDir().mkpath(targetPath)) {
        kLogger.warning()
                << "Failed to create target path"
                << targetPath;
        return;
    }
    if (!QFile::copy(oldLocation, newLocation)) {
        kLogger.warning()
                << "Failed to copy track file from"
                << oldLocation
                << "to"
                << newLocation;
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    if (!QFile::moveToTrash(oldLocation)) {
        kLogger.warning()
                << "Failed to move track file"
                << oldLocation
                << "into trash";
#else
    if (!QFile::remove(oldLocation)) {
        kLogger.warning()
                << "Failed to remove track file"
                << oldLocation;
#endif
        return;
    }
    if (!m_pTrackCollectionManager->hideTracks({pTrack->getId()})) {
        kLogger.warning()
                << "Failed to hide track"
                << oldLocation;
    }
}

} // namespace mixxx

#include "aoide/customtransformers.h"

#include <QRegularExpression>

#include "util/logger.h"

namespace mixxx {

namespace aoide {

namespace {

const Logger kLogger("aoide Custom Transformers");

const QString kTokenSeparator = " ";

const QRegularExpression kRegexpEquiv("^~|=|==$");

const QRegularExpression kRegexpEpoch(
        "^[1-2][0-9]{2}0s$"); // calendar year decades in the format YYYYs

const QRegularExpression kRegexpEvent(
        "^B(irth)?day|Cruise|Driving|Holiday|Motivation|Enjoy|Love|Relax|Running|Summer|Vacation|"
        "Wedding.*|Workout|Xmas|RocknRoll$");

const QRegularExpression kRegexpVenue("^Bar|Beach|Dinner|Club|Lounge|Soundcheck(:.+)?$");

const QRegularExpression kRegexpCrowd("^Chica|Deutsch(:.+)?|Gaudi|Charts|Top40|Party.*$");

const QRegularExpression kRegexpSession(
        "^Warmup|(Floor)?Filler|Open(er|ing)|Clos(er|ing)|Peak|Afterhours?$");

const QRegularExpression kRegexpExclude("^Cue(\\+|-).+|Intro(\\+|-).+|Outro(\\+|-).+$");

const int kRatingValueMin = 0;
const int kRatingValueMax = 5;
const QRegularExpression kRegexpRating("^r([0-5])$"); // "r" like "r"ating

const int kEnergyValueMin = 0;
const int kEnergyValueMax = 5;
const QRegularExpression kRegexpEnergy("^e([0-5])$"); // "e" like in "e"nergy

const int kValenceValueMin = 0;
const int kValenceValueMax = 5;
const QRegularExpression kRegexpValence("^m([0-5])$"); // "m" like in "m"ood -> valence

const int kDanceabilityValueMin = 0;
const int kDanceabilityValueMax = 5;
const QRegularExpression kRegexpDanceability("^d([0-5])$"); // "d" like in "d"anceability

const QString kRegexpPrefixTemplate = "^%1(.*)$";

const QString kTagFacetCollection = "collection";
const QString kTagFacetCovers = "covers";
const QString kTagFacetCoveredBy = "covered-by";
const QString kTagFacetLyrics = "lyrics";
const QString kTagFacetPlayAfter = "play-after";
const QString kTagFacetPlayBefore = "play-before";
const QString kTagFacetSamples = "samples";
const QString kTagFacetSampledBy = "sampled-by";
const QString kTagFacetSoundtrack = "soundtrack";

const QString kRatingOwner = QString(); // anonymous or "mixxx.org"?

void addUniqueTermTag(AoideTrack* track, QString term, QString tagFacet = QString()) {
    AoideScoredTagVector tags = track->removeTags(tagFacet);
    for (auto&& tag : tags) {
        if (tag.term() == term) {
            track->addTags(std::move(tags));
            return;
        }
    }
    AoideScoredTag tag;
    tag.setFacet(std::move(tagFacet));
    tag.setTerm(std::move(term));
    tags += std::move(tag);
    track->addTags(std::move(tags));
}

bool transformTokenTag(
        AoideTrack* track, QString token, const QRegularExpression& regexp,
        QString tagFacet = QString()) {
    const QRegularExpressionMatch regexpMatch = regexp.match(token);
    if (regexpMatch.hasMatch()) {
        addUniqueTermTag(track, std::move(token), std::move(tagFacet));
        return true;
    } else {
        return false;
    }
}

bool transformPrefixedTag(
        AoideTrack* track, QString token, const QString& regexpPrefix,
        QString tagFacet = QString()) {
    const QRegularExpression regexp(kRegexpPrefixTemplate.arg(regexpPrefix));
    const QRegularExpressionMatch regexpMatch = regexp.match(token);
    if (!regexpMatch.hasMatch()) {
        return false;
    }
    QString term = regexpMatch.captured(1).trimmed();
    if (term.isEmpty()) {
        kLogger.warning() << "Prefixed token with empty term:" << token;
        return false;
    }
    addUniqueTermTag(track, std::move(term), std::move(tagFacet));
    return true;
}

bool transformToken(AoideTrack* track, const QString& token) {
    DEBUG_ASSERT(token.trimmed() == token);
    if (token.isEmpty()) {
        return false;
    }
    QRegularExpressionMatch matchRating = kRegexpRating.match(token);
    if (matchRating.hasMatch()) {
        DEBUG_ASSERT(matchRating.lastCapturedIndex() == 1);
        bool valid = false;
        int value = matchRating.captured(1).toInt(&valid);
        if (!valid && value < kRatingValueMin && value > kRatingValueMax) {
            kLogger.warning() << "Invalid rating token:" << token;
            return false;
        }
        AoideRatingVector ratings = track->removeRatings(kRatingOwner);
        VERIFY_OR_DEBUG_ASSERT(ratings.size() <= 1) {
            kLogger.warning() << "Track already has too many =" << ratings.size() << "ratings";
            track->addRatings(ratings);
            return false;
        }
        double score = static_cast<double>(value) / (kRatingValueMax - kRatingValueMin);
        AoideRating rating;
        if (ratings.isEmpty()) {
            rating.setOwner(kRatingOwner);
        } else {
            rating = ratings.first();
            kLogger.info() << "Replacing track rating score" << rating.score() << "with" << score;
        }
        rating.setScore(score);
        track->addRatings({std::move(rating)});
        return true;
    }

    QRegularExpressionMatch matchEnergy = kRegexpEnergy.match(token);
    if (matchEnergy.hasMatch()) {
        DEBUG_ASSERT(matchEnergy.lastCapturedIndex() == 1);
        bool valid = false;
        int value = matchEnergy.captured(1).toInt(&valid);
        if (!valid && value < kEnergyValueMin && value > kEnergyValueMax) {
            kLogger.warning() << "Invalid energy token:" << token;
            return false;
        }
        double score = static_cast<double>(value) / (kEnergyValueMax - kEnergyValueMin);
        AoideSongProfile profile = track->profile();
        profile.setFeature(AoideScoredSongFeature::kEnergy, score);
        track->setProfile(std::move(profile));
        return true;
    }

    QRegularExpressionMatch matchValence = kRegexpValence.match(token);
    if (matchValence.hasMatch()) {
        DEBUG_ASSERT(matchValence.lastCapturedIndex() == 1);
        bool valid = false;
        int value = matchValence.captured(1).toInt(&valid);
        if (!valid && value < kValenceValueMin && value > kValenceValueMax) {
            kLogger.warning() << "Invalid valence token:" << token;
            return false;
        }
        double score = static_cast<double>(value) / (kValenceValueMax - kValenceValueMin);
        AoideSongProfile profile = track->profile();
        profile.setFeature(AoideScoredSongFeature::kValence, score);
        track->setProfile(std::move(profile));
        return true;
    }

    QRegularExpressionMatch matchDanceability = kRegexpDanceability.match(token);
    if (matchDanceability.hasMatch()) {
        DEBUG_ASSERT(matchDanceability.lastCapturedIndex() == 1);
        bool valid = false;
        int value = matchDanceability.captured(1).toInt(&valid);
        if (!valid && value < kDanceabilityValueMin && value > kDanceabilityValueMax) {
            kLogger.warning() << "Invalid danceability token:" << token;
            return false;
        }
        double score = static_cast<double>(value) / (kDanceabilityValueMax - kDanceabilityValueMin);
        AoideSongProfile profile = track->profile();
        profile.setFeature(AoideScoredSongFeature::kDanceability, score);
        track->setProfile(std::move(profile));
        return true;
    }

    // The prefixed tokens have to be processed before processing
    // the remaining tokens without a prefix!
    if (transformPrefixedTag(track, token, "<", kTagFacetPlayBefore) ||
        transformPrefixedTag(track, token, ">", kTagFacetPlayAfter) ||
        transformPrefixedTag(track, token, "Collection:", kTagFacetCollection) ||
        transformPrefixedTag(track, token, "Covers?:", kTagFacetCovers) ||
        transformPrefixedTag(track, token, "CoveredBy:", kTagFacetCoveredBy) ||
        transformPrefixedTag(track, token, "Lyrics:", kTagFacetLyrics) ||
        transformPrefixedTag(track, token, "Samples?:", kTagFacetSamples) ||
        transformPrefixedTag(track, token, "SampledBy:", kTagFacetSampledBy) ||
        transformPrefixedTag(track, token, "Soundtrack:", kTagFacetSoundtrack)) {
        return true;
    }

    if (transformTokenTag(track, token, kRegexpEquiv, AoideScoredTag::kFacetEquiv) ||
        transformTokenTag(track, token, kRegexpEpoch, AoideScoredTag::kFacetEpoch) ||
        transformTokenTag(track, token, kRegexpEvent, AoideScoredTag::kFacetEvent) ||
        transformTokenTag(track, token, kRegexpVenue, AoideScoredTag::kFacetVenue) ||
        transformTokenTag(track, token, kRegexpCrowd, AoideScoredTag::kFacetCrowd) ||
        transformTokenTag(track, token, kRegexpSession, AoideScoredTag::kFacetSession)) {
        return true;
    }

    QRegularExpressionMatch matchExclude = kRegexpExclude.match(token);
    if (matchExclude.hasMatch()) {
        return false;
    } else {
        // Any other not excluded token becomes a free tag (without a facet)
        addUniqueTermTag(track, token);
        return true;
    }
}

} // anonymous namespace

AoideTrack CustomCommentsTransformer::importTrack(AoideTrack track) const {
    return track;
}

AoideTrack CustomCommentsTransformer::exportTrack(AoideTrack track) const {
    auto&& comments = track.removeComments();
    AoideCommentVector remainingComments;
    for (auto&& comment : comments) {
        QStringList unparsedTokens;
        const QStringList tokens = comment.text().split(kTokenSeparator, QString::SkipEmptyParts);
        for (auto&& token : tokens) {
            DEBUG_ASSERT(!token.isEmpty());
            if (!transformToken(&track, token.trimmed())) {
                unparsedTokens += std::move(token);
            }
        }
        if (!unparsedTokens.isEmpty()) {
            comment.setText(unparsedTokens.join(kTokenSeparator));
            remainingComments += std::move(comment);
        }
    }
    track.addComments(remainingComments);
    return track;
}

} // namespace aoide

} // namespace mixxx

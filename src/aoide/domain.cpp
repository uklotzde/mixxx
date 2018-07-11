#include "aoide/domain.h"

#include <QDateTime>
#include <QJsonArray>
#include <QTimeZone>
#include <QVariant>

#include "track/keyutils.h"

#include "util/assert.h"
#include "util/logger.h"

namespace {

const mixxx::Logger kLogger("aoide Domain");

} // anonymous namespace

void AoideJsonObject::putOptional(QString key, QString value) {
    if (value.isNull()) {
        m_jsonObject.remove(key);
    } else {
        m_jsonObject.insert(std::move(key), std::move(value));
    }
}

void AoideJsonObject::putOptional(QString key, QJsonArray array) {
    if (array.isEmpty()) {
        m_jsonObject.remove(key);
    } else {
        m_jsonObject.insert(std::move(key), std::move(array));
    }
}

void AoideJsonObject::putOptional(QString key, QJsonObject object) {
    if (object.isEmpty()) {
        m_jsonObject.remove(key);
    } else {
        m_jsonObject.insert(std::move(key), std::move(object));
    }
}

void AoideJsonObject::putOptionalNonEmpty(QString key, QString value) {
    if (value.isNull()) {
        m_jsonObject.remove(key);
    } else {
        VERIFY_OR_DEBUG_ASSERT(!value.isEmpty()) {
            // Empty value is considered invalid
            m_jsonObject.remove(key);
        }
        else {
            m_jsonObject.insert(std::move(key), std::move(value));
        }
    }
}

/*static*/ QDateTime AoideJsonBase::parseDateTimeOrYear(QString value) {
    const auto trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QDateTime(); // null
    }
    // To upper: 't' -> 'T', 'z' -> 'Z'
    QDateTime datetime = QDateTime::fromString(trimmed.toUpper(), Qt::ISODateWithMs);
    if (!datetime.isValid()) {
        // Try to parse ISO format without seconds
        datetime = QDateTime::fromString(trimmed, "yyyy-M-dTh:m");
        if (!datetime.isValid()) {
            // Try to parse year + month + day
            datetime = QDateTime::fromString(trimmed, "yyyy-M-d");
            if (!datetime.isValid()) {
                // Try to parse year + month
                datetime = QDateTime::fromString(trimmed, "yyyy-M");
                if (!datetime.isValid()) {
                    // Try to parse only a year
                    datetime = QDateTime::fromString(trimmed, "yyyy");
                    if (!datetime.isValid()) {
                        // Try to parse only a year with invalid month + day
                        datetime = QDateTime::fromString(trimmed, "yyyy-00-00");
                        if (!datetime.isValid()) {
                            // Try to parse only a year with invalid month + day
                            datetime = QDateTime::fromString(trimmed, "yyyy-0-0");
                        }
                    }
                }
            }
        }
    }
    VERIFY_OR_DEBUG_ASSERT(datetime.isValid()) {
        kLogger.warning() << "Failed to parse date/time from string" << value;
        return QDateTime(); // null
    }
    return datetime;
}

/*static*/ QString AoideJsonBase::formatDateTime(QDateTime value) {
    if (value.isNull()) {
        return QString(); // null
    }
    DEBUG_ASSERT(value.isValid());
    if (!value.timeZone().isValid() || (value.timeSpec() == Qt::LocalTime)) {
        // No time zone -> assume UTC
        value.setTimeZone(QTimeZone::utc());
    }
    if ((value.timeSpec() == Qt::LocalTime) || (value.timeSpec() == Qt::TimeZone)) {
        // Convert time zone to offset from UTC
        value = value.toTimeSpec(Qt::OffsetFromUTC);
    }
    if (value.timeZone() == QTimeZone::utc()) {
        // Enforce suffix 'Z' when formatting
        // NOTE(uklotzde, 2018-05-11): Don't remove this code!! Otherwise
        // the suffix might be omitted and tests will fail intentionally.
        value.setTimeSpec(Qt::UTC);
    }
    DEBUG_ASSERT(value.timeZone().isValid());
    DEBUG_ASSERT((value.timeSpec() == Qt::UTC) || (value.timeSpec() == Qt::OffsetFromUTC));
    if ((value.toMSecsSinceEpoch() % 1000) == 0) {
        // Omit milliseconds if zero
        return value.toString(Qt::ISODate);
    } else {
        // Use full precision
        return value.toString(Qt::ISODateWithMs);
    }
}

/*static*/ QString AoideJsonBase::formatUuid(QUuid uuid) {
    if (uuid.isNull()) {
        return QString(); // null
    } else {
        QString formatted = uuid.toString();
        if (formatted.size() == 38) {
            // We need to strip off the heading/trailing curly braces after formatting
            formatted = formatted.mid(1, 36);
        }
        DEBUG_ASSERT(formatted.size() == 36);
        return formatted;
    }
}

/*static*/ QStringList AoideJsonBase::toStringList(QJsonArray jsonArray) {
    QStringList result;
    result.reserve(jsonArray.size());
    for (auto&& jsonValue : qAsConst(jsonArray)) {
        result += jsonValue.toString();
    }
    return result;
}

void AoidePagination::addToQuery(QUrlQuery* query) const {
    DEBUG_ASSERT(query);
    if (offset > 0) {
        query->addQueryItem("offset", QString::number(offset));
    }
    if (limit > 0) {
        query->addQueryItem("limit", QString::number(limit));
    }
}

quint64 AoideEntityRevision::ordinal() const {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    if (m_jsonArray.size() == 2) {
        return m_jsonArray.at(0).toVariant().toULongLong();
    } else {
        return 0;
    }
}

QDateTime AoideEntityRevision::timestamp() const {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    if (m_jsonArray.size() == 2) {
        return QDateTime::fromString(m_jsonArray.at(1).toString(), Qt::ISODateWithMs);
    } else {
        return QDateTime();
    }
}

QString AoideEntityHeader::uid() const {
    return m_jsonObject.value("uid").toString();
}

AoideEntityRevision AoideEntityHeader::revision() const {
    return AoideEntityRevision(m_jsonObject.value("revision").toArray());
}

QString AoideCollection::name() const {
    return m_jsonObject.value("name").toString();
}

void AoideCollection::setName(QString name) {
    putOptionalNonEmpty("name", std::move(name));
}

QString AoideCollection::description() const {
    return m_jsonObject.value("description").toString();
}

void AoideCollection::setDescription(QString description) {
    putOptional("description", std::move(description));
}

AoideEntityHeader AoideCollectionEntity::header() const {
    return AoideEntityHeader(m_jsonObject.value("header").toObject());
}

AoideCollection AoideCollectionEntity::body() const {
    return AoideCollection(m_jsonObject.value("body").toObject());
}

int AoideAudioContent::channelsCount(int defaultCount) const {
    DEBUG_ASSERT(defaultCount >= 0);
    return m_jsonObject.value("channels").toObject().value("count").toInt(defaultCount);
}

void AoideAudioContent::setChannelsCount(int channelsCount) {
    DEBUG_ASSERT(channelsCount >= 0);
    m_jsonObject.insert("channels", QJsonObject{{"count", channelsCount}});
}

double AoideAudioContent::durationMs(double defaultMs) const {
    return m_jsonObject.value("durationMs").toDouble(defaultMs);
}

void AoideAudioContent::setDurationMs(double durationMs) {
    m_jsonObject.insert("durationMs", durationMs);
}

int AoideAudioContent::sampleRateHz(int defaultHz) const {
    DEBUG_ASSERT(defaultHz >= 0);
    return m_jsonObject.value("sampleRateHz").toInt(defaultHz);
}

void AoideAudioContent::setSampleRateHz(int sampleRateHz) {
    DEBUG_ASSERT(sampleRateHz >= 0);
    m_jsonObject.insert("sampleRateHz", sampleRateHz);
}

int AoideAudioContent::bitRateBps(int defaultBps) const {
    DEBUG_ASSERT(defaultBps >= 0);
    return m_jsonObject.value("bitRateBps").toInt(defaultBps);
}

void AoideAudioContent::setBitRateBps(int bitRateBps) {
    DEBUG_ASSERT(bitRateBps >= 0);
    m_jsonObject.insert("bitRateBps", bitRateBps);
}

AoideLoudness AoideAudioContent::loudness() const {
    return AoideLoudness(m_jsonObject.value("loudness").toArray());
}

void AoideAudioContent::setLoudness(AoideLoudness loudness) {
    putOptional("loudness", loudness.intoJsonArray());
}

QJsonObject AoideAudioContent::encoder() const {
    return m_jsonObject.value("encoder").toObject();
}

void AoideAudioContent::setEncoder(QJsonObject encoder) {
    putOptional("encoder", std::move(encoder));
}

/*static*/ const QString AoideTitle::kLevelMain = "main";
/*static*/ const QString AoideTitle::kLevelSub = "sub";
/*static*/ const QString AoideTitle::kLevelWork = "wrk";
/*static*/ const QString AoideTitle::kLevelMovement = "mvn";

QString AoideTitle::level() const {
    return m_jsonObject.value("level").toString(kLevelMain);
}

void AoideTitle::setLevel(QString level) {
    if (level == kLevelMain) {
        m_jsonObject.remove("level");
    } else {
        putOptionalNonEmpty("level", std::move(level));
    }
}

QString AoideTitle::name() const {
    return m_jsonObject.value("name").toString();
}

void AoideTitle::setName(QString name) {
    putOptional("name", std::move(name));
}

QString AoideTitle::language() const {
    return m_jsonObject.value("lang").toString();
}

void AoideTitle::setLanguage(QString language) {
    putOptionalNonEmpty("lang", std::move(language));
}

/*static*/ const QString AoideActor::kPrecedenceSummary = "summary";
/*static*/ const QString AoideActor::kPrecedencePrimary = "primary";
/*static*/ const QString AoideActor::kPrecedenceSecondary = "secondary";

QString AoideActor::precedence() const {
    return m_jsonObject.value("precedence").toString(kPrecedenceSummary);
}

void AoideActor::setPrecedence(QString precedence) {
    if (precedence == kPrecedenceSummary) {
        m_jsonObject.remove("precedence");
    } else {
        putOptionalNonEmpty("precedence", std::move(precedence));
    }
}

/*static*/ const QString AoideActor::kRoleArtist = "artist";
/*static*/ const QString AoideActor::kRoleComposer = "composer";
/*static*/ const QString AoideActor::kRoleConductor = "conductor";
/*static*/ const QString AoideActor::kRoleLyricist = "lyricist";
/*static*/ const QString AoideActor::kRoleRemixer = "remixer";

QString AoideActor::role() const {
    return m_jsonObject.value("role").toString(kRoleArtist);
}

void AoideActor::setRole(QString role) {
    if (role == kRoleArtist) {
        m_jsonObject.remove("role");
    } else {
        putOptionalNonEmpty("role", std::move(role));
    }
}

QString AoideActor::name() const {
    return m_jsonObject.value("name").toString();
}

void AoideActor::setName(QString name) {
    putOptional("name", std::move(name));
}

QStringList AoideActor::externalReferences() const {
    return toStringList(m_jsonObject.value("xrefs").toArray());
}

void AoideActor::setExternalReferences(QStringList references) {
    putOptional("xrefs", QJsonArray::fromStringList(std::move(references)));
}

/*static*/ const QString AoideScoredTag::kFacetLanguage = "lang";
/*static*/ const QString AoideScoredTag::kFacetContentGroup = "cgroup";
/*static*/ const QString AoideScoredTag::kFacetGenre = "genre";
/*static*/ const QString AoideScoredTag::kFacetStyle = "style";
/*static*/ const QString AoideScoredTag::kFacetMood = "mood";
/*static*/ const QString AoideScoredTag::kFacetEpoch = "epoch";
/*static*/ const QString AoideScoredTag::kFacetEvent = "event";
/*static*/ const QString AoideScoredTag::kFacetVenue = "venue";
/*static*/ const QString AoideScoredTag::kFacetCrowd = "crowd";
/*static*/ const QString AoideScoredTag::kFacetSession = "session";
/*static*/ const QString AoideScoredTag::kFacetEquiv = "equiv";

double AoideScoredTag::score(double defaultScore) const {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 3);
    if (m_jsonArray.size() == 3) {
        return m_jsonArray.at(0).toDouble(defaultScore);
    } else {
        return defaultScore;
    }
}

void AoideScoredTag::setScore(double score) {
    DEBUG_ASSERT(score >= 0);
    DEBUG_ASSERT(score <= 1);
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 3);
    if (m_jsonArray.size() == 3) {
        m_jsonArray.replace(0, score);
    } else {
        m_jsonArray = QJsonArray{score, QJsonValue(), QJsonValue()};
    }
}

QString AoideScoredTag::term() const {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 3);
    if (m_jsonArray.size() == 3) {
        return m_jsonArray.at(1).toString();
    } else {
        return QString();
    }
}

void AoideScoredTag::setTerm(QString term) {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 3);
    if (m_jsonArray.size() == 3) {
        m_jsonArray.replace(1, term);
    } else {
        m_jsonArray = QJsonArray{QJsonValue(1), term, QJsonValue()};
    }
}

QString AoideScoredTag::facet() const {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 3);
    if (m_jsonArray.size() == 3) {
        return m_jsonArray.at(2).toString();
    } else {
        return QString();
    }
}

void AoideScoredTag::setFacet(QString facet) {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 3);
    auto value = facet.isEmpty() ? QJsonValue() : QJsonValue(facet);
    if (m_jsonArray.size() == 3) {
        m_jsonArray.replace(2, value);
    } else {
        m_jsonArray = QJsonArray{QJsonValue(1), QJsonValue(), value};
    }
}

QString AoideComment::text() const {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    if (m_jsonArray.size() == 2) {
        return m_jsonArray.at(0).toString();
    } else {
        return QString();
    }
}

void AoideComment::setText(QString text) {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    if (m_jsonArray.size() == 2) {
        m_jsonArray.replace(1, text);
    } else {
        m_jsonArray = QJsonArray{text, QJsonValue()};
    }
}

QString AoideComment::owner() const {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    if (m_jsonArray.size() == 2) {
        return m_jsonArray.at(1).toString();
    } else {
        return QString();
    }
}

void AoideComment::setOwner(QString owner) {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    auto value = owner.isEmpty() ? QJsonValue() : QJsonValue(owner);
    if (m_jsonArray.size() == 2) {
        m_jsonArray.replace(1, value);
    } else {
        m_jsonArray = QJsonArray{QJsonValue(), value};
    }
}

double AoideRating::score(double defaultScore) const {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    if (m_jsonArray.size() == 2) {
        return m_jsonArray.at(0).toDouble(defaultScore);
    } else {
        return defaultScore;
    }
}

void AoideRating::setScore(double score) {
    DEBUG_ASSERT(score >= 0);
    DEBUG_ASSERT(score <= 1);
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    if (m_jsonArray.size() == 2) {
        m_jsonArray.replace(0, score);
    } else {
        m_jsonArray = QJsonArray{score, QJsonValue()};
    }
}

QString AoideRating::owner() const {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    if (m_jsonArray.size() == 2) {
        return m_jsonArray.at(1).toString();
    } else {
        return QString();
    }
}

void AoideRating::setOwner(QString owner) {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    auto value = owner.isEmpty() ? QJsonValue() : QJsonValue(owner);
    if (m_jsonArray.size() == 2) {
        m_jsonArray.replace(1, value);
    } else {
        m_jsonArray = QJsonArray{QJsonValue(), value};
    }
}

double AoideLoudness::ebuR128Lufs(double defaultDb) const {
    for (const auto jsonValue : m_jsonArray) {
        DEBUG_ASSERT(jsonValue.isObject());
        const auto innerValue = jsonValue.toObject().value("ebu-r128-lufs");
        if (innerValue.isDouble()) {
            return innerValue.toDouble(defaultDb);
        }
    }
    return defaultDb;
}

void AoideLoudness::setEbuR128Lufs(double ebuR128Lufs) {
    for (auto i = m_jsonArray.begin(); i != m_jsonArray.end(); ++i) {
        DEBUG_ASSERT(i->isObject());
        auto innerValue = i->toObject().value("ebu-r128-lufs");
        if (innerValue.isDouble()) {
            *i = QJsonObject{{"ebu-r128-lufs", ebuR128Lufs}};
            return;
        }
    }
    m_jsonArray += QJsonObject{{"ebu-r128-lufs", ebuR128Lufs}};
}

/*static*/ const QString AoideScoredSongFeature::kAcousticness = "acousticness";
/*static*/ const QString AoideScoredSongFeature::kDanceability = "danceability";
/*static*/ const QString AoideScoredSongFeature::kEnergy = "energy";
/*static*/ const QString AoideScoredSongFeature::kInstrumentalness = "instrumentalness";
/*static*/ const QString AoideScoredSongFeature::kLiveness = "liveness";
/*static*/ const QString AoideScoredSongFeature::kPopularity = "popularity";
/*static*/ const QString AoideScoredSongFeature::kSpeechiness = "speechiness";
/*static*/ const QString AoideScoredSongFeature::kValence = "valence";

double AoideScoredSongFeature::score(double defaultScore) const {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    if (m_jsonArray.size() == 2) {
        return m_jsonArray.at(0).toDouble(defaultScore);
    } else {
        return defaultScore;
    }
}

void AoideScoredSongFeature::setScore(double score) {
    DEBUG_ASSERT(score >= 0);
    DEBUG_ASSERT(score <= 1);
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    if (m_jsonArray.size() == 2) {
        m_jsonArray.replace(0, score);
    } else {
        m_jsonArray = QJsonArray{score, QJsonValue()};
    }
}

QString AoideScoredSongFeature::feature() const {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    if (m_jsonArray.size() == 2) {
        return m_jsonArray.at(1).toString();
    } else {
        return QString();
    }
}

void AoideScoredSongFeature::setFeature(QString feature) {
    DEBUG_ASSERT(m_jsonArray.isEmpty() || m_jsonArray.size() == 2);
    if (m_jsonArray.size() == 2) {
        m_jsonArray.replace(1, feature);
    } else {
        m_jsonArray = QJsonArray{QJsonValue(), feature};
    }
}

double AoideSongProfile::tempoBpm(double defaultBpm) const {
    return m_jsonObject.value("tempoBpm").toDouble(defaultBpm);
}

void AoideSongProfile::setTempoBpm(double tempoBpm) {
    DEBUG_ASSERT(tempoBpm >= 0);
    m_jsonObject.insert("tempoBpm", tempoBpm);
}

mixxx::track::io::key::ChromaticKey AoideSongProfile::keySignature() const {
    const int keyCode = m_jsonObject.value("keySig").toInt(0);
    DEBUG_ASSERT(keyCode >= 0);
    DEBUG_ASSERT(keyCode <= 24);
    if ((keyCode <= 0) || (keyCode >= 24)) {
        return mixxx::track::io::key::ChromaticKey::INVALID;
    }
    const int openKeyCode = 1 + (keyCode - 1) / 2;
    const bool major = (keyCode % 2) == 1;
    return KeyUtils::openKeyNumberToKey(openKeyCode, major);
}

void AoideSongProfile::setKeySignature(mixxx::track::io::key::ChromaticKey chromaticKey) {
    if (chromaticKey == mixxx::track::io::key::ChromaticKey::INVALID) {
        m_jsonObject.remove("keySig");
    } else {
        const int openKeyNumber = KeyUtils::keyToOpenKeyNumber(chromaticKey);
        DEBUG_ASSERT(openKeyNumber >= 1);
        DEBUG_ASSERT(openKeyNumber <= 12);
        const int keyCode = 2 * openKeyNumber - (KeyUtils::keyIsMajor(chromaticKey) ? 1 : 0);
        m_jsonObject.insert("keySig", keyCode);
    }
}

AoideScoredSongFeature AoideSongProfile::feature(QString feature) const {
    const QJsonArray jsonClassifications = m_jsonObject.value("features").toArray();
    for (auto&& jsonValue : jsonClassifications) {
        auto featureScore = AoideScoredSongFeature(jsonValue.toArray());
        if (featureScore.feature() == feature) {
            return featureScore;
        }
    }
    AoideScoredSongFeature featureScore;
    featureScore.setFeature(std::move(feature));
    featureScore.setScore(0);
    return featureScore;
}

void AoideSongProfile::setFeature(QString feature, double score) {
    DEBUG_ASSERT(score >= 0);
    DEBUG_ASSERT(score <= 1);
    AoideScoredSongFeature featureScore;
    featureScore.setFeature(std::move(feature));
    featureScore.setScore(score);
    replaceFeature(featureScore.feature(), &featureScore);
}

void AoideSongProfile::replaceFeature(
        const QString& feature, AoideScoredSongFeature* featureScore) {
    DEBUG_ASSERT(!feature.isEmpty());
    QJsonArray jsonFeatures = m_jsonObject.value("features").toArray();
    auto i = jsonFeatures.begin();
    while (i != jsonFeatures.end()) {
        auto featureScore = AoideScoredSongFeature(i->toArray());
        if (featureScore.feature() == feature) {
            i = jsonFeatures.erase(i);
        } else {
            ++i;
        }
    }
    if (featureScore) {
        DEBUG_ASSERT(featureScore->feature() == feature);
        jsonFeatures += featureScore->intoJsonArray();
    }
    if (jsonFeatures.isEmpty()) {
        clearFeatures();
    } else {
        putOptional("features", std::move(jsonFeatures));
    }
}

void AoideSongProfile::clearFeatures() {
    m_jsonObject.remove("features");
}

QStringList AoideTrackOrAlbumOrRelease::externalReferences() const {
    return toStringList(m_jsonObject.value("xrefs").toArray());
}

void AoideTrackOrAlbumOrRelease::setExternalReferences(QStringList references) {
    putOptional("xrefs", QJsonArray::fromStringList(std::move(references)));
}

AoideTitleVector AoideTrackOrAlbum::titles(const QString& level) const {
    const auto titleLevel = level.isEmpty() ? AoideTitle::kLevelMain : level;
    AoideTitleVector result;
    const QJsonArray jsonTitles = m_jsonObject.value("titles").toArray();
    for (auto&& jsonValue : jsonTitles) {
        auto title = AoideTitle(jsonValue.toObject());
        if (title.level() == titleLevel) {
            result += std::move(title);
        }
    }
    return result;
}

AoideTitleVector AoideTrackOrAlbum::allTitles() const {
    AoideTitleVector result;
    const QJsonArray jsonTitles = m_jsonObject.value("titles").toArray();
    result.reserve(jsonTitles.size());
    for (auto&& jsonValue : jsonTitles) {
        result += AoideTitle(jsonValue.toObject());
    }
    return result;
}

AoideTitleVector AoideTrackOrAlbum::removeTitles(const QString& level) {
    const auto titleLevel = level.isEmpty() ? AoideTitle::kLevelMain : level;
    AoideTitleVector result;
    QJsonArray jsonTitles = m_jsonObject.value("titles").toArray();
    auto i = jsonTitles.begin();
    while (i != jsonTitles.end()) {
        auto title = AoideTitle(i->toObject());
        if (title.level() == titleLevel) {
            result += std::move(title);
            i = jsonTitles.erase(i);
        } else {
            ++i;
        }
    }
    if (!result.isEmpty()) {
        putOptional("titles", jsonTitles);
    }
    return result;
}

AoideTitleVector AoideTrackOrAlbum::clearTitles() {
    AoideTitleVector result;
    const QJsonArray jsonTitles = m_jsonObject.take("titles").toArray();
    result.reserve(jsonTitles.size());
    for (const auto&& jsonValue : jsonTitles) {
        result += AoideTitle(jsonValue.toObject());
    }
    return result;
}

void AoideTrackOrAlbum::addTitles(AoideTitleVector titles) {
    if (titles.isEmpty()) {
        // Avoid any modifications if noop
        return;
    }
    QJsonArray jsonTitles = m_jsonObject.take("titles").toArray();
    for (auto&& title : titles) {
        jsonTitles += title.intoJsonObject();
    }
    putOptional("titles", std::move(jsonTitles));
}

AoideActorVector AoideTrackOrAlbum::actors(const QString& role, const QString& precedence) const {
    const auto actorRole = role.isEmpty() ? AoideActor::kRoleArtist : role;
    const auto actorPrecedence = precedence.isEmpty() ? AoideActor::kPrecedenceSummary : precedence;
    AoideActorVector result;
    const QJsonArray jsonActors = m_jsonObject.value("actors").toArray();
    for (auto&& jsonValue : jsonActors) {
        auto actor = AoideActor(jsonValue.toObject());
        if ((actor.role() == actorRole) && (actor.precedence() == actorPrecedence)) {
            result += std::move(actor);
        }
    }
    return result;
}

AoideActorVector AoideTrackOrAlbum::allActors() const {
    AoideActorVector result;
    const QJsonArray jsonActors = m_jsonObject.value("actors").toArray();
    result.reserve(jsonActors.size());
    for (auto&& jsonValue : jsonActors) {
        result += AoideActor(jsonValue.toObject());
    }
    return result;
}

AoideActorVector AoideTrackOrAlbum::removeActors(const QString& role) {
    const auto actorRole = role.isEmpty() ? AoideActor::kRoleArtist : role;
    AoideActorVector result;
    QJsonArray jsonActors = m_jsonObject.value("actors").toArray();
    auto i = jsonActors.begin();
    while (i != jsonActors.end()) {
        auto actor = AoideActor(i->toObject());
        if (actor.role() == actorRole) {
            result += std::move(actor);
            i = jsonActors.erase(i);
        } else {
            ++i;
        }
    }
    if (!result.isEmpty()) {
        putOptional("actors", jsonActors);
    }
    return result;
}

AoideActorVector AoideTrackOrAlbum::clearActors() {
    AoideActorVector result;
    const QJsonArray jsonActors = m_jsonObject.take("actors").toArray();
    result.reserve(jsonActors.size());
    for (const auto&& jsonValue : jsonActors) {
        result += AoideActor(jsonValue.toObject());
    }
    return result;
}

void AoideTrackOrAlbum::addActors(AoideActorVector actors) {
    if (actors.isEmpty()) {
        // Avoid any modifications if noop
        return;
    }
    QJsonArray jsonActors = m_jsonObject.take("actors").toArray();
    for (auto&& actor : actors) {
        jsonActors += actor.intoJsonObject();
    }
    putOptional("actors", std::move(jsonActors));
}

bool AoideAlbum::compilation(bool defaultValue) const {
    return m_jsonObject.value("compilation").toBool(defaultValue);
}

void AoideAlbum::setCompilation(bool compilation) {
    m_jsonObject.insert("compilation", compilation);
}

void AoideAlbum::resetCompilation() {
    m_jsonObject.remove("compilation");
}

QDateTime AoideRelease::releasedAt() const {
    return QDateTime::fromString(m_jsonObject.value("releasedAt").toString(), Qt::ISODateWithMs);
}

void AoideRelease::setReleasedAt(QDateTime released) {
    putOptional("releasedAt", formatDateTime(std::move(released)));
}

QString AoideRelease::releasedBy() const {
    return m_jsonObject.value("releasedBy").toString();
}

void AoideRelease::setReleasedBy(QString label) {
    putOptional("releasedBy", std::move(label));
}

QString AoideRelease::copyright() const {
    return m_jsonObject.value("copyright").toString();
}

void AoideRelease::setCopyright(QString copyright) {
    putOptional("copyright", std::move(copyright));
}

QStringList AoideRelease::licenses() const {
    return toStringList(m_jsonObject.value("licenses").toArray());
}

void AoideRelease::setLicenses(QStringList licenses) {
    putOptional("licenses", QJsonArray::fromStringList(std::move(licenses)));
}

QString AoideTrack::uri(const QString& collectionUid) const {
    DEBUG_ASSERT(!collectionUid.isEmpty());
    const QJsonArray resources = m_jsonObject.value("resources").toArray();
    for (const auto& jsonValue : resources) {
        const QJsonObject resource = jsonValue.toObject();
        if (resource.value("collection").toObject().value("uid") == collectionUid) {
            return resource.value("source").toObject().value("uri").toString();
        }
    }
    DEBUG_ASSERT(resources.isEmpty());
    return QString();
}

AoideRelease AoideTrack::release() const {
    return AoideRelease(m_jsonObject.value("release").toObject());
}

void AoideTrack::setRelease(AoideRelease release) {
    putOptional("release", release.intoJsonObject());
}

AoideAlbum AoideTrack::album() const {
    return AoideAlbum(m_jsonObject.value("album").toObject());
}

void AoideTrack::setAlbum(AoideAlbum album) {
    putOptional("album", album.intoJsonObject());
}

AoideSongProfile AoideTrack::profile() const {
    return AoideSongProfile(m_jsonObject.value("profile").toObject());
}

void AoideTrack::setProfile(AoideSongProfile profile) {
    putOptional("profile", profile.intoJsonObject());
}

AoideScoredTagVector AoideTrack::tags(const QString& facet) const {
    const auto tagFacet = facet.isEmpty() ? QString() : facet;
    AoideScoredTagVector result;
    const QJsonArray jsonTags = m_jsonObject.value("tags").toArray();
    for (auto&& jsonValue : jsonTags) {
        auto tag = AoideScoredTag(jsonValue.toArray());
        if (tag.facet() == tagFacet) {
            result += std::move(tag);
        }
    }
    return result;
}

AoideScoredTagVector AoideTrack::allTags() const {
    AoideScoredTagVector result;
    const QJsonArray jsonTags = m_jsonObject.value("tags").toArray();
    result.reserve(jsonTags.size());
    for (auto&& jsonValue : jsonTags) {
        result += AoideScoredTag(jsonValue.toArray());
    }
    return result;
}

AoideScoredTagVector AoideTrack::removeTags(const QString& facet) {
    const auto tagFacet = facet.isEmpty() ? QString() : facet;
    AoideScoredTagVector result;
    QJsonArray jsonTags = m_jsonObject.value("tags").toArray();
    auto i = jsonTags.begin();
    while (i != jsonTags.end()) {
        auto tag = AoideScoredTag(i->toArray());
        if (tag.facet() == tagFacet) {
            result += std::move(tag);
            i = jsonTags.erase(i);
        } else {
            ++i;
        }
    }
    if (!result.isEmpty()) {
        putOptional("tags", jsonTags);
    }
    return result;
}

AoideScoredTagVector AoideTrack::clearTags() {
    AoideScoredTagVector result;
    const QJsonArray jsonTags = m_jsonObject.take("tags").toArray();
    result.reserve(jsonTags.size());
    for (const auto&& jsonValue : jsonTags) {
        result += AoideScoredTag(jsonValue.toArray());
    }
    return result;
}

void AoideTrack::addTags(AoideScoredTagVector tags) {
    if (tags.isEmpty()) {
        // Avoid any modifications if noop
        return;
    }
    QJsonArray jsonTags = m_jsonObject.take("tags").toArray();
    for (auto&& tag : tags) {
        jsonTags += tag.intoJsonArray();
    }
    putOptional("tags", std::move(jsonTags));
}

AoideCommentVector AoideTrack::comments(const QString& owner) const {
    const auto commentOwner = owner.isEmpty() ? QString() : owner;
    AoideCommentVector result;
    const QJsonArray jsonComments = m_jsonObject.value("comments").toArray();
    for (auto&& jsonValue : jsonComments) {
        auto comment = AoideComment(jsonValue.toArray());
        if (comment.owner() == commentOwner) {
            result += std::move(comment);
        }
    }
    return result;
}

AoideCommentVector AoideTrack::allComments() const {
    AoideCommentVector result;
    const QJsonArray jsonComments = m_jsonObject.value("comments").toArray();
    result.reserve(jsonComments.size());
    for (auto&& jsonValue : jsonComments) {
        result += AoideComment(jsonValue.toArray());
    }
    return result;
}

AoideCommentVector AoideTrack::removeComments(const QString& owner) {
    const auto commentOwner = owner.isEmpty() ? QString() : owner;
    AoideCommentVector result;
    QJsonArray jsonComments = m_jsonObject.value("comments").toArray();
    auto i = jsonComments.begin();
    while (i != jsonComments.end()) {
        auto comment = AoideComment(i->toArray());
        if (comment.owner() == commentOwner) {
            result += std::move(comment);
            i = jsonComments.erase(i);
        } else {
            ++i;
        }
    }
    if (!result.isEmpty()) {
        putOptional("comments", jsonComments);
    }
    return result;
}

AoideCommentVector AoideTrack::clearComments() {
    AoideCommentVector result;
    const QJsonArray jsonComments = m_jsonObject.take("comments").toArray();
    result.reserve(jsonComments.size());
    for (const auto&& jsonValue : jsonComments) {
        result += AoideComment(jsonValue.toArray());
    }
    return result;
}

void AoideTrack::addComments(AoideCommentVector comments) {
    if (comments.isEmpty()) {
        // Avoid any modifications if noop
        return;
    }
    QJsonArray jsonComments = m_jsonObject.take("comments").toArray();
    for (auto&& comment : comments) {
        jsonComments += comment.intoJsonArray();
    }
    putOptional("comments", std::move(jsonComments));
}

AoideRatingVector AoideTrack::ratings(const QString& owner) const {
    const auto ratingOwner = owner.isEmpty() ? QString() : owner;
    AoideRatingVector result;
    const QJsonArray jsonRatings = m_jsonObject.value("ratings").toArray();
    for (auto&& jsonValue : jsonRatings) {
        auto rating = AoideRating(jsonValue.toArray());
        if (rating.owner() == ratingOwner) {
            result += std::move(rating);
        }
    }
    return result;
}

AoideRatingVector AoideTrack::allRatings() const {
    AoideRatingVector result;
    const QJsonArray jsonRatings = m_jsonObject.value("ratings").toArray();
    result.reserve(jsonRatings.size());
    for (auto&& jsonValue : jsonRatings) {
        result += AoideRating(jsonValue.toArray());
    }
    return result;
}

AoideRatingVector AoideTrack::removeRatings(const QString& owner) {
    const auto ratingOwner = owner.isEmpty() ? QString() : owner;
    AoideRatingVector result;
    QJsonArray jsonRatings = m_jsonObject.value("ratings").toArray();
    auto i = jsonRatings.begin();
    while (i != jsonRatings.end()) {
        auto rating = AoideRating(i->toArray());
        if (rating.owner() == ratingOwner) {
            result += std::move(rating);
            i = jsonRatings.erase(i);
        } else {
            ++i;
        }
    }
    if (!result.isEmpty()) {
        putOptional("ratings", jsonRatings);
    }
    return result;
}

AoideRatingVector AoideTrack::clearRatings() {
    AoideRatingVector result;
    const QJsonArray jsonRatings = m_jsonObject.take("ratings").toArray();
    result.reserve(jsonRatings.size());
    for (const auto&& jsonValue : jsonRatings) {
        result += AoideRating(jsonValue.toArray());
    }
    return result;
}

void AoideTrack::addRatings(AoideRatingVector ratings) {
    if (ratings.isEmpty()) {
        // Avoid any modifications if noop
        return;
    }
    QJsonArray jsonRatings = m_jsonObject.take("ratings").toArray();
    for (auto&& rating : ratings) {
        jsonRatings += rating.intoJsonArray();
    }
    putOptional("ratings", std::move(jsonRatings));
}

AoideEntityHeader AoideTrackEntity::header() const {
    return AoideEntityHeader(m_jsonObject.value("header").toObject());
}

AoideTrack AoideTrackEntity::body() const {
    return AoideTrack(m_jsonObject.value("body").toObject());
}

void AoideTrackEntity::setBody(AoideTrack body) {
    putOptional("body", body);
}

QString AoideScoredTagFacetCount::facet() const {
    return m_jsonObject.value("facet").toString();
}

quint64 AoideScoredTagFacetCount::count() const {
    return m_jsonObject.value("count").toVariant().toULongLong();
}

AoideScoredTag AoideScoredTagCount::tag() const {
    return AoideScoredTag(m_jsonObject.value("tag").toArray());
}

quint64 AoideScoredTagCount::count() const {
    return m_jsonObject.value("count").toVariant().toULongLong();
}

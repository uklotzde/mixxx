#include "aoide/translator.h"

#include <QMimeDatabase>
#include <QTextStream>

#include <sstream>

#include "library/starrating.h"
#include "track/track.h"
#include "util/logger.h"
#include "util/math.h"

namespace mixxx {

namespace aoide {

namespace {

const Logger kLogger("aoide Translator");

inline void insertOptional(QJsonObject* jsonObject, const QString& key, const QString& value) {
    if (!value.isEmpty()) {
        jsonObject->insert(key, value);
    }
}

inline void insertColor(QJsonObject* jsonObject, const QString& key, const QColor& color) {
    QString colorCode;
    QTextStream ts(&colorCode);
    ts << '#' << noshowbase << uppercasedigits << qSetFieldWidth(8) << qSetPadChar('0') << right
        << hex << color.rgba();
    jsonObject->insert(key, colorCode);
}

QJsonValue optionalPositiveIntJsonValue(const QString& value) {
    bool valid = false;
    int intValue = value.toInt(&valid);
    if (valid && (intValue > 0)) {
        return QJsonValue(intValue);
    } else {
        return QJsonValue();
    }
}

inline void insertOptional(QJsonObject* jsonObject, const QString& key, const QJsonArray& value) {
    if (!value.isEmpty()) {
        jsonObject->insert(key, value);
    }
}

const QString ISRC_REF_PREFIX = "isrc:";
const QString MBID_REF_PREFIX = "http://musicbrainz.org/";
const QString MBID_ARTIST_REF_PREFIX = MBID_REF_PREFIX + "artist/";
const QString MBID_RECORDING_REF_PREFIX = MBID_REF_PREFIX + "recording/";
const QString MBID_TRACK_REF_PREFIX = MBID_REF_PREFIX + "track/";
const QString MBID_WORK_REF_PREFIX = MBID_REF_PREFIX + "work/";
const QString MBID_RELEASE_REF_PREFIX = MBID_REF_PREFIX + "release/";
const QString MBID_RELEASE_GROUP_REF_PREFIX = MBID_REF_PREFIX + "release-group/";

const QString kCommentOwner = QString(); // anonymous

const QString kRatingOwner = QString(); // anonymous or "mixxx.org"?

inline void appendReference(
        QStringList* references, const QString& reference, const QString& prefix = QString()) {
    if (!reference.isEmpty()) {
        *references += prefix + reference;
    }
}

inline void appendReference(
        QStringList* references,
        const QUuid& uuid, // e.g. MusicBrainz MBIDs
        const QString& prefix = QString()) {
    appendReference(references, AoideJsonObject::formatUuid(uuid), prefix);
}

void appendTitle(AoideTitleVector* titles, QString name, QString level = QString()) {
    if (!name.isNull()) {
        AoideTitle title;
        title.setName(std::move(name));
        title.setLevel(std::move(level));
        *titles += std::move(title);
    }
}

void appendActor(AoideActorVector* actors, QString role, QString name, QUuid mbrainzId = QUuid()) {
    if (!name.isNull()) {
        AoideActor actor;
        actor.setRole(std::move(role));
        actor.setName(std::move(name));
        QStringList refs;
        appendReference(&refs, std::move(mbrainzId), MBID_ARTIST_REF_PREFIX);
        actor.setExternalReferences(std::move(refs));
        *actors += std::move(actor);
    }
}

} // anonymous namespace

Translator::Translator(QString collectionUid) : m_collectionUid(std::move(collectionUid)) {
    DEBUG_ASSERT(!m_collectionUid.isEmpty());
}

AoideTrack Translator::exportTrack(const Track& track) const {
    TrackRecord trackRecord;
    track.getTrackRecord(&trackRecord);
    TrackMetadata trackMetadata;
    track.getTrackMetadata(&trackMetadata);
    const TrackInfo& trackInfo = trackMetadata.getTrackInfo();
    const AlbumInfo& albumInfo = trackMetadata.getAlbumInfo();

    // TODO: Successively extend the domain model to avoid plain
    // JSON access

    QJsonObject trackCollection{
            {"uid", m_collectionUid},
    };
    if (trackRecord.getDateAdded().isValid()) {
        trackCollection.insert(
                "since", AoideJsonObject::formatDateTime(trackRecord.getDateAdded()));
    }

    AoideAudioContent audioContent;
    audioContent.setDurationMs(track.getDuration() * 1000);
    audioContent.setChannelsCount(track.getChannels());
    audioContent.setSampleRateHz(track.getSampleRate());
    audioContent.setBitRateBps(track.getBitrate() * 1000);
    if (trackInfo.getReplayGain().hasRatio()) {
        // Assumption: Gain has been calculated with the new EBU R128 algorithm
        AoideLoudness loudness;
        loudness.setEbuR128LufsDb(ratio2db(trackInfo.getReplayGain().getRatio()));
        audioContent.setLoudness(std::move(loudness));
    }

    QJsonObject audioEncoder;
    insertOptional(&audioEncoder, "name", trackInfo.getEncoder());
    insertOptional(&audioEncoder, "settings", trackInfo.getEncoderSettings());
    audioContent.setEncoder(std::move(audioEncoder));

    QJsonObject trackSource{
            {"uri", track.getLocationUri()},
            {"mediaType", QMimeDatabase().mimeTypeForFile(track.getLocation()).name()},
            {"audioContent", audioContent.intoJsonObject()},
    };

    QJsonArray trackResources{QJsonObject{
            {"collection", std::move(trackCollection)},
            {"source", std::move(trackSource)},
            {"playCount", trackRecord.getPlayCounter().getTimesPlayed()},
    }};

    QJsonObject jsonTrack{
            {"resources", std::move(trackResources)},
    };

    QJsonArray trackNumbers{
            optionalPositiveIntJsonValue(trackInfo.getTrackNumber()),
            optionalPositiveIntJsonValue(trackInfo.getTrackTotal()),
    };
    insertOptional(&jsonTrack, "trackNumbers", std::move(trackNumbers));

    QJsonArray discNumbers{
            optionalPositiveIntJsonValue(trackInfo.getDiscNumber()),
            optionalPositiveIntJsonValue(trackInfo.getDiscTotal()),
    };
    insertOptional(&jsonTrack, "discNumbers", std::move(discNumbers));

    QJsonArray trackMarkers;
    bool hasLoadCue = false;
    const auto scaleCueToMillis = 1000.0 / (track.getSampleRate() * track.getChannels());
    const auto cuePoints = track.getCuePoints();
    for (const auto& cuePoint : cuePoints) {
        QString mark;
        double position;
        double length;
        switch (cuePoint->getType()) {
            case Cue::LOAD:
                if (hasLoadCue) {
                    kLogger.warning() << "Multiple load cues detected - skipping";
                    continue;
                }
                // Use the position of the main cue point from the track for
                // the singular load cue. The first load cue only provides
                // additional metadata like a label and a color.
                position = trackRecord.getCuePoint();
                length = 0;
                if (position != cuePoint->getPosition()) {
                    std::ostringstream oss;
                    oss.precision(std::numeric_limits<double>::max_digits10);
                    oss << "Load cue differs from track cue position:"
                        << " expected = " << position << ", actual = " << cuePoint->getPosition();
                    kLogger.warning() << oss.str().c_str();
                }
                mark = "load-cue";
                hasLoadCue = true;
                break;
            case Cue::CUE:
                position = cuePoint->getPosition();
                length = 0;
                mark = "hot-cue";
                break;
            case Cue::LOOP:
                position = cuePoint->getPosition();
                length = cuePoint->getLength();
                mark = "loop";
                break;
            default:
                kLogger.debug() << "Ignoring cue point of type" << cuePoint->getType();
                continue;
        }
        QJsonObject trackMarker;
        trackMarker.insert("mark", mark);
        trackMarker.insert("offset", QJsonObject{{ "ms", position * scaleCueToMillis}});
        if (length > 0) {
            trackMarker.insert("length", QJsonObject{{ "ms", length * scaleCueToMillis}});
        }
        if (cuePoint->getHotCue() >= 0) {
            trackMarker.insert("number", cuePoint->getHotCue());
        }
        insertOptional(&trackMarker, "label", cuePoint->getLabel());
        insertColor(&trackMarker, "color", cuePoint->getColor());
        trackMarkers += std::move(trackMarker);
    }
    if (!hasLoadCue && (trackRecord.getCuePoint() > 0.0)) {
        // Don't rely solely on load cues that might be absent
        QJsonObject trackMarker;
        trackMarker.insert("mark", "load-cue");
        trackMarker.insert("offsetMs", trackRecord.getCuePoint() * scaleCueToMillis);
        trackMarkers += std::move(trackMarker);
    }
    insertOptional(&jsonTrack, "markers", std::move(trackMarkers));

    QJsonArray trackLocks;
    if (trackRecord.getBpmLocked()) {
        trackLocks += "tempo";
    }
    insertOptional(&jsonTrack, "locks", std::move(trackLocks));

    // ...now continue with the export by building the JSON object
    // using type safe domain model wrappers (see TODO above).
    AoideTrack aoideTrack(std::move(jsonTrack));

    // Track Titles
    AoideTitleVector trackTitles;
    appendTitle(&trackTitles, trackInfo.getTitle());
    appendTitle(&trackTitles, trackInfo.getSubtitle(), AoideTitle::kLevelSub);
    appendTitle(&trackTitles, trackInfo.getWork(), AoideTitle::kLevelWork);
    appendTitle(&trackTitles, trackInfo.getMovement(), AoideTitle::kLevelMovement);
    aoideTrack.addTitles(std::move(trackTitles));

    // Track Actors
    AoideActorVector trackActors;
    appendActor(
            &trackActors, AoideActor::kRoleArtist, trackInfo.getArtist(),
            trackInfo.getMusicBrainzArtistId());
    appendActor(&trackActors, AoideActor::kRoleComposer, trackInfo.getComposer());
    appendActor(&trackActors, AoideActor::kRoleConductor, trackInfo.getConductor());
    appendActor(&trackActors, AoideActor::kRoleLyricist, trackInfo.getLyricist());
    appendActor(&trackActors, AoideActor::kRoleRemixer, trackInfo.getRemixer());
    aoideTrack.addActors(std::move(trackActors));

    // Track Refs
    QStringList trackRefs;
    appendReference(&trackRefs, trackInfo.getISRC(), ISRC_REF_PREFIX);
    appendReference(&trackRefs, trackInfo.getMusicBrainzRecordingId(), MBID_RECORDING_REF_PREFIX);
    appendReference(&trackRefs, trackInfo.getMusicBrainzReleaseId(), MBID_TRACK_REF_PREFIX);
    appendReference(&trackRefs, trackInfo.getMusicBrainzWorkId(), MBID_WORK_REF_PREFIX);
    aoideTrack.setExternalReferences(std::move(trackRefs));

    // Release {
    AoideRelease aoideRelease = aoideTrack.release();

    aoideRelease.setReleasedAt(AoideJsonObject::parseDateTimeOrYear(trackInfo.getYear()));
    aoideRelease.setReleasedBy(albumInfo.getRecordLabel());
    aoideRelease.setCopyright(albumInfo.getCopyright());
    if (!albumInfo.getLicense().isEmpty()) {
        aoideRelease.setLicenses({albumInfo.getLicense()});
    }

    QStringList releaseRefs;
    appendReference(&releaseRefs, albumInfo.getMusicBrainzReleaseId(), MBID_RELEASE_REF_PREFIX);
    aoideRelease.setExternalReferences(std::move(releaseRefs));
    aoideTrack.setRelease(std::move(aoideRelease));
    // } Release

    // Album {
    AoideAlbum aoideAlbum = aoideTrack.album();

    // Album Titles
    AoideTitleVector albumTitles;
    appendTitle(&albumTitles, albumInfo.getTitle());
    aoideAlbum.addTitles(std::move(albumTitles));

    // Album Actors
    AoideActorVector albumActors;
    appendActor(
            &albumActors, AoideActor::kRoleArtist, albumInfo.getArtist(),
            albumInfo.getMusicBrainzArtistId());
    aoideAlbum.addActors(std::move(albumActors));

    // Album Refs
    QStringList albumRefs;
    appendReference(
            &albumRefs, albumInfo.getMusicBrainzReleaseGroupId(), MBID_RELEASE_GROUP_REF_PREFIX);
    aoideAlbum.setExternalReferences(std::move(albumRefs));

    aoideTrack.setAlbum(std::move(aoideAlbum));
    // } Album

    // Song Profile {
    AoideSongProfile aoideProfile = aoideTrack.profile();

    if (trackInfo.getBpm().hasValue()) {
        aoideProfile.setTempoBpm(trackInfo.getBpm().getValue());
    }
    aoideProfile.setKeySignature(trackRecord.getGlobalKey());

    aoideTrack.setProfile(std::move(aoideProfile));
    // } Song Profile

    // Tags
    if (!trackInfo.getGenre().isNull()) {
        AoideScoredTag tag;
        tag.setTerm(std::move(trackInfo.getGenre()));
        tag.setFacet(AoideScoredTag::kFacetGenre);
        aoideTrack.addTags({std::move(tag)});
    }
    if (!trackInfo.getLanguage().isNull()) {
        AoideScoredTag tag;
        tag.setTerm(trackInfo.getLanguage());
        tag.setFacet(AoideScoredTag::kFacetLanguage);
        aoideTrack.addTags({std::move(tag)});
    }
    if (!trackInfo.getGrouping().isNull()) {
        AoideScoredTag tag;
        tag.setTerm(trackInfo.getGrouping());
        tag.setFacet(AoideScoredTag::kFacetContentGroup);
        aoideTrack.addTags({std::move(tag)});
    }
    if (!trackInfo.getMood().isNull()) {
        AoideScoredTag tag;
        tag.setTerm(trackInfo.getMood());
        tag.setFacet(AoideScoredTag::kFacetMood);
        aoideTrack.addTags({std::move(tag)});
    }

    // Comment(s)
    if (!trackInfo.getComment().isNull()) {
        AoideComment comment;
        comment.setText(trackInfo.getComment());
        comment.setOwner(kCommentOwner);
        aoideTrack.addComments({std::move(comment)});
    }

    // Rating(s)
    if (trackRecord.hasRating()) {
        const StarRating starRating(trackRecord.getRating());
        AoideRating rating;
        double score = math_min(starRating.starCount(), starRating.maxStarCount()) /
                static_cast<double>(starRating.maxStarCount());
        rating.setScore(score);
        rating.setOwner(kRatingOwner);
        aoideTrack.addRatings({std::move(rating)});
    }

    return aoideTrack;
}

} // namespace aoide

} // namespace mixxx

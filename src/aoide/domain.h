#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>
#include <QUrlQuery>
#include <QUuid>

#include "proto/keys.pb.h"

///////////////////////////////////////////////////////////////////////////////
// Thin and efficient wrappers around QJsonObject with some accessors and
// only limited editing functionality.
//
// Signal types should to be placed in the global namespace! Handling of
// meta types in namespaces is hardly documented may not work as expected.
///////////////////////////////////////////////////////////////////////////////

struct AoidePagination {
    quint64 offset = 0;
    quint64 limit = 0;

    void addToQuery(QUrlQuery* query) const;
};

Q_DECLARE_METATYPE(AoidePagination);

class AoideJsonBase {
  public:
    virtual ~AoideJsonBase() {
    }

    static QDateTime parseDateTimeOrYear(QString value);
    static QString formatDateTime(QDateTime value);

    static QString formatUuid(QUuid uuid);

    static QStringList toStringList(QJsonArray jsonArray);
};

Q_DECLARE_METATYPE(AoideJsonBase);

class AoideJsonObject : public AoideJsonBase {
  public:
    explicit AoideJsonObject(QJsonObject jsonObject = QJsonObject())
        : m_jsonObject(std::move(jsonObject)) {
    }

    operator const QJsonObject&() const {
        return m_jsonObject;
    }

    QJsonObject intoJsonObject() {
        return std::move(m_jsonObject);
    }

  protected:
    void putOptional(QString key, QString value);
    void putOptionalNonEmpty(QString key, QString value);
    void putOptional(QString key, QJsonObject object);
    void putOptional(QString key, QJsonArray array);

    QJsonObject m_jsonObject;
};

Q_DECLARE_METATYPE(AoideJsonObject);

class AoideJsonArray : public AoideJsonBase {
  public:
    explicit AoideJsonArray(QJsonArray jsonArray = QJsonArray())
        : m_jsonArray(std::move(jsonArray)) {
    }

    operator const QJsonArray&() const {
        return m_jsonArray;
    }

    QJsonArray intoJsonArray() {
        return std::move(m_jsonArray);
    }

  protected:
    QJsonArray m_jsonArray;
};

Q_DECLARE_METATYPE(AoideJsonArray);

class AoideEntityRevision : public AoideJsonArray {
  public:
    explicit AoideEntityRevision(QJsonArray jsonArray = QJsonArray())
        : AoideJsonArray(std::move(jsonArray)) {
    }

    quint64 ordinal() const;

    QDateTime timestamp() const;
};

Q_DECLARE_METATYPE(AoideEntityRevision);

class AoideEntityHeader : public AoideJsonObject {
  public:
    explicit AoideEntityHeader(QJsonObject jsonObject = QJsonObject())
        : AoideJsonObject(std::move(jsonObject)) {
    }

    QString uid() const;

    AoideEntityRevision revision() const;
};

Q_DECLARE_METATYPE(AoideEntityHeader);

class AoideCollection : public AoideJsonObject {
  public:
    explicit AoideCollection(QJsonObject jsonObject = QJsonObject())
        : AoideJsonObject(std::move(jsonObject)) {
    }

    QString name() const;
    void setName(QString name = QString());

    QString description() const;
    void setDescription(QString description = QString());
};

Q_DECLARE_METATYPE(AoideCollection);

class AoideCollectionEntity : public AoideJsonObject {
  public:
    explicit AoideCollectionEntity(QJsonObject jsonObject = QJsonObject())
        : AoideJsonObject(std::move(jsonObject)) {
    }

    AoideEntityHeader header() const;

    AoideCollection body() const;
};

Q_DECLARE_METATYPE(AoideCollectionEntity);

class AoideLoudness : public AoideJsonArray {
  public:
    explicit AoideLoudness(QJsonArray jsonArray = QJsonArray())
        : AoideJsonArray(std::move(jsonArray)) {
    }

    double ebuR128LufsDb(double defaultDb = 0) const;
    void setEbuR128LufsDb(double ebuR128LufsDb);
};

Q_DECLARE_METATYPE(AoideLoudness);

class AoideAudioContent : public AoideJsonObject {
  public:
    explicit AoideAudioContent(QJsonObject jsonObject = QJsonObject())
        : AoideJsonObject(std::move(jsonObject)) {
    }

    double durationMs(double defaultMs = 0) const;
    void setDurationMs(double durationMs);

    int channelsCount(int defaultCount = 0) const;
    void setChannelsCount(int channelsCount);

    int sampleRateHz(int defaultHz = 0) const;
    void setSampleRateHz(int sampleRateHz);

    int bitRateBps(int defaultBps = 0) const;
    void setBitRateBps(int bitRateBps);

    AoideLoudness loudness() const;
    void setLoudness(AoideLoudness loudness = AoideLoudness());

    QJsonObject encoder() const;
    void setEncoder(QJsonObject encoder);
};

Q_DECLARE_METATYPE(AoideAudioContent);

class AoideTitle : public AoideJsonObject {
  public:
    explicit AoideTitle(QJsonObject jsonObject = QJsonObject())
        : AoideJsonObject(std::move(jsonObject)) {
    }

    static const QString kLevelMain; // default if none specified
    static const QString kLevelSub;
    static const QString kLevelWork;
    static const QString kLevelMovement;

    QString level() const;
    void setLevel(QString level = QString());

    QString name() const;
    void setName(QString name = QString());

    QString language() const;
    void setLanguage(QString language = QString());
};

Q_DECLARE_METATYPE(AoideTitle);

typedef QVector<AoideTitle> AoideTitleVector;

Q_DECLARE_METATYPE(AoideTitleVector);

class AoideActor : public AoideJsonObject {
  public:
    explicit AoideActor(QJsonObject jsonObject = QJsonObject())
        : AoideJsonObject(std::move(jsonObject)) {
    }

    static const QString kRoleArtist; // default if none specified
    static const QString kRoleComposer;
    static const QString kRoleConductor;
    static const QString kRoleLyricist;
    static const QString kRoleRemixer;

    QString role() const;
    void setRole(QString role);

    static const QString kPrecedenceSummary; // default if none specified
    static const QString kPrecedencePrimary;
    static const QString kPrecedenceSecondary;

    QString precedence() const;
    void setPrecedence(QString precedence = QString());

    QString name() const;
    void setName(QString name = QString());

    QStringList externalReferences() const;
    void setExternalReferences(QStringList references = QStringList());
};

Q_DECLARE_METATYPE(AoideActor);

typedef QVector<AoideActor> AoideActorVector;

Q_DECLARE_METATYPE(AoideActorVector);

class AoideScoredTag : public AoideJsonArray {
  public:
    static const QString kFacetLanguage;
    static const QString kFacetContentGroup; // aka "Grouping"
    static const QString kFacetGenre;
    static const QString kFacetStyle;
    static const QString kFacetMood;
    static const QString kFacetEpoch;
    static const QString kFacetEvent;
    static const QString kFacetVenue;
    static const QString kFacetCrowd;
    static const QString kFacetSession;
    static const QString kFacetEquiv;

    explicit AoideScoredTag(QJsonArray jsonArray = QJsonArray())
        : AoideJsonArray(std::move(jsonArray)) {
    }

    double score(double defaultScore = 0) const;
    void setScore(double score);

    QString term() const;
    void setTerm(QString term);

    QString facet() const;
    void setFacet(QString facet = QString());
};

Q_DECLARE_METATYPE(AoideScoredTag);

typedef QVector<AoideScoredTag> AoideScoredTagVector;

Q_DECLARE_METATYPE(AoideScoredTagVector);

class AoideComment : public AoideJsonArray {
  public:
    explicit AoideComment(QJsonArray jsonArray = QJsonArray())
        : AoideJsonArray(std::move(jsonArray)) {
    }

    QString text() const;
    void setText(QString text = QString());

    QString owner() const;
    void setOwner(QString owner = QString());
};

Q_DECLARE_METATYPE(AoideComment);

typedef QVector<AoideComment> AoideCommentVector;

Q_DECLARE_METATYPE(AoideCommentVector);

class AoideRating : public AoideJsonArray {
  public:
    explicit AoideRating(QJsonArray jsonArray = QJsonArray())
        : AoideJsonArray(std::move(jsonArray)) {
    }

    double score(double defaultScore = 0) const;
    void setScore(double score);

    QString owner() const;
    void setOwner(QString owner = QString());
};

Q_DECLARE_METATYPE(AoideRating);

typedef QVector<AoideRating> AoideRatingVector;

Q_DECLARE_METATYPE(AoideRatingVector);

class AoideScoredSongFeature : public AoideJsonArray {
  public:
    explicit AoideScoredSongFeature(QJsonArray jsonArray = QJsonArray())
        : AoideJsonArray(std::move(jsonArray)) {
    }

    static const QString kAcousticness;
    static const QString kDanceability;
    static const QString kEnergy;
    static const QString kInstrumentalness;
    static const QString kLiveness;
    static const QString kPopularity;
    static const QString kSpeechiness;
    static const QString kValence;

    double score(double defaultScore = 0) const;
    void setScore(double score);

    QString feature() const;
    void setFeature(QString feature = QString());
};

Q_DECLARE_METATYPE(AoideScoredSongFeature);

typedef QVector<AoideScoredSongFeature> AoideScoredSongFeatureVector;

Q_DECLARE_METATYPE(AoideScoredSongFeatureVector);

class AoideSongProfile : public AoideJsonObject {
  public:
    explicit AoideSongProfile(QJsonObject jsonObject = QJsonObject())
        : AoideJsonObject(std::move(jsonObject)) {
    }

    double tempoBpm(double defaultBpm) const;
    void setTempoBpm(double tempoBpm);

    mixxx::track::io::key::ChromaticKey keySignature() const;
    void setKeySignature(
            mixxx::track::io::key::ChromaticKey chromaticKey =
                    mixxx::track::io::key::ChromaticKey::INVALID);

    AoideScoredSongFeature feature(QString feature) const;
    void setFeature(QString feature, double score);
    void removeFeature(const QString& feature) {
        replaceFeature(feature, nullptr);
    }
    void clearFeatures();

  private:
    void replaceFeature(const QString& feature, AoideScoredSongFeature* featureScore);
};

Q_DECLARE_METATYPE(AoideSongProfile);

class AoideTrackOrAlbumOrRelease : public AoideJsonObject {
  public:
    explicit AoideTrackOrAlbumOrRelease(QJsonObject jsonObject = QJsonObject())
        : AoideJsonObject(std::move(jsonObject)) {
    }

    QStringList externalReferences() const;
    void setExternalReferences(QStringList references = QStringList());
};

class AoideTrackOrAlbum : public AoideTrackOrAlbumOrRelease {
  public:
    explicit AoideTrackOrAlbum(QJsonObject jsonObject = QJsonObject())
        : AoideTrackOrAlbumOrRelease(std::move(jsonObject)) {
    }

    AoideTitleVector titles(const QString& level = QString()) const;
    AoideTitleVector allTitles() const;
    AoideTitleVector removeTitles(const QString& level = QString());
    AoideTitleVector clearTitles();
    void addTitles(AoideTitleVector titles);

    AoideActorVector actors(
            const QString& role = QString(), const QString& priority = QString()) const;
    AoideActorVector allActors() const;
    AoideActorVector removeActors(const QString& role = QString());
    AoideActorVector clearActors();
    void addActors(AoideActorVector actors);
};

Q_DECLARE_METATYPE(AoideTrackOrAlbum);

class AoideRelease : public AoideTrackOrAlbumOrRelease {
  public:
    explicit AoideRelease(QJsonObject jsonObject = QJsonObject())
        : AoideTrackOrAlbumOrRelease(std::move(jsonObject)) {
    }

    QDateTime releasedAt() const;
    void setReleasedAt(QDateTime released = QDateTime());

    QString releasedBy() const;
    void setReleasedBy(QString label = QString());

    QString copyright() const;
    void setCopyright(QString copyright = QString());

    QStringList licenses() const;
    void setLicenses(QStringList licenses = QStringList());
};

Q_DECLARE_METATYPE(AoideRelease);

class AoideAlbum : public AoideTrackOrAlbum {
  public:
    explicit AoideAlbum(QJsonObject jsonObject = QJsonObject())
        : AoideTrackOrAlbum(std::move(jsonObject)) {
    }

    bool compilation(bool defaultValue = false) const;
    void setCompilation(bool compilation);
    void resetCompilation();
};

Q_DECLARE_METATYPE(AoideAlbum);

class AoideTrack : public AoideTrackOrAlbum {
  public:
    explicit AoideTrack(QJsonObject jsonObject = QJsonObject())
        : AoideTrackOrAlbum(std::move(jsonObject)) {
    }

    QString uri(const QString& collectionUid) const;

    AoideRelease release() const;
    void setRelease(AoideRelease release = AoideRelease());

    AoideAlbum album() const;
    void setAlbum(AoideAlbum album = AoideAlbum());

    AoideSongProfile profile() const;
    void setProfile(AoideSongProfile music = AoideSongProfile());

    AoideScoredTagVector tags(const QString& facet = QString()) const;
    AoideScoredTagVector allTags() const;
    AoideScoredTagVector removeTags(const QString& facet = QString());
    AoideScoredTagVector clearTags();
    void addTags(AoideScoredTagVector tags);

    AoideCommentVector comments(const QString& owner = QString()) const;
    AoideCommentVector allComments() const;
    AoideCommentVector removeComments(const QString& owner = QString());
    AoideCommentVector clearComments();
    void addComments(AoideCommentVector comments);

    AoideRatingVector ratings(const QString& owner = QString()) const;
    AoideRatingVector allRatings() const;
    AoideRatingVector removeRatings(const QString& owner = QString());
    AoideRatingVector clearRatings();
    void addRatings(AoideRatingVector ratings);
};

Q_DECLARE_METATYPE(AoideTrack);

class AoideTrackEntity : public AoideJsonObject {
  public:
    explicit AoideTrackEntity(QJsonObject jsonObject = QJsonObject())
        : AoideJsonObject(std::move(jsonObject)) {
    }

    AoideEntityHeader header() const;

    AoideTrack body() const;
    void setBody(AoideTrack body);
};

Q_DECLARE_METATYPE(AoideTrackEntity);

class AoideScoredTagFacetCount : public AoideJsonObject {
  public:
    explicit AoideScoredTagFacetCount(QJsonObject jsonObject = QJsonObject())
        : AoideJsonObject(std::move(jsonObject)) {
    }

    QString facet() const;

    quint64 count() const;
};

Q_DECLARE_METATYPE(AoideScoredTagFacetCount);

class AoideScoredTagCount : public AoideJsonObject {
  public:
    explicit AoideScoredTagCount(QJsonObject jsonObject = QJsonObject())
        : AoideJsonObject(std::move(jsonObject)) {
    }

    AoideScoredTag tag() const;

    quint64 count() const;
};

Q_DECLARE_METATYPE(AoideScoredTagCount);

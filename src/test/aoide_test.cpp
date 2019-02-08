#include <gtest/gtest.h>

#include <QtDebug>
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)

#include "aoide/translator.h"

#include "track/track.h"

const QDir kTestDir(QDir::current().absoluteFilePath("src/test/id3-test-data"));

namespace mixxx {

namespace aoide {

class AoideTest : public testing::Test {};

class AoideExportTest : public AoideTest {
  protected:
    void formatDateTimeOrYear(const QString& input, const QString& expectedOutput) {
        const auto actualOutput =
                AoideJsonObject::formatDateTime(AoideJsonObject::parseDateTimeOrYear(input));
        if (actualOutput != expectedOutput) {
            qWarning() << "expected =" << expectedOutput << ", actual =" << actualOutput;
            EXPECT_EQ(expectedOutput, actualOutput);
        }
    }
};

TEST_F(AoideExportTest, FormatDateTimeOrYear) {
    // Unmodified
    formatDateTimeOrYear("2018-01-01T01:02:03.457Z", "2018-01-01T01:02:03.457Z");
    formatDateTimeOrYear("2018-01-01T01:02:03.457+02:00", "2018-01-01T01:02:03.457+02:00");

    // Round to milliseconds
    formatDateTimeOrYear("2018-01-01T01:02:03.45678Z", "2018-01-01T01:02:03.457Z");
    formatDateTimeOrYear("2018-01-01T01:02:03.45678+02:00", "2018-01-01T01:02:03.457+02:00");

    // Strip zero milliseconds
    formatDateTimeOrYear("2018-04-27T07:00:00.000Z", "2018-04-27T07:00:00Z");
    formatDateTimeOrYear("2018-04-27T07:00:00.000-06:00", "2018-04-27T07:00:00-06:00");

    // Without milliseconds
    formatDateTimeOrYear("2018-04-27T07:00:00Z", "2018-04-27T07:00:00Z");
    formatDateTimeOrYear("2018-04-27T07:00:00-06:00", "2018-04-27T07:00:00-06:00");

    // Missing time zone or spec -> assume UTC
    formatDateTimeOrYear("2018-04-27T07:00:00.123", "2018-04-27T07:00:00.123Z");
    formatDateTimeOrYear("2018-04-27T07:00:00", "2018-04-27T07:00:00Z");

    // Missing time zone or spec and missing seconds -> assume UTC
    formatDateTimeOrYear("2018-04-27T07:00", "2018-04-27T07:00:00Z");

    // Only a date without a time
    formatDateTimeOrYear("2007-11-16", "2007-11-16T00:00:00Z");
    formatDateTimeOrYear("1989-3-9", "1989-03-09T00:00:00Z");

    // Only a year + month
    formatDateTimeOrYear("2007-11", "2007-11-01T00:00:00Z");
    formatDateTimeOrYear("2007-4", "2007-04-01T00:00:00Z");

    // Only a year
    formatDateTimeOrYear("2007", "2007-01-01T00:00:00Z");
}

TEST_F(AoideExportTest, ExportTrack) {
    const QFileInfo testFile(kTestDir.absoluteFilePath("cover-test.flac"));
    ASSERT_TRUE(testFile.exists());

    TrackPointer trackPtr = Track::newTemporary(testFile);

    trackPtr->setTitle("Track Title");
    trackPtr->setArtist("Track Artist");
    trackPtr->setAlbum("Album Title");
    trackPtr->setAlbumArtist("Album Artist");
    trackPtr->setGenre("Genre");
    trackPtr->setComment("Comment");
    trackPtr->setRating(3);

    const QString collectionUid = "collection1";
    trackPtr->setDateAdded(QDateTime::currentDateTime());

    AoideTrack aoideTrack = Translator(collectionUid).exportTrack(*trackPtr);

    AoideScoredTag freeTag;
    freeTag.setTerm("A free tag");
    aoideTrack.addTags({freeTag});

    AoideComment ownedComment;
    ownedComment.setOwner("mixxx.org");
    ownedComment.setText("A note from mixxx.org");
    aoideTrack.addComments({ownedComment});

    AoideRating ownedRating;
    ownedRating.setOwner("mixxx.org");
    ownedRating.setScore(0.9);
    aoideTrack.addRatings({ownedRating});

    EXPECT_EQ(1, aoideTrack.allTitles().size());
    EXPECT_EQ(1, aoideTrack.titles().size());
    EXPECT_EQ(trackPtr->getTitle(), aoideTrack.titles().first().name());
    EXPECT_EQ(1, aoideTrack.allActors().size());
    EXPECT_EQ(1, aoideTrack.actors(AoideActor::kRoleArtist).size());
    EXPECT_EQ(trackPtr->getArtist(), aoideTrack.actors(AoideActor::kRoleArtist).first().name());
    EXPECT_EQ(1, aoideTrack.album().allTitles().size());
    EXPECT_EQ(1, aoideTrack.album().titles().size());
    EXPECT_EQ(trackPtr->getAlbum(), aoideTrack.album().titles().first().name());
    EXPECT_EQ(1, aoideTrack.album().allActors().size());
    EXPECT_EQ(1, aoideTrack.album().actors(AoideActor::kRoleArtist).size());
    EXPECT_EQ(
            trackPtr->getAlbumArtist(),
            aoideTrack.album().actors(AoideActor::kRoleArtist).first().name());

    // Tags (incl. Genre)
    EXPECT_EQ(2, aoideTrack.allTags().size());
    EXPECT_EQ(1, aoideTrack.tags("genre").size());
    EXPECT_EQ(trackPtr->getGenre(), aoideTrack.tags("genre").first().term());
    EXPECT_EQ(1, aoideTrack.tags("genre").first().score());
    EXPECT_EQ(1, aoideTrack.tags(freeTag.facet()).size());
    EXPECT_EQ(freeTag.term(), aoideTrack.tags(freeTag.facet()).first().term());

    // Comments
    EXPECT_EQ(2, aoideTrack.allComments().size());
    EXPECT_EQ(1, aoideTrack.comments().size());
    EXPECT_EQ(trackPtr->getComment(), aoideTrack.comments().first().text());
    EXPECT_EQ(1, aoideTrack.comments(ownedComment.owner()).size());
    EXPECT_EQ(ownedComment.text(), aoideTrack.comments(ownedComment.owner()).first().text());

    // Ratings
    EXPECT_EQ(2, aoideTrack.allRatings().size());
    EXPECT_EQ(1, aoideTrack.ratings().size());
    EXPECT_EQ(static_cast<double>(trackPtr->getRating()), aoideTrack.ratings().first().score() * 5);
    EXPECT_EQ(1, aoideTrack.ratings(ownedRating.owner()).size());
    EXPECT_EQ(ownedRating.score(), aoideTrack.ratings(ownedRating.owner()).first().score());

    aoideTrack.removeTitles();
    AoideTitle trackTitle;
    trackTitle.setName("New Track Title");
    aoideTrack.addTitles({trackTitle});
    EXPECT_EQ(1, aoideTrack.titles().size());
    EXPECT_EQ(AoideTitle::kLevelMain, aoideTrack.titles().first().level());
    EXPECT_EQ(trackTitle.name(), aoideTrack.titles().first().name());

    aoideTrack.removeActors(AoideActor::kRoleArtist);
    AoideActor trackArtist;
    trackArtist.setRole(AoideActor::kRoleArtist);
    trackArtist.setName("New Track Artist");
    aoideTrack.addActors({trackArtist});
    EXPECT_EQ(1, aoideTrack.actors(AoideActor::kRoleArtist).size());
    EXPECT_EQ(
            AoideActor::kPrecedenceSummary,
            aoideTrack.actors(AoideActor::kRoleArtist).first().precedence());
    EXPECT_EQ(trackArtist.name(), aoideTrack.actors(AoideActor::kRoleArtist).first().name());

    AoideAlbum album = aoideTrack.album();

    album.removeTitles();
    AoideTitle albumTitle;
    albumTitle.setName("New Album Title");
    album.addTitles({albumTitle});
    EXPECT_EQ(1, album.titles().size());
    EXPECT_EQ(AoideTitle::kLevelMain, album.titles().first().level());
    EXPECT_EQ(albumTitle.name(), album.titles().first().name());

    album.removeActors(AoideActor::kRoleArtist);
    AoideActor albumArtist;
    albumArtist.setRole(AoideActor::kRoleArtist);
    albumArtist.setName("New Album Artist");
    album.addActors({albumArtist});
    EXPECT_EQ(1, album.actors(AoideActor::kRoleArtist).size());
    EXPECT_EQ(
            AoideActor::kPrecedenceSummary,
            album.actors(AoideActor::kRoleArtist).first().precedence());
    EXPECT_EQ(albumArtist.name(), album.actors(AoideActor::kRoleArtist).first().name());
}

} // namespace aoide

} // namespace mixxx

#endif

#pragma once

#include "library/trackprocessing.h"

namespace mixxx {

class CustomTagsFromTrackCommentExtractor : public TrackPointerOperation {
  public:
    explicit CustomTagsFromTrackCommentExtractor(
            const TrackCollectionManager* pTrackCollectionManager)
            : m_pTrackCollectionManager(pTrackCollectionManager) {
    }
    ~CustomTagsFromTrackCommentExtractor() override = default;

  private:
    void doApply(
            const TrackPointer& pTrack) const override;

    const TrackCollectionManager* const m_pTrackCollectionManager;
};

struct FileLocationReplacement {
    QString oldPrefix;
    QString newPrefix;
};

class TrackFileMover : public TrackPointerOperation {
  public:
    explicit TrackFileMover(
            const TrackCollectionManager* pTrackCollectionManager,
            const FileLocationReplacement& fileLocationReplacement)
            : m_pTrackCollectionManager(pTrackCollectionManager),
              m_fileLocationReplacement(fileLocationReplacement) {
    }
    ~TrackFileMover() override = default;

  private:
    void doApply(
            const TrackPointer& pTrack) const override;

    const TrackCollectionManager* const m_pTrackCollectionManager;
    const FileLocationReplacement m_fileLocationReplacement;
};

} // namespace mixxx

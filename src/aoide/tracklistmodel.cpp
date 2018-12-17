#include "aoide/tracklistmodel.h"

#include "track/bpm.h"
#include "util/assert.h"
#include "util/logger.h"

namespace mixxx {

namespace aoide {

namespace {

const Logger kLogger("aoide TrackListModel");

const TrackListModel::Item kEmptyItem;

} // anonymous namespace

TrackListModel::TrackListModel(
        QPointer<Subsystem> subsystem,
        QObject* parent)
        : QAbstractListModel(parent),
          m_subsystem(subsystem),
          m_itemsPerPage(200),
          m_pendingRequestFirstRow(0),
          m_pendingRequestLastRow(0) {
    connect(m_subsystem, &Subsystem::searchTracksResult, this,
            &TrackListModel::searchTracksResult);
    kLogger.debug() << "Created instance" << this;
}

TrackListModel::~TrackListModel() {
    kLogger.debug() << "Destroying instance" << this;
}

int TrackListModel::rowCount(const QModelIndex& parent) const {
    DEBUG_ASSERT(!parent.isValid());
    if (m_itemPages.isEmpty()) {
        return 0;
    } else {
        const auto& lastPage = m_itemPages.last();
        return lastPage.m_firstRow + lastPage.m_items.size();
    }
}

QModelIndex TrackListModel::parent(const QModelIndex& /*index*/) const {
    return QModelIndex();
}

int TrackListModel::findItemPageIndex(int row) const {
    DEBUG_ASSERT(row >= 0);
    if (row >= rowCount()) {
        return -1;
    }
    int lowerIndex = 0;
    int upperIndex = m_itemPages.size();
    while (lowerIndex < upperIndex) {
        if (lowerIndex == (upperIndex - 1)) {
            DEBUG_ASSERT(lowerIndex < m_itemPages.size());
            const auto& lowerItemPage = m_itemPages[lowerIndex];
            DEBUG_ASSERT(lowerItemPage.m_firstRow <= row);
            DEBUG_ASSERT((row - lowerItemPage.m_firstRow) < lowerItemPage.m_items.size());
            return lowerIndex;
        }
        auto middleIndex = lowerIndex + (upperIndex - lowerIndex) / 2;
        DEBUG_ASSERT(middleIndex < m_itemPages.size());
        const auto& middleItemPage = m_itemPages[middleIndex];
        if (row < middleItemPage.m_firstRow) {
            upperIndex = middleIndex;
        } else {
            lowerIndex = middleIndex;
        }
    }
    DEBUG_ASSERT(!"unreachable");
    return -1;
}

const TrackListModel::Item& TrackListModel::itemAt(const QModelIndex& index) const {
    DEBUG_ASSERT(index.isValid());
    DEBUG_ASSERT(index.column() == 0);
    const auto row = index.row();
    const auto itemPageIndex = findItemPageIndex(row);
    VERIFY_OR_DEBUG_ASSERT((itemPageIndex >= 0) && (itemPageIndex < m_itemPages.size())) {
        // Not available
        return kEmptyItem;
    }
    const auto& itemPage = m_itemPages[itemPageIndex];
    DEBUG_ASSERT(row >= itemPage.m_firstRow);
    const auto pageRow = row - itemPage.m_firstRow;
    DEBUG_ASSERT(pageRow < itemPage.m_items.size());
    return itemPage.m_items[pageRow];
}

QVariant TrackListModel::itemData(const Item& item, ItemDataRole role) const {
    switch (role) {
    case ItemDataRole::Uid:
        return item.header().uid();
    case ItemDataRole::RevisionOrdinal:
        return item.header().revision().ordinal();
    case ItemDataRole::RevisionTimestamp:
        return item.header().revision().timestamp();
    case ItemDataRole::ContentType:
        return item.body().source().contentType();
    case ItemDataRole::ContentUri:
        return item.body().source().contentUri();
    case ItemDataRole::ContentUrl:
        return QUrl(item.body().source().contentUri());
    case ItemDataRole::AudioChannelsCount:
        return item.body().source().audioContent().channelsCount();
    case ItemDataRole::AudioDurationMs:
        return item.body().source().audioContent().durationMs();
    case ItemDataRole::AudioSampleRateHz:
        return item.body().source().audioContent().sampleRateHz();
    case ItemDataRole::AudioBitRateBps:
        return item.body().source().audioContent().bitRateBps();
    case ItemDataRole::AudioLoudnessItuBs1770Lufs:
        return item.body().source().audioContent().loudness().ituBs1770Lufs();
    case ItemDataRole::AudioEncoderName:
        return item.body().source().audioContent().encoder().value("name");
    case ItemDataRole::AudioEncoderSettings:
        return item.body().source().audioContent().encoder().value("settings");
    case ItemDataRole::Title: {
        const auto& titles = item.body().titles();
        DEBUG_ASSERT(titles.size() <= 1);
        return titles.isEmpty() ? QString() : titles.first().name();
    }
    case ItemDataRole::Artist: {
        const auto& actors = item.body().actors();
        DEBUG_ASSERT(actors.size() <= 1);
        return actors.isEmpty() ? QString() : actors.first().name();
    }
    case ItemDataRole::AlbumTitle: {
        const auto& titles = item.body().album().titles();
        DEBUG_ASSERT(titles.size() <= 1);
        return titles.isEmpty() ? QString() : titles.first().name();
    }
    case ItemDataRole::AlbumArtist: {
        const auto& actors = item.body().album().actors();
        DEBUG_ASSERT(actors.size() <= 1);
        return actors.isEmpty() ? QString() : actors.first().name();
    }
    case ItemDataRole::Composer: {
        const auto& actors = item.body().album().actors(AoideActor::kRoleComposer);
        DEBUG_ASSERT(actors.size() <= 1);
        return actors.isEmpty() ? QString() : actors.first().name();
    }
    case ItemDataRole::Comment: {
        const auto& comments = item.body().comments();
        DEBUG_ASSERT(comments.size() <= 1);
        return comments.isEmpty() ? QString() : comments.first().text();
    }
    case ItemDataRole::Grouping: {
        const auto& groupingTags = item.body().tags(AoideScoredTag::kFacetContentGroup);
        DEBUG_ASSERT(groupingTags.size() <= 1);
        return groupingTags.isEmpty() ? QString() : groupingTags.first().term();
    }
    case ItemDataRole::Genre: {
        const auto& genreTags = item.body().tags(AoideScoredTag::kFacetGenre);
        QString multiGenre;
        for (const auto genreTag: genreTags) {
            if (!multiGenre.isEmpty()) {
                multiGenre += m_subsystem->settings().multiGenreSeparator();
            }
            multiGenre += genreTag.term();
        }
        return multiGenre;
    }
    case ItemDataRole::MusicTempoBpm: {
        return item.body().profile().tempoBpm(Bpm::kValueUndefined);
    }
    default:
        DEBUG_ASSERT(!"TODO");
        return QVariant();
    }
}

QVariant TrackListModel::data(const QModelIndex& index, int role) const {
    const auto& item = itemAt(index);
    switch (role) {
    case Qt::DisplayRole:
        // TODO: What data should be returned here???
        return itemData(item, ItemDataRole::ContentUrl);
    case Qt::ToolTipRole:
        // Display the file URL as tooltip
        return itemData(item, ItemDataRole::ContentUrl);
    default:
        if (role >= Qt::UserRole) {
            return itemData(item, ItemDataRole(role));
        } else {
            // TODO
            return QVariant();
        }
    }
}

bool TrackListModel::canFetchMore(const QModelIndex& parent) const {
    DEBUG_ASSERT(!parent.isValid());
    return !m_phraseQuery.isNull();
}

void TrackListModel::fetchMore(const QModelIndex& parent) {
    VERIFY_OR_DEBUG_ASSERT(canFetchMore(parent)) {
        return;
    }
    if (m_pendingRequestId.isValid()) {
        kLogger.debug()
                << "Can't fetch more rows while a search request is pending"
                << m_pendingRequestId;
        return;
    }
    DEBUG_ASSERT(m_itemsPerPage > 0);
    AoidePagination pagination;
    pagination.offset = rowCount();
    pagination.limit = m_itemsPerPage;
    m_pendingRequestId = m_subsystem->searchTracksAsync(
            m_phraseQuery,
            pagination);
    DEBUG_ASSERT(m_pendingRequestId.isValid());
    m_pendingRequestFirstRow = pagination.offset;
    m_pendingRequestLastRow = m_pendingRequestFirstRow + (pagination.limit - 1);
}

void TrackListModel::searchTracks(
        QString searchText) {
    if (m_pendingRequestId.isValid()) {
        kLogger.warning()
                << "Discarding pending search request"
                << m_pendingRequestId;
    }
    auto phraseQuery = searchText.isNull() ? QString("") : searchText;
    DEBUG_ASSERT(m_itemsPerPage > 0);
    AoidePagination pagination;
    pagination.offset = 0;
    pagination.limit = m_itemsPerPage;
    m_pendingRequestId = m_subsystem->searchTracksAsync(
            phraseQuery,
            pagination);
    DEBUG_ASSERT(m_pendingRequestId.isValid());
    m_pendingRequestFirstRow = pagination.offset;
    m_pendingRequestLastRow = m_pendingRequestFirstRow + (pagination.limit - 1);
    m_searchText = searchText;
    m_phraseQuery = phraseQuery;
}

void TrackListModel::searchTracksResult(
    mixxx::AsyncRestClient::RequestId requestId,
    QVector<Item> result) {
    DEBUG_ASSERT(requestId.isValid());
    if (m_pendingRequestId == requestId) {
        m_pendingRequestId.reset();
        DEBUG_ASSERT(!m_pendingRequestId.isValid());
        DEBUG_ASSERT(m_pendingRequestFirstRow >= 0);
        if (m_pendingRequestFirstRow == 0) {
            beginResetModel();
            m_itemPages.clear();
            endResetModel();
        }
        DEBUG_ASSERT(m_pendingRequestFirstRow == rowCount());
        kLogger.debug()
                << "Received"
                << result.size()
                << "tracks from subsystem";
        if (result.isEmpty()) {
            // No more results available
            m_phraseQuery = QString();
        } else {
            int firstRow = m_pendingRequestFirstRow;
            int lastRow = m_pendingRequestFirstRow + (result.size() - 1);
            if (lastRow < m_pendingRequestLastRow) {
                // No more results available
                m_phraseQuery = QString();
            }
            beginInsertRows(QModelIndex(), firstRow, lastRow);
            m_itemPages += ItemPage(m_pendingRequestFirstRow, std::move(result));
            endInsertRows();
        }
    } else {
        kLogger.debug()
                << "Discarding search response"
                << requestId
                << "from subsystem";
        if (m_pendingRequestId.isValid()) {
            kLogger.debug()
                    << "Waiting for search response"
                    << requestId
                    << "from subsystem";
        }
    }
}

} // namespace aoide

} // namespace mixxx

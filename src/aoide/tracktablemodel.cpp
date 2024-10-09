#include "aoide/tracktablemodel.h"

#include <QRegularExpression>

#include "aoide/libraryfeature.h"
#include "aoide/web/searchtrackstask.h"
#include "library/dao/trackschema.h"
#include "library/tabledelegates/coverartdelegate.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "mixer/playerinfo.h"
#include "mixer/playermanager.h"
#include "track/track.h"
#include "util/assert.h"
#include "util/duration.h"
#include "util/logger.h"

namespace {

const mixxx::Logger kLogger("aoide TrackTableModel");

const char* const kSettingsNamespace = "aoide";

// The initial search request seems to take longer than
// subsequent requests!? To prevent a timeout without a
// result we need to be generous here.
constexpr int kSearchTimeoutMillis = 20000;

// Currently complete track objects are deserialized from the database,
// serialized as JSON and then transmitted. Until optimizations for the
// track listing use case are in place the number of tracks that are
// loaded at once should be strictly limited to keep the UI responsive.
constexpr int kRowsPerPage = 200;

} // anonymous namespace

namespace aoide {

namespace {

const TrackTableModel::RowItem kEmptyItem;

const QStringList kTableColumns = {
        LIBRARYTABLE_ALBUM,
        LIBRARYTABLE_ALBUMARTIST,
        LIBRARYTABLE_ARTIST,
        LIBRARYTABLE_BPM,
        LIBRARYTABLE_BPM_LOCK,
        LIBRARYTABLE_BITRATE,
        LIBRARYTABLE_CHANNELS,
        LIBRARYTABLE_COLOR,
        LIBRARYTABLE_COMMENT,
        LIBRARYTABLE_COMPOSER,
        LIBRARYTABLE_COVERART,
        LIBRARYTABLE_DATETIMEADDED,
        LIBRARYTABLE_DURATION,
        LIBRARYTABLE_FILETYPE,
        LIBRARYTABLE_GENRE,
        LIBRARYTABLE_GROUPING,
        LIBRARYTABLE_KEY,
        LIBRARYTABLE_KEY_ID,
        TRACKLOCATIONSTABLE_LOCATION,
        TRACKLOCATIONSTABLE_FSDELETED,
        LIBRARYTABLE_PLAYED,
        LIBRARYTABLE_PREVIEW,
        LIBRARYTABLE_RATING,
        LIBRARYTABLE_REPLAYGAIN,
        LIBRARYTABLE_SAMPLERATE,
        LIBRARYTABLE_TIMESPLAYED,
        LIBRARYTABLE_LAST_PLAYED_AT,
        LIBRARYTABLE_TITLE,
        LIBRARYTABLE_TRACKNUMBER,
        LIBRARYTABLE_YEAR,
};

} // anonymous namespace

TrackTableModel::TrackTableModel(
        TrackCollectionManager* trackCollectionManager,
        Subsystem* subsystem,
        LibraryFeature* parent)
        : BaseTrackTableModel(
                  parent,
                  trackCollectionManager,
                  kSettingsNamespace),
          m_subsystem(subsystem),
          m_rowsPerPage(kRowsPerPage),
          m_canFetchMore(false),
          m_pendingRequestFirstRow(0),
          m_pendingRequestLastRow(0) {
    initTableColumnsAndHeaderProperties(kTableColumns);
    connect(m_pTrackCollectionManager->internalCollection(),
            &TrackCollection::trackDirty,
            this,
            [this](TrackId trackId) { slotTracksChangedOrRemoved(QSet<TrackId>{trackId}); });
    connect(m_pTrackCollectionManager->internalCollection(),
            &TrackCollection::tracksChanged,
            this,
            &TrackTableModel::slotTracksChangedOrRemoved);
    connect(m_pTrackCollectionManager->internalCollection(),
            &TrackCollection::tracksRemoved,
            this,
            &TrackTableModel::slotTracksChangedOrRemoved);
    kLogger.debug() << "Created instance" << this;
}

TrackTableModel::~TrackTableModel() {
    kLogger.debug() << "Destroying instance" << this;
}

TrackModel::Capabilities TrackTableModel::getCapabilities() const {
    return Capability::AddToTrackSet |
            Capability::AddToAutoDJ |
            Capability::EditMetadata |
            Capability::LoadToDeck |
            Capability::LoadToSampler |
            Capability::LoadToPreviewDeck |
            Capability::Hide |
            Capability::ResetPlayed |
            Capability::RemoveFromDisk |
            Capability::Analyze |
            Capability::Properties;
}

bool TrackTableModel::setTrackValueForColumn(
        const TrackPointer& pTrack,
        int column,
        const QVariant& value,
        int role) {
    DEBUG_ASSERT(!"TODO");
    kLogger.warning()
            << "TODO: setTrackValueForColumn"
            << pTrack->getId()
            << column
            << value
            << role;
    return false;
}

int TrackTableModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    DEBUG_ASSERT(!parent.isValid());
    int rowCount;
    if (m_rowItemPages.isEmpty()) {
        rowCount = 0;
    } else {
        const auto& lastPage = m_rowItemPages.last();
        rowCount = lastPage.m_firstRow + lastPage.m_rowItems.size();
    }
    return rowCount;
}

int TrackTableModel::findRowItemPageIndex(int row) const {
    DEBUG_ASSERT(row >= 0);
    if (row >= rowCount()) {
        return -1;
    }
    int lowerIndex = 0;
    int upperIndex = m_rowItemPages.size();
    while (lowerIndex < upperIndex) {
        if (lowerIndex == (upperIndex - 1)) {
            DEBUG_ASSERT(lowerIndex < m_rowItemPages.size());
            const auto& lowerItemPage = m_rowItemPages[lowerIndex];
            Q_UNUSED(lowerItemPage);
            DEBUG_ASSERT(lowerItemPage.m_firstRow <= row);
            DEBUG_ASSERT((row - lowerItemPage.m_firstRow) < lowerItemPage.m_rowItems.size());
            return lowerIndex;
        }
        auto middleIndex = lowerIndex + (upperIndex - lowerIndex) / 2;
        DEBUG_ASSERT(middleIndex < m_rowItemPages.size());
        const auto& middleItemPage = m_rowItemPages[middleIndex];
        if (row < middleItemPage.m_firstRow) {
            upperIndex = middleIndex;
        } else {
            lowerIndex = middleIndex;
        }
    }
    DEBUG_ASSERT(!"unreachable");
    return -1;
}

const TrackTableModel::RowItem& TrackTableModel::rowItem(int row) const {
    const auto pageIndex = findRowItemPageIndex(row);
    VERIFY_OR_DEBUG_ASSERT((pageIndex >= 0) && (pageIndex < m_rowItemPages.size())) {
        // Not available
        return kEmptyItem;
    }
    const auto& page = m_rowItemPages[pageIndex];
    DEBUG_ASSERT(row >= page.m_firstRow);
    const auto pageRow = row - page.m_firstRow;
    DEBUG_ASSERT(pageRow < page.m_rowItems.size());
    return page.m_rowItems[pageRow];
}

const TrackTableModel::RowItem& TrackTableModel::rowItem(
        const QModelIndex& index) const {
    int row = index.row();
    if (row < 0 || row >= rowCount()) {
        return kEmptyItem;
    }
    return rowItem(row);
}

Qt::ItemFlags TrackTableModel::flags(
        const QModelIndex& index) const {
    // TODO: Enable in-place editing if row is not stale
    return readOnlyFlags(index);
}

QVariant TrackTableModel::roleValue(
        const QModelIndex& index,
        QVariant&& rawValue,
        int role) const {
    DEBUG_ASSERT(index.isValid());
    // TODO: Display rows that might contain outdated or missing data differently?
    // if (m_staleRows.contains(index.row())) {
    //     return QVariant();
    // }
    return BaseTrackTableModel::roleValue(
            index,
            std::move(rawValue),
            role);
}

QUrl TrackTableModel::rowItemUrl(
        const RowItem& rowItem) const {
    const auto subsystem = m_subsystem.data();
    VERIFY_OR_DEBUG_ASSERT(subsystem) {
        return {};
    }
    const auto activeCollection = subsystem->activeCollection();
    VERIFY_OR_DEBUG_ASSERT(activeCollection) {
        return {};
    }
    const auto contentUrl = rowItem.entity.body().contentUrl();
    DEBUG_ASSERT(contentUrl.isValid());
    return contentUrl;
}

QString TrackTableModel::rowItemLocation(
        const RowItem& rowItem) const {
    return mixxx::FileInfo::fromQUrl(rowItemUrl(rowItem)).location();
}

QVariant TrackTableModel::rawValue(
        const QModelIndex& index) const {
    VERIFY_OR_DEBUG_ASSERT(index.isValid()) {
        return {};
    }
    const auto field = mapColumn(index.column());
    if (field == ColumnCache::COLUMN_LIBRARYTABLE_INVALID) {
        return {};
    }
    const auto item = rowItem(index.row());
    switch (field) {
    case ColumnCache::COLUMN_LIBRARYTABLE_PREVIEW: {
        if (!previewDeckTrackId().isValid()) {
            return false;
        }
        auto cachedRow = m_trackIdRowCache.value(previewDeckTrackId(), -1);
        if (cachedRow >= 0) {
            return cachedRow == index.row();
        }
        return previewDeckTrackId() == getTrackId(index);
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_ALBUM: {
        const auto& titles = item.entity.body().track().album().titles();
        DEBUG_ASSERT(titles.size() <= 1);
        return titles.isEmpty() ? QString() : titles.first().name();
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_ALBUMARTIST: {
        const auto& artists = item.entity.body().track().album().artists();
        DEBUG_ASSERT(artists.size() <= 1);
        return artists.isEmpty() ? QString() : artists.first().name();
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_ARTIST: {
        const auto& artists = item.entity.body().track().artists();
        DEBUG_ASSERT(artists.size() <= 1);
        return artists.isEmpty() ? QString() : artists.first().name();
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_BITRATE: {
        return QVariant::fromValue(item.entity.body()
                        .track()
                        .mediaSource()
                        .content()
                        .audioMetadata()
                        .bitrate());
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_BPM: {
        const auto bpm = item.entity.body().track().musicMetrics().bpm();
        return QVariant::fromValue(bpm);
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_BPM_LOCK: {
        return item.entity.body().track().musicMetrics().bpmLocked();
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_CHANNELS: {
        return QVariant::fromValue(item.entity.body()
                        .track()
                        .mediaSource()
                        .content()
                        .audioMetadata()
                        .channelCount());
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_COLOR: {
        auto color = item.entity.body().track().color();
        if (!color) {
            // Use predominant artwork color as a fallback
            color = item.entity.body().track().mediaSource().artwork().image().color();
        }
        return mixxx::RgbColor::toQVariant(color);
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_COMMENT: {
        return item.tags.comment();
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_COMPOSER: {
        const auto& actors = item.entity.body().track().actors(json::Actor::kRoleComposer);
        DEBUG_ASSERT(actors.size() <= 1);
        return actors.isEmpty() ? QString() : actors.first().name();
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_DATETIMEADDED: {
        const auto collectedAt = item.entity.body().track().mediaSource().collectedAt();
        VERIFY_OR_DEBUG_ASSERT(collectedAt) {
            return QVariant{};
        }
        return *collectedAt;
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_DURATION: {
        return QVariant::fromValue(item.entity.body()
                        .track()
                        .mediaSource()
                        .content()
                        .audioMetadata()
                        .duration());
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_GENRE: {
        return item.tags.joinGenres();
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_GROUPING: {
        return item.tags.grouping();
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_FILETYPE: {
        auto contentType = item.entity.body().track().mediaSource().content().typeName();
        if (contentType.startsWith("audio/")) {
            return contentType.right(contentType.size() - 6);
        } else {
            return contentType;
        }
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_KEY: {
        const auto key = item.entity.body().track().musicMetrics().key();
        if (key == mixxx::track::io::key::INVALID) {
            return {};
        }
        return KeyUtils::keyToString(key);
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_KEY_ID: {
        return item.entity.body().track().musicMetrics().key();
    }
    case ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION: {
        return rowItemLocation(item);
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_REPLAYGAIN: {
        return QVariant::fromValue(item.entity.body()
                        .track()
                        .mediaSource()
                        .content()
                        .audioMetadata()
                        .replayGain());
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_SAMPLERATE: {
        return QVariant::fromValue(item.entity.body()
                        .track()
                        .mediaSource()
                        .content()
                        .audioMetadata()
                        .sampleRate());
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_TITLE: {
        const auto& titles = item.entity.body().track().titles();
        DEBUG_ASSERT(titles.size() <= 1);
        return titles.isEmpty() ? QString() : titles.first().name();
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_TRACKNUMBER: {
        return item.entity.body().track().trackNumbers();
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_YEAR: {
        const auto releasedAt = item.entity.body().track().releasedAt();
        if (!releasedAt.isEmpty()) {
            return releasedAt;
        }
        // Use the recording date as a fallback as Mixxx does not
        // distinguish between release and recording dates.
        return item.entity.body().track().recordedAt();
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_PLAYED: {
        const auto* pLibraryFeature = static_cast<const LibraryFeature*>(parent());
        return pLibraryFeature->sessionCache().isTrackLocationPlayed(rowItemLocation(item));
    }
    case ColumnCache::COLUMN_LIBRARYTABLE_COVERART:
    case ColumnCache::COLUMN_LIBRARYTABLE_RATING:
    case ColumnCache::COLUMN_LIBRARYTABLE_LAST_PLAYED_AT:
    case ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED:
    case ColumnCache::COLUMN_TRACKLOCATIONSTABLE_FSDELETED:
        // Not supported/implemented
        break;
    default:
        kLogger.critical()
                << "Unmapped field"
                << field
                << '@'
                << index.row();
        DEBUG_ASSERT(!"unreachable");
        break;
    }
    return {};
}

bool TrackTableModel::canFetchMore(
        const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return m_canFetchMore;
}

void TrackTableModel::fetchMore(
        const QModelIndex& parent) {
    VERIFY_OR_DEBUG_ASSERT(canFetchMore(parent)) {
        return;
    }
    if (m_pendingSearchTask) {
        // Await the pending search results and ignore all
        // intermediate requests
        return;
    }
    VERIFY_OR_DEBUG_ASSERT(m_canFetchMore) {
        kLogger.debug()
                << "No more rows available for fetching";
        return;
    }
    m_pendingRequestFirstRow = rowCount();
    DEBUG_ASSERT(m_rowsPerPage > 0);
    m_pendingRequestLastRow = m_pendingRequestFirstRow + (m_rowsPerPage - 1);
    Pagination pagination;
    pagination.offset = m_pendingRequestFirstRow;
    pagination.limit = (m_pendingRequestLastRow - m_pendingRequestFirstRow) + 1;
    startNewSearch(pagination);
}

void TrackTableModel::startNewSearch(
        const Pagination& pagination) {
    abortPendingSearch();
    DEBUG_ASSERT(!m_pendingSearchTask);
    auto* const pendingSearchTask = m_subsystem.data()->searchTracks(
            m_baseQuery,
            m_searchOverlayFilter,
            m_searchTerms,
            pagination);
    DEBUG_ASSERT(pendingSearchTask);
    connect(pendingSearchTask,
            &SearchTracksTask::succeeded,
            this,
            &TrackTableModel::slotSearchTracksSucceeded,
            Qt::UniqueConnection);
    pendingSearchTask->invokeStart(kSearchTimeoutMillis);
    m_pendingSearchTask = pendingSearchTask;
}

namespace {
const QRegularExpression kRegexpWhitespace("\\s+");

} // anonymous namespace

void TrackTableModel::abortPendingSearch() {
    auto* const pendingSearchTask = m_pendingSearchTask.data();
    if (pendingSearchTask) {
        kLogger.debug()
                << "Aborting pending search task"
                << pendingSearchTask;
        pendingSearchTask->disconnect(this);
        pendingSearchTask->invokeAbort();
        pendingSearchTask->deleteLater();
        m_pendingSearchTask.clear();
    }
}

void TrackTableModel::reset() {
    beginResetModel();
    abortPendingSearch();
    clearRowItems();
    m_collectionUid = QString();
    m_baseQuery = QJsonObject();
    m_searchText = QString();
    m_canFetchMore = false;
    m_pendingRequestFirstRow = 0;
    m_pendingRequestLastRow = 0;
    endResetModel();
}

void TrackTableModel::clearRows() {
    if (rowCount() <= 0) {
        return;
    }
    beginRemoveRows(QModelIndex(), 0, rowCount() - 1);
    clearRowItems();
    endRemoveRows();
}

void TrackTableModel::searchTracks(
        const QJsonObject& baseQuery,
        const TrackSearchOverlayFilter& overlayFilter,
        const QString& searchText) {
    if (!m_subsystem.data()->activeCollection()) {
        kLogger.warning()
                << "Search not available without an active collection";
        return;
    }
    abortPendingSearch();
    DEBUG_ASSERT(m_rowsPerPage > 0);
    Pagination pagination;
    pagination.offset = 0;
    pagination.limit = m_rowsPerPage;
    m_collectionUid = m_subsystem.data()->activeCollection()->header().uid();
    m_baseQuery = baseQuery;
    m_searchOverlayFilter = overlayFilter;
    m_searchText = searchText;
    // TODO: Parse the query string
    m_searchTerms = m_searchText.split(
            kRegexpWhitespace,
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            Qt::SkipEmptyParts);
#else
            QString::SkipEmptyParts);
#endif
    m_canFetchMore = true;
    m_pendingRequestFirstRow = pagination.offset;
    m_pendingRequestLastRow = m_pendingRequestFirstRow + (pagination.limit - 1);
    startNewSearch(pagination);
}

void TrackTableModel::clearRowItems() {
    m_rowItemPages.clear();
    m_staleRows.clear();
    m_staleTrackIds.clear();
    m_trackIdRowCache.clear();
}

void TrackTableModel::slotSearchTracksSucceeded(
        const QJsonArray& searchResults) {
    auto* const finishedSearchTask = qobject_cast<SearchTracksTask*>(sender());
    if (!finishedSearchTask) {
        // The sender might have been dropped already
        return;
    }
    const auto finishedSearchTaskDeleter = mixxx::ScopedDeleteLater(finishedSearchTask);

    VERIFY_OR_DEBUG_ASSERT(finishedSearchTask == m_pendingSearchTask.data()) {
        // Previously aborted?
        return;
    }
    m_pendingSearchTask.clear();

    if (m_pendingRequestFirstRow == 0) {
        clearRows();
    }
    DEBUG_ASSERT(m_pendingRequestFirstRow == rowCount());
    kLogger.debug()
            << "Received"
            << searchResults.size()
            << "search results from subsystem";
    if (searchResults.isEmpty()) {
        // No more results available
        m_canFetchMore = false;
        return;
    }
    int firstRow = m_pendingRequestFirstRow;
    DEBUG_ASSERT(searchResults.size() >= 1);
    int lastRow = m_pendingRequestFirstRow + (searchResults.size() - 1);
    if (lastRow < m_pendingRequestLastRow) {
        // No more results available
        m_canFetchMore = false;
    }
    beginInsertRows(QModelIndex(), firstRow, lastRow);
    QVector<RowItem> rowItems;
    rowItems.reserve(searchResults.size());
    for (const auto& searchResult : searchResults) {
        // TODO: Improve parsing and validation
        DEBUG_ASSERT(searchResult.isArray());
        auto entity = json::TrackEntity(searchResult.toArray());
        rowItems += RowItem(
                std::move(entity)
                /*, m_pTrackCollectionManager->taggingConfig()*/);
    }
    m_rowItemPages += RowItemPage(m_pendingRequestFirstRow, std::move(rowItems));
    endInsertRows();
    if (m_trackIdRowCache.capacity() <= 0) {
        // Initially reserve some capacity for row cache
        m_trackIdRowCache.reserve(rowCount());
    }
}

TrackId TrackTableModel::getTrackIdByRow(int row) const {
    const auto trackFileRef = getTrackFileRefByRow(row);
    if (!trackFileRef.isValid()) {
        return TrackId();
    }
    if (kLogger.debugEnabled()) {
        kLogger.debug()
                << "Looking up id of track in internal collection:"
                << trackFileRef;
    }
    DEBUG_ASSERT(m_pTrackCollectionManager);
    const auto trackId =
            m_pTrackCollectionManager->internalCollection()->getTrackDAO().getTrackIdByRef(
                    trackFileRef);
    if (trackId.isValid()) {
        const auto iter = m_trackIdRowCache.find(trackId);
        // Each track is expected to appear only once, i.e. no duplicates!
        DEBUG_ASSERT(
                iter == m_trackIdRowCache.end() ||
                iter.value() == row);
        if (iter == m_trackIdRowCache.end()) {
            m_trackIdRowCache.insert(trackId, row);
            if (m_staleTrackIds.contains(trackId) && !m_staleRows.contains(row)) {
                m_staleRows.insert(row);
                const_cast<TrackTableModel*>(this)->emitRowDataChanged(row);
            }
        }
    }
    return trackId;
}

TrackPointer TrackTableModel::getTrack(const QModelIndex& index) const {
    VERIFY_OR_DEBUG_ASSERT(index.isValid()) {
        return {};
    }
    const auto trackRef = getTrackFileRefByRow(index.row());
    VERIFY_OR_DEBUG_ASSERT(trackRef.isValid()) {
        return {};
    }
    if (kLogger.debugEnabled()) {
        kLogger.debug()
                << "Loading track from internal collection:"
                << trackRef;
    }
    DEBUG_ASSERT(m_pTrackCollectionManager);
    return m_pTrackCollectionManager->getTrackByRef(trackRef);
}

TrackId TrackTableModel::getTrackId(const QModelIndex& index) const {
    VERIFY_OR_DEBUG_ASSERT(index.isValid()) {
        return {};
    }
    return getTrackIdByRow(index.row());
}

QUrl TrackTableModel::getTrackUrl(const QModelIndex& index) const {
    VERIFY_OR_DEBUG_ASSERT(index.isValid()) {
        return {};
    }
    return getTrackUrlByRow(index.row());
}

QString TrackTableModel::getTrackLocation(const QModelIndex& index) const {
    VERIFY_OR_DEBUG_ASSERT(index.isValid()) {
        return {};
    }
    const auto trackRef = getTrackFileRefByRow(index.row());
    return trackRef.getLocation();
}

CoverInfo TrackTableModel::getCoverInfo(
        const QModelIndex& index) const {
    CoverInfo coverInfo;
    coverInfo.trackLocation = getTrackLocation(index);
    const auto artwork = rowItem(index).entity.body().track().mediaSource().artwork();
    if (artwork.isEmpty()) {
        return coverInfo;
    }
    const auto artworkImage = artwork.image();
    if (artworkImage.isEmpty()) {
        return coverInfo;
    }
    const auto uri = artwork.uri();
    if (uri.isValid()) {
        coverInfo.coverLocation =
                mixxx::FileInfo::fromQUrl(uri.toQUrl()).location();
    }
    coverInfo.color = artworkImage.color();
    coverInfo.setImageDigest(artworkImage.digest());
    DEBUG_ASSERT(coverInfo.imageDigest().isEmpty() == artworkImage.thumbnail().isNull());
    if (coverInfo.imageDigest().isEmpty()) {
        DEBUG_ASSERT(artwork.source().isEmpty() ||
                artwork.source() == QStringLiteral("missing"));
    } else {
        DEBUG_ASSERT(!artwork.source().isEmpty());
        if (coverInfo.coverLocation.isEmpty()) {
            DEBUG_ASSERT(artwork.source() == QStringLiteral("embedded"));
            coverInfo.type = CoverInfo::Type::METADATA;
        } else {
            DEBUG_ASSERT(artwork.source() == QStringLiteral("linked"));
            coverInfo.type = CoverInfo::Type::FILE;
        }
    }
    // The following properties are not available
    DEBUG_ASSERT(coverInfo.source == CoverInfo::Source::UNKNOWN);
    DEBUG_ASSERT(coverInfo.legacyHash() == CoverInfo::defaultLegacyHash());
    return coverInfo;
}

QImage TrackTableModel::getCoverThumbnail(
        const QModelIndex& index) const {
    return rowItem(index).entity.body().track().mediaSource().artwork().image().preview();
}

const QVector<int> TrackTableModel::getTrackRows(TrackId trackId) const {
    VERIFY_OR_DEBUG_ASSERT(trackId.isValid()) {
        return {};
    }
    // Each track is expected to appear only once, i.e. no duplicates!
    QVector<int> rows;
    int row = m_trackIdRowCache.value(trackId, -1);
    if (row >= 0) {
        DEBUG_ASSERT(row < rowCount());
        rows.append(row);
    } else {
        // Not cached -> full table scan
        kLogger.debug()
                << "Starting full table scan to"
                << "find row of track with id"
                << trackId;
        for (row = 0; row < rowCount(); ++row) {
            if (getTrackIdByRow(row) == trackId) {
                kLogger.debug()
                        << "Found track with id"
                        << trackId
                        << "in row"
                        << row;
                rows.append(row);
                break;
            }
        }
    }
    return rows;
}

void TrackTableModel::search(const QString& searchText) {
    m_searchText = searchText;
    select();
}

const QString TrackTableModel::currentSearch() const {
    return m_searchText;
}

bool TrackTableModel::isColumnInternal(int column) {
    // Copied from LibraryTableModel.
    return column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ID) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_URL) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_CUEPOINT) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_SAMPLERATE) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_MIXXXDELETED) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_HEADERPARSED) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_PLAYED) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_KEY_ID) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_BPM_LOCK) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_BEATS_VERSION) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_CHANNELS) ||
            column == fieldIndex(ColumnCache::COLUMN_TRACKLOCATIONSTABLE_DIRECTORY) ||
            column == fieldIndex(ColumnCache::COLUMN_TRACKLOCATIONSTABLE_FSDELETED) ||
            (PlayerInfo::instance().numPreviewDecks() == 0 &&
                    column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_PREVIEW)) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART_SOURCE) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART_TYPE) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART_LOCATION) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART_COLOR) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART_DIGEST) ||
            column == fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART_HASH);
}

void TrackTableModel::select() {
    searchTracks(m_searchOverlayFilter, m_searchText);
}

void TrackTableModel::emitRowDataChanged(
        int row) {
    const auto topLeft = index(row, 0);
    DEBUG_ASSERT(columnCount() > 0);
    const auto bottomRight = index(row, columnCount() - 1);
    emit dataChanged(topLeft, bottomRight);
}

void TrackTableModel::invalidateTrackId(
        TrackId trackId) const {
    DEBUG_ASSERT(trackId.isValid());
    m_staleTrackIds.insert(trackId);
    auto row = m_trackIdRowCache.value(trackId, -1);
    if (row < 0) {
        // Row not cached, but might still be visible
        // TODO: How to find and invalidate the corresponding
        // rows?
        return;
    }
    m_staleRows.insert(row);
    const_cast<TrackTableModel*>(this)->emitRowDataChanged(row);
}

void TrackTableModel::slotTracksChangedOrRemoved(
        QSet<TrackId> trackIds) {
    for (auto trackId : std::as_const(trackIds)) {
        invalidateTrackId(trackId);
    }
}

QString TrackTableModel::modelKey(bool noSearch) const {
    if (noSearch) {
        return QStringLiteral("aoide");
    } else {
        return QStringLiteral("aoide#") + currentSearch();
    }
}

} // namespace aoide

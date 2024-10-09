#include "aoide/tracksearchlistmodel.h"

#include <mutex> // std::once_flag

#include "aoide/gateway.h"
#include "aoide/subsystem.h"
#include "util/assert.h"
#include "util/logger.h"

namespace {

const mixxx::Logger kLogger("aoide TrackSearchListModel");

constexpr int kRequestTimeoutMillis = 10000;

std::once_flag registerMetaTypesOnceFlag;

void registerMetaTypesOnce() {
    qRegisterMetaType<aoide::TrackSearchListParams>("aoide::TrackSearchListParams");
    qRegisterMetaType<std::optional<aoide::TrackSearchListParams>>(
            "std::optional<aoide::TrackSearchListParams>");
}

inline quint64 validPageSize(quint64 pageSize) {
    if (pageSize > 0) {
        return pageSize;
    } else {
        return aoide::TrackSearchListModel::kDefaultPageSize;
    }
}

} // anonymous namespace

namespace aoide {

QDebug operator<<(QDebug dbg, const TrackSearchListParams& arg) {
    const QDebugStateSaver saver(dbg);
    dbg = dbg.maybeSpace() << "TrackSearchListParams";
    return dbg.nospace()
            << '{'
            << arg.baseQuery
            << ','
            << arg.overlayFilter
            << ','
            << arg.searchTerms
            << '}';
}

TrackSearchListItem::TrackSearchListItem(
        json::TrackEntity&& argEntity)
        : entity(std::move(argEntity)),
          tags(entity.body().track().removeTags()) {
}

TrackSearchListModel::TrackSearchListModel(
        Subsystem* subsystem,
        QObject* parent)
        : QAbstractListModel(parent),
          m_subsystem(subsystem),
          m_canFetchMore(true) {
    std::call_once(registerMetaTypesOnceFlag, registerMetaTypesOnce);
    kLogger.debug() << "Created instance" << this;
}

TrackSearchListModel::~TrackSearchListModel() {
    kLogger.debug() << "Destroying instance" << this;
    abortPendingTask();
}

int TrackSearchListModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)
    DEBUG_ASSERT(!parent.isValid());
    return m_rowItems.size();
}

QHash<int, QByteArray> TrackSearchListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[ItemRole] = "item";
    roles[TrackArtistRole] = "trackArtist";
    roles[TrackTitleRole] = "trackTitle";
    roles[AlbumArtistRole] = "albumArtist";
    roles[AlbumTitleRole] = "albumTitle";
    return roles;
}

QVariant TrackSearchListModel::data(
        const QModelIndex& index,
        int role) const {
    DEBUG_ASSERT(!index.parent().isValid());
    VERIFY_OR_DEBUG_ASSERT(index.isValid()) {
        return QVariant();
    }
    const auto row = index.row();
    VERIFY_OR_DEBUG_ASSERT(row >= 0 && row < m_rowItems.size()) {
        return QVariant();
    }

    const auto& rowItem = m_rowItems[row];

    if (role < Qt::UserRole) {
        switch (role) {
        case Qt::DisplayRole:
            return QStringLiteral("TODO: Qt::DisplayRole");
        case Qt::ToolTipRole:
            return QStringLiteral("TODO: Qt::ToolTipRole");
        case Qt::EditRole:
            return QStringLiteral("TODO: Qt::EditRole");
        default:
            return QVariant{};
        }
    }

    const auto& entity = rowItem.entity;
    switch (role) {
    case ItemRole:
        return QVariant::fromValue(rowItem);
    case IdRole:
        return entity.header().uid().value();
    case EntityUidRole:
        return QVariant::fromValue(entity.header().uid());
    case CollectedAtRole:
        return entity.body().track().mediaSource().collectedAt().value_or(QDateTime{});
    case ContentPathRole:
        return entity.body().track().mediaSource().content().link().path();
    case ContentUrlRole:
        return entity.body().contentUrl();
    case ContentTypeNameRole:
        return entity.body().track().mediaSource().content().typeName();
    case AudioContentDurationRole:
        return QVariant::fromValue(entity.body()
                                           .track()
                                           .mediaSource()
                                           .content()
                                           .audioMetadata()
                                           .duration());
    case AudioContentDurationMillisRole:
        return entity.body()
                .track()
                .mediaSource()
                .content()
                .audioMetadata()
                .duration()
                .toDoubleMillis();
    case AudioContentChannelCountRole:
        return QVariant::fromValue(entity.body()
                                           .track()
                                           .mediaSource()
                                           .content()
                                           .audioMetadata()
                                           .channelCount());
    case AudioContentChannelCountValueRole: {
        const auto& channelCount = entity.body()
                                           .track()
                                           .mediaSource()
                                           .content()
                                           .audioMetadata()
                                           .channelCount();
        if (channelCount.isValid()) {
            return channelCount.value();
        } else {
            return QVariant{};
        }
    }
    case AudioContentSampleRateRole:
        return QVariant::fromValue(entity.body()
                                           .track()
                                           .mediaSource()
                                           .content()
                                           .audioMetadata()
                                           .sampleRate());
    case AudioContentSampleRateHzRole: {
        const auto& sampleRate = entity.body()
                                         .track()
                                         .mediaSource()
                                         .content()
                                         .audioMetadata()
                                         .sampleRate();
        if (sampleRate.isValid()) {
            return sampleRate.value();
        } else {
            return QVariant{};
        }
    }
    case AudioContentBitrateRole:
        return QVariant::fromValue(entity.body()
                                           .track()
                                           .mediaSource()
                                           .content()
                                           .audioMetadata()
                                           .bitrate());
    case AudioContentBitrateBpsRole: {
        const auto& bitrate = entity.body()
                                      .track()
                                      .mediaSource()
                                      .content()
                                      .audioMetadata()
                                      .bitrate();
        if (bitrate.isValid()) {
            return bitrate.value();
        } else {
            return QVariant{};
        }
    }
    case AudioContentReplayGainRole:
        return QVariant::fromValue(entity.body()
                                           .track()
                                           .mediaSource()
                                           .content()
                                           .audioMetadata()
                                           .replayGain());
    case AudioContentReplayGainRatioRole: {
        const auto& replayGain = entity.body()
                                         .track()
                                         .mediaSource()
                                         .content()
                                         .audioMetadata()
                                         .replayGain();
        if (replayGain.hasRatio()) {
            return replayGain.getRatio();
        } else {
            return QVariant{};
        }
    }
    case MusicMetricsBpmRole:
        return QVariant::fromValue(entity.body().track().musicMetrics().bpm());
    case MusicMetricsBpmValueRole: {
        const auto& bpm = entity.body().track().musicMetrics().bpm();
        if (bpm.isValid()) {
            return bpm.value();
        } else {
            return QVariant{};
        }
    }
    case MusicMetricsBpmLockedRole:
        return entity.body().track().musicMetrics().bpmLocked();
    case MusicMetricsChromaticKeyRole: {
        const auto& key = entity.body().track().musicMetrics().key();
        return key;
    }
    case MusicMetricsKeyLockedRole:
        return entity.body().track().musicMetrics().keyLocked();
    case TrackArtistRole: {
        const auto& artists = entity.body().track().artists();
        DEBUG_ASSERT(artists.size() <= 1);
        if (artists.isEmpty()) {
            return QVariant{};
        }
        return artists.first().name();
    }
    case TrackTitleRole: {
        const auto& titles = entity.body().track().titles();
        DEBUG_ASSERT(titles.size() <= 1);
        if (titles.isEmpty()) {
            return QVariant{};
        }
        return titles.first().name();
    }
    case AlbumArtistRole: {
        const auto& artists = entity.body().track().album().artists();
        DEBUG_ASSERT(artists.size() <= 1);
        if (artists.isEmpty()) {
            return QVariant{};
        }
        return artists.first().name();
    }
    case AlbumTitleRole: {
        const auto& titles = entity.body().track().album().titles();
        DEBUG_ASSERT(titles.size() <= 1);
        if (titles.isEmpty()) {
            return QVariant{};
        }
        return titles.first().name();
    }
    case ComposerRole: {
        const auto& actors = entity.body().track().actors(json::Actor::kRoleComposer);
        DEBUG_ASSERT(actors.size() <= 1);
        if (actors.isEmpty()) {
            return QVariant{};
        }
        return actors.first().name();
    }
    case GenresRole: {
        return rowItem.tags.genres();
    }
    case MoodsRole: {
        return rowItem.tags.moods();
    }
    case CommentRole:
        return rowItem.tags.comment();
    case GroupingRole:
        return rowItem.tags.grouping();
    case RecordedAtRole:
        return entity.body().track().recordedAt();
    case ReleasedAtRole:
        return entity.body().track().releasedAt();
    case ReleasedByRole:
        return entity.body().track().publisher();
    case CopyrightRole:
        return entity.body().track().copyright();
    case TrackNumbersRole:
        return entity.body().track().trackNumbers();
    case DiscNumbersRole:
        return entity.body().track().discNumbers();
    case RgbColorRole:
        return mixxx::RgbColor::toQVariant(entity.body().track().color());
    case QColorRole:
        return mixxx::RgbColor::toQColor(entity.body().track().color());
    default:
        DEBUG_ASSERT(!"unknown property role");
        return QVariant{};
    }
}

bool TrackSearchListModel::canFetchMore(
        const QModelIndex& parent) const {
    Q_UNUSED(parent)
    DEBUG_ASSERT(!parent.isValid());
    return m_params && m_canFetchMore;
}

void TrackSearchListModel::fetchMore(
        const QModelIndex& parent) {
    Q_UNUSED(parent)
    DEBUG_ASSERT(!parent.isValid());
    VERIFY_OR_DEBUG_ASSERT(m_params) {
        return;
    }
    loadNextPage(*m_params);
}

void TrackSearchListModel::abortPendingTask() {
    if (!m_pendingTask) {
        return;
    }
    auto* const pendingTask = m_pendingTask.data();
    pendingTask->disconnect(this);
    pendingTask->invokeAbort();
    pendingTask->deleteLater();
    m_pendingTask.clear();
    DEBUG_ASSERT(!isPending());
    emit pendingChanged(false);
}

void TrackSearchListModel::setParams(
        std::optional<TrackSearchListParams> params) {
    if (params) {
        params->normalize();
    }
    if (isPending()) {
        if (params && *params == m_pendingParams) {
            return;
        }
        abortPendingTask();
    }
    DEBUG_ASSERT(!isPending());
    if (m_params == params) {
        return;
    }
    if (params) {
        loadNextPage(*params);
    } else {
        resetModel();
        DEBUG_ASSERT(m_params == params);
        emit paramsChanged(m_params);
    }
}

void TrackSearchListModel::resetModel() {
    VERIFY_OR_DEBUG_ASSERT(!isPending()) {
        return;
    }
    beginResetModel();
    m_params = std::nullopt;
    m_rowItems.clear();
    m_canFetchMore = true;
    endResetModel();
}

Pagination TrackSearchListModel::nextPagination() const {
    return Pagination{
            static_cast<quint64>(m_rowItems.size()),
            validPageSize(m_pageSize),
    };
}

void TrackSearchListModel::loadNextPage(
        const TrackSearchListParams& params) {
    VERIFY_OR_DEBUG_ASSERT(!isPending()) {
        return;
    }
    auto* subsystem = m_subsystem.data();
    VERIFY_OR_DEBUG_ASSERT(subsystem) {
        return;
    }
    if (m_params && *m_params != params) {
        // Start a new list with different parameters
        resetModel();
    }
    m_pendingParams = params;
    m_pendingPagination = nextPagination();
    auto* const pendingTask = subsystem->searchTracks(
            m_pendingParams.baseQuery,
            m_pendingParams.overlayFilter,
            m_pendingParams.searchTerms,
            m_pendingPagination);
    DEBUG_ASSERT(pendingTask);
    m_pendingTask = pendingTask;
    connect(pendingTask,
            &SearchTracksTask::succeeded,
            this,
            &TrackSearchListModel::onPendingTaskSucceeded,
            Qt::UniqueConnection);
    connect(pendingTask,
            &SearchTracksTask::destroyed,
            this,
            &TrackSearchListModel::onPendingTaskDestroyed,
            Qt::UniqueConnection);
    pendingTask->invokeStart(kRequestTimeoutMillis);
    DEBUG_ASSERT(isPending());
    emit pendingChanged(true);
}

void TrackSearchListModel::onPendingTaskSucceeded(
        const QJsonArray& nextRows) {
    VERIFY_OR_DEBUG_ASSERT(sender() == m_pendingTask.data()) {
        return;
    }
    VERIFY_OR_DEBUG_ASSERT(static_cast<quint64>(m_rowItems.size()) == m_pendingPagination.offset) {
        kLogger.warning()
                << "Received mismatching page of rows starting at"
                << m_pendingPagination.offset
                << "instead of"
                << m_rowItems.size();
        return;
    }
    VERIFY_OR_DEBUG_ASSERT(static_cast<quint64>(nextRows.size()) <= m_pendingPagination.limit) {
        kLogger.warning()
                << "Received more rows than expected:"
                << nextRows.size()
                << '>'
                << m_pendingPagination.limit;
        return;
    }
    m_canFetchMore = static_cast<quint64>(nextRows.size()) >= m_pendingPagination.limit;
    if (!nextRows.isEmpty()) {
        const auto parentIndex = QModelIndex{};
        const int firstIndex = m_rowItems.size();
        const int lastIndex = firstIndex + nextRows.size() - 1;
        m_rowItems.reserve(m_rowItems.size() + nextRows.size());
        beginInsertRows(parentIndex, firstIndex, lastIndex);
        for (QJsonValue&& nextRow : nextRows) {
            DEBUG_ASSERT(nextRow.isArray());
            m_rowItems.push_back(TrackSearchListItem{json::TrackEntity{nextRow.toArray()}});
        }
        endInsertRows();
    }
    auto params = std::make_optional(m_pendingParams);
    if (m_params != params) {
        m_params = std::move(params);
        emit paramsChanged(m_params);
    }
}

void TrackSearchListModel::onPendingTaskDestroyed(
        QObject* obj) {
    Q_UNUSED(obj)
    if (isPending()) {
        // Another request is already pending
        return;
    }
    emit pendingChanged(false);
}

} // namespace aoide

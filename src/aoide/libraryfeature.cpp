#include "aoide/libraryfeature.h"

#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>

#include "aoide/collectionlistmodel.h"
#include "aoide/settings.h"
#include "aoide/tracksearchoverlayfilterdlg.h"
#include "aoide/tracktablemodel.h"
#include "aoide/web/exporttrackfilestask.h"
#include "aoide/web/listplayliststask.h"
#include "library/library.h"
#include "library/treeitem.h"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/assert.h"
#include "util/cmdlineargs.h"
#include "util/dnd.h"
#include "util/logger.h"
#include "widget/wlibrary.h"

namespace {

const mixxx::Logger kLogger("aoide LibraryFeature");

const QString kDefaultPlaylistKind = QStringLiteral("org.mixxx");
const QString kSessionPlaylistKind = QStringLiteral("org.mixxx.session");
const QString kAutoDjPlaylistKind = QStringLiteral("org.mixxx.autodj");

const QString kInitialSearch = QStringLiteral("");

QString defaultQueriesFilePath(
        const UserSettingsPointer& settings) {
    auto filePath = aoide::Settings(settings).queriesFilePath();
    if (filePath.isEmpty()) {
        filePath = CmdlineArgs::Instance().getSettingsPath();
    }
    return filePath;
}

QJsonArray loadQueries(const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        kLogger.warning()
                << "Failed to open file:"
                << fileName;
        return QJsonArray();
    }
    QByteArray jsonData = file.readAll();
    file.close();
    if (jsonData.isEmpty()) {
        kLogger.warning()
                << "Empty file"
                << fileName;
        return QJsonArray();
    }
    QJsonParseError parseError;
    const auto jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
    // QJsonDocument::fromJson() returns a non-null document
    // if parsing succeeds and otherwise null on error. The
    // parse error must only be evaluated if the returned
    // document is null!
    if (jsonDoc.isNull() &&
            parseError.error != QJsonParseError::NoError) {
        kLogger.warning()
                << "Failed to parse JSON data:"
                << parseError.errorString()
                << "at offset"
                << parseError.offset;
        return QJsonArray();
    }
    if (!jsonDoc.isArray()) {
        kLogger.warning()
                << "Expected a JSON array with groups and queries:"
                << jsonDoc;
        return QJsonArray();
    }
    return jsonDoc.array();
}

// Returns a null QString on success, otherwise an error message
QString saveQueries(
        const QString& fileName,
        const QJsonArray& queries) {
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        QByteArray jsonData =
                QJsonDocument(queries).toJson(QJsonDocument::Compact);
        const auto bytesWritten = file.write(jsonData);
        file.close();
        DEBUG_ASSERT(bytesWritten <= jsonData.size());
        if (bytesWritten >= jsonData.size()) {
            return QString(); // success
        }
        kLogger.warning()
                << "Failed to save queries into file:"
                << fileName
                << file.errorString();
    } else {
        kLogger.warning()
                << "Failed to open file for writing:"
                << fileName
                << file.errorString();
    }
    DEBUG_ASSERT(!file.errorString().isNull());
    return file.errorString();
}

} // anonymous namespace

namespace aoide {

LibraryFeature::LibraryFeature(
        Library* library,
        UserSettingsPointer settings,
        Subsystem* subsystem)
        : ::LibraryFeature(library, settings, QStringLiteral("aoide")),
          m_title(QStringLiteral("aoide")),
          m_queriesIcon(QStringLiteral(
                  ":/images/library/ic_library_tag-search-filter.svg")),
          m_sessionsIcon(
                  QStringLiteral(":/images/library/ic_library_history.svg")),
          m_trackSearchOverlayFilterAction(
                  new QAction(tr("Track search overlay filter..."), this)),
          m_loadQueriesAction(
                  new QAction(tr("Load queries..."), this)),
          m_saveQueriesAction(
                  new QAction(tr("Save queries..."), this)),
          m_refreshQueryResultsAction(
                  new QAction(tr("Refresh query results"), this)),
          m_exportQueryTrackFilesAction(
                  new QAction(tr("Export query track files"), this)),
          m_reloadSessionPlaylistsAction(new QAction(tr("Reload sessions"), this)),
          m_refreshSessionPlaylistEntriesAction(
                  new QAction(tr("Refresh session entries"), this)),
          m_subsystem(subsystem),
          m_collectionListModel(
                  make_parented<CollectionListModel>(subsystem, this)),
          m_trackTableModel(
                  make_parented<TrackTableModel>(
                          library->trackCollectionManager(), subsystem, this)),
          m_sidebarModel(make_parented<TreeItemModel>(this)),
          m_previousSearch(kInitialSearch) {
    restartSession(QDateTime::currentDateTime());

    m_sidebarModel->setRootItem(TreeItem::newRoot(this));

    // Actions
    connect(m_trackSearchOverlayFilterAction,
            &QAction::triggered,
            this,
            &LibraryFeature::slotTrackSearchOverlayFilter);
    connect(m_loadQueriesAction,
            &QAction::triggered,
            this,
            &LibraryFeature::slotLoadQueries);
    connect(m_saveQueriesAction,
            &QAction::triggered,
            this,
            &LibraryFeature::slotSaveQueries);
    connect(m_refreshQueryResultsAction,
            &QAction::triggered,
            this,
            &LibraryFeature::slotRefreshQueryResults);
    connect(m_exportQueryTrackFilesAction,
            &QAction::triggered,
            this,
            &LibraryFeature::slotExportQueryTrackFiles);
    connect(m_reloadSessionPlaylistsAction,
            &QAction::triggered,
            this,
            &LibraryFeature::slotReloadSessions);
    connect(m_refreshSessionPlaylistEntriesAction,
            &QAction::triggered,
            this,
            &LibraryFeature::slotRefreshSessionPlaylistEntries);

    // Subsystem
    connect(m_subsystem,
            &Subsystem::connected,
            this,
            [this]() {
                reloadSessionPlaylists();
            });
    connect(m_subsystem,
            &Subsystem::disconnected,
            this,
            [this]() {
                reloadSessionPlaylists();
            });
    connect(m_subsystem,
            &Subsystem::collectionsChanged,
            this,
            [this](int flags) {
                if (flags & Subsystem::CollectionsChangedFlags::ACTIVE_COLLECTION) {
                    reloadSessionPlaylists();
                }
            });

    QString queriesFilePath =
            Settings(m_pConfig).queriesFilePath();
    if (!queriesFilePath.isEmpty()) {
        reloadQueries(queriesFilePath);
    }

    kLogger.debug() << "Created instance" << this;
}

LibraryFeature::~LibraryFeature() {
    kLogger.debug() << "Destroying instance" << this;
}

void LibraryFeature::restartSession(
        QDateTime startedAt) {
    m_sessionCache.restart(*m_pLibrary->trackCollectionManager(), startedAt);
}

void LibraryFeature::onTrackLoadedIntoDeck(
        const QString& deckGroup,
        TrackPointer pTrack) {
    Q_UNUSED(deckGroup);
    VERIFY_OR_DEBUG_ASSERT(pTrack) {
        return;
    }
    m_sessionCache.updateTrack(pTrack->getId(), pTrack->getLocation(), pTrack->getPlayCounter());
    // Watch the play counter of a track.
    // Assumption: The location of a track does not change.
    connect(pTrack.get(),
            &Track::timesPlayedChanged,
            this,
            [this]() {
                auto* pTrack = static_cast<Track*>(sender());
                VERIFY_OR_DEBUG_ASSERT(pTrack) {
                    return;
                }
                m_sessionCache.updateTrack(pTrack->getId(), pTrack->getLocation(), pTrack->getPlayCounter());
            });
}

QVariant LibraryFeature::title() {
    return m_title;
}

void LibraryFeature::bindLibraryWidget(
        WLibrary* /*libraryWidget*/, KeyboardEventFilter* /*keyboard*/) {
}

void LibraryFeature::bindSidebarWidget(WLibrarySidebar* /*sidebarWidget*/) {
}

TreeItemModel* LibraryFeature::sidebarModel() const {
    return m_sidebarModel;
}

void LibraryFeature::activate() {
    emit showTrackModel(m_trackTableModel);
    emit enableCoverArtDisplay(true);
}

void LibraryFeature::activateChild(const QModelIndex& index) {
    const auto currentSearch = m_trackTableModel->searchText();
    if (!currentSearch.isNull()) {
        m_previousSearch = currentSearch;
    }
    auto query = queryAt(index);
    if (query.isEmpty()) {
        const auto sessionPlaylist = sessionPlaylistAt(index);
        if (sessionPlaylist.isEmpty()) {
            // Nothing
            if (m_activeChildIndex != index) {
                // Initial activation
                m_activeChildIndex = index;
                m_trackTableModel->reset();
            }
        } else {
            // Activate playlist
            if (m_activeChildIndex != index) {
                // Initial activation
                m_activeChildIndex = index;
                // TODO: Populate table model with playlist entries
                m_trackTableModel->reset();
            }
        }
    } else {
        // Activate prepared query
        if ((m_activeChildIndex != index) ||
                m_trackTableModel->searchText().isNull()) {
            // Initial activation
            m_activeChildIndex = index;
            DEBUG_ASSERT(!m_previousSearch.isNull());
            m_trackTableModel->searchTracks(
                    query,
                    m_trackSearchOverlayFilter,
                    m_previousSearch);
        }
        emit restoreSearch(m_trackTableModel->searchText());
    }
    activate();
    emit switchToView(m_title);
}

void LibraryFeature::reactivateChild() {
    auto activeIndex = m_activeChildIndex;
    m_activeChildIndex = QModelIndex();
    activateChild(activeIndex);
}

QJsonObject LibraryFeature::queryAt(const QModelIndex& index) const {
    if (!index.isValid()) {
        return QJsonObject{};
    }
    auto parentIndex = index;
    while (parentIndex.parent().parent().isValid()) {
        parentIndex = parentIndex.parent();
    }
    if (parentIndex.row() != 0) {
        return QJsonObject{};
    }
    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    if (!item) {
        return QJsonObject{};
    }
    return item->getData().toJsonObject();
}

json::PlaylistWithEntriesSummaryEntity LibraryFeature::sessionPlaylistAt(
        const QModelIndex& index) const {
    if (!index.isValid()) {
        return json::PlaylistWithEntriesSummaryEntity();
    }
    auto parentIndex = index;
    while (parentIndex.parent().parent().isValid()) {
        parentIndex = parentIndex.parent();
    }
    if (parentIndex.row() != 1) {
        return json::PlaylistWithEntriesSummaryEntity();
    }
    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    if (!item) {
        return json::PlaylistWithEntriesSummaryEntity();
    }
    int row = item->getData().toInt();
    if (row < 0 || row >= m_sessionPlaylists.size()) {
        return json::PlaylistWithEntriesSummaryEntity();
    }
    return m_sessionPlaylists.at(row);
}

namespace {

std::vector<std::unique_ptr<TreeItem>> buildQuerySubtreeModel(
        const QJsonArray& jsonItems) {
    std::vector<std::unique_ptr<TreeItem>> treeItems;
    treeItems.reserve(jsonItems.size());
    for (auto i = 0; i < jsonItems.size(); ++i) {
        if (!jsonItems[i].isObject()) {
            kLogger.warning() << "invalid JSON item" << jsonItems[i];
            continue;
        }
        const auto& jsonItem = jsonItems[i].toObject();
        auto treeItem =
                std::make_unique<TreeItem>(jsonItem.value("label").toString());
        treeItem->setToolTip(jsonItem.value("notes").toString());
        auto jsonQuery = jsonItem.value("query");
        if (!jsonQuery.isUndefined() && !jsonQuery.isNull()) {
            if (jsonQuery.isObject()) {
                treeItem->setData(jsonQuery);
            } else {
                kLogger.warning() << "Tree item" << treeItem->getLabel()
                                  << "contains invalid query" << jsonQuery;
            }
        }
        auto jsonItems = jsonItem.value("items");
        if (!jsonItems.isUndefined() && !jsonItems.isNull()) {
            if (jsonItems.isArray()) {
                treeItem->insertChildren(
                        treeItem->childRows(),
                        buildQuerySubtreeModel(jsonItems.toArray()));
            } else {
                kLogger.warning() << "Tree item" << treeItem->getLabel()
                                  << "contains invalid child items" << jsonItems;
            }
        }
        treeItems.push_back(std::move(treeItem));
    }
    return treeItems;
}

std::vector<std::unique_ptr<TreeItem>> buildSessionSubtreeModel(
        const QVector<json::PlaylistWithEntriesSummaryEntity>& playlistEntites) {
    std::vector<std::unique_ptr<TreeItem>> treeItems;
    treeItems.reserve(playlistEntites.size());
    for (int i = 0; i < playlistEntites.size(); ++i) {
        const auto playlist = playlistEntites[i].body();
        auto label = QString("%1 (%2)")
                             .arg(playlist.title())
                             .arg(playlist.entries().totalTracksCount());
        auto treeItem = std::make_unique<TreeItem>(std::move(label), i);
        treeItem->setToolTip(playlist.notes());
        treeItems.push_back(std::move(treeItem));
    }
    return treeItems;
}

} // anonymous namespace

void LibraryFeature::rebuildChildModel() {
    TreeItem* rootItem = m_sidebarModel->getRootItem();
    VERIFY_OR_DEBUG_ASSERT(rootItem) {
        return;
    }
    m_sidebarModel->removeRows(0, rootItem->childRows());

    auto queriesRoot =
            std::make_unique<TreeItem>(tr("Queries"));
    queriesRoot->setIcon(m_queriesIcon);
    queriesRoot->insertChildren(
            queriesRoot->childRows(),
            buildQuerySubtreeModel(m_queries));

    auto sessionsRoot =
            std::make_unique<TreeItem>(tr("Sessions"));
    sessionsRoot->setIcon(m_sessionsIcon);
    sessionsRoot->insertChildren(
            sessionsRoot->childRows(),
            buildSessionSubtreeModel(m_sessionPlaylists));

    auto rootRows = std::vector<std::unique_ptr<TreeItem>>{};
    rootRows.push_back(std::move(queriesRoot));
    rootRows.push_back(std::move(sessionsRoot));
    m_sidebarModel->insertTreeItemRows(std::move(rootRows), 0, {});
}

void LibraryFeature::onRightClick(const QPoint& globalPos) {
    // TODO
    Q_UNUSED(globalPos);
}

void LibraryFeature::onRightClickChild(
        const QPoint& globalPos, const QModelIndex& index) {
    kLogger.debug() << "onRightClickChild" << index;
    VERIFY_OR_DEBUG_ASSERT(index.isValid()) {
        return;
    }
    const auto parentIndex = index.parent();
    DEBUG_ASSERT(parentIndex.isValid());
    if (!parentIndex.parent().isValid()) {
        // 1st level
        DEBUG_ASSERT(index.column() == 0);
        switch (index.row()) {
        case 0: {
            // Queries
            QMenu menu;
            menu.addAction(m_trackSearchOverlayFilterAction);
            menu.addSeparator();
            menu.addAction(m_loadQueriesAction);
            menu.addAction(m_saveQueriesAction);
            menu.exec(globalPos);
            return;
        }
        case 1: {
            // Sessions
            QMenu menu;
            menu.addAction(m_reloadSessionPlaylistsAction);
            menu.exec(globalPos);
            return;
        }
        default:
            DEBUG_ASSERT(!"unreachable");
        }
    }
    DEBUG_ASSERT(parentIndex.parent().isValid());
    if (!parentIndex.parent().parent().isValid()) {
        // 2nd level
        if (parentIndex.row() == 1) {
            // Session item
            DEBUG_ASSERT(index.column() == 0); // no nesting (yet)
            if (m_activeChildIndex != index) {
                activateChild(index);
            }
            QMenu menu;
            menu.addAction(m_refreshSessionPlaylistEntriesAction);
            menu.exec(globalPos);
            return;
        }
    }
    // Prepared query item
    if (m_activeChildIndex != index) {
        activateChild(index);
    }
    auto query = queryAt(index);
    if (query.isEmpty()) {
        return;
    }
    QMenu menu;
    menu.addAction(m_refreshQueryResultsAction);
    menu.addAction(m_exportQueryTrackFilesAction);
    menu.exec(globalPos);
}

void LibraryFeature::slotTrackSearchOverlayFilter() {
    auto dlg = TrackSearchOverlayFilterDlg{
            m_sessionCache.startedAt(),
            m_trackSearchOverlayFilter};
    if (dlg.exec() == QDialog::Accepted) {
        if (m_sessionCache.startedAt() != dlg.sessionStartedAt()) {
            restartSession(dlg.sessionStartedAt());
        }
        m_trackSearchOverlayFilter = dlg.overlayFilter();
        // Refresh the search results
        m_trackTableModel->searchTracks(
                m_trackSearchOverlayFilter,
                m_previousSearch);
    }
}

void LibraryFeature::slotLoadQueries() {
    const auto msgBoxTitle = tr("aoide: Load Queries from File");
    const auto filePath = QFileDialog::getOpenFileName(nullptr,
            msgBoxTitle,
            defaultQueriesFilePath(m_pConfig),
            "*.json");
    if (filePath.isEmpty()) {
        kLogger.info() << "No file with queries selected";
        return;
    }
    if (!reloadQueries(filePath)) {
        // TODO: Display more detailed error message
        QMessageBox(QMessageBox::Warning,
                msgBoxTitle,
                tr("Failed to load queries.") +
                        QStringLiteral("\n\n") + filePath,
                QMessageBox::Close)
                .exec();
    }
}

void LibraryFeature::slotSaveQueries() {
    const auto msgBoxTitle = tr("aoide: Save Queries into File");
    const auto filePath = QFileDialog::getSaveFileName(nullptr,
            msgBoxTitle,
            defaultQueriesFilePath(m_pConfig),
            "*.json");
    if (filePath.isEmpty()) {
        kLogger.info() << "No file for saving queries selected";
        return;
    }
    const auto errorMessage = saveQueries(filePath, m_queries);
    if (errorMessage.isNull()) {
        Settings(m_pConfig).setQueriesFilePath(filePath);
        QMessageBox(QMessageBox::Information,
                msgBoxTitle,
                tr("Saved queries.") + QStringLiteral("\n\n") +
                        filePath,
                QMessageBox::Ok)
                .exec();
    } else {
        QMessageBox(QMessageBox::Warning,
                msgBoxTitle,
                tr("Failed to save queries:") + QChar(' ') +
                        errorMessage + QStringLiteral("\n\n") + filePath,
                QMessageBox::Close)
                .exec();
    }
}

bool LibraryFeature::reloadQueries(const QString& filePath) {
    auto queries = loadQueries(filePath);
    if (queries.isEmpty()) {
        kLogger.warning() << "Failed to load queries from file:"
                          << filePath;
        return false;
    }
    m_queries = queries;
    Settings(m_pConfig).setQueriesFilePath(filePath);
    // TODO: Only rebuild the subtree underneath the queries
    // node instead of the whole child model
    rebuildChildModel();
    return true;
}

bool LibraryFeature::reloadSessionPlaylists() {
    if (!m_subsystem ||
            !m_subsystem->isConnected() ||
            !m_subsystem->activeCollection()) {
        m_sessionPlaylists.clear();
        // TODO: Only rebuild the subtree underneath the queries
        // node instead of the whole child model
        rebuildChildModel();
        return false;
    }
    auto* reloadSessionPlaylistsTask = m_reloadSessionPlaylistsTask.data();
    if (reloadSessionPlaylistsTask) {
        kLogger.info() << "Discarding pending request"
                       << "for loading sessions";
        reloadSessionPlaylistsTask->disconnect(this);
        reloadSessionPlaylistsTask->invokeAbort();
        reloadSessionPlaylistsTask->deleteLater();
        m_reloadSessionPlaylistsTask.clear();
    }
    reloadSessionPlaylistsTask = m_subsystem->listPlaylists(kSessionPlaylistKind);
    DEBUG_ASSERT(reloadSessionPlaylistsTask);
    connect(reloadSessionPlaylistsTask,
            &ListPlaylistsTask::succeeded,
            this,
            &LibraryFeature::slotReloadSessionsTaskSucceeded,
            Qt::UniqueConnection);
    reloadSessionPlaylistsTask->invokeStart();
    m_reloadSessionPlaylistsTask = reloadSessionPlaylistsTask;
    return true;
}

void LibraryFeature::slotReloadSessionsTaskSucceeded(
        const QVector<json::PlaylistWithEntriesSummaryEntity>& result) {
    auto* finishedListPlaylistsTask =
            qobject_cast<ListPlaylistsTask*>(sender());
    VERIFY_OR_DEBUG_ASSERT(finishedListPlaylistsTask) {
        return;
    }
    const auto finishedListPlaylistsTaskDeleter =
            mixxx::ScopedDeleteLater(finishedListPlaylistsTask);

    VERIFY_OR_DEBUG_ASSERT(finishedListPlaylistsTask ==
            m_reloadSessionPlaylistsTask.data()) {
        return;
    }
    m_reloadSessionPlaylistsTask.clear();

    m_sessionPlaylists = std::move(result);
    // TODO: Only rebuild the subtree underneath the sessions
    // node instead of the whole child model
    rebuildChildModel();
}

void LibraryFeature::slotRefreshQueryResults() {
    reactivateChild();
}

void LibraryFeature::slotExportQueryTrackFiles() {
    const auto query = queryAt(m_activeChildIndex);
    VERIFY_OR_DEBUG_ASSERT(!query.isEmpty()) {
        return;
    }
    const auto filterValue = query.value(QStringLiteral("filter"));
    VERIFY_OR_DEBUG_ASSERT(filterValue.isObject()) {
        return;
    }
    const auto trackFilter = filterValue.toObject();
    VERIFY_OR_DEBUG_ASSERT(!trackFilter.isEmpty()) {
        return;
    }
    QString targetRootPath = QFileDialog::getExistingDirectory(nullptr,
            tr("Choose target root directory for exporting track files"),
            QStandardPaths::writableLocation(
                    QStandardPaths::MusicLocation));
    if (targetRootPath.isEmpty()) {
        return;
    }
    const auto targetRootDir = QDir{targetRootPath};
    VERIFY_OR_DEBUG_ASSERT(targetRootDir.exists() && targetRootDir.isAbsolute()) {
        return;
    }
    auto* const exportTrackFilesTask = m_subsystem.data()->exportTrackFiles(
            trackFilter,
            targetRootPath);
    DEBUG_ASSERT(exportTrackFilesTask);
    connect(exportTrackFilesTask, &ExportTrackFilesTask::succeeded, [=](const QJsonObject& outcome) {
                qInfo() << "Exported track files:" << outcome;
                exportTrackFilesTask->deleteLater(); });
    connect(exportTrackFilesTask, &ExportTrackFilesTask::failed, [=](const mixxx::network::JsonWebResponse& response) {
                qWarning() << "Failed to export track files:" << response;
                exportTrackFilesTask->deleteLater(); });
    connect(exportTrackFilesTask, &ExportTrackFilesTask::networkError, [=](QNetworkReply::NetworkError errorCode, const QString& errorString, const mixxx::network::WebResponseWithContent& responseWithContent) {
                qWarning() << "Could not export track files:" << errorCode << errorString << responseWithContent;
                exportTrackFilesTask->deleteLater(); });
    qInfo() << "Export track files"
               << "using filter" << trackFilter
               << "into target directory" << targetRootDir;
    exportTrackFilesTask->invokeStart();
}

void LibraryFeature::slotReloadSessions() {
    reloadSessionPlaylists();
}

void LibraryFeature::slotRefreshSessionPlaylistEntries() {
    reactivateChild();
}

bool LibraryFeature::dragMoveAcceptChild(
        const QModelIndex& index,
        const QList<QUrl>& urls) {
    VERIFY_OR_DEBUG_ASSERT(index.isValid()) {
        return false;
    }
    const auto playlist = sessionPlaylistAt(index);
    if (playlist.isEmpty()) {
        // Dropping is only supported on sessions
        return false;
    }

    return DragAndDropHelper::urlsContainSupportedTrackFiles(urls, true);
}

bool LibraryFeature::dropAcceptChild(
        const QModelIndex& index,
        const QList<QUrl>& urls,
        QObject* pSource) {
    VERIFY_OR_DEBUG_ASSERT(index.isValid()) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(!urls.isEmpty()) {
        return false;
    }
    DEBUG_ASSERT(pSource);
    Q_UNUSED(pSource);
    const auto playlist = sessionPlaylistAt(index);
    if (playlist.isEmpty()) {
        // Dropping is only supported on sessions
        return false;
    }
    kLogger.warning()
            << "Adding tracks to playlist is not implemented:"
            << playlist
            << urls;
    return false;
}

} // namespace aoide

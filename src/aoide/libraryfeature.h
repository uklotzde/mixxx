#pragma once

#include <QAction>
#include <QJsonArray>
#include <QPointer>

#include "aoide/json/playlist.h"
#include "aoide/sessioncache.h"
#include "aoide/tracksearchoverlayfilter.h"
#include "library/libraryfeature.h"
#include "library/treeitemmodel.h"
#include "track/playcounter.h"
#include "util/parented_ptr.h"
#include "util/qt.h"

class Library;

namespace aoide {

class Subsystem;
class ListPlaylistsTask;
class CollectionListModel;
class TrackTableModel;

class LibraryFeature : public ::LibraryFeature {
    Q_OBJECT

  public:
    LibraryFeature(
            Library* library,
            UserSettingsPointer settings,
            Subsystem* subsystem);
    ~LibraryFeature() override;

    QVariant title() override;

    void bindLibraryWidget(
            WLibrary* libraryWidget,
            KeyboardEventFilter* keyboard) override;
    void bindSidebarWidget(
            WLibrarySidebar* sidebarWidget) override;
    TreeItemModel* sidebarModel() const override;

    bool hasTrackTable() override {
        return true;
    }

    bool dragMoveAcceptChild(
            const QModelIndex& index,
            const QList<QUrl>& urls) override;
    bool dropAcceptChild(
            const QModelIndex& index,
            const QList<QUrl>& urls,
            QObject* pSource) override;

    const SessionCache& sessionCache() const {
        return m_sessionCache;
    }

    void restartSession(QDateTime startedAt = {});

  public slots:
    void activate() override;
    void activateChild(
            const QModelIndex& index) override;
    void onRightClick(
            const QPoint& globalPos) override;
    void onRightClickChild(
            const QPoint& globalPos,
            const QModelIndex& index) override;

    void onTrackLoadedIntoDeck(const QString& deckGroup, TrackPointer pTrack);

  private slots:
    void slotTrackSearchOverlayFilter();

    void slotLoadQueries();
    void slotSaveQueries();
    void slotRefreshQueryResults();
    void slotExportQueryTrackFiles();

    void slotReloadSessions();
    void slotRefreshSessionPlaylistEntries();
    void slotReloadSessionsTaskSucceeded(
            const QVector<json::PlaylistWithEntriesSummaryEntity>& result);

    void reactivateChild();

  private:
    const QString m_title;

    const QIcon m_queriesIcon;
    const QIcon m_sessionsIcon;

    SessionCache m_sessionCache;

    QAction* const m_trackSearchOverlayFilterAction;

    QAction* const m_loadQueriesAction;
    QAction* const m_saveQueriesAction;

    QAction* const m_refreshQueryResultsAction;
    QAction* const m_exportQueryTrackFilesAction;

    QAction* const m_reloadSessionPlaylistsAction;
    QAction* const m_refreshSessionPlaylistEntriesAction;

    const QPointer<Subsystem> m_subsystem;

    const parented_ptr<CollectionListModel> m_collectionListModel;

    const parented_ptr<TrackTableModel> m_trackTableModel;

    parented_ptr<TreeItemModel> m_sidebarModel;

    QJsonArray m_queries;

    TrackSearchOverlayFilter m_trackSearchOverlayFilter;

    QVector<json::PlaylistWithEntriesSummaryEntity> m_sessionPlaylists;

    bool reloadQueries(const QString& filePath);
    bool reloadSessionPlaylists();

    void rebuildChildModel();

    QJsonObject queryAt(
            const QModelIndex& index) const;
    json::PlaylistWithEntriesSummaryEntity sessionPlaylistAt(
            const QModelIndex& index) const;

    QModelIndex m_activeChildIndex;

    QString m_previousSearch;

    mixxx::SafeQPointer<ListPlaylistsTask> m_reloadSessionPlaylistsTask;
};

} // namespace aoide

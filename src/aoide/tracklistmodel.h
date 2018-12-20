#pragma once

#include <QAbstractListModel>
#include <QList>

#include "aoide/subsystem.h"

namespace mixxx {

namespace aoide {

class TrackListModel: public QAbstractListModel {
    Q_OBJECT

  public:
    typedef AoideTrackEntity Item;

    enum ItemDataRole {
        Uid = Qt::UserRole,
        RevisionOrdinal,
        RevisionTimestamp,
        ContentType,
        ContentUri,
        ContentUrl, // QUrl of the decoded URI string
        AudioChannelsCount,
        AudioChannelsLayout,
        AudioDurationMs,
        AudioSampleRateHz,
        AudioBitRateBps,
        AudioLoudnessItuBs1770Lufs,
        AudioEncoderName,
        AudioEncoderSettings,
        Title,
        Artist,
        Composer,
        Genre,
        Comment,
        Grouping,
        AlbumTitle,
        AlbumArtist,
        MusicTempoBpm,
        // TODO: Add missing roles...
    };

    explicit TrackListModel(
            QPointer<Subsystem> subsystem,
            QObject* parent = nullptr);
    ~TrackListModel() override;

    QHash<int, QByteArray> roleNames() const override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    QModelIndex parent(const QModelIndex& index) const override;

    const Item& itemAt(const QModelIndex& index) const;
    QVariant itemData(const Item& item, ItemDataRole role) const;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;

    const QString& searchText() const {
        return m_searchText;
    }

  public slots:
    void searchTracks(
            QString phraseQuery);

  private slots:
    void searchTracksResult(
            mixxx::AsyncRestClient::RequestId requestId,
            QVector<Item> result);

  private:
    const QPointer<Subsystem> m_subsystem;

    int m_itemsPerPage;

    QString m_searchText;

    Subsystem::RequestId m_pendingRequestId;
    int m_pendingRequestFirstRow;
    int m_pendingRequestLastRow;

    QString m_phraseQuery;

    struct ItemPage {
        ItemPage(int firstRow, QVector<Item> items)
            : m_firstRow(firstRow),
              m_items(std::move(items)) {
        }
        int m_firstRow;
        QVector<Item> m_items;
    };

    QList<ItemPage> m_itemPages;

    int findItemPageIndex(int row) const;
};

} // namespace aoide

} // namespace mixxx

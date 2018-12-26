#pragma once

#include <QListView>
#include <QPointer>

#include "library/libraryview.h"

namespace mixxx {

namespace aoide {

class TrackListModel;

class TrackListView : public QListView, public LibraryView {
    Q_OBJECT

  public:
    explicit TrackListView(
            TrackListModel* model,
            QWidget* parent = nullptr);
    ~TrackListView() override;

    void onShow() override;
    bool hasFocus() const override;
    void onSearch(const QString& text) override;

    void setModel(QAbstractItemModel* model) final;

  private:
    QPointer<TrackListModel> m_model;
};

} // namespace aoide

} // namespace mixxx

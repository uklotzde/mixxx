#pragma once

#include <QtQuickWidgets/QQuickWidget>
#include <QPointer>
#include <QWidget>

#include "library/libraryview.h"

namespace mixxx {

namespace aoide {

class TrackListModel;

class TrackListView : public QQuickWidget, public LibraryView {
    Q_OBJECT

  public:
    explicit TrackListView(
            TrackListModel* model,
            QWidget* parent = nullptr);
    ~TrackListView() override;

    void onShow() override;
    bool hasFocus() const override;
    void onSearch(const QString& text) override;

  private slots:
    void quickWidgetStatusChanged(QQuickWidget::Status status);
    void sceneGraphError(QQuickWindow::SceneGraphError error, const QString& message);

  private:
    const QPointer<TrackListModel> m_model;
};

} // namespace aoide

} // namespace mixxx

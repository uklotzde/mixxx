#pragma once

#include <QPointer>

#include "library/libraryfeature.h"
#include "library/treeitemmodel.h"

namespace mixxx {

namespace aoide {

class Subsystem;
class TrackListModel;
class TrackListView;

class LibraryFeature: public ::LibraryFeature {
    Q_OBJECT

  public:
    explicit LibraryFeature(
            QPointer<Subsystem> subsystem,
            UserSettingsPointer settings,
            QObject* parent = nullptr);
    ~LibraryFeature() override;

    QVariant title() override;
    QIcon getIcon() override;

    void bindWidget(WLibrary* libraryWidget, KeyboardEventFilter* keyboard) override;
    TreeItemModel* getChildModel() override;

  public slots:
    void activate() override;

  private:
    const QString m_title;
    const QIcon m_icon;

    const QPointer<TrackListModel> m_trackListModel;

    QPointer<TrackListView> m_trackListView;

    TreeItemModel m_childModel;
};

} // namespace aoide

} // namespace mixxx

#include "aoide/libraryfeature.h"

#include "aoide/tracklistmodel.h"
#include "aoide/tracklistview.h"

#include "util/assert.h"
#include "util/logger.h"

#include "widget/wlibrary.h"

namespace mixxx {

namespace aoide {

namespace {

const Logger kLogger("aoide LibraryFeature");

} // anonymous namespace

LibraryFeature::LibraryFeature(
        QPointer<Subsystem> subsystem,
        UserSettingsPointer settings,
        QObject* parent)
        : ::LibraryFeature(settings, parent),
            m_title(tr("aoide")),
            m_trackListModel(new TrackListModel(std::move(subsystem), this)),
            m_childModel(this) {
    kLogger.debug() << "Created instance" << this;
}

LibraryFeature::~LibraryFeature() {
    kLogger.debug() << "Destroying instance" << this;
}

QVariant LibraryFeature::title() {
    return m_title;
}

QIcon LibraryFeature::getIcon() {
    return m_icon;
}

void LibraryFeature::bindWidget(
        WLibrary* libraryWidget,
        KeyboardEventFilter* /*keyboard*/) {
    DEBUG_ASSERT(!m_trackListView);
    m_trackListView = new TrackListView(m_trackListModel, libraryWidget);
    libraryWidget->registerView(m_title, m_trackListView);
}

TreeItemModel* LibraryFeature::getChildModel() {
    return &m_childModel;
}

void LibraryFeature::activate() {
    emit switchToView(m_title);
    if (!m_trackListModel->phraseQuery().isNull()) {
        emit restoreSearch(m_trackListModel->phraseQuery());
    }
}

} // namespace aoide

} // namespace mixxx

#include "aoide/tracklistview.h"

#include "aoide/tracklistmodel.h"

#include "util/assert.h"
#include "util/logger.h"

namespace mixxx {

namespace aoide {

namespace {

const Logger kLogger("aoide TrackListView");

} // anonymous namespace

TrackListView::TrackListView(
        TrackListModel* model,
        QWidget* parent)
        : QListView(parent) {
    setModel(model);
    kLogger.debug() << "Created instance" << this;
}

TrackListView::~TrackListView() {
    kLogger.debug() << "Destroying instance" << this;
}

void TrackListView::setModel(QAbstractItemModel* model) {
    DEBUG_ASSERT(dynamic_cast<TrackListModel*>(model));
    QListView::setModel(model);
    m_model = static_cast<TrackListModel*>(model);
}

void TrackListView::onShow() {
    DEBUG_ASSERT(m_model);
}

bool TrackListView::hasFocus() const {
    return QListView::hasFocus();
}

void TrackListView::onSearch(const QString& text) {
    m_model->searchTracks(text);
}

} // namespace aoide

} // namespace mixxx
